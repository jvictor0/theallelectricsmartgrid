#pragma once

#include "StereoUtils.hpp"
#include "QuadUtils.hpp"
#include "LinkwitzRileyCrossover.hpp"
#include "PhaseUtils.hpp"
#include "Metering.hpp"
#include "SampleTimer.hpp"
#include <complex>

template<size_t NumBands, size_t NumChannels>
struct MultibandSaturator
{
    using Vector = MultiChannelFloat<NumChannels>;
    using Crossover = LinkwitzRileyCrossoverGeneric<float, NumChannels>;

    Crossover m_linkwitzRileyCrossover[NumBands - 1];

    Vector m_output;
    float m_sub;

    MeterGeneric<float, NumChannels> m_meter[NumBands];
    MeterGeneric<float, NumChannels> m_masterMeter;

    MultibandSaturator()
        : m_output{}
        , m_sub(0.0f)
        , m_meter{}
        , m_masterMeter{}
    {
    }

    struct Snapshot : VectorResponseSnapshot<1>
    {
        static constexpr size_t x_responseWidth = []
        {
            size_t width = 1;
            while (width < NumBands)
            {
                width *= 2;
            }

            return width;
        }();
        using BandVector = MultiChannelNumber<float, x_responseWidth>;
        using BandComplex = MultiChannelComplex<double, x_responseWidth>;
        using Cutoffs = CrossoverResponseCoefficients<x_responseWidth>;
        BandVector m_gain;
        float m_masterGain = 1;
        Cutoffs m_crossovers;

        template<size_t Count, size_t Index>
        static void BuildResponses(BandComplex& bands, const typename Cutoffs::Pair& crossovers)
        {
            if constexpr (Count > 1)
            {
                constexpr size_t x_middle = Index + Count / 2;
                BandComplex factors(1.0);
                for (size_t i = Index; i < Index + Count; ++i)
                {
                    factors.SetChannel(i, i < x_middle
                        ? crossovers.m_lowPass[x_middle - 1] : crossovers.m_highPass[x_middle - 1]);
                }

                bands *= factors;
                BuildResponses<Count / 2, Index>(bands, crossovers);
                BuildResponses<(Count + 1) / 2, x_middle>(bands, crossovers);
            }
        }

        ComplexVector ComplexResponses(const ResponseFrequency& frequency) const override
        {
            auto crossovers = m_crossovers.Responses(frequency);
            BandComplex bands(1.0);
            BuildResponses<NumBands, 0>(bands, crossovers);
            bands *= (m_gain * m_masterGain).template Convert<double>();
            double real = 0;
            double imag = 0;
            for (size_t i = 0; i < NumBands; ++i)
            {
                real += bands.m_real[i];
                imag += bands.m_imag[i];
            }

            return {real, imag};
        }
    };

    struct UIState : TransferFunctionGeneric<Snapshot>
    {
        using TransferFunctionGeneric<Snapshot>::TransferFunctionGeneric;

        MeterReaderGeneric<float, NumChannels> m_meterReader[NumBands];
        MeterReaderGeneric<float, NumChannels> m_masterMeterReader;

        void Process()
        {
            for (auto& meter : m_meterReader)
            {
                meter.Process();
            }

            m_masterMeterReader.Process();
        }
    };

    SampledSnapshotWriter<UIState> m_responseSampler;

    void SetupUIState(UIState* uiState)
    {
        m_responseSampler.Bind(uiState);
        for (size_t i = 0; i < NumBands; ++i)
        {
            uiState->m_meterReader[i].SetMeter(&m_meter[i]);
        }

        uiState->m_masterMeterReader.SetMeter(&m_masterMeter);
    }

    struct Input
    {
        PhaseUtils::ExpParam m_gain[NumBands];
        PhaseUtils::ExpParam m_masterGain;
        PhaseUtils::ExpParam m_bassFreq;
        PhaseUtils::ExpParam m_crossoverFreqFactor[NumBands - 2];

