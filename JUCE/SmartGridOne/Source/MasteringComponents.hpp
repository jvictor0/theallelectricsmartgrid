#pragma once

#include "SmartGridInclude.hpp"
#include <JuceHeader.h>
#include "PathDrawer.hpp"
#include "VUBarDrawer.hpp"

struct MultibandEQComponent : public juce::Component
{
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;
    WindowedFFT m_windowedFFT[4];

    MultibandEQComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : m_uiState(uiState)
    {
        setSize(400, 200);
        for (size_t i = 0; i < 4; ++i)
        {
            m_windowedFFT[i] = WindowedFFT(&m_uiState->m_squiggleBoyUIState.m_quadScopeWriter, static_cast<size_t>(SmartGridOne::QuadScopes::Stereo));
        }
    }

    MultibandSaturator<4, 2>::UIState* GetMasteringChainUIState()
    {
        return &m_uiState->m_squiggleBoyUIState.m_stereoMasteringChainUIState;
    }

    struct EQResponseDrawer
    {
        MultibandSaturator<4, 2>::UIState* m_uiState;

        EQResponseDrawer(MultibandSaturator<4, 2>::UIState* uiState)
            : m_uiState(uiState)
        {
        }

        float operator()(float freq)
        {
            return PathDrawer::AmpToDbNormalized(m_uiState->FrequencyResponse(freq)) / 2;
        }
    };

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        for (size_t i = 0; i < 4; ++i)
        {
            m_windowedFFT[i].Compute(i);
            PathDrawer pathDrawer(getHeight(), getWidth(), 0, 0);
            pathDrawer.DrawWindowedDFT(g, i < 2 ? juce::Colours::white : juce::Colours::grey, &m_windowedFFT[i]);
        }

        PathDrawer pathDrawer(getHeight(), getWidth(), 0, 0);
        pathDrawer.DrawPath(g, juce::Colours::white, EQResponseDrawer(GetMasteringChainUIState()));
    }
};

struct MultibandGainReductionComponent : public juce::Component
{
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;

    MultibandGainReductionComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : m_uiState(uiState)
    {
        setSize(400, 200);
    }

    MultibandSaturator<4, 2>::UIState* GetMasteringChainUIState()
    {
        return &m_uiState->m_squiggleBoyUIState.m_stereoMasteringChainUIState;
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        VUBarDrawer vuBarDrawer(getHeight(), getWidth(), 0, 0);

        for (size_t i = 0; i < 5; ++i)
        {
            StereoMeterReader* meterReader = i < 4 ? &GetMasteringChainUIState()->m_meterReader[i] : &GetMasteringChainUIState()->m_masterMeterReader;
            for (size_t j = 0; j < 2; ++j)
            {
                float rms = meterReader->GetRMSDbFSNormalized(j);

                float x0Normalized = (4 * i + 1 + j) / 21.0;
                float x1Normalized = (4 * i + 2 + j) / 21.0;

                vuBarDrawer.DrawBar(g, juce::Colours::green, x0Normalized, x1Normalized, rms);

                float reduction = meterReader->GetReductionDbFSNormalized(j);
                x0Normalized = (4 * i + 0 + 3 * j) / 21.0;
                x1Normalized = (4 * i + 1 + 3 * j) / 21.0;

                vuBarDrawer.DrawReduction(g, juce::Colours::red, x0Normalized, x1Normalized, reduction);
            }
        }   
    }
};

struct SourceMixerFrequencyComponent : public juce::Component
{
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;
    WindowedFFT m_windowedFFT[SourceMixer::x_numSources][2];

    SourceMixerFrequencyComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : m_uiState(uiState)
    {
        setSize(400, 200);

        for (int i = 0; i < SourceMixer::x_numSources; ++i)
        {
            m_windowedFFT[i][0] = WindowedFFT(&m_uiState->m_squiggleBoyUIState.m_sourceMixerScopeWriter, static_cast<size_t>(SmartGridOne::SourceScopes::PreFilter));
            m_windowedFFT[i][1] = WindowedFFT(&m_uiState->m_squiggleBoyUIState.m_sourceMixerScopeWriter, static_cast<size_t>(SmartGridOne::SourceScopes::PostFilter));
        }
    }

