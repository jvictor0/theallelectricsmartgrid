#pragma once

#include "BitSet.hpp"
#include "EncoderBankBank.hpp"
#include "MachineFlags.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "VoiceMachineEnums.hpp"
#include <algorithm>
#include <array>
#include <cmath>

struct SmartGridOneEncoders
{
    static constexpr size_t x_numTrios = 3;
    static constexpr size_t x_voicesPerTrio = 3;
    static constexpr size_t x_numVoiceBanks = 4 * x_numTrios;
    static constexpr size_t x_numQuadBanks = 4;
    static constexpr size_t x_numGlobalBanks = 4;
    static constexpr size_t x_totalNumBanks = x_numVoiceBanks + x_numQuadBanks + x_numGlobalBanks;

    enum class BankMode : int
    {
        VoiceWater = 0,
        VoiceFire = 1,
        VoiceEarth = 2,
        Quad = 3,
        Global = 4,
        NumModes = 5,
        Voice = VoiceWater
    };

    static constexpr size_t x_numBankModes = static_cast<size_t>(BankMode::NumModes);

    enum class Bank : int
    {
        SourceWater = 0,
        SourceFire = 1,
        SourceEarth = 2,
        FilterAndAmpWater = 3,
        FilterAndAmpFire = 4,
        FilterAndAmpEarth = 5,
        PanningAndSequencingWater = 6,
        PanningAndSequencingFire = 7,
        PanningAndSequencingEarth = 8,
        VoiceLFOsWater = 9,
        VoiceLFOsFire = 10,
        VoiceLFOsEarth = 11,
        Delay = 12,
        Reverb = 13,
        PartialMachine = 14,
        QuadLFOs = 15,
        TheoryOfTime = 16,
        Mastering = 17,
        Inputs = 18,
        DeepVocoder = 19,
        NumBanks = 20,
        Source = SourceWater,
        FilterAndAmp = FilterAndAmpWater,
        PanningAndSequencing = PanningAndSequencingWater,
        VoiceLFOs = VoiceLFOsWater
    };

    static constexpr std::array<Bank, 4> x_voiceSelectorBanks =
    {{Bank::Source, Bank::FilterAndAmp, Bank::PanningAndSequencing, Bank::VoiceLFOs}};
    static constexpr std::array<Bank, 4> x_quadSelectorBanks =
    {{Bank::Delay, Bank::Reverb, Bank::PartialMachine, Bank::QuadLFOs}};
    static constexpr std::array<Bank, 4> x_globalSelectorBanks =
    {{Bank::TheoryOfTime, Bank::Mastering, Bank::Inputs, Bank::DeepVocoder}};

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
#define F(name, shortName, bank, x, y, default, description, color, sourceMachines, filterMachines, switchValues, bipolar) name,
#include "ForEachSmartGridOneParam.hpp"
#undef F
    };

    static constexpr size_t x_numParams =
        0