        Input()
        {
            for (size_t i = 0; i < NumBands; ++i)
            {
                m_gain[i] = PhaseUtils::ExpParam(1.0 / 16.0f, 2.0f);
                m_gain[i].m_expParam = 1.0;
            }

            m_masterGain = PhaseUtils::ExpParam(1.0 / 4.0f, 2.0f);
            m_masterGain.m_expParam = 1.0;

            // The crossover range is defined in cycles per host sample, so it keeps
            // its frequencies in Hz at any engine rate.
            //
            m_bassFreq = PhaseUtils::ExpParam(
                SampleTimer::HzToCycPerSamp(SampleTimer::CycPerHostSampToHz(0.5 / 1024.0f)),
                SampleTimer::HzToCycPerSamp(SampleTimer::CycPerHostSampToHz(0.5 / (std::pow(1024, static_cast<float>(NumBands - 2) / (NumBands - 1))))));
            m_bassFreq.Update(0.5);

            for (size_t i = 0; i < NumBands - 2; ++i)
            {
                m_crossoverFreqFactor[i] = PhaseUtils::ExpParam(1.0, 32.0f);
                m_crossoverFreqFactor[i].Update(0.5);
            }
        }

        void PopulateUIState(UIState* uiState)
        {
            uiState->Process();
        }

    };

    template<size_t Count, size_t Index>
    void BuildBands(Vector* bands, const Vector& in)
    {
        if constexpr (Count == 1)
        {
            bands[Index] = in;
        }
        else
        {
            constexpr size_t x_cutIndex = Index + Count / 2;
            auto crossover = m_linkwitzRileyCrossover[x_cutIndex - 1].Process(in);
            BuildBands<Count / 2, Index>(bands, crossover.m_lowPass);
            BuildBands<(Count + 1) / 2, x_cutIndex>(bands, crossover.m_highPass);
        }
    }

    Snapshot MakeSnapshot(const Input& input) const
    {
        Snapshot snapshot;
        for (size_t i = 0; i < NumBands; ++i)
        {
            snapshot.m_gain[i] = input.m_gain[i].m_expParam;
        }

        snapshot.m_masterGain = input.m_masterGain.m_expParam;
        for (size_t i = 0; i < NumBands - 1; ++i)
        {
            snapshot.m_crossovers.m_tangent[i] = static_cast<double>(m_linkwitzRileyCrossover[i].m_first.m_a2)
                / m_linkwitzRileyCrossover[i].m_first.m_a1;
        }

        return snapshot;
    }

    void PopulateUIStateSample(const Input& input)
    {
        m_responseSampler.PopulateUIStateSample([&]
        {
            return MakeSnapshot(input);
        });
    }

    template<size_t First>
    void SetCrossoverFrequencies(const float* frequencies)
    {
        constexpr size_t x_remaining = NumBands - 1 - First;
        constexpr size_t x_count = x_remaining < 4 ? x_remaining : 4;
        Crossover::SetCyclesPerSampleBatch(m_linkwitzRileyCrossover + First,
            MultiChannelNumber<float, x_count>(frequencies + First));
        if constexpr (x_count < x_remaining)
        {
            SetCrossoverFrequencies<First + x_count>(frequencies);
        }
    }

    Vector Process(const Input& input, const Vector& in, bool monoTheBass)
    {
        float frequencies[NumBands - 1];
        frequencies[0] = input.m_bassFreq.m_expParam;
        for (size_t j = 1; j < NumBands - 1; ++j)
        {
            frequencies[j] = frequencies[j - 1] * input.m_crossoverFreqFactor[j - 1].m_expParam;
        }

        SetCrossoverFrequencies<0>(frequencies);
        PopulateUIStateSample(input);
        Vector bands[NumBands];
        BuildBands<NumBands, 0>(bands, in);

        Vector output = Vector::Splat(0);
        for (size_t j = monoTheBass ? 1 : 0; j < NumBands; ++j)
        {
            output += m_meter[j].ProcessAndSaturate(bands[j] * input.m_gain[j].m_expParam);
        }

        m_sub = 0;
        if (monoTheBass)
        {
            for (size_t i = 0; i < NumChannels; ++i)
            {
                m_sub += bands[0][i];
            }

            // The summed bass feeds every channel of the bass band's meter.
            //
            float sub = input.m_gain[0].m_expParam * m_sub / NumChannels;
            m_sub = m_meter[0].ProcessAndSaturate(Vector::Splat(sub))[0];
        }

        output = m_masterMeter.ProcessAndSaturate((output + m_sub) * input.m_masterGain.m_expParam);
        m_output = output;
        return m_output;
    }
};