    SourceMixer::UIState* GetSourceMixerUIState()
    {
        return &m_uiState->m_squiggleBoyUIState.m_sourceMixerUIState;
    }

    struct ResponseDrawer
    {
        SourceMixer::Source::UIState* m_uiState;

        ResponseDrawer(SourceMixer::Source::UIState* uiState)
            : m_uiState(uiState)
        {
        }

        float operator()(float freq)
        {
            return PathDrawer::AmpToDbNormalized(m_uiState->FrequencyResponse(freq)) / 2;
        }
    };

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            m_windowedFFT[i][0].Compute(i);
            PathDrawer pathDrawer(getHeight(), getWidth(), 0, 0);
            SmartGrid::Color color = SourceMixer::UIState::Color(i).Dim();
            pathDrawer.DrawWindowedDFT(g, J(color), &m_windowedFFT[i][0]);
        }

        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            m_windowedFFT[i][1].Compute(i);
            PathDrawer pathDrawer(getHeight(), getWidth(), 0, 0);
            SmartGrid::Color color = SourceMixer::UIState::Color(i);
            pathDrawer.DrawWindowedDFT(g, J(color), &m_windowedFFT[i][1]);
        }

        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            PathDrawer pathDrawer(getHeight(), getWidth(), 0, 0);
            pathDrawer.DrawPath(g, J(SourceMixer::UIState::Color(i)), ResponseDrawer(&GetSourceMixerUIState()->m_sources[i]));
        }

        // UNDONE(DEEP_VOCODER)
        //
        size_t which = GetSourceMixerUIState()->m_deepVocoderUIState.Which();
        size_t numFrequencies = GetSourceMixerUIState()->m_deepVocoderUIState.GetNumFrequencies(which);
        for (size_t i = 0; i < numFrequencies; ++i)
        {
            float frequency = GetSourceMixerUIState()->m_deepVocoderUIState.GetFrequency(which, i);
            float x = PathDrawer::LinearToLog(frequency) * getWidth();
            g.setColour(juce::Colours::white);
            g.drawLine(x, 0, x, getHeight(), 1.5f);
        }

        for (size_t i = 0; i < 9; ++i)
        {
            float usedOmega = GetSourceMixerUIState()->m_deepVocoderUIState.GetUsedOmega(i);
            float x = PathDrawer::LinearToLog(usedOmega) * getWidth();
            g.setColour(J(TheNonagonSmartGrid::VoiceColor(i)));
            g.drawLine(x, 0, x, getHeight(), 1.5f);
        }

        float threshY = PathDrawer::AmpToDbNormalized(GetSourceMixerUIState()->m_deepVocoderUIState.GetThreshold() * 2);
        float yCoord = getHeight() * (1.0f - threshY);
        g.setColour(juce::Colours::white);
        g.drawLine(0, yCoord, getWidth(), yCoord, 1.5f);
    }
};

struct SourceMixerReductionComponent : public juce::Component
{
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;

    SourceMixerReductionComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : m_uiState(uiState)
    {
        setSize(400, 200);
    }

    SourceMixer::UIState* GetSourceMixerUIState()
    {
        return &m_uiState->m_squiggleBoyUIState.m_sourceMixerUIState;
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        VUBarDrawer vuBarDrawer(getHeight(), getWidth(), 0, 0);

        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            MeterReader* meterReader = &GetSourceMixerUIState()->m_sources[i].m_meterReader;
            float rms = meterReader->GetRMSDbFSNormalized();
            float reduction = meterReader->GetReductionDbFSNormalized();
            float x0Normalized = 2 * i / static_cast<float>(2 * SourceMixer::x_numSources);
            float x1Normalized = (2 * i + 1) / static_cast<float>(2 * SourceMixer::x_numSources);
            float x2Normalized = (2 * i + 2) / static_cast<float>(2 * SourceMixer::x_numSources);

            vuBarDrawer.DrawBar(g, J(SourceMixer::UIState::Color(i)), x0Normalized, x1Normalized, rms);
            vuBarDrawer.DrawReduction(g, juce::Colours::red, x1Normalized, x2Normalized, reduction);
        }   
    }
};