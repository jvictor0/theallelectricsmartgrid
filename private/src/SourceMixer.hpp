#pragma once

#include "Filter.hpp"
#include "TPTSVFFilter.hpp"
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
            float m_input[x_numSourceLanes];
            PhaseUtils::ZeroedExpParam m_gain;
            PhaseUtils::ExpParam m_hpCutoff;
            PhaseUtils::ExpParam m_lpFactor;
            SourceConfig m_config;

            Input()
                : m_input{}
                , m_gain(8.0f)
                , m_hpCutoff(SampleTimer::HzToCycPerSamp(20.0), SampleTimer::HzToCycPerSamp(20000.0))
                , m_lpFactor(1.0, 1000.0)
            {
            }
        };

        struct UIState
        {
            std::atomic<uint32_t> m_color{SmartGrid::Color::White.To32Bit()};
            std::atomic<SourceWidth> m_width{SourceWidth::Mono};

            SmartGrid::Color Color()
            {
                return SmartGrid::Color::From32Bit(m_color.load());
            }
        };

        using Vector = MultiChannelNumber<float, x_numSourceLanes>;

        Source()
            : m_output()
        {
        }

        void Process(const Input& input)
        {
            float gain = input.m_gain.m_expParam;
            float hpCutoff = input.m_hpCutoff.m_expParam;
            float lpFactor = input.m_lpFactor.m_expParam;
            Vector sample = Vector(input.m_input) * gain;

            m_hpFilter.SetCutoffAndResonance(Vector::Splat(hpCutoff), Vector());
            m_lpFilter.SetCutoffAndResonance(Vector::Splat(hpCutoff * lpFactor), Vector());
            m_lpFilter.Process(sample);
            m_hpFilter.Process(m_lpFilter.GetLowPass());
            m_output = m_hpFilter.GetHighPass();

            m_preFilterScopeSample = 0.5f * (sample[0] + sample[1]);
            m_postFilterScopeSample = 0.5f * (m_output[0] + m_output[1]);
        }

        void SetupUIState(UIState* uiState, const Input& input)
        {
            uiState->m_width.store(input.m_config.m_width);
        }

        Vector m_output;

        TPTSVFLowPassFilterGeneric<float, x_numSourceLanes> m_lpFilter;
        TPTSVFHighPassFilterGeneric<float, x_numSourceLanes> m_hpFilter;
        float m_preFilterScopeSample = 0.0f;
        float m_postFilterScopeSample = 0.0f;
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
            for (size_t i = 0; i < x_numSources; ++i)
            {
                size_t leftInput = i * x_numSourceLanes;
                size_t rightInput = leftInput + 1;

                m_sources[i].m_input[0] =
                    leftInput < audioInputBuffer.m_numInputs ? audioInputBuffer.m_input[leftInput] : 0.0f;

                if (m_sources[i].m_config.IsStereo() && rightInput < audioInputBuffer.m_numInputs)
                {
                    m_sources[i].m_input[1] = audioInputBuffer.m_input[rightInput];
                }
                else
                {
                    m_sources[i].m_input[1] = 0.0f;
                }
            }
        }
    };

    // Each stereo source shares one coefficient set. Pack the four independent
    // sources into response lanes instead of evaluating identical stereo lanes.
    //
    struct Snapshot : PowerResponseSnapshot<x_numSources>
    {
        TPTSVFHighPassFilterGeneric<float, x_numSources>::Snapshot m_hp;
        TPTSVFLowPassFilterGeneric<float, x_numSources>::Snapshot m_lp;

        ResponseVector MagnitudeSquaredResponses(const Frequency& frequency) const override
        {
            return m_hp.MagnitudeSquaredResponses(frequency) * m_lp.MagnitudeSquaredResponses(frequency);
        }
    };


    struct UIState : TransferFunctionGeneric<Snapshot>
    {
        using TransferFunctionGeneric<Snapshot>::TransferFunctionGeneric;

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

    SampledSnapshotWriter<UIState> m_responseSampler;

    Snapshot MakeSnapshot() const
    {
        Snapshot snapshot;
        for (size_t i = 0; i < x_numSources; ++i)
        {
            const auto high = m_sources[i].m_hpFilter.MakeSnapshot();
            const auto low = m_sources[i].m_lpFilter.MakeSnapshot();
            snapshot.m_hp.m_g[i] = high.m_g[0];
            snapshot.m_hp.m_k[i] = high.m_k[0];
            snapshot.m_lp.m_g[i] = low.m_g[0];
            snapshot.m_lp.m_k[i] = low.m_k[0];
        }

        return snapshot;
    }

    void PopulateUIState(UIState* uiState, const Input& input)
    {
        for (size_t i = 0; i < x_numSources; ++i)
        {
            uiState->m_sources[i].m_color.store(GetColor(i).To32Bit());
            uiState->m_sources[i].m_width.store(input.m_sources[i].m_config.m_width);
        }
    }

    void SetupUIState(UIState* uiState, const Input& input)
    {
        m_responseSampler.Bind(uiState);
        for (size_t i = 0; i < x_numSources; ++i)
        {
            m_sources[i].SetupUIState(&uiState->m_sources[i], input.m_sources[i]);
            uiState->m_sources[i].m_color.store(GetColor(i).To32Bit());
        }
    }

    void Process(const Input& input)
    {
        MultiChannelNumber<float, x_numSources> preFilter;
        MultiChannelNumber<float, x_numSources> postFilter;
        for (size_t i = 0; i < x_numSources; ++i)
        {
            m_sources[i].Process(input.m_sources[i]);
            preFilter[i] = m_sources[i].m_preFilterScopeSample;
            postFilter[i] = m_sources[i].m_postFilterScopeSample;
        }

        m_responseSampler.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
        m_preFilterScopeWriter.Write(preFilter);
        m_postFilterScopeWriter.Write(postFilter);
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

    void SetupScopeWriters(ScopeWriterInput<x_numSources>* scopeWriter)
    {
        m_preFilterScopeWriter = ScopeWriterHolderGeneric<x_numSources>(scopeWriter,
            static_cast<size_t>(SmartGridOne::QuadScopes::SourcePreFilter));
        m_postFilterScopeWriter = ScopeWriterHolderGeneric<x_numSources>(scopeWriter,
            static_cast<size_t>(SmartGridOne::QuadScopes::SourcePostFilter));
    }

    ScopeWriterHolderGeneric<x_numSources> m_preFilterScopeWriter;
    ScopeWriterHolderGeneric<x_numSources> m_postFilterScopeWriter;
    Source m_sources[x_numSources];
};
