#include "Bench.hpp"
#include "GangedRandomLFO.hpp"
#include "PartialMachine.hpp"
#include "PolyXFader.hpp"
#include "QuadDelay.hpp"
#include "QuadMasterChain.hpp"
#include "QuadReverb.hpp"
#include <cstring>
#include <type_traits>

constexpr size_t x_rate = SampleTimer::x_sampleRate;
constexpr size_t x_ratio = x_rate / 48000;

template <typename V>
void Put(V& v, size_t c, float x)
{
    if constexpr (std::is_arithmetic_v<V>)
    {
        v = x;
    }
    else
    {
        v[c] = x;
    }
}

template <typename V>
std::array<float, 4> Result(const V& v, size_t channels)
{
    std::array<float, 4> result{};
    for (size_t c = 0; c < channels; ++c)
    {
        result[c] = v[c];
    }

    return result;
}

float Motion(const Signals& signals, size_t n, size_t c, int mode)
{
    return (mode == 3 || (mode == 2 && c == 1)) ? signals.m_controls[n % signals.m_period][c] : 0;
}

struct ClockViews
{
    static constexpr size_t x_stride = 1;
    TheoryOfTime m_clock;
    TheoryOfTime::Input m_input;
    SmartGrid::MessageOutBuffer m_messages;
    std::vector<TheoryOfTime> m_views;
    ClockViews() : m_views(128 * x_ratio / x_stride)
    {
        m_clock.SetupMessageOutBuffer(&m_messages);
        m_input.m_running = true;
        m_input.m_freq = 1.0 / x_rate;
        for (size_t i = 0; i < TheoryOfTimeBase::x_numLoops; ++i)
        {
            m_input.m_input[i].m_parentIndex = static_cast<int>(i + 1);
            m_input.m_input[i].m_parentMult = 2;
        }
    }

    void PrepareBlock(size_t, size_t)
    {
        for (auto& view : m_views)
        {
            m_clock.Process(m_input);
            view.m_sample = m_clock.m_sample;
            view.m_globalPeriodSamples = m_clock.m_globalPeriodSamples;
            m_messages.Clear();
        }
    }

    TheoryOfTime* Get(size_t n)
    {
        return &m_views[(n % (128 * x_ratio)) / x_stride];
    }
};

template <int Mode>
struct Reverb
{
    Signals m_signals{x_rate};
    QuadReverb m_core;
    QuadReverbInputSetter m_setter;
    QuadReverb::Input m_input;
    QuadReverbInputSetter::Input m_knobs;
    Reverb()
    {
        Set(0);
        m_setter.Process(m_knobs, m_input);
        if constexpr (Mode == 0)
        {
            for (size_t c = 0; c < 4; ++c)
            {
                m_input.m_delayTimeSamples[c] = (2400 + 400 * c) * x_ratio;
                m_input.m_feedback[c] = 0.7f;
                m_input.m_widen[c] = 1.0f - 0.003f * c;
                m_input.m_bffBase[c] = 90.0f / x_rate;
                m_input.m_bffWidth[c] = 100;
            }
        }
    }

    void Set(size_t n)
    {
        for (size_t c = 0; c < 4; ++c)
        {
            float move = Motion(m_signals, n, c, Mode);
            m_knobs.m_delayOffsetKnob[c] = 0.5f + 0.03f * c + 0.2f * move;
            m_knobs.m_reverbTimeKnob[c] = 0.55f + 0.04f * c + 0.06f * move;
            m_knobs.m_feedbackKnob[c] = 0.65f + 0.02f * c + 0.025f * move;
            m_knobs.m_dampingBaseKnob[c] = 0.18f + 0.01f * c + 0.03f * move;
            m_knobs.m_dampingWidthKnob[c] = 0.61f - 0.02f * c + 0.04f * move;
        }
    }

