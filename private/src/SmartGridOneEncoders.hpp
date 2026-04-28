#pragma once

#include "BitSet.hpp"
#include "EncoderBankBank.hpp"
#include "MachineFlags.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "VoiceMachineEnums.hpp"
#include <algorithm>
#include <cmath>

struct SmartGridOneEncoders
{
    static constexpr size_t x_numBankModes = 3;
    static constexpr size_t x_numVoiceBanks = 4;
    static constexpr size_t x_numQuadBanks = 2;
    static constexpr size_t x_numGlobalBanks = 4;
    static constexpr size_t x_totalNumBanks = x_numVoiceBanks + x_numQuadBanks + x_numGlobalBanks;

    enum class BankMode : int
    {
        Voice = 0,
        Quad = 1,
        Global = 2
    };

    enum class Bank : int
    {
        Source = 0,
        FilterAndAmp = 1,
        PanningAndSequencing = 2,
        VoiceLFOs = 3,
        Delay = 4,
        Reverb = 5,
        TheoryOfTime = 6,
        Mastering = 7,
        Inputs = 8,
        DeepVocoder = 9,
        NumBanks = 10
    };

    static Bank BankFromOrdinal(size_t ordinal)
    {
        return static_cast<Bank>(ordinal);
    }

    struct ParamAddress
    {
        Bank bank;
        int x;
        int y;

        ParamAddress(Bank bank, int x, int y)
            : bank(bank)
            , x(x)
            , y(y)
        {
        }
    };

    struct ParamSwitch
    {
        int m_numValues;

        ParamSwitch(int numValues = 0)
            : m_numValues(numValues)
        {
        }

        bool IsSwitch()
        {
            return m_numValues > 1;
        }
    };

    enum class Param
    {
#define F(name, shortName, bank, x, y, default, description, color, sourceMachines, filterMachines, switchValues) name,
#include "ForEachSmartGridOneParam.hpp"
#undef F
    };

    static constexpr size_t x_numParams =
        0
#define F(name, shortName, bank, x, y, default, description, color, sourceMachines, filterMachines, switchValues) + 1
#include "ForEachSmartGridOneParam.hpp"
#undef F
        ;

    // Modulator skin information
    //
    struct ModulatorSkin
    {
        SmartGridOne::ModulationGlyphs m_glyph;
        SmartGrid::Color m_color;

        ModulatorSkin(SmartGridOne::ModulationGlyphs glyph, SmartGrid::Color color)
            : m_glyph(glyph)
            , m_color(color)
        {
        }
    };

