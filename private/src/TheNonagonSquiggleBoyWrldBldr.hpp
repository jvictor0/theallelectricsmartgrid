#pragma once

#include "TheNonagonSquiggleBoy.hpp"
#include "ShiftedCell.hpp"

struct TheNonagonSquiggleBoyWrldBldr
{
    enum class GridsMode : uint8_t
    {
        ComuteAndTheory = 0,
        Matrix = 1,
        Intervals = 2,
        SubSequencer = 3,
        NumGrids = 4,
    };

    enum class DisplayMode : uint8_t
    {
        Controller,
        Visualizer
    };

    SmartGrid::Grid* GetLeftGrid(GridsMode mode)
    {
        switch (mode)
        {
            case GridsMode::ComuteAndTheory:
                return m_internal->m_nonagon.m_lameJuisCoMuteGrid;
            case GridsMode::Matrix:
                return m_internal->m_nonagon.m_lameJuisMatrixGrid;
            case GridsMode::Intervals:
                return m_internal->m_nonagon.m_lameJuisCoMuteGrid;
            case GridsMode::SubSequencer:
            {
                switch (m_internal->m_activeTrio)
                {
                    case TheNonagonSmartGrid::Trio::Water:
                        return m_internal->m_nonagon.m_sheafViewGridWaterGrid;
                    case TheNonagonSmartGrid::Trio::Earth:
                        return m_internal->m_nonagon.m_sheafViewGridEarthGrid;
                    case TheNonagonSmartGrid::Trio::Fire:
                        return m_internal->m_nonagon.m_sheafViewGridFireGrid;
                    case TheNonagonSmartGrid::Trio::NumTrios:
                        return nullptr;
                }
            }
            default:
                return nullptr;
        }
    }

    SmartGrid::Grid* GetRightGrid(GridsMode mode)
    {
        switch (mode)
        {
            case GridsMode::ComuteAndTheory:
                return m_internal->m_nonagon.m_theoryOfTimeTopologyGrid;
            case GridsMode::Matrix:
                return m_internal->m_nonagon.m_lameJuisRHSGrid;
            case GridsMode::Intervals:
                return m_internal->m_nonagon.m_lameJuisIntervalGrid;
            case GridsMode::SubSequencer:
            {
                switch (m_internal->m_activeTrio)
                {
                    case TheNonagonSmartGrid::Trio::Water:
                        return m_internal->m_nonagon.m_indexArpWaterGrid;
                    case TheNonagonSmartGrid::Trio::Earth:
                        return m_internal->m_nonagon.m_indexArpEarthGrid;
                    case TheNonagonSmartGrid::Trio::Fire:
                        return m_internal->m_nonagon.m_indexArpFireGrid;
                    case TheNonagonSmartGrid::Trio::NumTrios:
                        return nullptr;
                }
            }
            default:
                return nullptr;
        }
    }

    struct LeftGrid : InteriorGridBase
    {
        TheNonagonSquiggleBoyWrldBldr* m_owner;
        LeftGrid(TheNonagonSquiggleBoyWrldBldr* owner)
            : m_owner(owner)
        {
            SetGrid(owner->GetLeftGrid(owner->m_gridsMode));
        }
    };

    struct RightGrid : InteriorGridBase
    {
        TheNonagonSquiggleBoyWrldBldr* m_owner;
        RightGrid(TheNonagonSquiggleBoyWrldBldr* owner)
            : m_owner(owner)
        {
            SetGrid(owner->GetRightGrid(owner->m_gridsMode));
        }
    };

    struct SetGridsModeCell : SmartGrid::Cell
    {
        TheNonagonSquiggleBoyWrldBldr* m_owner;
        GridsMode m_mode;
        SetGridsModeCell(TheNonagonSquiggleBoyWrldBldr* owner, GridsMode mode)
            : m_owner(owner)
            , m_mode(mode)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            return m_owner->m_gridsMode == m_mode ? m_owner->GetRightGrid(m_mode)->GetOnColor() : m_owner->GetRightGrid(m_mode)->GetOffColor();
        }