    std::array<float, 4> Process(size_t n)
    {
        if constexpr (Mode != 0)
        {
            Set(n);
            m_setter.Process(m_knobs, m_input);
        }

        for (size_t c = 0; c < 4; ++c)
        {
            m_input.m_input[c] = m_signals.m_audio[n % m_signals.m_period][c];
        }

        auto value = m_core.Process(m_input);
        m_input.m_return = value;
        return Result(value, 4);
    }
};

template <int Mode>
struct Delay
{
    Signals m_signals{x_rate};
    QuadDelay m_core;
    QuadDelayInputSetter m_setter;
    QuadDelay::Input m_input;
    QuadDelayInputSetter::Input m_knobs;
    ClockViews m_clock;
    Delay()
    {
        for (size_t c = 0; c < 4; ++c)
        {
            m_input.m_feedback[c] = 0.35f;
            m_input.m_rotate[c] = 0.1f * c;
            m_input.m_bffBase[c] = 60.0f / x_rate;
            m_input.m_bffWidth[c] = 150;
            m_knobs.m_loopSelectorSwitchVal[c] = static_cast<int>(c % 2);
            m_knobs.m_delayTimeFactorSwitchVal[c] = 3;
            m_knobs.m_readHeadSpeedSwitchVal[c] = 10;
            m_knobs.m_rotateSwitchVal[c] = static_cast<int>(c + 1);
            m_knobs.m_resynthShiftSwitchVal[c] = Mode >= 2 ? 12 : 9;
        }
    }

    void PrepareBlock(size_t n, size_t count)
    {
        if constexpr (Mode != 0)
        {
            m_clock.PrepareBlock(n, count);
        }
    }

    std::array<float, 4> Process(size_t n)
    {
        if constexpr (Mode != 0)
        {
            for (size_t c = 0; c < 4; ++c)
            {
                float move = Motion(m_signals, n, c, Mode);
                m_knobs.m_feedbackKnob[c] = 0.4f + 0.025f * move;
                m_knobs.m_dampingBaseKnob[c] = 0.12f + 0.01f * c + 0.03f * move;
                m_knobs.m_dampingWidthKnob[c] = 0.8f - 0.02f * c + 0.04f * move;
                m_knobs.m_resynthSlewUpKnob[c] = 0.15f;
                m_knobs.m_resynthShiftFadeKnob[c] = Mode >= 2 ? 0.45f + 0.1f * move : 0;
                m_knobs.m_resynthUnisonKnob[c] = Mode >= 2 ? 0.15f + 0.06f * move : 0;
            }

            m_knobs.m_theoryOfTime = m_clock.Get(n);
            m_setter.Process(m_knobs, m_input, &m_core);
        }
        else
        {
            double write = static_cast<double>(n) / x_ratio;
            m_input.m_writeHeadPosition = write;
            for (size_t c = 0; c < 4; ++c)
            {
                m_input.m_readHeadPosition[c] = write - (12000 + 2000 * c);
            }
        }

        for (size_t c = 0; c < 4; ++c)
        {
            m_input.m_input[c] = m_signals.m_audio[n % m_signals.m_period][c];
        }

        auto value = m_core.Process(m_input);
        m_input.m_return = value;
        return Result(value, 4);
    }
};

template <int Mode>
struct Partial
{
    Signals m_signals{x_rate};
    PartialMachine m_core;
    PartialMachine::InputSetter m_setter;
    PartialMachine::Input m_input;
    PartialMachine::InputSetter::Input m_knobs;
    Partial()
    {
        Set(0);
        m_setter.SetInput(m_knobs, m_input);
    }

    void Set(size_t n)
    {
        for (size_t c = 0; c < 4; ++c)
        {
            float move = Motion(m_signals, n, c, Mode);
            m_knobs.m_parameterLinearFrequency[c] = 0.1f + 0.2f * c;
            m_knobs.m_attack[c] = 0.2f + 0.05f * move;
            m_knobs.m_decay[c] = 0.45f + 0.05f * move;
            m_knobs.m_portamento[c] = 0.25f + 0.05f * move;
            m_knobs.m_density[c] = 0.4f + 0.05f * move;
            m_knobs.m_bwBaseFrequency[c] = 0.4f + 0.03f * move;
            m_knobs.m_bwWidth[c] = 0.5f + 0.04f * move;
            m_knobs.m_bassCutoff[c] = 0.1f;
            m_knobs.m_azimuthFactor[c] = 0.4f + 0.05f * move;
            m_knobs.m_reductionFeedback[c] = 0.1f;
            m_knobs.m_unison[c] = 0.1f;
            m_knobs.m_pitchShiftDepth[c] = 0.2f;
            m_knobs.m_pitchShift[c] = 0.5f + 0.05f * move;
        }
    }

