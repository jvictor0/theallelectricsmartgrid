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

    StereoMeter m_meter[NumBands];
    StereoMeter m_masterMeter;

    struct UIState
    {
        std::atomic<float> m_gain[NumBands];
        std::atomic<float> m_masterGain;
        std::atomic<float> m_crossoverFreq[NumBands - 1];

        StereoMeterReader m_meterReader[NumBands];
        StereoMeterReader m_masterMeterReader;

        UIState()
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
        PhaseUtils::ExpParam m_crossoverFreq[NumBands - 1];

        Input()
        {
            for (size_t i = 0; i < NumBands; ++i)
            {
                m_gain[i] = PhaseUtils::ExpParam(1.0 / 16.0f, 2.0f);
                m_gain[i].m_expParam = 1.0;
            }

            m_masterGain = PhaseUtils::ExpParam(1.0 / 4.0f, 2.0f);
            m_masterGain.m_expParam = 1.0;

            for (size_t i = 0; i < NumBands - 1; ++i)
            {
                float lowerFreq = 0.5 / std::pow(1024, static_cast<float>(NumBands - i - 1) / (NumBands - 1));
                float upperFreq = 0.5 / std::pow(1024, static_cast<float>(NumBands - i - 2) / (NumBands - 1));
                m_crossoverFreq[i] = PhaseUtils::ExpParam(lowerFreq, upperFreq);
                m_crossoverFreq[i].m_expParam = std::sqrt(lowerFreq * upperFreq);
            }
        }

        void PopulateUIState(UIState* uiState)
        {
            for (size_t i = 0; i < NumBands; ++i)
            {
                uiState->m_gain[i].store(m_gain[i].m_expParam);
                if (i < NumBands - 1)
                {
                    uiState->m_crossoverFreq[i].store(m_crossoverFreq[i].m_expParam);
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

            for (size_t j = 0; j < NumBands - 1; ++j)
            {
                m_linkwitzRileyCrossover[i][j].SetCyclesPerSample(input.m_crossoverFreq[j].m_expParam);
            }
        }

        for (size_t i = 0; i < NumChannels; ++i)
        {
            BuildBandsRecursive(bands[i], in[i], NumBands, 0, i);
        }

        float sub = 0;
        for (size_t i = 0; i < NumChannels; ++i)
        {
            m_output[i] = 0.0f;
            for (size_t j = monoTheBass ? 1 : 0; j < NumBands; ++j)
            {
                m_output[i] += m_meter[j].m_meters[i].ProcessAndSaturate(input.m_gain[j].m_expParam * bands[i][j]);
            }

            if (monoTheBass)
            {
                sub += bands[i][0];
            }
        }

        if (monoTheBass)
        {
            sub = m_meter[0].m_meters[0].ProcessAndSaturate(input.m_gain[0].m_expParam * sub / NumChannels);
        }

        for (size_t i = 0; i < NumChannels; ++i)
        {
            m_output[i] += sub;
            m_output[i] = m_masterMeter.m_meters[i].ProcessAndSaturate(input.m_masterGain.m_expParam * m_output[i]);
        }
    }

    StereoFloat Process(const Input& input, StereoFloat in, bool monoTheBass)
    {
        assert(NumChannels == 2);
        float inputArray[2];
        inputArray[0] = in[0];
        inputArray[1] = in[1];
        Process(input, inputArray, monoTheBass);
        return StereoFloat(m_output[0], m_output[1]);
    }

    QuadFloat Process(const Input& input, QuadFloat in, bool monoTheBass)
    {
        assert(NumChannels == 4);
        float inputArray[4];
        inputArray[0] = in[0];
        inputArray[1] = in[1];
        inputArray[2] = in[2];
        inputArray[3] = in[3];
        Process(input, inputArray, monoTheBass);
        return QuadFloat(m_output[0], m_output[1], m_output[2], m_output[3]);
    }
};