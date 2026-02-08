#pragma once

#include "QuadUtils.hpp"
#include "DelayLine.hpp"
#include "QuadLFO.hpp"
#include "WavWriter.hpp"
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include "Noise.hpp"
#include "QuadMasterChain.hpp"
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

    DualMasteringChain m_masterChain;
    QuadToStereoMixdown m_quadToStereoMixdown;

    QuadMixerInternal()
    {
    }
    
    struct Input
    {
        size_t m_numInputs;
        size_t m_numMonoInputs;
        float m_input[16];
        float m_monoIn[16];
        PhaseUtils::ZeroedExpParam m_gain[16];
        PhaseUtils::ZeroedExpParam m_sendGain[16][x_numSends];
        float m_x[16];
        float m_y[16];
        QuadFloat m_return[x_numSends];
        PhaseUtils::ZeroedExpParam m_returnGain[x_numSends];
        bool m_noiseMode;
        bool m_monitor[16];

        DualMasteringChain::Input m_masterChainInput;

        Input()
        {
            m_noiseMode = false;
            m_numInputs = 0;
            m_numMonoInputs = 0;
            
            for (size_t i = 0; i < 16; ++i)
            {
                m_monitor[i] = true;                
                m_input[i] = 0.0f;
                m_monoIn[i] = 0.0f;
                m_x[i] = 0.0f;
                m_y[i] = 0.0f;
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

        // Generate filename based on current date and time (ISO 8601 format)
        //
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        
        std::tm timeInfo;
        localtime_r(&timeT, &timeInfo);
        
        std::ostringstream oss;
        oss << m_recordingDirectory << "/recording-";
        oss << std::setfill('0') << std::setw(4) << (timeInfo.tm_year + 1900);
        oss << "-";
        oss << std::setfill('0') << std::setw(2) << (timeInfo.tm_mon + 1);
        oss << "-";
        oss << std::setfill('0') << std::setw(2) << timeInfo.tm_mday;
        oss << "T";
        oss << std::setfill('0') << std::setw(2) << timeInfo.tm_hour;
        oss << std::setfill('0') << std::setw(2) << timeInfo.tm_min;
        oss << std::setfill('0') << std::setw(2) << timeInfo.tm_sec;
        oss << ".";
        oss << std::setfill('0') << std::setw(3) << ms.count();
        oss << ".wav";
        
        std::string filename = oss.str();

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
                m_output.m_output += QuadFloat(input.m_input[i], input.m_input[i], input.m_input[i], input.m_input[i]) * input.m_gain[i].m_expParam;
                m_quadToStereoMixdown.MixSample(0.5, 0.5, input.m_input[i] * input.m_gain[i].m_expParam);
            }
        }
        else
        {
            for (size_t i = 0; i < input.m_numInputs; ++i)
            {
                if (input.m_monitor[i])
                {
                    m_quadToStereoMixdown.MixSample(input.m_x[i], input.m_y[i], input.m_input[i] * input.m_gain[i].m_expParam);
                    m_quadToStereoMixdown.MixSample(0.5f, 0.5f, input.m_monoIn[i] * input.m_gain[i].m_expParam);
                }

                QuadFloat pan = QuadFloat::Pan(input.m_x[i], input.m_y[i], input.m_input[i]);

                for (size_t j = 0; j < x_numSends; ++j)
                {
                    m_send[j] += pan * input.m_sendGain[i][j].m_expParam;
                }

                float reduction;
                m_voiceMeters[i].ProcessAndSaturate((input.m_input[i] + input.m_monoIn[i]) * input.m_gain[i].m_expParam, &reduction);

                QuadFloat mono = QuadFloat::Pan(0.5f, 0.5f, input.m_monoIn[i]);                
                QuadFloat postFader = (pan + mono) * (input.m_gain[i].m_expParam * reduction);

                if (input.m_monitor[i])
                {
                    m_output.m_output += postFader;
                }
                // Write post-fader input to wave file
                //
                m_wavWriter.WriteSampleIfOpen(4 * static_cast<uint16_t>(i), postFader);
            }
        }
    }

    QuadFloatWithStereoAndSub ProcessReturns(const Input& input)
    {
        if (!input.m_noiseMode)
        {
            for (size_t j = 0; j < x_numSends; ++j)
            {
                m_quadToStereoMixdown.MixQuadSample(input.m_return[j] * input.m_returnGain[j].m_expParam);
                QuadFloat postReturn = input.m_return[j] * input.m_returnGain[j].m_expParam;
                postReturn = m_returnMeters[j].ProcessAndSaturate(postReturn);
                m_output.m_output += postReturn;            

                m_wavWriter.WriteSampleIfOpen(4 * static_cast<uint16_t>(input.m_numInputs + j), postReturn);
            }
        }

        m_output = m_masterChain.Process(input.m_masterChainInput, m_output.m_output, m_quadToStereoMixdown.m_output);
        
        m_wavWriter.WriteSampleIfOpen(4 * static_cast<uint16_t>(input.m_numInputs + x_numSends), m_output.m_output);
        m_wavWriter.WriteSampleIfOpen(4 * static_cast<uint16_t>(input.m_numInputs + x_numSends + 1), m_output.m_stereoOutput);
        
        m_masterMeter.Process(m_output.m_output);
        m_stereoMeter.Process(m_output.m_stereoOutput);

        if (m_isRecording && m_wavWriter.m_error)
        {
            INFO("QuadMixer error: WavWriter reported error during recording");
            StopRecording();
        }

        return m_output;
    }

    QuadFloatWithStereoAndSub Process(const Input& input)
    {
        ProcessInputs(input);
        return ProcessReturns(input);
    }
};