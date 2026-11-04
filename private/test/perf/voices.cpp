#include "Bench.hpp"
#include "DualWaveShapingVCO.hpp"
#include "LadderFilter.hpp"
#include "PhysicalModelingSource.hpp"
#include "StateVariableFilter.hpp"
#include <cstring>
#include <type_traits>

template <typename V>
void Lane(V& value, size_t lane, float x)
{
    if constexpr (std::is_arithmetic_v<V>)
    {
        value = x;
    }
    else
    {
        value[lane] = x;
    }
}

template <typename V>
float Lane(const V& value, size_t lane)
{
    if constexpr (std::is_arithmetic_v<V>)
    {
        return value;
    }
    else
    {
        return value[lane];
    }
}

template <int Kind, size_t Size, int Mode>
struct Filters
{
    using V = MultiChannelNumber<float, Size>;
    using Core = std::conditional_t<Kind == 0, LadderFilterLPGeneric<float, Size>,
                                    LinearStateVariableFilterGeneric<float, Size>>;
    Signals m_signals{SampleTimer::x_sampleRate};
    Core m_core[3 / Size];
    void Set(size_t g, const V& cutoff, const V& resonance)
    {
        if constexpr (Kind == 1)
        {
            m_core[g].SetCutoffAndResonance(cutoff, resonance);
        }
        else
        {
            m_core[g].SetCutoff(cutoff);
            m_core[g].SetResonance(resonance);
        }
    }

    Filters()
    {
        for (size_t g = 0; g < 3 / Size; ++g)
        {
            V cutoff{}, resonance{};
            for (size_t c = 0; c < Size; ++c)
            {
                Lane(cutoff, c, 0.006f * (g * Size + c + 1));
                Lane(resonance, c, 0.2f + 0.2f * (g * Size + c));
            }

            Set(g, cutoff, resonance);
        }
    }

    std::array<float, 4> Process(size_t n)
    {
        size_t i = n % m_signals.m_period;
        std::array<float, 4> result{};
        for (size_t g = 0; g < 3 / Size; ++g)
        {
            V input{}, cutoff{}, resonance{};
            for (size_t c = 0; c < Size; ++c)
            {
                size_t channel = g * Size + c;
                float move =
                    (Mode == 3 || (Mode == 2 && channel == 1)) ? m_signals.m_controls[i][channel] : 0;
                Lane(input, c, m_signals.m_audio[i][channel]);
                Lane(cutoff, c, 0.006f * (channel + 1) * (1 + 0.5f * move));
                Lane(resonance, c, 0.2f + 0.2f * channel + 0.12f * move);
            }

            if constexpr (Mode != 0)
            {
                Set(g, cutoff, resonance);
            }

            V output{};
            if constexpr (Kind == 0)
            {
                output = m_core[g].Process(input);
            }
            else
            {
                m_core[g].Process(input);
                output = m_core[g].GetLowPass();
            }

            for (size_t c = 0; c < Size; ++c)
            {
                result[g * Size + c] = Lane(output, c);
            }
        }

        return result;
    }
};

std::unique_ptr<AdaptiveWaveTable> MakeTable(int harmonic)
{
    auto table = std::make_unique<AdaptiveWaveTable>();
    for (size_t i = 0; i < BasicWaveTable::x_tableSize; ++i)
    {
        double phase = 2 * std::acos(-1.0) * i / BasicWaveTable::x_tableSize;
        table->m_waveTable.m_table[i] =
            static_cast<float>(std::sin(phase) + 0.5 * std::sin(harmonic * phase));
    }

    table->Generate();
    return table;
}

template <size_t Size, int Mode>
struct Dual
{
    using Core = DualWaveShapingVCOGeneric<Size>;
    Signals m_signals{SampleTimer::x_sampleRate};
    std::unique_ptr<AdaptiveWaveTable> m_left = MakeTable(3);
    std::unique_ptr<AdaptiveWaveTable> m_right = MakeTable(5);
    Core m_core[3 / Size];
    typename Core::Input m_input[3 / Size];
    Dual()
    {
        for (auto& core : m_core)
        {
            for (size_t j = 0; j < 2; ++j)
            {
                for (size_t c = 0; c < Size; ++c)
                {
                    core.m_vco[j].SetLeft(c, m_left.get());
                    core.m_vco[j].SetRight(c, m_right.get());
                }
            }
        }
    }

