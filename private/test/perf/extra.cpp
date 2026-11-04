#include "Bench.hpp"
#include "Lissajous.hpp"
#include "QuadMixer.hpp"
#include "VCO.hpp"
#include <cstring>

constexpr size_t x_rate = SampleTimer::x_sampleRate;

template <size_t Size, int Kind, int Mode>
struct Modulators
{
    using Vector = MultiChannelNumber<float, Size>;
    Signals m_signals{x_rate};
    LissajousLFOGeneric<Size> m_lfo[3 / Size];
    typename LissajousLFOGeneric<Size>::Input m_input[3 / Size];
    VCOGeneric<Size> m_vco[3 / Size];
    BiDirectionalSlewGeneric<Size> m_slew[3 / Size];
    PhaseUtils::ExpParamGeneric<float, Size> m_drive[3 / Size];
    Modulators()
    {
        for (auto& drive : m_drive)
        {
            drive = PhaseUtils::ExpParamGeneric<float, Size>(0.125f, 32.0f);
        }
    }

    std::array<float, 4> Process(size_t n)
    {
        std::array<float, 4> result{};
        for (size_t group = 0; group < 3 / Size; ++group)
        {
            Vector phase, freq, knob;
            auto& input = m_input[group];
            for (size_t lane = 0; lane < Size; ++lane)
            {
                size_t ch = group * Size + lane;
                float move = Mode == 3 ? m_signals.m_controls[n % m_signals.m_period][ch] : 0;
                phase[lane] =
                    static_cast<float>(std::fmod(static_cast<double>(n) / x_rate * (0.37 + 0.03 * ch), 1.0));
                freq[lane] = (55.0f + 13 * ch) * (1 + .1f * move) / x_rate;
                knob[lane] = .5f + .1f * move;
                input.m_multX[lane] = 2.3f + .2f * ch;
                input.m_multY[lane] = 3.7f + .2f * ch;
                input.m_centerX[lane] = .1f + .2f * move;
                input.m_centerY[lane] = -.2f + .1f * move;
                input.m_phaseShift[lane] = .1f * ch;
            }

            Vector out;
            if constexpr (Kind == 0)
            {
                input.m_radius.Update(knob);
                m_lfo[group].Process(phase, input);
                out = m_lfo[group].m_outputX + m_lfo[group].m_outputY;
            }
            else
            {
                out = m_vco[group].Process(freq);
                if constexpr (Kind == 2)
                {
                    using Saturator = TanhSaturatorGeneric<float, Size, true>;
                    Vector drive = m_drive[group].Update(knob);
                    out = Saturator::Tanh(out * drive) / Saturator::Tanh(drive);
                    m_slew[group].SetSlewUp(Vector::Splat(1.0f / (2 * M_PI * x_rate * .03f)));
                    m_slew[group].SetSlewDown(Vector::Splat(1.0f / (2 * M_PI * x_rate * .3f)));
                    out *= m_slew[group].Process(knob);
                }
            }

            for (size_t lane = 0; lane < Size; ++lane)
            {
                result[group * Size + lane] = out[lane];
            }
        }

        return result;
    }
};

template <int Kind, int Mode>
struct Mixer
{
    SmartGridOneContext m_context;
    QuadMixerInternal m_core{&m_context};
    QuadMixerInternal::Input m_input;
    Signals m_signals{x_rate};
    Mixer()
    {
        m_input.m_numInputs = 17;
        m_input.m_numMonoInputs = 9;
        for (size_t i = 0; i < 17; ++i)
        {
            m_input.m_gain[i].m_expParam = .7f;
            for (auto& gain : m_input.m_sendGain[i])
            {
                gain.m_expParam = .2f;
            }
        }

        for (auto& gain : m_input.m_returnGain)
        {
            gain.m_expParam = .7f;
        }

        m_core.ProcessInputs(m_input);
    }

    std::array<float, 4> Process(size_t n)
    {
        for (size_t i = 0; i < 17; ++i)
        {
            m_input.m_input[i] = m_signals.m_audio[n % m_signals.m_period][i % 4] * .1f;
            m_input.m_x[i] = .2f + .2f * (i % 4);
            m_input.m_y[i] = .6f;
            if constexpr (Mode == 3)
            {
                m_input.m_y[i] += .2f * m_signals.m_controls[n % m_signals.m_period][i % 4];
            }
        }

        for (size_t i = 0; i < 9; ++i)
        {
            m_input.m_monoIn[i] = m_input.m_input[i];
        }

        for (auto& ret : m_input.m_return)
        {
            ret = QuadFloat(m_input.m_input);
        }

        if constexpr (Kind == 0)
        {
            m_core.ProcessInputs(m_input);
            auto out = m_core.m_dryOutput + m_core.m_send[0] + m_core.m_send[1] + m_core.m_send[2];
            return {out[0], out[1], out[2], out[3]};
        }
        else
        {
            auto out = m_core.Process(m_input).m_output;
            return {out[0], out[1], out[2], out[3]};
        }
    }
};

template <size_t Size, int Mode>
void Run(const char* kind, double seconds, const char* path)
{
    if (std::strcmp(kind, "lissajous") == 0)
    {
        auto w = std::make_unique<Modulators<Size, 0, Mode>>();
        Measure(*w, kind, Mode, x_rate, seconds, .5, path);
    }
    else if (std::strcmp(kind, "sub_osc") == 0)
    {
        auto w = std::make_unique<Modulators<Size, 1, Mode>>();
        Measure(*w, kind, Mode, x_rate, seconds, .5, path);
    }
    else if (std::strcmp(kind, "sub_kernel") == 0)
    {
        auto w = std::make_unique<Modulators<Size, 2, Mode>>();
        Measure(*w, kind, Mode, x_rate, seconds, .5, path);
    }
    else if (std::strcmp(kind, "mixer_inputs") == 0)
    {
        auto w = std::make_unique<Mixer<0, Mode>>();
        Measure(*w, kind, Mode, x_rate, seconds, .5, path);
    }
    else
    {
        auto w = std::make_unique<Mixer<1, Mode>>();
        Measure(*w, kind, Mode, x_rate, seconds, .5, path);
    }
}

int main(int argc, char** argv)
{
    return BenchmarkMain(argc, argv, [](const ComponentOptions& options)
    {
        std::string kind(options.m_kind);
        bool mixer = kind == "mixer_inputs" || kind == "mixer_full";
        bool modulator = kind == "lissajous" || kind == "sub_osc" || kind == "sub_kernel";
        Require((mixer && options.m_lanes == 4) ||
                    (modulator && (options.m_lanes == 1 || options.m_lanes == 3)),
                "Unknown extra kind or unsupported lane count");
        Require(options.m_mode == 1 || options.m_mode == 3, "Extras support MODE 1 (fixed) or 3 (modulated)");
        if (options.m_lanes == 1)
        {
            if (options.m_mode == 1)
            {
                Run<1, 1>(options.m_kind, options.m_seconds, options.m_dump);
            }
            else
            {
                Run<1, 3>(options.m_kind, options.m_seconds, options.m_dump);
            }
        }
        else
        {
            if (options.m_mode == 1)
            {
                Run<3, 1>(options.m_kind, options.m_seconds, options.m_dump);
            }
            else
            {
                Run<3, 3>(options.m_kind, options.m_seconds, options.m_dump);
            }
        }
    });
}
