#include "Resynthesis.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>

template<typename Bits, typename Number>
Bits AsBits(Number value)
{
    static_assert(sizeof(Bits) == sizeof(Number));
    Bits bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

template<typename DFT, size_t Size>
void FillSpectrum(DFT& dft, size_t frame)
{
    dft.Init();
    for (size_t lane = 0; lane < Size; ++lane)
    {
        const size_t center = 32 + 8 * lane;
        for (size_t bin = center - 1; bin <= center + 9; ++bin)
        {
            float amplitude = 0.0f;
            if (frame != 3)
            {
                if (bin == center)
                {
                    amplitude = 0.5f;
                }
                else if (bin + 1 == center)
                {
                    amplitude = 0.25f;
                }
                else if (bin == center + 1)
                {
                    amplitude = 0.125f;
                }
                else if (bin == center + 8)
                {
                    amplitude = 0.0625f;
                }
                else if (bin == center + 9)
                {
                    amplitude = 0.03125f;
                }
            }

            amplitude *= frame % 2 == 0 ? 1.0f : 0.75f;
            auto& value = dft.m_components[bin];
            if ((frame + bin) % 2 == 0)
            {
                value.m_real[lane] = amplitude;
            }
            else
            {
                value.m_imag[lane] = amplitude;
            }
        }
    }
}

template<size_t Size>
void Run()
{
    using Synth = ResynthesizerGeneric<float, Size>;
    using DFT = typename Synth::DFT;
    auto synth = std::make_unique<Synth>();
    auto dft = std::make_unique<DFT>();
    std::array<ResynthesizerInput, Size> input;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        input[lane].m_shift[0] = lane % 3 == 0 ? Q(1, 1) : lane % 3 == 1 ? Q(3, 2) : Q(1, 2);
        input[lane].m_fade[0] = 0.75f;
        input[lane].m_unisonGain = 0.5f;
        input[lane].m_unisonDetune = 1.0625f;
        input[lane].m_slewUp = lane % 2 == 0 ? 0.5f : 0.0625f;
    }

    size_t cursor = 0;
    const auto nextPhase = [&]()
    {
        const float draw = (static_cast<float>(cursor % 8) - 4.0f) / 8.0f;
        ++cursor;
        return draw;
    };

    for (size_t grain = 1; grain < 6; ++grain)
    {
        FillSpectrum<DFT, Size>(*dft, grain == 4 ? 1 : grain - 1);
        synth->PrimePhases(*dft);
        FillSpectrum<DFT, Size>(*dft, grain);
        typename Synth::Vector cutoff;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            cutoff[lane] = input[lane].m_slewUp;
        }

        synth->SetSlewUp(cutoff);
        synth->ProcessPhases(*dft);
        std::array<size_t, Size> cursors;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            auto& tracker = synth->m_pvdr[lane];
            tracker.Clear();
            tracker.AnalyzeStorage(nextPhase);
            cursors[lane] = cursor;
        }

        synth->SynthesizeSpectrum(input.data());
        for (size_t lane = 0; lane < Size; ++lane)
        {
            const size_t center = 32 + 8 * lane;
            const auto gate = static_cast<unsigned>(synth->m_pvdr[lane].m_gate[center]);
            std::cout << Size << ' ' << grain << ' ' << lane << ' ' << cursors[lane] << ' ' << gate << ' '
                      << AsBits<uint32_t>(synth->SynthesisMagnitudes()[center][lane]);
            for (const auto& oscillator : synth->m_oscillators)
            {
                std::cout << ' ' << AsBits<uint64_t>(oscillator.m_synthesisPhase[center][lane]);
            }

            for (size_t bin = 0; bin < 128; ++bin)
            {
                const auto& z = synth->m_synthDft.m_components[bin];
                std::cout << ' ' << AsBits<uint32_t>(z.m_real[lane]) << ' ' << AsBits<uint32_t>(z.m_imag[lane]);
            }

            std::cout << '\n';
        }
    }
}

template<size_t Size>
void VerifyGrainSequence()
{
    using Synth = ResynthesizerGeneric<float, Size>;
    auto integrated = std::make_unique<Synth>();
    auto staged = std::make_unique<Synth>();
    auto previous = std::make_unique<typename Synth::Buffer>();
    auto current = std::make_unique<typename Synth::Buffer>();
    auto output = std::make_unique<typename Synth::SynthesisFrame>();
    auto reference = std::make_unique<typename Synth::SynthesisFrame>();
    std::array<ResynthesizerInput, Size> input;
    typename Synth::Vector cutoff;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        input[lane].m_shift[0] = Q(3, 2);
        input[lane].m_fade[0] = 0.75f;
        input[lane].m_unisonGain = 0.25f;
        input[lane].m_slewUp = 0.1f * static_cast<float>(lane + 1);
        cutoff[lane] = input[lane].m_slewUp;
        for (size_t i = 0; i < Synth::x_N; ++i)
        {
            const auto value = static_cast<float>((i * (lane + 3)) % 73) * (1.0f / 73.0f);
            previous->m_table[i][lane] = value * Math4096::Hann(i);
            current->m_table[i][lane] = previous->m_table[i][lane] * 0.75f;
        }
    }

    RGen::s_gen.seed(4821);
    integrated->Process(*previous, *current, *output, input.data());
    RGen::s_gen.seed(4821);
    staged->SetSlewUp(cutoff);
    staged->PrimeAnalysis(*previous);
    staged->Analyze(staged->m_pvdr, *current);
    staged->SynthesizeSpectrum(input.data());
    staged->m_synthDft.InverseTransform(*reference, Synth::x_maxComponents, staged->m_workspace);
    MathGeneric<Synth::x_synthesisBits>::Hann(*reference);
    for (size_t i = 0; i < Synth::x_synthesisFrameSize; ++i)
    {
        for (size_t lane = 0; lane < Size; ++lane)
        {
            assert(AsBits<uint32_t>(output->m_table[i][lane]) == AsBits<uint32_t>(reference->m_table[i][lane]));
        }
    }
}

int main()
{
    VerifyGrainSequence<1>();
    VerifyGrainSequence<3>();
    VerifyGrainSequence<4>();
    Run<1>();
    Run<3>();
    Run<4>();
}