    std::array<float, 4> Process(size_t n)
    {
        std::array<float, 4> output{};
        const size_t i = n % m_signals.m_period;
        for (size_t g = 0; g < 3 / Size; ++g)
        {
            {
                auto& p = m_input[g];
                for (size_t c = 0; c < Size; ++c)
                {
                    size_t ch = g * Size + c;
                    float move = (Mode == 3 || (Mode == 2 && ch == 1)) ? m_signals.m_controls[i][ch] : 0;
                    for (size_t j = 0; j < 2; ++j)
                    {
                        Lane(p.m_v[j], c, 0.35f + 0.15f * ch + 0.1f * move + 0.05f * j);
                        Lane(p.m_d[j], c, 0.25f + 0.2f * ch + 0.08f * move + 0.05f * j);
                        Lane(p.m_wtBlend[j], c, 0.25f + 0.1f * ch + 0.15f * move);
                        Lane(p.m_morphHarmonics[j], c, 0.4f + 0.1f * j + 0.1f * move);
                        Lane(p.m_crossModIndex[j], c, 0.08f + 0.03f * ch);
                    }

                    Lane(p.m_baseFreq, c, 0.001f * (ch + 1) * (1 + 0.12f * move));
                    Lane(p.m_offsetFreqFactor, c, 1.5f);
                    Lane(p.m_detune, c, 1.01f);
                    Lane(p.m_fade, c, 0.3f + 0.1f * ch + 0.1f * move);
                    Lane(p.m_bitCrushAmount, c, 0.15f + 0.1f * ch + 0.08f * move);
                }
            }

            auto value = m_core[g].Process(m_input[g]);
            for (size_t c = 0; c < Size; ++c)
            {
                output[g * Size + c] = value[c];
            }
        }

        return output;
    }
};

template <size_t Size, int Mode>
struct VPS
{
    using Core = VectorPhaseShaperGeneric<Size>;
    Signals m_signals{SampleTimer::x_sampleRate};
    std::unique_ptr<AdaptiveWaveTable> m_tables[6];
    Core m_core[3 / Size];
    typename Core::Input m_input[3 / Size];
    VPS()
    {
        for (size_t c = 0; c < 6; ++c)
        {
            m_tables[c] = MakeTable(3 + c);
        }

        for (size_t g = 0; g < 3 / Size; ++g)
        {
            for (size_t c = 0; c < Size; ++c)
            {
                m_core[g].SetLeft(c, m_tables[2 * (g * Size + c)].get());
                m_core[g].SetRight(c, m_tables[2 * (g * Size + c) + 1].get());
            }
        }
    }

    std::array<float, 4> Process(size_t n)
    {
        std::array<float, 4> output{};
        for (size_t g = 0; g < 3 / Size; ++g)
        {
            auto& in = m_input[g];
            for (size_t c = 0; c < Size; ++c)
            {
                size_t lane = g * Size + c;
                float move = (Mode == 3 || (Mode == 2 && lane == 1))
                                 ? m_signals.m_controls[n % m_signals.m_period][lane]
                                 : 0.0f;
                in.m_freq[c] = (220.0f + lane * 97.0f) / static_cast<float>(SampleTimer::x_sampleRate) *
                               (1 + 0.2f * move);
                in.m_v[c] = 0.45f + 0.1f * lane + 0.13f * move;
                in.m_d[c] = 0.35f + 0.08f * lane + 0.11f * move;
                in.m_phaseMod[c] = 0.1f * move;
                in.m_morphHarmonics[c] = 0.15f + 0.03f * lane + 0.06f * move;
                in.m_wtBlend[c] = 0.4f + 0.09f * lane + 0.12f * move;
            }

            m_core[g].Process(in);
            for (size_t c = 0; c < Size; ++c)
            {
                output[g * Size + c] = m_core[g].m_out[c];
            }
        }

        return output;
    }
};

template <size_t Size, int Mode>
struct Physical
{
    using Core = PhysicalModelingSourceGeneric<Size>;
    using Vector = typename Core::Vector;
    Signals m_signals{SampleTimer::x_sampleRate};
    std::unique_ptr<Core> m_core[3 / Size];
    typename Core::Input m_input[3 / Size];
    Physical()
    {
        for (size_t group = 0; group < 3 / Size; ++group)
        {
            m_core[group] = std::make_unique<Core>(42 + group * Size);
            Set(group, 0);
        }
    }