        virtual void OnPress(uint8_t velocity) override
        {
            m_owner->m_uiState.m_displayMode.store(DisplayMode::Controller);
            m_owner->SetGridsMode(m_mode);
        }
    };

    struct SetDisplayModeCell : SmartGrid::Cell
    {
        TheNonagonSquiggleBoyWrldBldr* m_owner;
        DisplayMode m_mode;
        SetDisplayModeCell(TheNonagonSquiggleBoyWrldBldr* owner, DisplayMode mode)
            : m_owner(owner)
            , m_mode(mode)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            return m_owner->m_uiState.m_displayMode.load() == m_mode ? SmartGrid::Color::Yellow : SmartGrid::Color::Yellow.Dim();
        }

        virtual void OnPress(uint8_t velocity) override
        {
            m_owner->m_uiState.m_displayMode.store(m_mode);
        }
    };

    struct SetActiveTrioCell : SmartGrid::Cell
    {
        TheNonagonSquiggleBoyWrldBldr* m_owner;
        TheNonagonSmartGrid::Trio m_trio;
        SetActiveTrioCell(TheNonagonSquiggleBoyWrldBldr* owner, TheNonagonSmartGrid::Trio trio)
            : m_owner(owner)
        {
            m_trio = trio;
        }

        virtual SmartGrid::Color GetColor() override
        {
            return m_owner->m_internal->m_activeTrio == m_trio ? TheNonagonSmartGrid::TrioColor(m_trio) : TheNonagonSmartGrid::TrioColor(m_trio).Dim();
        }

        virtual void OnPress(uint8_t velocity) override
        {
            m_owner->m_internal->SetActiveTrio(m_trio);
            m_owner->SetGridsMode(m_owner->m_gridsMode);
        }
    };

    SmartGrid::Cell* MakeAuxFocusCell()
    {
        return new SmartGrid::StateCell<bool>(
                SmartGrid::Color::Yellow /*offColor*/,
                SmartGrid::Color::Purple /*onColor*/,
                &m_auxFocus,
                true,
                false,
                SmartGrid::StateCell<bool>::Mode::Toggle);
    }

    struct AuxGrid : SmartGrid::Grid
    {
        TheNonagonSquiggleBoyWrldBldr* m_owner;
        AuxGrid(TheNonagonSquiggleBoyWrldBldr* owner)
            : m_owner(owner)
        {
            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                Put(i, 0, new TheNonagonSquiggleBoyInternal::GestureSelectorCell(owner->m_internal, i));
            }

            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                Put(i, 1, new TheNonagonSquiggleBoyInternal::GestureSelectorCell(owner->m_internal, i + SmartGrid::x_baseGridSize));
            }

            for (size_t i = 0; i < SquiggleBoyWithEncoderBank::x_numQuadBanks; ++i)
            {
                Put(i, 2, new SquiggleBoyWithEncoderBank::SelectorCell(
                    &owner->m_internal->m_squiggleBoy,
                    i + SquiggleBoyWithEncoderBank::x_numVoiceBanks));
            }

            for (size_t i = 0; i < SquiggleBoyWithEncoderBank::x_numGlobalBanks; ++i)
            {
                Put(i + SquiggleBoyWithEncoderBank::x_numQuadBanks, 2, new SquiggleBoyWithEncoderBank::SelectorCell(
                    &owner->m_internal->m_squiggleBoy,
                    i + SquiggleBoyWithEncoderBank::x_numVoiceBanks + SquiggleBoyWithEncoderBank::x_numQuadBanks));
            }

            for (size_t i = 0; i < SquiggleBoyWithEncoderBank::x_numVoiceBanks; ++i)
            {
                Put(i, 3, new SquiggleBoyWithEncoderBank::SelectorCell(&owner->m_internal->m_squiggleBoy, i));
            }

            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                Put(i, 2, new ShiftedCell(
                    GetShared(i, 2),
                    std::shared_ptr<SmartGrid::Cell>(new TheNonagonSquiggleBoyInternal::SceneSelectorCell(m_owner->m_internal, i)),
                    &m_owner->m_auxFocus));
            }

            for (size_t i = 0; i < static_cast<size_t>(GridsMode::NumGrids); ++i)
            {
                Put(i, 3, new ShiftedCell(
                    GetShared(i, 3),
                    std::shared_ptr<SmartGrid::Cell>(new SetGridsModeCell(m_owner, static_cast<GridsMode>(i))),
                    &m_owner->m_auxFocus));
            }

            Put(SmartGrid::x_baseGridSize - 1, 3, new ShiftedCell(
                GetShared(SmartGrid::x_baseGridSize - 1, 3),
                std::shared_ptr<SmartGrid::Cell>(new SetDisplayModeCell(m_owner, DisplayMode::Visualizer)),
                &m_owner->m_auxFocus));

            for (size_t i = 0; i < TheNonagonInternal::x_numTrios; ++i)
            {
                int xPos = 2 + 2 * i;
                Put(xPos, 1, new ShiftedCell(
                    std::shared_ptr<SmartGrid::Cell>(new SetActiveTrioCell(m_owner, static_cast<TheNonagonSmartGrid::Trio>(i))),
                    GetShared(xPos, 1),
                    &m_owner->m_auxFocus));

                Put(xPos, 0, new ShiftedCell(
                    std::shared_ptr<SmartGrid::Cell>(m_owner->m_internal->m_nonagon.MakeMuteCell(static_cast<TheNonagonSmartGrid::Trio>(i), 0)),
                    GetShared(xPos, 0),
                    &m_owner->m_auxFocus));

                Put(xPos + 1, 0, new ShiftedCell(
                    std::shared_ptr<SmartGrid::Cell>(m_owner->m_internal->m_nonagon.MakeMuteCell(static_cast<TheNonagonSmartGrid::Trio>(i), 1)),
                    GetShared(xPos + 1, 0),
                    &m_owner->m_auxFocus));

                Put(xPos + 1, 1, new ShiftedCell(
                    std::shared_ptr<SmartGrid::Cell>(m_owner->m_internal->m_nonagon.MakeMuteCell(static_cast<TheNonagonSmartGrid::Trio>(i), 2)),
                    GetShared(xPos + 1, 1),
                    &m_owner->m_auxFocus));
            }

            Put(0, 0, new ShiftedCell(
                nullptr,
                GetShared(0, 0),
                &m_owner->m_auxFocus));

            Put(0, 1, new ShiftedCell(
                nullptr,
                GetShared(0, 1),
                &m_owner->m_auxFocus));

            Put(1, 0, new ShiftedCell(
                nullptr,
                GetShared(1, 0),
                &m_owner->m_auxFocus));

            Put(1, 1, new ShiftedCell(
                nullptr,
                GetShared(1, 1),
                &m_owner->m_auxFocus));
                
            Put(0, 4, m_owner->m_internal->MakeShiftCell());
            Put(0, 5, m_owner->MakeAuxFocusCell());
            Put(1, 4, m_owner->m_internal->MakeRunningCell());
            Put(1, 5, new TheNonagonSquiggleBoyInternal::RecordCell(m_owner->m_internal));
            Put(3, 4, new TheNonagonSquiggleBoyInternal::SaveLoadJSONCell(m_owner->m_internal, true));
            Put(3, 5, new TheNonagonSquiggleBoyInternal::SaveLoadJSONCell(m_owner->m_internal, false));
            Put(2, 4, m_owner->m_internal->MakeNoiseModeCell());
            Put(2, 5, new TheNonagonSquiggleBoyInternal::RevertToDefaultCell(m_owner->m_internal));

            for (size_t i = 0; i < 4; ++i)
            {
                for (size_t j = 0; j < 2; ++j)
                {
                    Put(i + 4, j + 4, new TimerCell(&m_owner->m_internal->m_timer, 2 * (i + 4 * j)));
                }
            }
        }
    };

    void SetGridsMode(GridsMode mode)
    {
        m_gridsMode = mode;
        if (mode != GridsMode::NumGrids)
        {
            m_leftGrid.SetGrid(GetLeftGrid(mode));
            m_rightGrid.SetGrid(GetRightGrid(mode));
        }
    }

    enum class Routes : int
    {
        Encoder = 4,
        Analog = 5,
        LeftGrid = 6,
        RightGrid = 7,
        AuxGrid = 8,
    };

    void Apply(SmartGrid::MessageIn msg)
    {
        Routes route = static_cast<Routes>(msg.m_routeId);
        switch (route)
        {
            case Routes::Encoder:
            case Routes::Analog:
            {
                m_internal->Apply(msg);
                break;
            }
            case Routes::LeftGrid:
            {
                m_leftGrid.Apply(msg);
                break;
            }
            case Routes::RightGrid:
            {
                m_rightGrid.Apply(msg);
                break;
            }
            case Routes::AuxGrid:
            {
                m_auxGrid.Apply(msg);
                break;
            }
        }
    }

    void ProcessMessages(size_t timestamp)
    {
        m_messageBus.ProcessMessages(this, timestamp);
    }

    void SendMessage(SmartGrid::MessageIn msg)
    {
        m_messageBus.Push(msg);
    }

    void SendMessage(SmartGrid::BasicMidi msg)
    {
        m_messageBus.Push(msg);
    }

    struct UIState
    {
        SmartGrid::SmartBusColor m_colorBus[3];
        std::atomic<DisplayMode> m_displayMode;

        UIState()
            : m_displayMode(DisplayMode::Controller)
        {
        }
    };

    void WriteUIState(UIState& uiState)
    {
        m_leftGrid.OutputToBus(&uiState.m_colorBus[0]);
        m_rightGrid.OutputToBus(&uiState.m_colorBus[1]);
        m_auxGrid.OutputToBus(&uiState.m_colorBus[2]);
    }

    static constexpr size_t x_launchpadStartRouteId = 6;

    TheNonagonSquiggleBoyInternal* m_internal;
    GridsMode m_gridsMode;
    LeftGrid m_leftGrid;
    RightGrid m_rightGrid;
    AuxGrid m_auxGrid;
    bool m_auxFocus;
    SmartGrid::MessageInBus m_messageBus;
    UIState m_uiState;

    TheNonagonSquiggleBoyWrldBldr(TheNonagonSquiggleBoyInternal* internal)
        : m_internal(internal)
        , m_gridsMode(GridsMode::ComuteAndTheory)
        , m_leftGrid(this)
        , m_rightGrid(this)
        , m_auxGrid(this)
        , m_auxFocus(false)
    {
        m_internal->SetActiveTrio(TheNonagonSmartGrid::Trio::Water);

        m_messageBus.SetRouteType(static_cast<int>(Routes::Encoder), SmartGrid::MidiToMessageIn::RouteType::Encoder);
        m_messageBus.SetRouteType(static_cast<int>(Routes::Analog), SmartGrid::MidiToMessageIn::RouteType::Param14);
        m_messageBus.SetRouteType(static_cast<int>(Routes::LeftGrid), SmartGrid::MidiToMessageIn::RouteType::LaunchPad);
        m_messageBus.SetRouteType(static_cast<int>(Routes::RightGrid), SmartGrid::MidiToMessageIn::RouteType::LaunchPad);
        m_messageBus.SetRouteType(static_cast<int>(Routes::AuxGrid), SmartGrid::MidiToMessageIn::RouteType::LaunchPad);
    }

    void ProcessSample(size_t timestamp)
    {
        ProcessMessages(timestamp);
    }

    void ProcessFrame()
    {
        WriteUIState(m_uiState);
    }

    PadUIGrid MkLaunchPadUIGrid(Routes route)
    {
        return PadUIGrid(static_cast<int>(route), &m_uiState.m_colorBus[static_cast<int>(route) - x_launchpadStartRouteId], &m_messageBus);
    }

    EncoderBankUI MkEncoderBankUI()
    {
        return EncoderBankUI(static_cast<int>(Routes::Encoder), &m_internal->m_uiState.m_squiggleBoyUIState.m_encoderBankUIState, &m_messageBus);
    }

    AnalogUI MkAnalogUI(int ix)
    {
        return m_internal->m_uiState.m_analogUIState.MkAnalogUI(static_cast<int>(Routes::Analog), ix, &m_messageBus);
    }
};