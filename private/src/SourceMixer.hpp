#pragma once

#include "Filter.hpp"
#include "StateVariableFilter.hpp"
#include "PhaseUtils.hpp"
#include "AudioInputBuffer.hpp"
#include "SampleTimer.hpp"
#include "Slew.hpp"

struct SourceMixer
{
    static constexpr size_t x_numSources = 4;

    struct Source
    {
        struct Input
        {
            float m_uBlockInput[SampleTimer::x_controlFrameRate];
            PhaseUtils::ZeroedExpParam m_gain;
            PhaseUtils::ExpParam m_hpCutoff;
            PhaseUtils::ExpParam m_lpFactor;

            Input()
                : m_gain(8.0f)
                , m_hpCutoff(20.0 / 48000.0, 20000.0 / 48000.0)
                , m_lpFactor(1.0, 1000.0)
            {
                for (size_t i = 0; i < SampleTimer::x_controlFrameRate; ++i)
                {
                    m_uBlockInput[i] = 0.0f;
                }
            }
        };

        struct UIState
        {
            LinearStateVariableFilter::UIState m_hpFilter;
            LinearStateVariableFilter::UIState m_lpFilter;

            float FrequencyResponse(float freq)
            {
                float lpResponse = m_lpFilter.LowPassFrequencyResponse(freq);
                float hpResponse = m_hpFilter.HighPassFrequencyResponse(freq);
                return lpResponse * hpResponse;
            }
        };

        Source()
            : m_output(0.0f)
        {
            for (size_t i = 0; i < SampleTimer::x_controlFrameRate; ++i)
            {
                m_uBlockOutput[i] = 0.0f;
            }
        }

        void ProcessUBlock(const Input& input)
        {
            // Update slew targets
            //
            m_gainSlew.Update(input.m_gain.m_expParam);
            m_hpCutoffSlew.Update(input.m_hpCutoff.m_expParam);
            m_lpFactorSlew.Update(input.m_lpFactor.m_expParam);

            for (size_t i = 0; i < SampleTimer::x_controlFrameRate; ++i)
            {
                float gain = m_gainSlew.Process();
                float hpCutoff = m_hpCutoffSlew.Process();
                float lpFactor = m_lpFactorSlew.Process();

                float sample = input.m_uBlockInput[i] * gain;
                m_preFilterScopeWriter.Write(i, sample);
                m_hpFilter.SetCutoff(hpCutoff);
                m_lpFilter.SetCutoff(hpCutoff * lpFactor);
                m_lpFilter.Process(sample);
                sample = m_lpFilter.GetLowPass();
                m_hpFilter.Process(sample);
                sample = m_hpFilter.GetHighPass();
                m_postFilterScopeWriter.Write(i, sample);
                m_uBlockOutput[i] = sample;
            }

            m_output = m_uBlockOutput[SampleTimer::x_controlFrameRate - 1];
        }

        void PopulateUIState(UIState* uiState)
        {
            m_hpFilter.PopulateUIState(&uiState->m_hpFilter);
            m_lpFilter.PopulateUIState(&uiState->m_lpFilter);
        }

        void SetupUIState(UIState* uiState)
        {
        }

        float m_output;
        float m_uBlockOutput[SampleTimer::x_controlFrameRate];

        LinearStateVariableFilter m_lpFilter;
        LinearStateVariableFilter m_hpFilter;
        ScopeWriterHolder m_preFilterScopeWriter;
        ScopeWriterHolder m_postFilterScopeWriter;

        // Slewed parameters
        //
        ParamSlew m_gainSlew;
        ParamSlew m_hpCutoffSlew;
        ParamSlew m_lpFactorSlew;

        void SetupScopeWriters(ScopeWriter* scopeWriter, size_t sourceIx)
        {
            m_preFilterScopeWriter = ScopeWriterHolder(scopeWriter, sourceIx, static_cast<size_t>(SmartGridOne::SourceScopes::PreFilter));
            m_postFilterScopeWriter = ScopeWriterHolder(scopeWriter, sourceIx, static_cast<size_t>(SmartGridOne::SourceScopes::PostFilter));
        }
    };

    struct Input
    {
        Source::Input m_sources[x_numSources];
        bool m_deepVocoderSend[x_numSources];

        Input()
        {
            for (size_t i = 0; i < x_numSources; ++i)
            {
                m_deepVocoderSend[i] = false;
            }
        }

        void SetInputs(const AudioInputBuffer& audioInputBuffer)
        {
            size_t uBlockIndex = SampleTimer::GetUBlockIndex();
            for (size_t i = 0; i < audioInputBuffer.m_numInputs; ++i)
            {
                m_sources[i].m_uBlockInput[uBlockIndex] = audioInputBuffer.m_input[i];
            }
        }
    };

    struct UIState
    {
        Source::UIState m_sources[x_numSources];
        
        static SmartGrid::Color Color(size_t i)
        {
            if (i == 0)
            {
                return SmartGrid::Color::Cyan;
            }
            else if (i == 1)
            {
                return SmartGrid::Color::SeaGreen;
            }
            else if (i == 2)
            {
                return SmartGrid::Color::Ocean;
            }
            else
            {
                return SmartGrid::Color::Indigo;
            }
        }

    };

    void PopulateUIState(UIState* uiState)
    {
        for (size_t i = 0; i < x_numSources; ++i)
        {
            m_sources[i].PopulateUIState(&uiState->m_sources[i]);
        }
    }

    void SetupUIState(UIState* uiState)
    {
        for (size_t i = 0; i < x_numSources; ++i)
        {
            m_sources[i].SetupUIState(&uiState->m_sources[i]);
        }
    }

    void ProcessUBlock(Input& input)
    {
        for (size_t i = 0; i < x_numSources; ++i)
        {
            m_sources[i].ProcessUBlock(input.m_sources[i]);
        }
    }

    float GetDeepVocoderInput(const Input& input)
    {
        float dvInput = 0.0f;
        for (size_t i = 0; i < x_numSources; ++i)
        {
            if (input.m_deepVocoderSend[i])
            {
                dvInput += m_sources[i].m_output;
            }
        }

        return dvInput;
    }

    bool IsVocoderEnabled(const Input& input)
    {
        for (size_t i = 0; i < x_numSources; ++i)
        {
            if (input.m_deepVocoderSend[i])
            {
                return true;
            }
        }
        
        return false;
    }

    void SetupScopeWriters(ScopeWriter* scopeWriter)
    {
        for (size_t i = 0; i < x_numSources; ++i)
        {
            m_sources[i].SetupScopeWriters(scopeWriter, i);
        }
    }

    Source m_sources[x_numSources];
};