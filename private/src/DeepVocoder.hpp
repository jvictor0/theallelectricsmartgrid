#pragma once

#include "SpectralModel.hpp"
#include "PhaseUtils.hpp"
#include "TheNonagon.hpp"
#include "AHD.hpp"
#include <limits>

struct DeepVocoder
{
    static constexpr size_t x_tableSize = SpectralModel::x_tableSize;
    static constexpr size_t x_maxComponents = SpectralModel::x_maxComponents;
    static constexpr size_t x_H = SpectralModel::x_H;
    static constexpr size_t x_numVoices = TheNonagonInternal::x_numVoices;

    static float MagnitudeThreshold(float omegaCenter, float omegaTest, float slopeUp, float slopeDown, float gainThreshold)
    {
        float omegaRatioUp = omegaTest / omegaCenter;
        float omegaRatioDown = omegaCenter / omegaTest;
        float omegaRatio = std::max(omegaRatioUp, omegaRatioDown);
        float slope = omegaRatioUp > omegaRatioDown ? slopeUp : slopeDown;
        float factor = std::powf(omegaRatio, slope);
        return gainThreshold * factor;
    }

    struct VoiceInput
    {
        PhaseUtils::ExpParam m_gainThreshold;
        PhaseUtils::ExpParam m_slopeUp;
        PhaseUtils::ExpParam m_slopeDown;
        PhaseUtils::ExpParam m_pitchRatioPre;
        float m_pitchCenter;
        float m_pitchRatioPost;

        VoiceInput()
            : m_gainThreshold(0.001f, 0.2f)
            , m_slopeUp(1.0f / 16.0f, 16.0f)
            , m_slopeDown(1.0f / 16.0f, 16.0f)
            , m_pitchRatioPre(1.0f / 16.0f, 8.0f)
            , m_pitchCenter(0.0f)
            , m_pitchRatioPost(1.0f)
        {
        }
    };

    struct Input
    {
        PhaseUtils::ExpParam m_slewUp;
        PhaseUtils::ExpParam m_slewDown;
        bool m_enabled;
        VoiceInput m_voiceInput[x_numVoices];
        size_t m_numAtoms;

        Input()
            : m_slewUp(0.1f * 48000.0f / x_H, 10.0f * 48000.0f / x_H)
            , m_slewDown(0.5f * 48000.0f / x_H, 60.0f * 48000.0f / x_H)
            , m_enabled(false)
            , m_numAtoms(64)
        {
        }

        void SetSlew(float slewUpKnob, float slewDownKnob)
        {
            m_slewUp.Update(slewUpKnob);
            m_slewDown.Update(slewDownKnob);
        }

        SpectralModel::Input MakeSpectralInput() const
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
        , m_enabled(false)
    {
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_buffer[i] = 0.0f;
        }
    }

