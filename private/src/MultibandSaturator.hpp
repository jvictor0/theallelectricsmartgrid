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

    struct UIState
    {
        std::atomic<float> m_gain[NumBands];
        std::atomic<float> m_masterGain;
        std::atomic<float> m_crossoverFreq[NumBands - 1];

        MeterReaderGeneric<float, NumChannels> m_meterReader[NumBands];
        MeterReaderGeneric<float, NumChannels> m_masterMeterReader;

        UIState()
            : m_gain{}
            , m_masterGain{}
            , m_crossoverFreq{}
        {
            for (size_t i = 0; i < NumBands; ++i)
            {
                m_gain[i] = 0.0f;
            }

            m_masterGain = 0.0f;
            for (size_t i = 0; i < NumBands - 1; ++i)
            {
                m_crossoverFreq[i] = 0.0f;
            }
        }

        void Process()
        {
            for (size_t i = 0; i < NumBands; ++i)
            {
                m_meterReader[i].Process();
            }

            m_masterMeterReader.Process();
        }

        std::complex<float> TransferFunction(float freq)
        {
            return MultibandSaturator::TransferFunction(this, freq);
        }

        float FrequencyResponse(float freq)
        {
            return std::abs(TransferFunction(freq));
        }
    };

    void SetupUIState(UIState* uiState)
    {
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
            uiState->m_crossoverFreq[0].store(m_bassFreq.m_expParam);
            float crossoverFreq = m_bassFreq.m_expParam;
            for (size_t i = 0; i < NumBands; ++i)
            {
                uiState->m_gain[i].store(m_gain[i].m_expParam);
                if (0 < i && i < NumBands - 1)
                {
                    crossoverFreq *= m_crossoverFreqFactor[i - 1].m_expParam;
                    uiState->m_crossoverFreq[i].store(crossoverFreq);
                }
            }

            uiState->m_masterGain.store(m_masterGain.m_expParam);

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

    static void TransferFunctionRecursive(UIState* uiState, std::complex<float>* bands, float freq, size_t size, size_t index)
    {
        if (size == 1)
        {
            bands[index] = std::complex<float>(1.0f, 0.0f);
        }
        else if (size == 2)
        {
            LinkwitzRileyCrossoverBase::ComplexCrossoverOutput crossover = LinkwitzRileyCrossoverBase::TransferFunction(uiState->m_crossoverFreq[index].load(), freq);
            bands[index] = crossover.m_lowPass;
            bands[index + 1] = crossover.m_highPass;
        }
        else
        {
            size_t cutIndex = index + size / 2;
            TransferFunctionRecursive(uiState, bands, freq, size / 2, index);
            TransferFunctionRecursive(uiState, bands, freq, (size + 1) / 2, cutIndex);

            LinkwitzRileyCrossoverBase::ComplexCrossoverOutput crossover = LinkwitzRileyCrossoverBase::TransferFunction(uiState->m_crossoverFreq[cutIndex - 1].load(), freq);
            for (size_t i = index; i < index + size / 2; ++i)
            {
                bands[i] *= crossover.m_lowPass;
            }

            for (size_t i = index + size / 2; i < index + size; ++i)
            {
                bands[i] *= crossover.m_highPass;
            }
        }
    }

    static std::complex<float> TransferFunction(UIState* uiState, float freq)
    {
        std::complex<float> bands[NumBands];
        for (size_t i = 0; i < NumBands; ++i)
        {
            bands[i] = std::complex<float>(0.0f, 0.0f);
        }

        TransferFunctionRecursive(uiState, bands, freq, NumBands, 0);

        std::complex<float> response(0.0f, 0.0f);
        for (size_t i = 0; i < NumBands; ++i)
        {
            response += bands[i] * uiState->m_gain[i].load() * uiState->m_masterGain.load();
        }

        return response;
    }

    static float FrequencyResponse(UIState* uiState, float freq)
    {
        return std::abs(TransferFunction(uiState, freq));
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