#define F(name, shortName, bank, x, y, default, description, color, sourceMachines, filterMachines, switchValues, bipolar) + 1
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
                if (IsVoiceMode(mode) || mode == BankMode::Quad)
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::SmoothRandom, x_colors[0]);
                }
                return ModulatorSkin(SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
            }
            case 1:
            {
                if (IsVoiceMode(mode) || mode == BankMode::Quad)
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
                if (mode == BankMode::Quad)
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::LFO, SmartGrid::Color::Pink);
                }

                return ModulatorSkin(SmartGridOne::ModulationGlyphs::LFO, x_colors[4]);
            }
            case 7:
            {
                if (mode == BankMode::Quad)
                {
                    return ModulatorSkin(SmartGridOne::ModulationGlyphs::LFO, SmartGrid::Color::Purple);
                }

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
                if (IsVoiceMode(mode))
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
                if (IsVoiceMode(mode))
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
                if (IsVoiceMode(mode))
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
                if (IsVoiceMode(mode))
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
                if (IsVoiceMode(mode))
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
        return GetModulatorSkin(6 + i, BankMode::VoiceWater).m_color;
    }

    static SmartGrid::Color ADSRColor(size_t i)
    {
        return GetModulatorSkin(4 + i, BankMode::VoiceWater).m_color;
    }

    static SmartGrid::Color SheafColor(size_t i)
    {
        return GetModulatorSkin(8 + i, BankMode::VoiceWater).m_color;
    }

    static SmartGrid::Color QuadratureColor(size_t i)
    {
        return GetModulatorSkin(4 + i, BankMode::Quad).m_color;
    }

    static SmartGrid::Color QuadLFOColor(size_t i)
    {
        return GetModulatorSkin(6 + i, BankMode::Quad).m_color;
    }

    EncoderBankBank m_encoderBankBank;
    Bank m_selectedBank;
    size_t m_selectedTrio;

    SmartGridOneEncoders(SmartGridOneContext* context)
        : m_encoderBankBank(
            static_cast<int>(Bank::NumBanks),
            x_numBankModes,
            x_numParams * x_numTrios,
            context)
        , m_selectedBank(Bank::SourceWater)
        , m_selectedTrio(0)
    {
        Init();
    }

    static bool IsVoiceBank(Bank bank)
    {
        return static_cast<size_t>(bank) < x_numVoiceBanks;
    }

    static Bank BankForTrio(Bank bank, size_t trio)
    {
        if (!IsVoiceBank(bank))
        {
            return bank;
        }

        size_t family = static_cast<size_t>(bank) / x_numTrios;
        return static_cast<Bank>(family * x_numTrios + trio);
    }

    static bool IsVoiceMode(BankMode mode)
    {
        return mode == BankMode::VoiceWater || mode == BankMode::VoiceFire || mode == BankMode::VoiceEarth;
    }

    static BankMode VoiceModeForTrio(size_t trio)
    {
        switch (trio)
        {
            case 0:
            {
                return BankMode::VoiceWater;
            }
            case 1:
            {
                return BankMode::VoiceFire;
            }
            default:
            {
                return BankMode::VoiceEarth;
            }
        }
    }

    size_t EncoderIndex(Param param, size_t trio)
    {
        return static_cast<size_t>(param) * x_numTrios + trio;
    }

    // Param address lookup
    //
    ParamAddress GetParamAddress(Param param)
    {
        switch (param)
        {
#define F(name, shortName, bank, x, y, default, description, color, sourceMachines, filterMachines, switchValues, bipolar) case Param::name: return ParamAddress(Bank::bank, x, y);
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
#define F(name, shortName, bank, x, y, default, description, color, sourceMachines, filterMachines, switchValues, bipolar) case Param::name: return ParamSwitch(switchValues);
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
        size_t bankIx = static_cast<size_t>(bank);
        if (bankIx < x_numVoiceBanks)
        {
            return VoiceModeForTrio(bankIx % x_numTrios);
        }

        if (bankIx < x_numVoiceBanks + x_numQuadBanks)
        {
            return BankMode::Quad;
        }

        return BankMode::Global;
    }

    // Value getters - use encoder index (Param enum) for O(1) lookup, not grid position
    //
    float GetValue(Param param, int voice)
    {
        ParamAddress address = GetParamAddress(param);
        if (IsVoiceMode(GetModeForBank(address.bank)))
        {
            size_t trio = static_cast<size_t>(voice) / x_voicesPerTrio;
            size_t trioVoice = static_cast<size_t>(voice) % x_voicesPerTrio;
            return m_encoderBankBank.GetValueByEncoderIndex(EncoderIndex(param, trio), trioVoice);
        }

        return m_encoderBankBank.GetValueByEncoderIndex(EncoderIndex(param, 0), static_cast<size_t>(voice));
    }

    float GetValue(Param param)
    {
        return GetValue(param, 0);
    }

    float GetValueNoSlew(Param param, int voice)
    {
        ParamAddress address = GetParamAddress(param);
        if (IsVoiceMode(GetModeForBank(address.bank)))
        {
            size_t trio = static_cast<size_t>(voice) / x_voicesPerTrio;
            size_t trioVoice = static_cast<size_t>(voice) % x_voicesPerTrio;
            return m_encoderBankBank.GetValueNoSlewByEncoderIndex(EncoderIndex(param, trio), trioVoice);
        }

        return m_encoderBankBank.GetValueNoSlewByEncoderIndex(EncoderIndex(param, 0), static_cast<size_t>(voice));
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

        ParamAddress address = GetParamAddress(param);
        bool isVoiceMode = IsVoiceMode(GetModeForBank(address.bank));
        size_t trio = isVoiceMode ? static_cast<size_t>(voice) / x_voicesPerTrio : 0;
        size_t channel = isVoiceMode ? static_cast<size_t>(voice) % x_voicesPerTrio : static_cast<size_t>(voice);
        int switchVal = static_cast<int>(std::round(m_encoderBankBank.GetNormalizedValueNoSlewByEncoderIndex(EncoderIndex(param, trio), channel) * static_cast<float>(paramSwitch.m_numValues - 1)));
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
    void Init()
    {
        for (size_t trio = 0; trio < x_numTrios; ++trio)
        {
            BankMode mode = VoiceModeForTrio(trio);
            m_encoderBankBank.InitMode(static_cast<size_t>(mode), x_voicesPerTrio);
            m_encoderBankBank.InitBank(static_cast<size_t>(BankForTrio(Bank::Source, trio)), static_cast<size_t>(mode), SmartGrid::Color::Red);
            m_encoderBankBank.InitBank(static_cast<size_t>(BankForTrio(Bank::FilterAndAmp, trio)), static_cast<size_t>(mode), SmartGrid::Color::Green);
            m_encoderBankBank.InitBank(static_cast<size_t>(BankForTrio(Bank::PanningAndSequencing, trio)), static_cast<size_t>(mode), SmartGrid::Color::Orange);
            m_encoderBankBank.InitBank(static_cast<size_t>(BankForTrio(Bank::VoiceLFOs, trio)), static_cast<size_t>(mode), SmartGrid::Color::Blue);
        }

        m_encoderBankBank.InitMode(static_cast<size_t>(BankMode::Quad), 4);
        m_encoderBankBank.InitMode(static_cast<size_t>(BankMode::Global), 1);

        m_encoderBankBank.InitBank(static_cast<int>(Bank::Delay), static_cast<size_t>(BankMode::Quad), SmartGrid::Color::Pink);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::Reverb), static_cast<size_t>(BankMode::Quad), SmartGrid::Color::Fuscia);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::PartialMachine), static_cast<size_t>(BankMode::Quad), SmartGrid::Color::Cyan);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::QuadLFOs), static_cast<size_t>(BankMode::Quad), SmartGrid::Color::DarkPurple);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::TheoryOfTime), static_cast<size_t>(BankMode::Global), SmartGrid::Color::Yellow);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::Mastering), static_cast<size_t>(BankMode::Global), SmartGrid::Color::SeaGreen);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::Inputs), static_cast<size_t>(BankMode::Global), SmartGrid::Color::White);
        m_encoderBankBank.InitBank(static_cast<int>(Bank::DeepVocoder), static_cast<size_t>(BankMode::Global), SmartGrid::Color::Ocean);

