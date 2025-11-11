#pragma once

#include "SmartGridInclude.hpp"
#include <JuceHeader.h>
#include "PathDrawer.hpp"

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
            m_windowedFFT[i] = WindowedFFT(&m_uiState->m_squiggleBoyUIState.m_quadScopeWriter, static_cast<size_t>(SquiggleBoyVoice::QuadScopes::Stereo));
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

        for (size_t i = 0; i < 5; ++i)
        {
            StereoMeterReader* meterReader = i < 4 ? &GetMasteringChainUIState()->m_meterReader[i] : &GetMasteringChainUIState()->m_masterMeterReader;
            for (size_t j = 0; j < 2; ++j)
            {
                float rms = meterReader->GetRMSDbFSNormalized(j);
                // float peak = meterReader->GetPeakDbFSNormalized(j);

                float x0Normalized = (4 * i + 1 + j) / 21.0;
                float x1Normalized = (4 * i + 2 + j) / 21.0;
                float yNorm = 1 - rms;

                float screenX0 = x0Normalized * getWidth();
                float screenX1 = x1Normalized * getWidth();
                float screenY0 = yNorm * getHeight();
                float screenY1 = 1.0f * getHeight();

                g.setColour(juce::Colours::green);
                g.fillRect(screenX0, screenY0, screenX1 - screenX0, screenY1 - screenY0);

                float reduction = meterReader->GetReductionDbFSNormalized(j);
                yNorm = 1 - reduction;
                x0Normalized = (4 * i + 0 + 3 * j) / 21.0;
                x1Normalized = (4 * i + 1 + 3 * j) / 21.0;
                screenX0 = x0Normalized * getWidth();
                screenX1 = x1Normalized * getWidth();
                screenY0 = 0;
                screenY1 = yNorm * getHeight();
                g.setColour(juce::Colours::red);
                g.fillRect(screenX0, screenY0, screenX1 - screenX0, screenY1 - screenY0);
            }
        }   
    }
};