    static ModulatorSkin GetModulatorSkin(size_t index, BankMode mode)
    {
        static const SmartGrid::Color x_colors[5] =
        {
            SmartGrid::Color::Orange.AdjustBrightness(0.5),
            SmartGrid::Color::Yellow.AdjustBrightness(0.5),
            SmartGrid::Color::Cyan.AdjustBrightness(0.5),
            SmartGrid::Color::Indigo.AdjustBrightness(0.5),
            SmartGrid::Color::SeaGreen.AdjustBrightness(0.5)
        };

        switch (index)
        {
            case 0:
            {
                if (mode == BankMode::Voice)
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::SmoothRandom, x_colors[0]);
                }
                return ModulatorSkin(SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
            }
            case 1:
            {
                if (mode == BankMode::Voice)
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::SmoothRandom, x_colors[1]);
                }
                return ModulatorSkin(SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
            }
            case 2:
            {
                return ModulatorSkin(SmartGridOne::ModulationGlyphs::SmoothRandom, x_colors[2]);
            }
            case 3:
            {
                return ModulatorSkin(SmartGridOne::ModulationGlyphs::SmoothRandom, x_colors[3]);
            }
            case 6:
            {
                return ModulatorSkin(SmartGridOne::ModulationGlyphs::LFO, x_colors[4]);
            }
            case 7:
            {
                return ModulatorSkin(SmartGridOne::ModulationGlyphs::LFO, x_colors[2]);
            }
            case 11:
            {
                return ModulatorSkin(SmartGridOne::ModulationGlyphs::Spread, x_colors[3]);
            }
            case 14:
            {
                return ModulatorSkin(SmartGridOne::ModulationGlyphs::Noise, x_colors[4]);
            }
            case 4:
            {
                if (mode == BankMode::Voice)
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::ADSR, x_colors[2]);
                }
                else if (mode == BankMode::Quad)
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::Quadrature, SmartGrid::Color::Pink);
                }
                else
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
                }
            }
            case 5:
            {
                if (mode == BankMode::Voice)
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::ADSR, x_colors[3]);
                }
                else if (mode == BankMode::Quad)
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::Quadrature, SmartGrid::Color::Purple);
                }
                else
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
                }
            }
            case 8:
            {
                if (mode == BankMode::Voice)
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::Sheaf, x_colors[2]);
                }
                else
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
                }
            }
            case 9:
            {
                if (mode == BankMode::Voice)
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::Sheaf, x_colors[3]);
                }
                else
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
                }
            }
            case 10:
            {
                if (mode == BankMode::Voice)
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::Sheaf, x_colors[4]);
                }
                else
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
                }
            }
            default:
            {
                return ModulatorSkin(SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
            }
        }
    }

    static SmartGrid::Color LFOColor(size_t i)
    {
        return GetModulatorSkin(6 + i, BankMode::Voice).m_color;
    }

    static SmartGrid::Color ADSRColor(size_t i)
    {
        return GetModulatorSkin(4 + i, BankMode::Voice).m_color;
    }

    static SmartGrid::Color SheafColor(size_t i)
    {
        return GetModulatorSkin(8 + i, BankMode::Voice).m_color;
    }

    static SmartGrid::Color QuadratureColor(size_t i)
    {
        return GetModulatorSkin(4 + i, BankMode::Quad).m_color;
    }

    EncoderBankBank m_encoderBankBank;
    Bank m_selectedBank;

    SmartGridOneEncoders()
        : m_encoderBankBank(static_cast<int>(Bank::NumBanks), x_numBankModes, x_numParams)
        , m_selectedBank(Bank::Source)
    {
    }

    // Param address lookup
    //
    ParamAddress GetParamAddress(Param param)
    {
        switch (param)
        {
#define F(name, shortName, bank, x, y, default, description, color, sourceMachines, filterMachines, switchValues) case Param::name: return ParamAddress(Bank::bank, x, y);
#include "ForEachSmartGridOneParam.hpp"
#undef F
            default:
                return ParamAddress(Bank::Source, 0, 0);
        }
    }

    static ParamSwitch GetParamSwitch(Param param)
    {
        switch (param)
        {
#define F(name, shortName, bank, x, y, default, description, color, sourceMachines, filterMachines, switchValues) case Param::name: return ParamSwitch(switchValues);
#include "ForEachSmartGridOneParam.hpp"
#undef F
            default:
                return ParamSwitch();
        }
    }

    // BankMode lookup for a bank
    //
    BankMode GetModeForBank(Bank bank)
    {
        return static_cast<BankMode>(m_encoderBankBank.GetModeForBank(static_cast<size_t>(bank)));
    }

    // Value getters - use encoder index (Param enum) for O(1) lookup, not grid position
    //
    float GetValue(Param param, int voice)
    {
        return m_encoderBankBank.GetValueByEncoderIndex(static_cast<size_t>(param), static_cast<size_t>(voice));
    }

    float GetValue(Param param)
    {
        return GetValue(param, 0);
    }

    float GetValueNoSlew(Param param, int voice)
    {
        return m_encoderBankBank.GetValueNoSlewByEncoderIndex(static_cast<size_t>(param), static_cast<size_t>(voice));
    }

    float GetValueNoSlew(Param param)
    {
        return GetValueNoSlew(param, 0);
    }

    int GetSwitchVal(Param param, int voice)
    {
        ParamSwitch paramSwitch = GetParamSwitch(param);
        if (!paramSwitch.IsSwitch())
        {
            return 0;
        }

        int switchVal = static_cast<int>(std::round(GetValueNoSlew(param, voice) * static_cast<float>(paramSwitch.m_numValues - 1)));
        return std::max(0, std::min(paramSwitch.m_numValues - 1, switchVal));
    }

    int GetSwitchVal(Param param)
    {
        return GetSwitchVal(param, 0);
    }

    // Modulator values access
    //
    SmartGrid::BankedEncoderCell::ModulatorValues& GetModulatorValues(BankMode mode)
    {
        return m_encoderBankBank.GetModulatorValues(static_cast<size_t>(mode));
    }

    // Initialization
    //
    void Init(SmartGrid::SceneManager* sceneManager, size_t numTrios, size_t voicesPerTrio)
    {
        m_encoderBankBank.InitSceneManager(sceneManager);
        m_encoderBankBank.InitMode(static_cast<int>(BankMode::Voice), numTrios, voicesPerTrio);
        m_encoderBankBank.InitMode(static_cast<int>(BankMode::Quad), 1, 4);
        m_encoderBankBank.InitMode(static_cast<int>(BankMode::Global), 1, 1);

        m_encoderBankBank.InitBank(static_cast<int>(Bank::Source), static_cast<int>(BankMode::Voice), SmartGrid::Color::Red);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::FilterAndAmp), static_cast<int>(BankMode::Voice), SmartGrid::Color::Green);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::PanningAndSequencing), static_cast<int>(BankMode::Voice), SmartGrid::Color::Orange);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::VoiceLFOs), static_cast<int>(BankMode::Voice), SmartGrid::Color::Blue);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::Delay), static_cast<int>(BankMode::Quad), SmartGrid::Color::Pink);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::Reverb), static_cast<int>(BankMode::Quad), SmartGrid::Color::Fuscia);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::TheoryOfTime), static_cast<int>(BankMode::Global), SmartGrid::Color::Yellow);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::Mastering), static_cast<int>(BankMode::Global), SmartGrid::Color::SeaGreen);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::Inputs), static_cast<int>(BankMode::Global), SmartGrid::Color::White);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::DeepVocoder), static_cast<int>(BankMode::Global), SmartGrid::Color::Ocean);

