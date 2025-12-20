#pragma once

#include "Filter.hpp"
#include "LadderFilter.hpp"
#include "PhaseUtils.hpp"
#include "Metering.hpp"
#include "AudioInputBuffer.hpp"

struct SourceMixer
{
    static constexpr size_t x_numSources = 4;

    struct Source
    {
        struct Input
        {
            float m_input;
            float m_gain;
            PhaseUtils::ExpParam m_hpCutoff;
            PhaseUtils::ExpParam m_lpFactor;

            Input()
                : m_input(0.0f)
                , m_gain(8.0f)
                , m_hpCutoff(20.0 / 48000.0, 20000.0 / 48000.0)
                , m_lpFactor(1.0, 1000.0)
            {
            }
        };

        struct UIState
        {
            std::atomic<float> m_hpAlpha;
            std::atomic<float> m_lpAlpha;

            MeterReader m_meterReader;

            UIState()
                : m_hpAlpha(0.0f)
                , m_lpAlpha(0.0f)
                , m_meterReader()
            {
            }

            void ProcessMeter()
            {
                m_meterReader.Process();
            }

            float FrequencyResponse(float freq)
            {
                float lpResponse = LadderFilter::FrequencyResponse(m_lpAlpha.load(), 0.0f, freq);
                float hpResponse = LadderFilter::FrequencyResponseHP(m_hpAlpha.load(), 0.0f, freq);
                return lpResponse * hpResponse;
            }                
        };

        Source()
            : m_output(0.0f)
            , m_lpFilter()
            , m_hpFilter()
            , m_meter()
        {
            m_lpFilter.SetResonance(0.0f);
            m_hpFilter.SetResonance(0.0f);
        }

        float Process(const Input& input)
        {
            m_output = input.m_input * input.m_gain;
            m_preFilterScopeWriter.Write(m_output);
            m_hpFilter.SetCutoff(input.m_hpCutoff.m_expParam);
            m_lpFilter.SetCutoff(input.m_hpCutoff.m_expParam * input.m_lpFactor.m_expParam);
            m_output = m_lpFilter.Process(m_output);
            m_output = m_hpFilter.ProcessHP(m_output);
            m_output = m_meter.ProcessAndSaturate(m_output);
            m_postFilterScopeWriter.Write(m_output);
            return m_output;
        }

        void PopulateUIState(UIState* uiState)
        {
            uiState->m_hpAlpha.store(m_hpFilter.m_stage4.m_alpha);
            uiState->m_lpAlpha.store(m_lpFilter.m_stage4.m_alpha);
        }

        void SetupUIState(UIState* uiState)
        {
            uiState->m_meterReader.m_meter = &m_meter;
        }

        float m_output;

        LadderFilter m_lpFilter;
        LadderFilter m_hpFilter;
        Meter m_meter;
        ScopeWriterHolder m_preFilterScopeWriter;
        ScopeWriterHolder m_postFilterScopeWriter;

        void SetupScopeWriters(ScopeWriter* scopeWriter, size_t sourceIx)
        {
            m_preFilterScopeWriter = ScopeWriterHolder(scopeWriter, sourceIx, static_cast<size_t>(SmartGridOne::SourceScopes::PreFilter));
            m_postFilterScopeWriter = ScopeWriterHolder(scopeWriter, sourceIx, static_cast<size_t>(SmartGridOne::SourceScopes::PostFilter));
        }
    };

    struct Input
    {
        Source::Input m_sources[x_numSources];

        void SetInputs(const AudioInputBuffer& audioInputBuffer)
        {
            for (size_t i = 0; i < audioInputBuffer.m_numInputs; ++i)
            {
                m_sources[i].m_input = audioInputBuffer.m_input[i];
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

        void ProcessMeters()
        {
            for (size_t i = 0; i < x_numSources; ++i)
            {
                m_sources[i].ProcessMeter();
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

    void Process(Input& input)
    {
        for (size_t i = 0; i < x_numSources; ++i)
        {
            m_sources[i].Process(input.m_sources[i]);
        }
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