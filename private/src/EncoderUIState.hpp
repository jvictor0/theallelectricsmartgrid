#pragma once

#include <atomic>
#include <array>
#include "SmartGrid.hpp"
#include "AtomicString.hpp"
#include "EncoderEnums.hpp"
#include "SmartGridOneScopeEnums.hpp"

struct EncoderUIState
{
    static constexpr size_t x_numModulators = 15;
    static constexpr size_t x_maxVoices = 16;

    struct ModulationWeights
    {
        SmartGridOne::EncoderBankMode m_mode = SmartGridOne::EncoderBankMode::NumModes;
        size_t m_numVoices = 0;
        std::array<std::array<float, x_numModulators>, x_maxVoices> m_weights{};
    };

    std::atomic<size_t> m_weightsRevision{0};
    std::atomic<float> m_modulatorWeights[x_maxVoices][x_numModulators]{};
    std::atomic<SmartGrid::Color> m_color;
    std::atomic<float> m_brightness;
    std::atomic<float> m_values[16];
    std::atomic<float> m_minValues[16];
    std::atomic<float> m_maxValues[16];
    std::atomic<bool> m_connected;
    std::atomic<SmartGrid::EncoderType> m_encoderType{SmartGrid::EncoderType::BaseParam};
    std::atomic<int> m_index{-1};
    std::atomic<BitSet16> m_modulatorsAffecting;
    std::atomic<BitSet16> m_gesturesAffecting;
    std::atomic<const char*> m_shortName;
    std::atomic<int> m_switchValues;
    std::atomic<bool> m_bipolar{false};
    std::atomic<SmartGrid::Color> m_indicatorColor[16];
    std::atomic<SmartGrid::Color> m_mainIndicatorColor;
    std::atomic<int> m_numVoices;
    std::atomic<SmartGridOne::EncoderBankMode> m_mode{SmartGridOne::EncoderBankMode::NumModes};
    std::atomic<SmartGridOne::ModulationGlyphs> m_modulationGlyphs[16];
    std::atomic<SmartGrid::Color> m_modulationGlyphColor[16];

    bool ReadModulationWeights(ModulationWeights& result) const
    {
        for (size_t attempt = 0; attempt < 4; ++attempt)
        {
            size_t revision = m_weightsRevision.load();
            if (revision % 2 != 0)
            {
                continue;
            }

            result.m_mode = m_mode.load();
            result.m_numVoices = static_cast<size_t>(m_numVoices.load());
            for (size_t voice = 0; voice < x_maxVoices; ++voice)
            {
                for (size_t modulator = 0; modulator < x_numModulators; ++modulator)
                {
                    result.m_weights[voice][modulator] = m_modulatorWeights[voice][modulator].load();
                }
            }

            if (m_weightsRevision.load() == revision)
            {
                return true;
            }
        }

        return false;
    }

    EncoderUIState()
        : m_color(SmartGrid::Color::Off)
        , m_brightness(0)
        , m_values{}
        , m_minValues{}
        , m_maxValues{}
        , m_connected(false)
        , m_modulatorsAffecting{}
        , m_gesturesAffecting{}
        , m_shortName(nullptr)
        , m_switchValues(0)
        , m_indicatorColor{}
        , m_mainIndicatorColor(SmartGrid::Color::Off)
        , m_numVoices(0)
        , m_modulationGlyphs{}
        , m_modulationGlyphColor{}
    {
        for (size_t i = 0; i < 16; ++i)
        {
            m_values[i] = 0;
            m_minValues[i] = 0;
            m_maxValues[i] = 1;
            m_indicatorColor[i].store(SmartGrid::Color::Off);
            m_modulationGlyphs[i].store(SmartGridOne::ModulationGlyphs::None);
            m_modulationGlyphColor[i].store(SmartGrid::Color::Off);
        }
    }
};

struct EncoderBankUIState
{
    EncoderUIState m_states[4][4];
    AtomicString m_strings[4][4];

    EncoderBankUIState()
        : m_states{}
        , m_strings{}
    {
    }

    SmartGrid::Color GetColor(size_t i, size_t j)
    {
        return GetBrightness(i, j) > 0 ? m_states[i][j].m_color.load() : SmartGrid::Color::Off;
    }

