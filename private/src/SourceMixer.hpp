#pragma once

#include "Filter.hpp"
#include "StateVariableFilter.hpp"
#include "PhaseUtils.hpp"
#include "AudioInputBuffer.hpp"
#include "Color.hpp"
#include "SampleTimer.hpp"
#include "ScopeWriter.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "Slew.hpp"

struct SourceMixer
{
    enum class SourceWidth
    {
        Mono,
        Stereo,
    };

    enum class SourceLane
    {
        Left,
        Right,
    };

    struct SourceConfig
    {
        SourceWidth m_width;

        SourceConfig()
            : m_width(SourceWidth::Mono)
        {
        }

        bool IsStereo() const
        {
            return m_width == SourceWidth::Stereo;
        }
    };

    static constexpr size_t x_numSources = 4;
    static constexpr size_t x_numSourceLanes = 2;
    static constexpr size_t x_numOutputChannels = x_numSources * x_numSourceLanes;
    static constexpr size_t x_numPhysicalInputChannels = x_numSources * x_numSourceLanes;

    static constexpr size_t LaneIndex(SourceLane lane)
    {
        return lane == SourceLane::Right ? 1 : 0;
    }

    static SmartGrid::Color GetColor(size_t i)
    {
        static const SmartGrid::Color x_sourceColors[x_numSources] =
        {
            SmartGrid::Color::Cyan,
            SmartGrid::Color::SeaGreen,
            SmartGrid::Color::Ocean,
            SmartGrid::Color::Indigo,
        };

        if (i < x_numSources)
        {
            return x_sourceColors[i];
        }

        return SmartGrid::Color::White;
    }

    struct Source
    {
        struct Input
        {
            float m_uBlockInput[x_numSourceLanes][SampleTimer::x_controlFrameRate];
            PhaseUtils::ZeroedExpParam m_gain;
            PhaseUtils::ExpParam m_hpCutoff;
            PhaseUtils::ExpParam m_lpFactor;
            SourceConfig m_config;

            Input()
                : m_gain(8.0f)
                , m_hpCutoff(20.0 / 48000.0, 20000.0 / 48000.0)
                , m_lpFactor(1.0, 1000.0)
            {
                for (size_t lane = 0; lane < x_numSourceLanes; ++lane)
                {
                    for (size_t i = 0; i < SampleTimer::x_controlFrameRate; ++i)
                    {
                        m_uBlockInput[lane][i] = 0.0f;
                    }
                }
            }
        };

        struct UIState
        {
            LinearStateVariableFilter::UIState m_hpFilter[x_numSourceLanes];
            LinearStateVariableFilter::UIState m_lpFilter[x_numSourceLanes];
            std::atomic<uint32_t> m_color;
            std::atomic<SourceWidth> m_width;

            UIState()
                : m_color(SmartGrid::Color::White.To32Bit())
                , m_width(SourceWidth::Mono)
            {
            }

            float FrequencyResponse(float freq)
            {
                float lpResponse = m_lpFilter[0].LowPassFrequencyResponse(freq);
                float hpResponse = m_hpFilter[0].HighPassFrequencyResponse(freq);
                return lpResponse * hpResponse;
            }

            SmartGrid::Color Color()
            {
                return SmartGrid::Color::From32Bit(m_color.load());
            }
        };

        Source()
        {
            for (size_t lane = 0; lane < x_numSourceLanes; ++lane)
            {
                m_output[lane] = 0.0f;
                for (size_t i = 0; i < SampleTimer::x_controlFrameRate; ++i)
                {
                    m_uBlockOutput[lane][i] = 0.0f;
                }
            }
        }