#define F(name, shortName, bank, x, y, default, description, color, sourceMachines, filterMachines, switchValues, bipolar) \
        { \
            Bank baseBank = Bank::bank; \
            if (IsVoiceMode(GetModeForBank(baseBank))) \
            { \
                const char* x_names[x_numTrios] = {#name "Water", #name "Fire", #name "Earth"}; \
                const char* x_shortNames[x_numTrios] = {#shortName, #shortName, #shortName}; \
                for (size_t trio = 0; trio < x_numTrios; ++trio) \
                { \
                    size_t index = m_encoderBankBank.CreateEncoder(EncoderIndex(Param::name, trio), static_cast<size_t>(VoiceModeForTrio(trio)), default, x_names[trio], x_shortNames[trio], color, switchValues, bipolar); \
                    m_encoderBankBank.PlaceEncoder(index, static_cast<size_t>(BankForTrio(baseBank, trio)), x, y); \
                } \
            } \
            else \
            { \
                BankMode mode = GetModeForBank(baseBank); \
                size_t index = m_encoderBankBank.CreateEncoder(EncoderIndex(Param::name, 0), static_cast<size_t>(mode), default, #name, #shortName, color, switchValues, bipolar); \
                m_encoderBankBank.PlaceEncoder(index, static_cast<size_t>(baseBank), x, y); \
            } \
        }
#include "ForEachSmartGridOneParam.hpp"
#undef F

        // Set modulator colors for each mode
        //
        for (size_t modeIx = 0; modeIx < x_numBankModes; ++modeIx)
        {
            BankMode mode = static_cast<BankMode>(modeIx);
            auto& modValues = m_encoderBankBank.GetModulatorValues(modeIx);
            for (size_t i = 0; i < SmartGrid::BankedEncoderCell::x_numModulators; ++i)
            {
                modValues.SetModulatorColor(i, GetModulatorSkin(i, mode).m_color);
            }
        }

        SelectBank(Bank::Source);
    }

    // Bank selection
    //
    void SelectBank(Bank bank)
    {
        bank = BankForTrio(bank, m_selectedTrio);
        m_selectedBank = bank;
        m_encoderBankBank.SelectGrid(static_cast<int>(bank));
    }

    BankMode GetSelectedMode()
    {
        return GetModeForBank(m_selectedBank);
    }

    bool IsVoiceBankSelected()
    {
        return IsVoiceMode(GetSelectedMode());
    }

    // Track selection routes to the corresponding voice bank
    //
    void SetTrack(size_t track)
    {
        m_selectedTrio = track;
        if (IsVoiceBank(m_selectedBank))
        {
            SelectBank(BankForTrio(m_selectedBank, track));
        }
    }

    int GetCurrentTrack()
    {
        return static_cast<int>(m_selectedTrio);
    }

    void UpdateEncodersForMachine(
        VoiceMachine::SourceMachine sourceMachine,
        VoiceMachine::FilterMachine filterMachine)
    {
        for (Bank bank : {Bank::Source, Bank::FilterAndAmp, Bank::PanningAndSequencing, Bank::VoiceLFOs})
        {
            m_encoderBankBank.NullBank(static_cast<size_t>(BankForTrio(bank, m_selectedTrio)));
        }

#define F(name, shortName, bank, x, y, default, description, color, sourceMachines, filterMachines, switchValues, bipolar) \
        { \
            if (IsVoiceMode(GetModeForBank(Bank::bank))) \
            { \
                MachineFlags flags{BitSet8(sourceMachines), BitSet8(filterMachines)}; \
                bool applies = flags.AppliesToSource(sourceMachine) && flags.AppliesToFilter(filterMachine); \
                if (applies) \
                { \
                    m_encoderBankBank.PlaceEncoder(EncoderIndex(Param::name, m_selectedTrio), static_cast<size_t>(BankForTrio(Bank::bank, m_selectedTrio)), x, y); \
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
        return m_encoderBankBank.GetGesturesAffectingBank(static_cast<size_t>(BankForTrio(bank, track)));
    }

    bool IsGestureAffectingBank(int gesture, Bank bank, size_t track)
    {
        return m_encoderBankBank.IsGestureAffectingBank(gesture, static_cast<size_t>(BankForTrio(bank, track)));
    }

    // Color getters
    //
    SmartGrid::Color GetSelectorColor(Bank bank)
    {
        return m_encoderBankBank.GetSelectorColor(static_cast<int>(BankForTrio(bank, m_selectedTrio)));
    }

    SmartGrid::Color GetBankColor(Bank bank)
    {
        return m_encoderBankBank.m_bankConfigs[static_cast<int>(BankForTrio(bank, m_selectedTrio))].m_color;
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

    void RevertToDefault(bool allScenes)
    {
        m_encoderBankBank.RevertToDefault(allScenes);
    }

    void ResetBank(Bank bank)
    {
        m_encoderBankBank.ResetGrid(static_cast<uint64_t>(BankForTrio(bank, m_selectedTrio)));
    }

    // Serialization
    //
    JSON ToJSON(JsonArena& a)
    {
        return m_encoderBankBank.ToJSON(a);
    }

    void FromJSON(JSON rootJ)
    {
        m_encoderBankBank.FromJSON(rootJ);
    }
};
