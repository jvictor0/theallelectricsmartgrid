#pragma once

#include "BitSet.hpp"
#include "EncoderBankBank.hpp"
#include "ModulationMatrixUIState.hpp"
#include "SmartGridOneLayout.hpp"
#include "SmartGridOneModulators.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "VoiceMachineEnums.hpp"
#include <algorithm>
#include <array>
#include <cmath>

struct SmartGridOneEncoders
{
    using Layout = SmartGridOneLayout;
    using Param = Layout::Param;
    using ParamMode = Layout::ParamMode;
    using Page = Layout::Page;
    using Slot = Layout::Slot;

    static constexpr size_t x_numTrios = Layout::x_numTrios;
    static constexpr size_t x_voicesPerTrio = 3;
    static constexpr size_t x_totalNumBanks = Layout::x_tables.m_numSlotInstances;
    static constexpr size_t x_numParams = Layout::x_numParams;

    using BankMode = SmartGridOne::EncoderBankMode;

    static constexpr size_t x_numBankModes = static_cast<size_t>(BankMode::NumModes);

    enum class Bank : int
    {
        SourceWater = Layout::GetSlotInstance(Slot::Source, 0),
        SourceFire = Layout::GetSlotInstance(Slot::Source, 1),
        SourceEarth = Layout::GetSlotInstance(Slot::Source, 2),
        ShapeWater = Layout::GetSlotInstance(Slot::Shape, 0),
        ShapeFire = Layout::GetSlotInstance(Slot::Shape, 1),
        ShapeEarth = Layout::GetSlotInstance(Slot::Shape, 2),
        PanningAndSequencingWater = Layout::GetSlotInstance(Slot::PanningAndSequencing, 0),
        PanningAndSequencingFire = Layout::GetSlotInstance(Slot::PanningAndSequencing, 1),
        PanningAndSequencingEarth = Layout::GetSlotInstance(Slot::PanningAndSequencing, 2),
        VoiceLFOsWater = Layout::GetSlotInstance(Slot::VoiceLFOs, 0),
        VoiceLFOsFire = Layout::GetSlotInstance(Slot::VoiceLFOs, 1),
        VoiceLFOsEarth = Layout::GetSlotInstance(Slot::VoiceLFOs, 2),
        Delay = Layout::GetSlotInstance(Slot::Delay, 0),
        Reverb = Layout::GetSlotInstance(Slot::Reverb, 0),
        PartialMachine = Layout::GetSlotInstance(Slot::PartialMachine, 0),
        QuadLFOs = Layout::GetSlotInstance(Slot::QuadLFOs, 0),
        TheoryOfTime = Layout::GetSlotInstance(Slot::TheoryOfTime, 0),
        Mastering = Layout::GetSlotInstance(Slot::Mastering, 0),
        Inputs = Layout::GetSlotInstance(Slot::Inputs, 0),
        DeepVocoder = Layout::GetSlotInstance(Slot::DeepVocoder, 0),
        FXMatrixWater = Layout::GetSlotInstance(Slot::FXMatrix, 0),
        FXMatrixFire = Layout::GetSlotInstance(Slot::FXMatrix, 1),
        FXMatrixEarth = Layout::GetSlotInstance(Slot::FXMatrix, 2),
        SubWater = Layout::GetSlotInstance(Slot::Sub, 0),
        SubFire = Layout::GetSlotInstance(Slot::Sub, 1),
        SubEarth = Layout::GetSlotInstance(Slot::Sub, 2),
        NumBanks = x_totalNumBanks,
        Source = SourceWater,
        Shape = ShapeWater,
        PanningAndSequencing = PanningAndSequencingWater,
        VoiceLFOs = VoiceLFOsWater,
        FXMatrix = FXMatrixWater,
        Sub = SubWater
    };