    std::array<float, 4> Process(size_t n)
    {
        if constexpr (Mode != 0)
        {
            Set(n);
            m_setter.SetInput(m_knobs, m_input);
        }

        QuadFloat in;
        for (size_t c = 0; c < 4; ++c)
        {
            in[c] = m_signals.m_audio[n % m_signals.m_period][c];
        }

        return Result(m_core.Process(in, m_input), 4);
    }
};

template <size_t Channels, int Mode>
struct Master
{
    Signals m_signals{x_rate};
    MasteringChain<Channels> m_core;
    typename MasteringChain<Channels>::Input m_input;
    Master()
    {
        Set(0);
    }

    void Set(size_t n)
    {
        float move = Motion(m_signals, n, 1, Mode);
        auto& p = m_input.m_saturatorInput;
        for (size_t b = 0; b < 4; ++b)
        {
            p.m_gain[b].Update(0.6f + 0.04f * b + 0.1f * move);
        }

        p.m_masterGain.Update(0.7f + 0.05f * move);
        p.m_bassFreq.Update(0.3f + 0.04f * move);
        p.m_crossoverFreqFactor[0].Update(0.45f + 0.04f * move);
        p.m_crossoverFreqFactor[1].Update(0.55f + 0.04f * move);
    }

    std::array<float, 4> Process(size_t n)
    {
        if constexpr (Mode != 0)
        {
            Set(n);
        }

        MultiChannelFloat<Channels> in;
        for (size_t c = 0; c < Channels; ++c)
        {
            in[c] = m_signals.m_audio[n % m_signals.m_period][c];
        }

        return Result(m_core.Process(m_input, in), Channels);
    }
};

template <int Mode>
struct Random
{
    GangedRandomLFO<3> m_core{12345};
    GangedRandomLFOInput m_input = GangedRandomLFOInput::Standard(0.08, 0.25f);
    Random()
    {
        if constexpr (Mode == 0)
        {
            m_input = GangedRandomLFOInput::Standard(10000, 0.25f);
        }

        if constexpr (Mode == 3)
        {
            m_input.m_waiting = {1e-6, 0, 0};
            m_input.m_moving = {10000, 0, 0};
        }
    }

    std::array<float, 4> Process(size_t)
    {
        m_core.Process(1.0 / x_rate, m_input);
        return {m_core.Output(0), m_core.Output(1), m_core.Output(2), 0};
    }
};

template <size_t Size, int Mode>
struct Poly
{
    using Core = PolyXFaderGeneric<Size>;
    Signals m_signals{x_rate};
    TheoryOfTimeBase m_clock;
    static constexpr size_t x_groups = Size == 4 ? 1 : 3 / Size;
    Core m_core[x_groups];
    typename Core::Input m_input[x_groups];
    Poly()
    {
        m_clock.m_globalPeriodSamples = x_rate;
        auto& sample = m_clock.m_sample;
        sample.m_running = true;
        for (size_t j = 0; j < 6; ++j)
        {
            sample.m_loops[j].m_cycleRatio = 1ll << (5 - j);
        }

        for (auto& p : m_input)
        {
            p.m_size = 6;
            p.m_theoryOfTime = &m_clock;
        }
    }

