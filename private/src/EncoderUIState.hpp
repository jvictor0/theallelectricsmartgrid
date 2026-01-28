#pragma once

#include <atomic>
#include "SmartGrid.hpp"
#include "AtomicString.hpp"
#include "SmartGridOneScopeEnums.hpp"

struct EncoderUIState
{
    std::atomic<SmartGrid::Color> m_color;
    std::atomic<float> m_brightness;
    std::atomic<float> m_values[16];
    std::atomic<float> m_minValues[16];
    std::atomic<float> m_maxValues[16];
    std::atomic<bool> m_connected;
    std::atomic<BitSet16> m_modulatorsAffecting;
    std::atomic<BitSet16> m_gesturesAffecting;

    EncoderUIState()
        : m_color(SmartGrid::Color::Off)
        , m_brightness(0)
        , m_connected(false)
    {
        for (size_t i = 0; i < 16; ++i)
        {
            m_values[i] = 0;
            m_minValues[i] = 0;
            m_maxValues[i] = 1;
        }
    }
};

struct EncoderBankUIState
{
    EncoderUIState m_states[4][4];
    AtomicString m_strings[4][4];

    std::atomic<SmartGrid::Color> m_indicatorColor[16];
    std::atomic<SmartGrid::Color> m_mainIndicatorColor;

    std::atomic<int> m_numTracks;
    std::atomic<int> m_numVoices;
    std::atomic<int> m_currentTrack;

    std::atomic<SmartGridOne::ModulationGlyphs> m_modulationGlyphs[16];
    std::atomic<SmartGrid::Color> m_modulationGlyphColor[16];

    EncoderBankUIState()
        : m_numTracks(0)
        , m_numVoices(1)
        , m_currentTrack(0)
    {
        for (size_t i = 0; i < 16; ++i)
        {
            m_indicatorColor[i].store(SmartGrid::Color::Off);
            m_modulationGlyphs[i].store(SmartGridOne::ModulationGlyphs::None);
            m_modulationGlyphColor[i].store(SmartGrid::Color::Off);
        }

        m_mainIndicatorColor.store(SmartGrid::Color::Off);
    }

    SmartGrid::Color GetColor(size_t i, size_t j)
    {
        return GetBrightness(i, j) > 0 ? m_states[i][j].m_color.load() : SmartGrid::Color::Off;
    }

    SmartGrid::Color GetBrightnessAdjustedColor(size_t i, size_t j)
    {
        return m_states[i][j].m_color.load().AdjustBrightness(m_states[i][j].m_brightness.load());
    }

    SmartGrid::Color GetIndicatorColor(size_t i)
    {
        return m_indicatorColor[i].load();
    }

    float GetBrightness(size_t i, size_t j)
    {
        return m_states[i][j].m_brightness.load();
    }

    float GetValue(size_t i, size_t j, size_t k)
    {
        return m_states[i][j].m_values[k].load();
    }

    float GetMinValue(size_t i, size_t j, size_t k)
    {
        return m_states[i][j].m_minValues[k].load();
    }

    float GetMaxValue(size_t i, size_t j, size_t k)
    {
        return m_states[i][j].m_maxValues[k].load();
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

    bool GetConnected(size_t i, size_t j)
    {
        return m_states[i][j].m_connected.load();
    }

    void SetColor(size_t i, size_t j, SmartGrid::Color color)
    {
        m_states[i][j].m_color.store(color);
    }
    
    void SetIndicatorColor(size_t i, SmartGrid::Color color)
    {
        m_indicatorColor[i].store(color);
    }

    void SetMainIndicatorColor(SmartGrid::Color color)
    {
        m_mainIndicatorColor.store(color);
    }
    
    SmartGrid::Color GetMainIndicatorColor()
    {
        return m_mainIndicatorColor.load();
    }

    void SetBrightness(size_t i, size_t j, float brightness)
    {
        m_states[i][j].m_brightness.store(brightness);
    }

    void SetValue(size_t i, size_t j, size_t k, float value)
    {
        m_states[i][j].m_values[k].store(value);
    }

    void SetMinValue(size_t i, size_t j, size_t k, float value)
    {
        m_states[i][j].m_minValues[k].store(value);
    }

    void SetMaxValue(size_t i, size_t j, size_t k, float value)
    {
        m_states[i][j].m_maxValues[k].store(value);
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

    void SetConnected(size_t i, size_t j, bool connected)
    {
        m_states[i][j].m_connected.store(connected);
    }

    void SetFromFloat(size_t i, size_t j, float value, const char* units)
    {
        m_strings[i][j].SetFromFloat(value, units);
    }

    void SetModulatorsAffecting(size_t i, size_t j, BitSet16 modulatorsAffecting)
    {
        m_states[i][j].m_modulatorsAffecting.store(modulatorsAffecting);
    }

    void SetGesturesAffecting(size_t i, size_t j, BitSet16 gesturesAffecting)
    {
        m_states[i][j].m_gesturesAffecting.store(gesturesAffecting);
    }

    BitSet16 GetModulatorsAffecting(size_t i, size_t j)
    {
        return m_states[i][j].m_modulatorsAffecting.load();
    }

    BitSet16 GetGesturesAffecting(size_t i, size_t j)
    {
        return m_states[i][j].m_gesturesAffecting.load();
    }

    void SetModulationGlyph(size_t i, SmartGridOne::ModulationGlyphs glyph, SmartGrid::Color color)
    {
        m_modulationGlyphs[i].store(glyph);
        m_modulationGlyphColor[i].store(color);
    }

    SmartGridOne::ModulationGlyphs GetModulationGlyph(size_t i)
    {
        return m_modulationGlyphs[i].load();
    }

    SmartGrid::Color GetModulationGlyphColor(size_t i)
    {
        return m_modulationGlyphColor[i].load();
    }
};

namespace SmartGrid
{
    struct MessageInBus;
}

struct EncoderBankUI
{
    int m_routeId;
    EncoderBankUIState* m_uiState;
    SmartGrid::MessageInBus* m_messageBus;

    EncoderBankUI(int routeId, EncoderBankUIState* uiState, SmartGrid::MessageInBus* messageBus)
        : m_routeId(routeId)
        , m_uiState(uiState)
        , m_messageBus(messageBus)
    {
    }
};