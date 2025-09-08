#pragma once

#include "TheNonagonSquiggleBoy.hpp"

struct TheNonagonSquiggleBoyWrldBldr
{
    enum class GridsMode : uint8_t
    {
        ComuteAndTheory = 0,
        Matrix = 1,
        Intervals = 2,
        Water = 3,
        Earth = 4,
        Fire = 5,
        NumGrids = 6,
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
            case GridsMode::Water:
                return m_internal->m_nonagon.m_sheafViewGridWaterGrid;
            case GridsMode::Earth:
                return m_internal->m_nonagon.m_sheafViewGridEarthGrid;
            case GridsMode::Fire:
                return m_internal->m_nonagon.m_sheafViewGridFireGrid;
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
            case GridsMode::Water:
                return m_internal->m_nonagon.m_indexArpWaterGrid;
            case GridsMode::Earth:
                return m_internal->m_nonagon.m_indexArpEarthGrid;
            case GridsMode::Fire:
                return m_internal->m_nonagon.m_indexArpFireGrid;
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
            m_owner->SetGridsMode(m_mode);
        }
    };

    struct AuxGrid : SmartGrid::Grid
    {
        TheNonagonSquiggleBoyWrldBldr* m_owner;
        AuxGrid(TheNonagonSquiggleBoyWrldBldr* owner)
            : m_owner(owner)
        {
            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                Put(i, 0, new TheNonagonSquiggleBoyInternal::SceneSelectorCell(owner->m_internal, i));
            }

            for (size_t i = 0; i < static_cast<size_t>(GridsMode::NumGrids); ++i)
            {
                Put(i, 1, new SetGridsModeCell(owner, static_cast<GridsMode>(i)));
            }

            for (size_t i = 0; i < SquiggleBoyWithEncoderBank::x_numGlobalBanks; ++i)
            {
                Put(i, 2, new SquiggleBoyWithEncoderBank::SelectorCell(
                    &owner->m_internal->m_squiggleBoy,
                    i + SquiggleBoyWithEncoderBank::x_numVoiceBanks));
            }

            for (size_t i = 0; i < SquiggleBoyWithEncoderBank::x_numVoiceBanks; ++i)
            {
                Put(i, 3, new SquiggleBoyWithEncoderBank::SelectorCell(&owner->m_internal->m_squiggleBoy, i));
            }

            Put(0, 4, owner->m_internal->MakeRunningCell());
            Put(0, 5, new TheNonagonSquiggleBoyInternal::RecordCell(owner->m_internal));
            Put(1, 4, new TheNonagonSquiggleBoyInternal::SaveLoadJSONCell(owner->m_internal, true));
            Put(1, 5, new TheNonagonSquiggleBoyInternal::SaveLoadJSONCell(owner->m_internal, false));
            Put(3, 4, owner->m_internal->MakeNoiseModeCell());
            Put(3, 5, new TheNonagonSquiggleBoyInternal::RevertToDefaultCell(owner->m_internal));

            for (size_t i = 0; i < 4; ++i)
            {
                for (size_t j = 0; j < 2; ++j)
                {
                    Put(i + 4, j + 4, new TimerCell(&owner->m_internal->m_timer, 2 * (i + 4 * j)));
                }
            }
        }
    };

    void SetGridsMode(GridsMode mode)
    {
        m_gridsMode = mode;
        m_leftGrid.SetGrid(GetLeftGrid(mode));
        m_rightGrid.SetGrid(GetRightGrid(mode));
        switch (mode)
        {
            case GridsMode::Water:
            {
                m_internal->SetActiveTrio(TheNonagonSmartGrid::Trio::Water);
                break;
            }
            case GridsMode::Earth:
            {
                m_internal->SetActiveTrio(TheNonagonSmartGrid::Trio::Earth);
                break;
            }
            case GridsMode::Fire:
            {
                m_internal->SetActiveTrio(TheNonagonSmartGrid::Trio::Fire);
                break;
            }
            default:
            {
                break;
            }
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
    SmartGrid::MessageInBus m_messageBus;
    UIState m_uiState;

    TheNonagonSquiggleBoyWrldBldr(TheNonagonSquiggleBoyInternal* internal)
        : m_internal(internal)
        , m_gridsMode(GridsMode::ComuteAndTheory)
        , m_leftGrid(this)
        , m_rightGrid(this)
        , m_auxGrid(this)
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
        return EncoderBankUI(static_cast<int>(Routes::Encoder), &m_internal->m_squiggleBoyUIState.m_encoderBankUIState, &m_messageBus);
    }

    AnalogUI MkAnalogUI(int ix)
    {
        return m_internal->m_analogUIState.MkAnalogUI(static_cast<int>(Routes::Analog), ix, &m_messageBus);
    }
};