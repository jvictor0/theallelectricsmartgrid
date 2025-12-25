#pragma once

#include "QuadUtils.hpp"
#include "DelayLine.hpp"
#include "QuadLFO.hpp"
#include "WavWriter.hpp"
#include <filesystem>
#include <iomanip>
#include <sstream>
#include "Noise.hpp"
#include "QuadMasterChain.hpp"
#include "StereoMasteringChain.hpp"
#include "QuadToStereoMixdown.hpp"
#include "Metering.hpp"

struct QuadMixerInternal
{
    static constexpr size_t x_numSends = 2;

    QuadFloatWithStereoAndSub m_output;
    QuadFloat m_send[x_numSends];
    MultichannelWavWriter m_wavWriter;
    std::string m_recordingDirectory;
    bool m_isRecording = false;
    PinkNoise m_pinkNoise;

    Meter m_voiceMeters[16];
    QuadMeter m_returnMeters[x_numSends];
    QuadMeter m_masterMeter;
    StereoMeter m_stereoMeter;

    static constexpr float x_smoothingAlpha = 0.0007;

    QuadMasterChain m_masterChain;
    QuadToStereoMixdown m_quadToStereoMixdown;
    StereoMasteringChain m_stereoMasteringChain;

    QuadMixerInternal()
    {
    }
    
    struct Input
    {
        size_t m_numInputs;
        float m_input[16];
        float m_gain[16];
        float m_sendGain[16][x_numSends];
        float m_x[16];
        float m_y[16];
        QuadFloat m_return[x_numSends];
        float m_returnGain[x_numSends];
        bool m_noiseMode;

        QuadMasterChain::Input m_masterChainInput;
        StereoMasteringChain::Input m_stereoMasteringChainInput;

        Input()
        {
            m_noiseMode = false;
            m_numInputs = 0;
            for (size_t i = 0; i < 16; ++i)
            {
                m_input[i] = 0.0f;
                m_gain[i] = 1.0f;
                m_x[i] = 0.0f;
                m_y[i] = 0.0f;
            }
            
            for (size_t i = 0; i < x_numSends; ++i)
            {
                m_returnGain[i] = 1.0f;
            }
        }
    };

    bool Open(size_t numInputs, const std::string& filename, uint32_t sampleRate)
    {
        return m_wavWriter.Open(static_cast<uint16_t>(numInputs + x_numSends + 1) * 4 + 2, filename, sampleRate);
    }

    void Close()
    {
        m_wavWriter.Close();
    }

    void StartRecording(size_t numInputs, uint32_t sampleRate)
    {
        if (m_isRecording || m_recordingDirectory.empty())
        {
            return;
        }

        // Find an empty slot
        //
        size_t ordinal = 1;
        std::string filename;
        
        do
        {
            std::ostringstream oss;
            oss << m_recordingDirectory << "/recording-" << std::setfill('0') << std::setw(5) << ordinal;
            filename = oss.str();
            ordinal++;
        } 
        while (rack::system::exists(filename + ".wav") || rack::system::exists(filename + ".wv"));

        filename += ".wav";

        // Open the wave writer with the appropriate number of channels
        //
        if (!Open(numInputs, filename, sampleRate))
        {
            assert(false);
        }   

        m_isRecording = true;
    }

    void StopRecording()
    {
        if (!m_isRecording)
        {
            return;
        }

        Close();
        m_isRecording = false;
    }

    void ToggleRecording(size_t numChannels, uint32_t sampleRate)
    {
        if (m_isRecording)
        {
            StopRecording();
        }
        else
        {
            StartRecording(numChannels, sampleRate);
        }
    }

    void ProcessInputs(const Input& input)
    {
        m_output.m_output = QuadFloat();
        m_quadToStereoMixdown.Clear();
        for (size_t i = 0; i < x_numSends; ++i)
        {
            m_send[i] = QuadFloat();
        }
            
        if (input.m_noiseMode)
        {
            float pink = m_pinkNoise.Generate();
            m_output.m_output += QuadFloat(pink, pink, pink, pink);
            for (size_t i = 0; i < input.m_numInputs; ++i)
            {
                m_output.m_output += QuadFloat(input.m_input[i], input.m_input[i], input.m_input[i], input.m_input[i]) * input.m_gain[i];
                m_quadToStereoMixdown.MixSample(0.5, 0.5, input.m_input[i] * input.m_gain[i]);
            }
        }
        else
        {
            for (size_t i = 0; i < input.m_numInputs; ++i)
            {
                m_quadToStereoMixdown.MixSample(input.m_x[i], input.m_y[i], input.m_input[i] * input.m_gain[i]);

                QuadFloat pan = QuadFloat::Pan(input.m_x[i], input.m_y[i], input.m_input[i]);
                QuadFloat postFader = pan * input.m_gain[i];
                m_output.m_output += postFader;

                m_voiceMeters[i].Process(input.m_input[i] * input.m_gain[i]);

                // Write post-fader input to wave file
                //
                m_wavWriter.WriteSampleIfOpen(4 * static_cast<uint16_t>(i), postFader);
                
                for (size_t j = 0; j < x_numSends; ++j)
                {
                    m_send[j] += pan * input.m_sendGain[i][j];
                }
            }
        }
    }

    QuadFloatWithStereoAndSub ProcessReturns(const Input& input)
    {
        if (!input.m_noiseMode)
        {
            for (size_t j = 0; j < x_numSends; ++j)
            {
                m_quadToStereoMixdown.MixQuadSample(input.m_return[j] * input.m_returnGain[j]);
                QuadFloat postReturn = input.m_return[j] * input.m_returnGain[j];
                m_output.m_output += postReturn;
                
                m_returnMeters[j].Process(postReturn);

                m_wavWriter.WriteSampleIfOpen(4 * static_cast<uint16_t>(input.m_numInputs + j), postReturn);
            }
        }

        m_output = m_masterChain.Process(input.m_masterChainInput, m_output.m_output);
        m_wavWriter.WriteSampleIfOpen(4 *static_cast<uint16_t>(input.m_numInputs + x_numSends), m_output.m_output);
        m_masterMeter.Process(m_output.m_output);

        m_output.m_stereoOutput = m_stereoMasteringChain.Process(input.m_stereoMasteringChainInput, m_quadToStereoMixdown.m_output);
        m_wavWriter.WriteSampleIfOpen(4 * static_cast<uint16_t>(input.m_numInputs + x_numSends + 1), m_output.m_stereoOutput);
        m_stereoMeter.Process(m_output.m_stereoOutput);

        return m_output;
    }

    QuadFloatWithStereoAndSub Process(const Input& input)
    {
        ProcessInputs(input);
        return ProcessReturns(input);
    }
};