#define F(name, shortName, bank, x, y, default, description, color, sourceMachines, filterMachines, switchValues) \
        { \
            size_t modeIx = static_cast<size_t>(GetModeForBank(Bank::bank)); \
            size_t index = m_encoderBankBank.CreateEncoder(sceneManager, static_cast<size_t>(Param::name), modeIx, default, #name, #shortName, color, switchValues); \
            m_encoderBankBank.PlaceEncoder(index, static_cast<size_t>(Bank::bank), x, y); \
        }
#include "ForEachSmartGridOneParam.hpp"
#undef F

        // Set modulator colors for each mode
        //
        for (size_t mode = 0; mode < x_numBankModes; ++mode)
        {
            auto& modValues = m_encoderBankBank.GetModulatorValues(mode);
            for (size_t i = 0; i < SmartGrid::BankedEncoderCell::x_numModulators; ++i)
            {
                modValues.SetModulatorColor(i, GetModulatorSkin(i, static_cast<BankMode>(mode)).m_color);
            }
        }

        SelectBank(Bank::Source);
    }

    // Bank selection
    //
    void SelectBank(Bank bank)
    {
        m_selectedBank = bank;
        m_encoderBankBank.SelectGrid(static_cast<int>(bank));
    }

    BankMode GetSelectedMode()
    {
        return GetModeForBank(m_selectedBank);
    }

    bool IsVoiceBankSelected()
    {
        return GetSelectedMode() == BankMode::Voice;
    }

    // Track selection (always routes to Voice mode)
    //
    void SetTrack(size_t track)
    {
        m_encoderBankBank.SetTrack(static_cast<size_t>(BankMode::Voice), track);
    }

    int GetCurrentTrack()
    {
        return m_encoderBankBank.GetCurrentTrack(static_cast<size_t>(BankMode::Voice));
    }

    void UpdateEncodersForMachine(
        VoiceMachine::SourceMachine sourceMachine,
        VoiceMachine::FilterMachine filterMachine)
    {
        for (size_t bankIx = 0; bankIx < static_cast<size_t>(Bank::NumBanks); ++bankIx)
        {
            if (GetModeForBank(static_cast<Bank>(bankIx)) == BankMode::Voice)
            {
                m_encoderBankBank.NullBank(bankIx);
            }
        }

#define F(name, shortName, bank, x, y, default, description, color, sourceMachines, filterMachines, switchValues) \
        { \
            if (GetModeForBank(Bank::bank) == BankMode::Voice) \
            { \
                MachineFlags flags{BitSet8(sourceMachines), BitSet8(filterMachines)}; \
                bool applies = flags.AppliesToSource(sourceMachine) && flags.AppliesToFilter(filterMachine); \
                size_t encoderIndex = applies ? static_cast<size_t>(Param::name) : x_numParams; \
                if (applies) \
                { \
                    m_encoderBankBank.PlaceEncoder(encoderIndex, static_cast<size_t>(Bank::bank), x, y); \
                } \
            } \
        }
#include "ForEachSmartGridOneParam.hpp"
#undef F
    }

    // Gesture handling
    //
    void SelectGesture(const BitSet16& gesture)
    {
        m_encoderBankBank.SelectGesture(gesture);
    }

    void ClearGesture(int gesture)
    {
        m_encoderBankBank.ClearGesture(gesture);
    }

    bool IsGestureAffecting(int gesture)
    {
        return m_encoderBankBank.GetGesturesAffecting().Get(gesture);
    }

    SmartGrid::Color GetGestureColor(int gesture)
    {
        return m_encoderBankBank.GetGestureColor(gesture);
    }

    BitSet16 GetGesturesAffectingBankForTrack(Bank bank, size_t track)
    {
        return m_encoderBankBank.GetGesturesAffectingBankForTrack(static_cast<size_t>(bank), track);
    }

    bool IsGestureAffectingBank(int gesture, Bank bank, size_t track)
    {
        return m_encoderBankBank.IsGestureAffectingBank(gesture, static_cast<size_t>(bank), track);
    }

    // Color getters
    //
    SmartGrid::Color GetSelectorColor(Bank bank)
    {
        return m_encoderBankBank.GetSelectorColor(static_cast<int>(bank));
    }

    SmartGrid::Color GetBankColor(Bank bank)
    {
        return m_encoderBankBank.m_bankConfigs[static_cast<int>(bank)].m_color;
    }

    // Processing
    //
    void Process()
    {
        m_encoderBankBank.Process();
    }

    void Apply(SmartGrid::MessageIn msg)
    {
        m_encoderBankBank.Apply(msg);
    }

    void PopulateUIState(EncoderBankUIState* uiState)
    {
        m_encoderBankBank.PopulateUIState(uiState);

        BankMode mode = GetSelectedMode();
        for (size_t i = 0; i < SmartGrid::BankedEncoderCell::x_numModulators; ++i)
        {
            ModulatorSkin skin = GetModulatorSkin(i, mode);
            uiState->SetModulationGlyph(i, skin.m_glyph, skin.m_color);
        }
    }

    // Scene operations
    //
    void CopyToScene(int scene)
    {
        m_encoderBankBank.CopyToScene(scene);
    }

    void RevertToDefault(bool allScenes, bool allTracks)
    {
        m_encoderBankBank.RevertToDefault(allScenes, allTracks);
    }

    void ResetBank(Bank bank, bool allScenes, bool allTracks)
    {
        m_encoderBankBank.ResetGrid(static_cast<uint64_t>(bank), allScenes, allTracks);
    }

    // Serialization
    //
    JSON ToJSON()
    {
        return m_encoderBankBank.ToJSON();
    }

    void FromJSON(JSON rootJ)
    {
        m_encoderBankBank.FromJSON(rootJ);
    }
};
