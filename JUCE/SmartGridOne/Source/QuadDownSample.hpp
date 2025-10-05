#pragma once

#include "QuadUtils.hpp"
#include <JuceHeader.h>

struct QuadDownSampler
{
    float* m_backBuffer[2];
    juce::Reverb m_reverb;
    int m_numSamples;

    void PrepareToPlay(int numSamples, double sampleRate)
    {
        juce::Reverb::Parameters p;
        p.roomSize   = 0.5f;
        p.damping    = 0.5f;
        p.wetLevel   = 0.3f;
        p.dryLevel   = 0.7f;
        p.width      = 0.25f;
        p.freezeMode = 0.0f;

        m_reverb.setParameters(p);
        m_reverb.setSampleRate(sampleRate);

        m_backBuffer[0] = new float[numSamples];
        m_backBuffer[1] = new float[numSamples];
        m_numSamples = numSamples;
    }

    QuadDownSampler()
    {
        m_backBuffer[0] = nullptr;
        m_backBuffer[1] = nullptr;
    }

    ~QuadDownSampler()
    {
        delete[] m_backBuffer[0];
        delete[] m_backBuffer[1];
    }

    void AddBackSamples(QuadFloat x, int sample)
    {
        m_backBuffer[0][sample] = x[2];
        m_backBuffer[1][sample] = x[3];
    }

    void ProcessReverb(const juce::AudioSourceChannelInfo& bufferToFill)
    {
        m_reverb.processStereo(m_backBuffer[0], m_backBuffer[1], m_numSamples);
        for (int i = 0; i < bufferToFill.numSamples; ++i)
        {
            float mixL = m_backBuffer[1][i] * 0.7 + bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample)[i] * 0.7;
            float mixR = m_backBuffer[0][i] * 0.7 + bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample)[i] * 0.7;
            bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample)[i] = mixL;
            bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample)[i] = mixR;
        }
    }
};