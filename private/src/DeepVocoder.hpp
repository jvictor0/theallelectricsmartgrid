#pragma once

#include "Resynthesis.hpp"
#include "PhaseUtils.hpp"

struct DeepVocoder
{
    static constexpr size_t x_tableSize = SpectralModel::x_tableSize;
    static constexpr size_t x_maxComponents = SpectralModel::x_maxComponents;
    static constexpr size_t x_H = SpectralModel::x_H;

    struct Input
    {
        PhaseUtils::ExpParam m_slewUp;
        PhaseUtils::ExpParam m_slewDown;
        PhaseUtils::ExpParam m_gainThreshold;
        float m_ratio;
        size_t m_numAtoms;

        Input()
            : m_slewUp(0.1f * 48000.0f / x_H, 10.0f * 48000.0f / x_H)
            , m_slewDown(0.5f * 48000.0f / x_H, 60.0f * 48000.0f / x_H)
            , m_gainThreshold(0.001f, 0.2f)
            , m_ratio(1.0f)
            , m_numAtoms(64)
        {
        }

        void SetSlew(float slewUpKnob, float slewDownKnob)
        {
            m_slewUp.Update(slewUpKnob);
            m_slewDown.Update(slewDownKnob);
        }

        SpectralModel::Input MakeSpectralInput()
        {
            SpectralModel::Input result;
            result.m_slewUpAlpha = 1.0f - std::exp(-1.0f / m_slewUp.m_expParam);
            result.m_slewDownAlpha = 1.0f - std::exp(-1.0f / m_slewDown.m_expParam);
            result.m_gainThreshold = 1e-4f;
            result.m_numAtoms = m_numAtoms;
            return result;
        }
    };

    DeepVocoder()
        : m_index(0)
        , m_ratio(1.0f)
        , m_gainThreshold(0.0f)
    {
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_buffer[i] = 0.0f;
        }

        for (size_t i = 0; i < 9; ++i)
        {
            m_usedOmegas[i] = -1.0f;
        }
    }

    void Process(float inputSample, Input& input)
    {
        m_buffer[m_index % x_tableSize] = inputSample;
        ++m_index;
        if (m_index % x_H == 0)
        {
            SpectralModel::Input spectralInput = input.MakeSpectralInput();
            m_ratio = std::powf(2.0f, std::round(input.m_ratio * 5.0f - 2.5f));
            m_gainThreshold = input.m_gainThreshold.m_expParam;

            SpectralModel::Buffer buffer;
            for (size_t i = 0; i < x_tableSize; ++i)
            {
                buffer.m_table[i] = m_buffer[(m_index + i) % x_tableSize] * Math4096::Hann(i);
            }

            m_spectralModel.ExtractAtoms(buffer, spectralInput);
        }
    }

    float Search(float omega, size_t index)
    {
        if (m_spectralModel.m_atoms.Empty())
        {
            m_usedOmegas[index] = -1.0f;
            return omega;
        }

        float targetOmega = omega * m_ratio;
        float bestOmega = -1.0f;
        float bestDist = std::abs(bestOmega - targetOmega);

        for (size_t i = 0; i < m_spectralModel.m_atoms.Size(); ++i)
        {
            if (m_spectralModel.m_atoms[i].m_synthesisMagnitude < m_gainThreshold)
            {
                continue;
            }

            float dist = std::abs(m_spectralModel.m_atoms[i].m_synthesisOmega - targetOmega);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestOmega = m_spectralModel.m_atoms[i].m_synthesisOmega;
            }
        }

        m_usedOmegas[index] = bestOmega;
        return bestOmega < 0.0f ? omega : bestOmega;
    }

    struct UIState
    {
        static constexpr size_t x_uiBufferSize = 16;
        std::atomic<float> m_threshold;
        std::atomic<size_t> m_numFrequencies[x_uiBufferSize];
        std::atomic<size_t> m_which;
        std::atomic<float> m_frequencies[x_uiBufferSize][x_maxComponents];
        std::atomic<float> m_magnitudes[x_uiBufferSize][x_maxComponents];
        std::atomic<float> m_usedOmegas[9];

        UIState()
            : m_threshold(0.0f)
            , m_which(0)
        {
            for (size_t i = 0; i < x_uiBufferSize; ++i)
            {
                m_numFrequencies[i].store(0);
                for (size_t j = 0; j < x_maxComponents; ++j)
                {
                    m_frequencies[i][j].store(0.0f);
                    m_magnitudes[i][j].store(0.0f);
                }
            }

            for (size_t i = 0; i < 9; ++i)
            {
                m_usedOmegas[i].store(0.0f);
            }
        }

        size_t Which()
        {
            return m_which.load();
        }

        float GetThreshold()
        {
            return m_threshold.load();
        }

        size_t GetNumFrequencies(size_t which)
        {
            return m_numFrequencies[which].load();
        }

        float GetFrequency(size_t which, size_t index)
        {
            return m_frequencies[which][index].load();
        }

        float GetMagnitude(size_t which, size_t index)
        {
            return m_magnitudes[which][index].load();
        }

        float GetUsedOmega(size_t index)
        {
            return m_usedOmegas[index].load();
        }
    };

    void PopulateUIState(UIState* uiState)
    {
        uiState->m_threshold.store(m_gainThreshold);

        size_t which = (uiState->m_which.load() + 1) % UIState::x_uiBufferSize;
        size_t numFreqs = m_spectralModel.m_atoms.Size();

        for (size_t i = 0; i < numFreqs; ++i)
        {
            uiState->m_frequencies[which][i].store(m_spectralModel.m_atoms[i].m_synthesisOmega);
            uiState->m_magnitudes[which][i].store(m_spectralModel.m_atoms[i].m_synthesisMagnitude);
        }

        uiState->m_numFrequencies[which].store(numFreqs);
        uiState->m_which.store(which);

        for (size_t i = 0; i < 9; ++i)
        {
            uiState->m_usedOmegas[i].store(m_usedOmegas[i]);
        }
    }

    SpectralModel m_spectralModel;
    float m_buffer[x_tableSize];
    size_t m_index;
    float m_usedOmegas[9];
    float m_ratio;
    float m_gainThreshold;
};