    std::array<float, 4> Process(size_t n)
    {
        auto& sample = m_clock.m_sample;
        sample.m_modulatedPhase = static_cast<double>(n) / x_rate;
        std::array<float, 4> out{};
        for (size_t g = 0; g < x_groups; ++g)
        {
            auto& p = m_input[g];
            for (size_t c = 0; c < Size; ++c)
            {
                size_t ch = g * Size + c;
                float move = Motion(m_signals, n, ch, Mode);
                Put(p.m_attackFrac, c, 0.3f + 0.1f * ch + 0.1f * move);
                Put(p.m_shape, c, 0.25f + 0.1f * move);
                Put(p.m_mult, c, 1.3f + 0.3f * ch + 0.1f * move);
                Put(p.m_amplitude, c, 0.8f);
                Put(p.m_phaseShift, c, 0.1f * ch);
                Put(p.m_slope, c, 0.65f + 0.1f * move);
                Put(p.m_center, c, 0.5f + 0.1f * move);
                Put(p.m_shFade, c, 0.1f);
            }

            m_core[g].Process(p);
            for (size_t c = 0; c < Size; ++c)
            {
                out[g * Size + c] = m_core[g].m_output[c];
            }
        }

        return out;
    }
};

template <int Mode>
void Run(const char* kind, int size, double seconds, const char* path)
{
    if (std::strcmp(kind, "reverb") == 0)
    {
        auto w = std::make_unique<Reverb<Mode>>();
        Measure(*w, kind, Mode, x_rate, seconds, 2, path);
    }
    else if (std::strcmp(kind, "delay") == 0)
    {
        auto w = std::make_unique<Delay<Mode>>();
        Measure(*w, kind, Mode, x_rate, seconds, 4, path);
    }
    else if (std::strcmp(kind, "partial") == 0)
    {
        auto w = std::make_unique<Partial<Mode>>();
        Measure(*w, kind, Mode, x_rate, seconds, 2, path);
    }
    else if (std::strcmp(kind, "stereo_master") == 0)
    {
        auto w = std::make_unique<Master<2, Mode>>();
        Measure(*w, kind, Mode, x_rate, seconds, 1, path);
    }
    else if (std::strcmp(kind, "quad_master") == 0)
    {
        auto w = std::make_unique<Master<4, Mode>>();
        Measure(*w, kind, Mode, x_rate, seconds, 1, path);
    }
    else if (std::strcmp(kind, "random") == 0)
    {
        auto w = std::make_unique<Random<Mode>>();
        Measure(*w, kind, Mode, x_rate, seconds, 2, path);
    }
    else
    {
        if (size == 4)
        {
            auto w = std::make_unique<Poly<4, Mode>>();
            Measure(*w, kind, Mode, x_rate, seconds, 1, path);
        }
        else if (size == 3)
        {
            auto w = std::make_unique<Poly<3, Mode>>();
            Measure(*w, kind, Mode, x_rate, seconds, 1, path);
        }
        else
        {
            auto w = std::make_unique<Poly<1, Mode>>();
            Measure(*w, kind, Mode, x_rate, seconds, 1, path);
        }
    }
}

int main(int argc, char** argv)
{
    return BenchmarkMain(argc, argv, [](const ComponentOptions& options)
    {
        std::string kind(options.m_kind);
        int size = options.m_lanes;
        bool valid = ((kind == "reverb" || kind == "delay" || kind == "partial" || kind == "quad_master") &&
                      size == 4) ||
                     (kind == "stereo_master" && size == 2) || (kind == "random" && size == 3) ||
                     (kind == "polyxfader" && (size == 1 || size == 3 || size == 4));
        Require(valid, "Unknown effect or unsupported lane count");
        RGen::s_gen.seed(12345);
        switch (options.m_mode)
        {
        case 0:
            Run<0>(options.m_kind, size, options.m_seconds, options.m_dump);
            break;
        case 1:
            Run<1>(options.m_kind, size, options.m_seconds, options.m_dump);
            break;
        case 2:
            Run<2>(options.m_kind, size, options.m_seconds, options.m_dump);
            break;
        case 3:
            Run<3>(options.m_kind, size, options.m_seconds, options.m_dump);
            break;
        }
    });
}