        void ProcessUBlock(const Input& input)
        {
            // Update slew targets.
            //
            m_gainSlew.Update(input.m_gain.m_expParam);
            m_hpCutoffSlew.Update(input.m_hpCutoff.m_expParam);
            m_lpFactorSlew.Update(input.m_lpFactor.m_expParam);

            for (size_t i = 0; i < SampleTimer::x_controlFrameRate; ++i)
            {
                float gain = m_gainSlew.Process();
                float hpCutoff = m_hpCutoffSlew.Process();
                float lpFactor = m_lpFactorSlew.Process();
                float preFilterScopeSample[x_numSourceLanes];
                float postFilterScopeSample[x_numSourceLanes];

                for (size_t lane = 0; lane < x_numSourceLanes; ++lane)
                {
                    float sample = input.m_uBlockInput[lane][i] * gain;
                    preFilterScopeSample[lane] = sample;

                    m_hpFilter[lane].SetCutoff(hpCutoff);
                    m_lpFilter[lane].SetCutoff(hpCutoff * lpFactor);
                    m_lpFilter[lane].Process(sample);
                    sample = m_lpFilter[lane].GetLowPass();
                    m_hpFilter[lane].Process(sample);
                    sample = m_hpFilter[lane].GetHighPass();

                    postFilterScopeSample[lane] = sample;
                    m_uBlockOutput[lane][i] = sample;
                }

                m_preFilterScopeWriter.Write(i, 0.5f * (preFilterScopeSample[0] + preFilterScopeSample[1]));
                m_postFilterScopeWriter.Write(i, 0.5f * (postFilterScopeSample[0] + postFilterScopeSample[1]));
            }

            for (size_t lane = 0; lane < x_numSourceLanes; ++lane)
            {
                m_output[lane] = m_uBlockOutput[lane][SampleTimer::x_controlFrameRate - 1];
            }
        }

        void PopulateUIState(UIState* uiState)
        {
            for (size_t lane = 0; lane < x_numSourceLanes; ++lane)
            {
                m_hpFilter[lane].PopulateUIState(&uiState->m_hpFilter[lane]);
                m_lpFilter[lane].PopulateUIState(&uiState->m_lpFilter[lane]);
            }
        }

        void SetupUIState(UIState* uiState, const Input& input)
        {
            uiState->m_width.store(input.m_config.m_width);
        }

        float m_output[x_numSourceLanes];
        float m_uBlockOutput[x_numSourceLanes][SampleTimer::x_controlFrameRate];

        LinearStateVariableFilter m_lpFilter[x_numSourceLanes];
        LinearStateVariableFilter m_hpFilter[x_numSourceLanes];
        ScopeWriterHolder m_preFilterScopeWriter;
        ScopeWriterHolder m_postFilterScopeWriter;

        // Slewed parameters.
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
            for (size_t i = 0; i < x_numSources; ++i)
            {
                size_t leftInput = i * x_numSourceLanes;
                size_t rightInput = leftInput + 1;

                m_sources[i].m_uBlockInput[0][uBlockIndex] =
                    leftInput < audioInputBuffer.m_numInputs ? audioInputBuffer.m_input[leftInput] : 0.0f;

                if (m_sources[i].m_config.IsStereo() && rightInput < audioInputBuffer.m_numInputs)
                {
                    m_sources[i].m_uBlockInput[1][uBlockIndex] = audioInputBuffer.m_input[rightInput];
                }
                else
                {
                    m_sources[i].m_uBlockInput[1][uBlockIndex] = 0.0f;
                }
            }
        }
    };

    struct UIState
    {
        Source::UIState m_sources[x_numSources];
        
        SmartGrid::Color Color(size_t i)
        {
            if (i < x_numSources)
            {
                return m_sources[i].Color();
            }

            return SmartGrid::Color::White;
        }
    };

    void PopulateUIState(UIState* uiState, const Input& input)
    {
        for (size_t i = 0; i < x_numSources; ++i)
        {
            m_sources[i].PopulateUIState(&uiState->m_sources[i]);
            uiState->m_sources[i].m_color.store(GetColor(i).To32Bit());
            uiState->m_sources[i].m_width.store(input.m_sources[i].m_config.m_width);
        }
    }

    void SetupUIState(UIState* uiState, const Input& input)
    {
        for (size_t i = 0; i < x_numSources; ++i)
        {
            m_sources[i].SetupUIState(&uiState->m_sources[i], input.m_sources[i]);
            uiState->m_sources[i].m_color.store(GetColor(i).To32Bit());
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
                dvInput += 0.5f * (m_sources[i].m_output[0] + m_sources[i].m_output[1]);
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