    static constexpr std::array<Bank, 6> x_voiceSelectorBanks =
    {{Bank::Source, Bank::Shape, Bank::FXMatrix, Bank::PanningAndSequencing, Bank::VoiceLFOs, Bank::Sub}};
    static constexpr std::array<Bank, 4> x_quadSelectorBanks =
    {{Bank::Delay, Bank::Reverb, Bank::PartialMachine, Bank::QuadLFOs}};
    static constexpr std::array<Bank, 4> x_globalSelectorBanks =
    {{Bank::TheoryOfTime, Bank::Mastering, Bank::Inputs, Bank::DeepVocoder}};

    static constexpr size_t x_numQuadBanks = x_quadSelectorBanks.size();

    struct ParamSwitch
    {
        int m_numValues;

        ParamSwitch(int numValues)
            : m_numValues(numValues)
        {
        }

        bool IsSwitch()
        {
            return m_numValues > 1;
        }
    };

    static_assert(static_cast<int>(Bank::SourceWater) == 0 &&
        static_cast<int>(Bank::ShapeWater) == 3 &&
        static_cast<int>(Bank::PanningAndSequencingWater) == 6 &&
        static_cast<int>(Bank::VoiceLFOsWater) == 9 &&
        static_cast<int>(Bank::Delay) == 12 && static_cast<int>(Bank::Reverb) == 13 &&
        static_cast<int>(Bank::PartialMachine) == 14 && static_cast<int>(Bank::QuadLFOs) == 15 &&
        static_cast<int>(Bank::TheoryOfTime) == 16 && static_cast<int>(Bank::Mastering) == 17 &&
        static_cast<int>(Bank::Inputs) == 18 && static_cast<int>(Bank::DeepVocoder) == 19 &&
        static_cast<int>(Bank::FXMatrixWater) == 20 &&
        static_cast<int>(Bank::SubWater) == 23 &&
        static_cast<int>(Bank::NumBanks) == 26, "Slot instances must preserve the Bank API");

    using ModulatorSkin = SmartGridOneModulators::Skin;

    static_assert(SmartGrid::BankedEncoderCell::x_numModulators == SmartGridOneModulators::x_skins[0].size(),
        "Modulator skin table must cover the persisted positions");

    static ModulatorSkin GetModulatorSkin(size_t index, BankMode mode)
    {
        size_t row = IsVoiceMode(mode) ? 0 : mode == BankMode::Quad ? 1 : 2;
        if (index >= SmartGridOneModulators::x_skins[0].size() ||
            (!IsVoiceMode(mode) && mode != BankMode::Quad && mode != BankMode::Global))
        {
            return {};
        }

        return SmartGridOneModulators::x_skins[row][index];
    }

    EncoderBankBank m_encoderBankBank;
    Bank m_selectedBank;
    size_t m_selectedTrio;
    std::array<size_t, x_totalNumBanks> m_boundPages;

    SmartGridOneEncoders(SmartGridOneContext* context)
        : m_encoderBankBank(
            static_cast<int>(Bank::NumBanks),
            x_numBankModes,
            Layout::x_numParamInstances,
            context)
        , m_selectedBank(Bank::SourceWater)
        , m_selectedTrio(0)
    {
        m_boundPages.fill(Layout::x_noRoute);
        Init();
    }

    static bool IsVoiceBank(Bank bank)
    {
        Slot slot = Layout::x_tables.m_slotInstances[static_cast<size_t>(bank)].m_slot;
        return Layout::x_tables.m_slots[static_cast<size_t>(slot)].m_numInstances == x_numTrios;
    }

