#pragma once

#include <atomic>
#include "SmartGrid.hpp"

struct EncoderUIState
{
    std::atomic<SmartGrid::Color> m_color;
    std::atomic<float> m_brightness;
    std::atomic<float> m_values[16];

    EncoderUIState()
        : m_color(SmartGrid::Color(0, 0, 0))
        , m_brightness(0)
    {
        for (size_t i = 0; i < 16; ++i)
        {
            m_values[i] = 0;
        }
    }
};

struct EncoderBankUIState
{
    EncoderUIState m_states[4][4];

    std::atomic<int> m_numTracks;
    std::atomic<int> m_numVoices;
    std::atomic<int> m_currentTrack;

    SmartGrid::Color GetColor(size_t i, size_t j)
    {
        return m_states[i][j].m_color.load();
    }

    float GetBrightness(size_t i, size_t j)
    {
        return m_states[i][j].m_brightness.load();
    }

    float GetValue(size_t i, size_t j, size_t k)
    {
        return m_states[i][j].m_values[k].load();
    }

    int GetNumTracks()
    {
        return m_numTracks.load();
    }

    int GetNumVoices()
    {
        return m_numVoices.load();
    }

    int GetCurrentTrack()
    {
        return m_currentTrack.load();
    }

    void SetColor(size_t i, size_t j, SmartGrid::Color color)
    {
        m_states[i][j].m_color.store(color);
    }
    
    void SetBrightness(size_t i, size_t j, float brightness)
    {
        m_states[i][j].m_brightness.store(brightness);
    }

    void SetValue(size_t i, size_t j, size_t k, float value)
    {
        m_states[i][j].m_values[k].store(value);
    }

    void SetValues(size_t i, size_t j, float* values)
    {
        for (size_t k = 0; k < 16; ++k)
        {
            m_states[i][j].m_values[k].store(values[k]);
        }
    }

    void SetNumTracks(int numTracks)
    {
        m_numTracks.store(numTracks);
    }

    void SetNumVoices(int numVoices)
    {
        m_numVoices.store(numVoices);
    }

    void SetCurrentTrack(int currentTrack)
    {
        m_currentTrack.store(currentTrack);
    }
};