    SmartGrid::Color GetRawColor(size_t i, size_t j)
    {
        return m_states[i][j].m_color.load();
    }

    SmartGrid::Color GetBrightnessAdjustedColor(size_t i, size_t j)
    {
        return m_states[i][j].m_color.load().AdjustBrightness(m_states[i][j].m_brightness.load());
    }

    SmartGrid::Color GetIndicatorColor(size_t i, size_t j, size_t channel)
    {
        return m_states[i][j].m_indicatorColor[channel].load();
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

    int GetSwitchValues(size_t i, size_t j)
    {
        return m_states[i][j].m_switchValues.load();
    }

    bool GetBipolar(size_t i, size_t j)
    {
        return m_states[i][j].m_bipolar.load();
    }

    void SetBipolar(size_t i, size_t j, bool bipolar)
    {
        m_states[i][j].m_bipolar.store(bipolar);
    }

    int GetNumVoices(size_t i, size_t j)
    {
        return m_states[i][j].m_numVoices.load();
    }

    bool GetConnected(size_t i, size_t j)
    {
        return m_states[i][j].m_connected.load();
    }

    SmartGridOne::EncoderBankMode GetMode(size_t i, size_t j)
    {
        return m_states[i][j].m_mode.load();
    }

    void SetMode(size_t i, size_t j, SmartGridOne::EncoderBankMode mode)
    {
        m_states[i][j].m_mode.store(mode);
    }

    SmartGrid::EncoderType GetEncoderType(size_t i, size_t j)
    {
        return m_states[i][j].m_encoderType.load();
    }

    void SetEncoderType(size_t i, size_t j, SmartGrid::EncoderType type)
    {
        m_states[i][j].m_encoderType.store(type);
    }

    int GetIndex(size_t i, size_t j)
    {
        return m_states[i][j].m_index.load();
    }

    void SetIndex(size_t i, size_t j, int index)
    {
        m_states[i][j].m_index.store(index);
    }

    void SetColor(size_t i, size_t j, SmartGrid::Color color)
    {
        m_states[i][j].m_color.store(color);
    }
    
    void SetIndicatorColor(size_t i, size_t j, size_t channel, SmartGrid::Color color)
    {
        m_states[i][j].m_indicatorColor[channel].store(color);
    }

    void SetMainIndicatorColor(size_t i, size_t j, SmartGrid::Color color)
    {
        m_states[i][j].m_mainIndicatorColor.store(color);
    }
    
    SmartGrid::Color GetMainIndicatorColor(size_t i, size_t j)
    {
        return m_states[i][j].m_connected.load() ? m_states[i][j].m_mainIndicatorColor.load() : SmartGrid::Color::Off;
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

    void SetSwitchValues(size_t i, size_t j, int switchValues)
    {
        m_states[i][j].m_switchValues.store(switchValues);
    }

    void SetValues(size_t i, size_t j, float* values)
    {
        for (size_t k = 0; k < 16; ++k)
        {
            m_states[i][j].m_values[k].store(values[k]);
        }
    }

    void SetNumVoices(size_t i, size_t j, int numVoices)
    {
        m_states[i][j].m_numVoices.store(numVoices);
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

    void SetModulationGlyph(size_t i, size_t j, size_t index, SmartGridOne::ModulationGlyphs glyph, SmartGrid::Color color)
    {
        m_states[i][j].m_modulationGlyphs[index].store(glyph);
        m_states[i][j].m_modulationGlyphColor[index].store(color);
    }

    SmartGridOne::ModulationGlyphs GetModulationGlyph(size_t i, size_t j, size_t index)
    {
        return m_states[i][j].m_modulationGlyphs[index].load();
    }

    SmartGrid::Color GetModulationGlyphColor(size_t i, size_t j, size_t index)
    {
        return m_states[i][j].m_modulationGlyphColor[index].load();
    }

    void SetShortName(size_t i, size_t j, const char* shortName)
    {
        m_states[i][j].m_shortName.store(shortName);
    }

    // May return nullptr
    //
    const char* GetShortName(size_t i, size_t j)
    {
        return m_states[i][j].m_shortName.load();
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