    void Set(size_t group, size_t n)
    {
        Vector frequency, decay, damping;
        auto& input = m_input[group];
        for (size_t lane = 0; lane < Size; ++lane)
        {
            size_t channel = group * Size + lane;
            float move = (Mode == 3 || (Mode == 2 && channel == 1))
                             ? m_signals.m_controls[n % m_signals.m_period][channel]
                             : 0;
            frequency[lane] = 0.001f * (channel + 1) * (1 + 0.12f * move);
            decay[lane] = 0.72f + 0.04f * channel + 0.04f * move;
            damping[lane] = 0.3f + 0.1f * channel + 0.08f * move;
            input.m_mainSVFCutoff[lane] = 0.006f * (channel + 1) * (1 + 0.5f * move);
            input.m_mainSVFResonance[lane] = 0.2f + 0.2f * channel + 0.12f * move;
            input.m_mainSVFMorph[lane] = 0.2f + 0.25f * channel + 0.1f * move;
            input.m_sampleRateReducerFreq[lane] = 0.4f + 0.15f * channel + 0.1f * move;
            input.m_envelope[lane] = 0.4f + 0.1f * channel + 0.15f * move;
        }

        m_core[group]->SetCombParams(frequency, decay, damping, SampleTimer::x_sampleRate);
    }

    std::array<float, 4> Process(size_t n)
    {
        std::array<float, 4> output{};
        for (size_t group = 0; group < 3 / Size; ++group)
        {
            if constexpr (Mode != 0)
            {
                Set(group, n);
            }

            auto value = m_core[group]->Process(m_input[group]);
            for (size_t lane = 0; lane < Size; ++lane)
            {
                output[group * Size + lane] = value[lane];
            }
        }

        return output;
    }
};

template <size_t Size, int Mode>
void Run(const char* kind, double seconds, const char* path)
{
    if (std::strcmp(kind, "vps") == 0)
    {
        auto w = std::make_unique<VPS<Size, Mode>>();
        Measure(*w, kind, Mode, SampleTimer::x_sampleRate, seconds, 0.5, path);
    }
    else if (std::strcmp(kind, "dual") == 0)
    {
        auto w = std::make_unique<Dual<Size, Mode>>();
        Measure(*w, kind, Mode, SampleTimer::x_sampleRate, seconds, 0.5, path);
    }
    else if (std::strcmp(kind, "ladder") == 0)
    {
        auto w = std::make_unique<Filters<0, Size, Mode>>();
        Measure(*w, kind, Mode, SampleTimer::x_sampleRate, seconds, 0.5, path);
    }
    else if (std::strcmp(kind, "physical") == 0)
    {
        auto w = std::make_unique<Physical<Size, Mode>>();
        Measure(*w, kind, Mode, SampleTimer::x_sampleRate, seconds, 0.5, path);
    }
    else
    {
        auto w = std::make_unique<Filters<1, Size, Mode>>();
        Measure(*w, kind, Mode, SampleTimer::x_sampleRate, seconds, 0.5, path);
    }
}

template <size_t Size>
void Modes(const char* kind, int mode, double seconds, const char* path)
{
    switch (mode)
    {
    case 0:
        Run<Size, 0>(kind, seconds, path);
        break;
    case 1:
        Run<Size, 1>(kind, seconds, path);
        break;
    case 2:
        Run<Size, 2>(kind, seconds, path);
        break;
    case 3:
        Run<Size, 3>(kind, seconds, path);
        break;
    }
}

int main(int argc, char** argv)
{
    return BenchmarkMain(argc, argv, [](const ComponentOptions& options)
    {
        std::string kind(options.m_kind);
        Require(kind == "vps" || kind == "dual" || kind == "ladder" || kind == "svf" || kind == "physical",
                "Unknown voice kind");
        Require(options.m_lanes == 1 || options.m_lanes == 3,
                "Voices require LANES 1 (three mono) or 3 (one trio)");
        if (options.m_lanes == 3)
        {
            Modes<3>(options.m_kind, options.m_mode, options.m_seconds, options.m_dump);
        }
        else
        {
            Modes<1>(options.m_kind, options.m_mode, options.m_seconds, options.m_dump);
        }
    });
}