    static Bank BankForTrio(Bank bank, size_t trio)
    {
        Slot slot = Layout::x_tables.m_slotInstances[static_cast<size_t>(bank)].m_slot;
        return static_cast<Bank>(Layout::GetSlotInstance(slot, trio));
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

    static constexpr size_t EncoderIndex(Param param, size_t trio)
    {
        return Layout::GetParamInstanceId(param, trio);
    }

    static ParamSwitch GetParamSwitch(Param param)
    {
        static constexpr std::array x_switchValues =
        {
#define F(name, shortName, mode, defaultValue, description, color, switchValues, bipolar) switchValues,
#include "ForEachSmartGridOneParam.hpp"
#undef F
        };

        return ParamSwitch(x_switchValues[static_cast<size_t>(param)]);
    }

    // BankMode lookup for a bank
    //
    BankMode GetModeForBank(Bank bank)
    {
        const auto& instance = Layout::x_tables.m_slotInstances[static_cast<size_t>(bank)];
        ParamMode mode = Layout::x_tables.m_slots[static_cast<size_t>(instance.m_slot)].m_mode;
        if (mode == ParamMode::Voice)
        {
            return VoiceModeForTrio(instance.m_trio);
        }

        return mode == ParamMode::Quad ? BankMode::Quad : BankMode::Global;
    }

    // Value getters - use encoder index (Param enum) for O(1) lookup, not grid position
    //
    float GetValue(Param param, int voice)
    {
        if (Layout::GetParamMode(param) == ParamMode::Voice)
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
        if (Layout::GetParamMode(param) == ParamMode::Voice)
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

        bool isVoiceMode = Layout::GetParamMode(param) == ParamMode::Voice;
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
    static SmartGrid::Color GetRouteColor(size_t route)
    {
        static const std::array x_colors =
        {
#define F(slot, enumtype, enumval, page, color) color,
#include "ForEachSmartGridOneSlot.hpp"
#undef F
        };

        return x_colors[route];
    }

    void BindPage(Slot slot, size_t trio, size_t route)
    {
        if (route == Layout::x_noRoute)
        {
            return;
        }

        size_t bankIx = Layout::GetSlotInstance(slot, trio);
        Page page = Layout::x_slotRoutes[route].m_page;
        size_t pageInstance = Layout::GetPageInstance(page, trio);
        m_encoderBankBank.m_bankConfigs[bankIx].m_color = GetRouteColor(route);
        if (m_boundPages[bankIx] == pageInstance)
        {
            return;
        }

        auto& bank = m_encoderBankBank.m_banks[bankIx];
        bank.Deselect();
        const auto& grid = Layout::x_tables.m_pages[static_cast<size_t>(page)].m_grid;
        for (size_t position = 0; position < Layout::x_gridSize; ++position)
        {
            Param param = grid[position];
            auto* cell = param == Param::NumParams ? nullptr : m_encoderBankBank.GetEncoder(
                EncoderIndex(param, Layout::GetParamMode(param) == ParamMode::Voice ? trio : 0));
            bank.PlaceEncoder(static_cast<int>(position % 4), static_cast<int>(position / 4), cell);
        }

        bank.ProcessTopology();
        m_boundPages[bankIx] = pageInstance;
    }

    void Init()
    {
        for (size_t trio = 0; trio < x_numTrios; ++trio)
        {
            m_encoderBankBank.InitMode(static_cast<size_t>(VoiceModeForTrio(trio)), x_voicesPerTrio);
        }

        m_encoderBankBank.InitMode(static_cast<size_t>(BankMode::Quad), 4);
        m_encoderBankBank.InitMode(static_cast<size_t>(BankMode::Global), 1);
        for (size_t bankIx = 0; bankIx < x_totalNumBanks; ++bankIx)
        {
            Slot slot = Layout::x_tables.m_slotInstances[bankIx].m_slot;
            size_t route = Layout::x_tables.m_slots[static_cast<size_t>(slot)].m_routes[0];
            m_encoderBankBank.InitBank(bankIx, static_cast<size_t>(GetModeForBank(static_cast<Bank>(bankIx))), GetRouteColor(route));
        }

        struct ParamDefinition
        {
            float m_defaultValue;
            const char* m_names[4];
            const char* m_shortName;
            SmartGrid::Color m_color;
            int m_switchValues;
            bool m_bipolar;
        };

        static const ParamDefinition x_params[] =
        {
#define F(name, shortName, mode, defaultValue, description, color, switchValues, bipolar) \
            {defaultValue, {#name, #name "Water", #name "Fire", #name "Earth"}, #shortName, color, switchValues, bipolar},
#include "ForEachSmartGridOneParam.hpp"
#undef F
        };

        for (size_t paramIx = 0; paramIx < x_numParams; ++paramIx)
        {
            Param param = static_cast<Param>(paramIx);
            ParamMode mode = Layout::GetParamMode(param);
            const auto& definition = x_params[paramIx];
            size_t numInstances = mode == ParamMode::Voice ? x_numTrios : 1;
            for (size_t trio = 0; trio < numInstances; ++trio)
            {
                BankMode bankMode = mode == ParamMode::Voice ? VoiceModeForTrio(trio) :
                    mode == ParamMode::Quad ? BankMode::Quad : BankMode::Global;
                m_encoderBankBank.CreateEncoder(EncoderIndex(param, trio),
                    static_cast<size_t>(bankMode), definition.m_defaultValue,
                    definition.m_names[mode == ParamMode::Voice ? trio + 1 : 0],
                    definition.m_shortName, definition.m_color, definition.m_switchValues, definition.m_bipolar);
            }
        }

        for (size_t bankIx = 0; bankIx < x_totalNumBanks; ++bankIx)
        {
            const auto& instance = Layout::x_tables.m_slotInstances[bankIx];
            size_t route = Layout::x_tables.m_slots[static_cast<size_t>(instance.m_slot)].m_routes[0];
            BindPage(instance.m_slot, instance.m_trio, route);
        }

        // Set modulator colors and short names for each mode
        //
        for (size_t modeIx = 0; modeIx < x_numBankModes; ++modeIx)
        {
            BankMode mode = static_cast<BankMode>(modeIx);
            auto& modValues = m_encoderBankBank.GetModulatorValues(modeIx);
            for (size_t i = 0; i < SmartGrid::BankedEncoderCell::x_numModulators; ++i)
            {
                ModulatorSkin skin = GetModulatorSkin(i, mode);
                modValues.SetModulatorSkin(i, skin.m_color, skin.m_shortName);
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

    template <typename Machine>
    void UpdateSlotsForMachine(Machine machine, size_t trio)
    {
        const auto& affected = Layout::x_tables.m_affectedSlots[static_cast<size_t>(Layout::GetMachineType(machine))];
        for (size_t i = 0; i < affected.m_count; ++i)
        {
            Slot slot = affected.m_slots[i];
            BindPage(slot, trio, Layout::GetRoute(slot, machine));
        }
    }

    void UpdateEncodersForMachine(
        VoiceMachine::SourceMachine sourceMachine,
        VoiceMachine::FilterMachine filterMachine,
        size_t trio)
    {
        UpdateSlotsForMachine(sourceMachine, trio);
        UpdateSlotsForMachine(filterMachine, trio);
    }

    void UpdateEncodersForMachine(
        VoiceMachine::SourceMachine sourceMachine,
        VoiceMachine::FilterMachine filterMachine)
    {
        UpdateEncodersForMachine(sourceMachine, filterMachine, m_selectedTrio);
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

    void PopulateModulationMatrixUIState(ModulationMatrixUIState* uiState)
    {
        m_encoderBankBank.ForEachNamedEncoder(
            [uiState](size_t paramId, SmartGrid::BankedEncoderCell* cell)
            {
                assert(cell->GetSharedEncoderState()->m_modeIx
                    == static_cast<size_t>(ModulationMatrixUIState::GetMode(paramId)));
                uiState->SetModulatorsAffecting(paramId, cell->m_modulatorsAffecting);
            });
    }

    void PopulateUIState(EncoderBankUIState* uiState)
    {
        m_encoderBankBank.PopulateUIState(uiState);

        for (size_t x = 0; x < 4; ++x)
        {
            for (size_t y = 0; y < 4; ++y)
            {
                BankMode mode = uiState->GetMode(x, y);
                for (size_t i = 0; i < 16; ++i)
                {
                    ModulatorSkin skin = GetModulatorSkin(i, mode);
                    uiState->SetModulationGlyph(x, y, i, skin.m_glyph, skin.m_color);
                }
            }
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
