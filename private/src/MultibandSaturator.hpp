#pragma once

#include "StereoUtils.hpp"
#include "QuadUtils.hpp"
#include "LinkwitzRileyCrossover.hpp"
#include "PhaseUtils.hpp"
#include "Metering.hpp"
#include <complex>

template<size_t NumBands, size_t NumChannels>
struct MultibandSaturator
{
    LinkwitzRileyCrossover m_linkwitzRileyCrossover[NumChannels][NumBands - 1];

    float m_output[NumChannels];
    float m_sub;

    MultichannelMeter<NumChannels> m_meter[NumBands];
    MultichannelMeter<NumChannels> m_masterMeter;

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

        MultichannelMeterReader<NumChannels> m_meterReader[NumBands];
        MultichannelMeterReader<NumChannels> m_masterMeterReader;

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
            uiState->m_meterReader[i].SetMeterReaders(&m_meter[i]);
        }

        uiState->m_masterMeterReader.SetMeterReaders(&m_masterMeter);
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

            m_bassFreq = PhaseUtils::ExpParam(0.5 / 1024.0f, 0.5 / (std::pow(1024, static_cast<float>(NumBands - 2) / (NumBands - 1))));
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

    void BuildBandsRecursive(float* bands, float in, size_t size, size_t index, size_t channel)
    {
        if (size == 1)
        {
            bands[index] = in;
        }
        else if (size == 2)
        {
            LinkwitzRileyCrossover::CrossoverOutput crossover = m_linkwitzRileyCrossover[channel][index].Process(in);
            bands[index] = crossover.m_lowPass;
            bands[index + 1] = crossover.m_highPass;        
        }
        else
        {
            size_t cutIndex = index + size / 2;
            LinkwitzRileyCrossover::CrossoverOutput crossover = m_linkwitzRileyCrossover[channel][cutIndex - 1].Process(in);
            BuildBandsRecursive(bands, crossover.m_lowPass, size / 2, index, channel);
            BuildBandsRecursive(bands, crossover.m_highPass, (size + 1) / 2, cutIndex, channel);
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
            LinkwitzRileyCrossover::ComplexCrossoverOutput crossover = LinkwitzRileyCrossover::TransferFunction(uiState->m_crossoverFreq[index].load(), freq);
            bands[index] = crossover.m_lowPass;
            bands[index + 1] = crossover.m_highPass;
        }
        else
        {
            size_t cutIndex = index + size / 2;
            TransferFunctionRecursive(uiState, bands, freq, size / 2, index);
            TransferFunctionRecursive(uiState, bands, freq, (size + 1) / 2, cutIndex);

            LinkwitzRileyCrossover::ComplexCrossoverOutput crossover = LinkwitzRileyCrossover::TransferFunction(uiState->m_crossoverFreq[cutIndex - 1].load(), freq);
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
    
    void Process(const Input& input, float* in, bool monoTheBass)
    {
        float bands[NumChannels][NumBands];
        for (size_t i = 0; i < NumChannels; ++i)
        {
            for (size_t j = 0; j < NumBands; ++j)
            {
                bands[i][j] = 0.0f;
            }

            m_linkwitzRileyCrossover[i][0].SetCyclesPerSample(input.m_bassFreq.m_expParam);
            float crossoverFreq = input.m_bassFreq.m_expParam;
            for (size_t j = 1; j < NumBands - 1; ++j)
            {
                crossoverFreq *= input.m_crossoverFreqFactor[j - 1].m_expParam;
                m_linkwitzRileyCrossover[i][j].SetCyclesPerSample(crossoverFreq);
            }
        }

        for (size_t i = 0; i < NumChannels; ++i)
        {
            BuildBandsRecursive(bands[i], in[i], NumBands, 0, i);
        }

        m_sub = 0;
        for (size_t i = 0; i < NumChannels; ++i)
        {
            m_output[i] = 0.0f;
            for (size_t j = monoTheBass ? 1 : 0; j < NumBands; ++j)
            {
                m_output[i] += m_meter[j].m_meters[i].ProcessAndSaturate(input.m_gain[j].m_expParam * bands[i][j]);
            }

            if (monoTheBass)
            {
                m_sub += bands[i][0];
            }
        }

        if (monoTheBass)
        {
            m_sub = m_meter[0].m_meters[0].ProcessAndSaturate(input.m_gain[0].m_expParam * m_sub / NumChannels);
        }

        for (size_t i = 0; i < NumChannels; ++i)
        {
            m_output[i] += m_sub;
            m_output[i] = m_masterMeter.m_meters[i].ProcessAndSaturate(input.m_masterGain.m_expParam * m_output[i]);
        }
    }

    MultiChannelFloat<NumChannels> Process(const Input& input, MultiChannelFloat<NumChannels> in, bool monoTheBass)
    {
        Process(input, in.m_values, monoTheBass);
        return MultiChannelFloat<NumChannels>(m_output);
    }
};