    void Process(float inputSample, const Input& input)
    {
        m_buffer[m_index % x_tableSize] = inputSample;
        ++m_index;
        m_enabled = input.m_enabled;
        if (m_index % x_H == 0)
        {
            SpectralModel::Input spectralInput = input.MakeSpectralInput();
            
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_voiceState[i].m_gainThreshold = input.m_voiceInput[i].m_gainThreshold.m_expParam;
                m_voiceState[i].m_slopeUp = input.m_voiceInput[i].m_slopeUp.m_expParam;
                m_voiceState[i].m_slopeDown = input.m_voiceInput[i].m_slopeDown.m_expParam;
                m_voiceState[i].m_pitchCenter = input.m_voiceInput[i].m_pitchCenter;
                m_voiceState[i].m_pitchRatioPre = input.m_voiceInput[i].m_pitchRatioPre.m_expParam;
                m_voiceState[i].m_pitchRatioPost = input.m_voiceInput[i].m_pitchRatioPost;
            }

            SpectralModel::Buffer buffer;
            for (size_t i = 0; i < x_tableSize; ++i)
            {
                buffer.m_table[i] = m_buffer[(m_index + i) % x_tableSize] * Math4096::Hann(i);
            }

            m_spectralModel.ExtractAtoms(buffer, spectralInput);
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                if (m_voiceState[i].m_atom && !m_spectralModel.IsAtomAllocated(m_voiceState[i].m_atom))
                {
                    m_voiceState[i].m_atom = nullptr;
                }

                m_voiceState[i].m_atomicRatio = AtomicRatio(i);
            }
        }
    }

    float AtomicRatio(size_t index)
    {
        if (!m_voiceState[index].m_atom)
        {
            return 1.0f;
        }

        float thresh = MagnitudeThreshold(
            m_voiceState[index].m_pitchCenter * m_voiceState[index].m_pitchRatioPre,
            m_voiceState[index].m_atom->m_synthesisOmega,
            m_voiceState[index].m_slopeUp,
            m_voiceState[index].m_slopeDown,
            m_voiceState[index].m_gainThreshold);
        return std::max(1.0f, m_voiceState[index].m_atom->m_synthesisMagnitude / thresh);
    }

    float TransformNote(size_t index, AHD::AHDControl* ahdControl)
    {
        if (!m_enabled)
        {
            m_voiceState[index].m_atom = nullptr;
            return m_voiceState[index].m_pitchCenter * m_voiceState[index].m_pitchRatioPost;
        }

        float omega = m_voiceState[index].m_pitchCenter;
        if (m_spectralModel.m_atoms.Empty())
        {
            m_voiceState[index].m_atom = nullptr;
            ahdControl->m_trig = false;            
            return omega * m_voiceState[index].m_pitchRatioPost;
        }

        float targetOmega = omega * m_voiceState[index].m_pitchRatioPre;
        SpectralModel::Atom* bestAtom = nullptr;
        float bestDist = std::numeric_limits<float>::min();

        for (size_t i = 0; i < m_spectralModel.m_atoms.Size(); ++i)
        {
            SpectralModel::Atom* atom = m_spectralModel.m_atoms[i];
            float threshold = MagnitudeThreshold(
                targetOmega, 
                atom->m_synthesisOmega, 
                m_voiceState[index].m_slopeUp, 
                m_voiceState[index].m_slopeDown, 
                m_voiceState[index].m_gainThreshold);
            if (atom->m_synthesisMagnitude < threshold)
            {
                continue;
            }

            float dist = atom->m_synthesisMagnitude / threshold;
            if (bestDist < dist)
            {
                bestDist = dist;
                bestAtom = atom;
            }
        }

        m_voiceState[index].m_atom = bestAtom;
        if (!bestAtom)
        {
            ahdControl->m_trig = false;
            return targetOmega * m_voiceState[index].m_pitchRatioPost;
        }

        return bestAtom->m_synthesisOmega * m_voiceState[index].m_pitchRatioPost;
    }

    struct VoiceState
    {
        float m_gainThreshold;
        float m_slopeUp;
        float m_slopeDown;
        float m_pitchCenter;
        float m_pitchRatioPre;
        float m_pitchRatioPost;
        float m_atomicRatio;
        SpectralModel::Atom* m_atom;

        VoiceState()
            : m_gainThreshold(0.0f)
            , m_slopeUp(1.0f)
            , m_slopeDown(1.0f)
            , m_pitchCenter(0.0f)
            , m_pitchRatioPre(1.0f)
            , m_pitchRatioPost(1.0f)
            , m_atomicRatio(1.0f)
            , m_atom(nullptr)
        {
        }
    };

    struct UIState
    {
        static constexpr size_t x_uiBufferSize = 16;
        struct VoiceUIState
        {
            std::atomic<float> m_threshold;
            std::atomic<float> m_slopeUp;
            std::atomic<float> m_slopeDown;
            std::atomic<float> m_pitchCenter;
            std::atomic<float> m_usedOmega;
            std::atomic<float> m_atomMagnitude;

            float MagnitudeThreshold(float testFrequency) const
            {
                float omegaCenter = m_pitchCenter.load();
                if (omegaCenter <= 0.0f)
                {
                    return 0.0f;
                }

                return DeepVocoder::MagnitudeThreshold(omegaCenter, testFrequency, m_slopeUp.load(), m_slopeDown.load(), m_threshold.load());
            }
        };
        
        VoiceUIState m_voiceUIState[x_numVoices];
        std::atomic<size_t> m_numFrequencies[x_uiBufferSize];
        std::atomic<size_t> m_which;
        std::atomic<float> m_frequencies[x_uiBufferSize][x_maxComponents];
        std::atomic<float> m_magnitudes[x_uiBufferSize][x_maxComponents];

        UIState()
            : m_which(0)
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_voiceUIState[i].m_threshold.store(0.0f);
                m_voiceUIState[i].m_slopeUp.store(1.0f);
                m_voiceUIState[i].m_slopeDown.store(1.0f);
                m_voiceUIState[i].m_pitchCenter.store(0.0f);
                m_voiceUIState[i].m_usedOmega.store(0.0f);
                m_voiceUIState[i].m_atomMagnitude.store(0.0f);
            }

            for (size_t i = 0; i < x_uiBufferSize; ++i)
            {
                m_numFrequencies[i].store(0);
                for (size_t j = 0; j < x_maxComponents; ++j)
                {
                    m_frequencies[i][j].store(0.0f);
                    m_magnitudes[i][j].store(0.0f);
                }
            }
        }

        size_t Which()
        {
            return m_which.load();
        }

        float GetThreshold(size_t index)
        {
            return m_voiceUIState[index].m_threshold.load();
        }

        float GetSlopeUp(size_t index)
        {
            return m_voiceUIState[index].m_slopeUp.load();
        }

        float GetSlopeDown(size_t index)
        {
            return m_voiceUIState[index].m_slopeDown.load();
        }

        float GetPitchCenter(size_t index)
        {
            return m_voiceUIState[index].m_pitchCenter.load();
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
            return m_voiceUIState[index].m_usedOmega.load();
        }

        float GetAtomMagnitude(size_t index)
        {
            return m_voiceUIState[index].m_atomMagnitude.load();
        }
    };

    void PopulateUIState(UIState* uiState)
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            uiState->m_voiceUIState[i].m_threshold.store(m_voiceState[i].m_gainThreshold);
            uiState->m_voiceUIState[i].m_slopeUp.store(m_voiceState[i].m_slopeUp);
            uiState->m_voiceUIState[i].m_slopeDown.store(m_voiceState[i].m_slopeDown);
            uiState->m_voiceUIState[i].m_pitchCenter.store(m_voiceState[i].m_pitchCenter * m_voiceState[i].m_pitchRatioPre);
            
            if (m_voiceState[i].m_atom != nullptr)
            {
                uiState->m_voiceUIState[i].m_usedOmega.store(m_voiceState[i].m_atom->m_synthesisOmega);
                uiState->m_voiceUIState[i].m_atomMagnitude.store(m_voiceState[i].m_atom->m_synthesisMagnitude);
            }
            else
            {
                uiState->m_voiceUIState[i].m_usedOmega.store(-1.0);
                uiState->m_voiceUIState[i].m_atomMagnitude.store(0.0f);
            }
        }

        size_t which = (uiState->m_which.load() + 1) % UIState::x_uiBufferSize;
        size_t numFreqs = m_spectralModel.m_atoms.Size();

        for (size_t i = 0; i < numFreqs; ++i)
        {
            uiState->m_frequencies[which][i].store(m_spectralModel.m_atoms[i]->m_synthesisOmega);
            uiState->m_magnitudes[which][i].store(m_spectralModel.m_atoms[i]->m_synthesisMagnitude);
        }

        uiState->m_numFrequencies[which].store(numFreqs);
        uiState->m_which.store(which);
    }

    SpectralModel m_spectralModel;
    float m_buffer[x_tableSize];
    size_t m_index;
    bool m_enabled;
    VoiceState m_voiceState[x_numVoices];
};
