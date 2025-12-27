#pragma once

#include "TheNonagonSquiggleBoy.hpp"
#include "ShiftedCell.hpp"
#include "SampleTimer.hpp"

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
                SmartGrid::StateCell<bool>::Mode::Momentary);
    }

    struct AuxGrid : SmartGrid::Grid
    {
        TheNonagonSquiggleBoyWrldBldr* m_owner;

        // Gesture selector that tracks held count and shows pink when affecting any bank
        //
        struct WrldBldrGestureSelectorCell : SmartGrid::Cell
        {
            TheNonagonSquiggleBoyWrldBldr* m_owner;
            int m_gesture;

            WrldBldrGestureSelectorCell(TheNonagonSquiggleBoyWrldBldr* owner, int gesture)
                : m_owner(owner)
                , m_gesture(gesture)
            {
            }

            virtual SmartGrid::Color GetColor() override
            {
                if (m_owner->m_internal->m_squiggleBoyState.m_selectedGesture.Get(m_gesture))
                {
                    return SmartGrid::Color::Red;
                }
                else if (m_owner->m_internal->m_squiggleBoy.IsGestureAffectingAnyBank(m_gesture))
                {
                    return SmartGrid::Color::Pink;
                }
                else
                {
                    return SmartGrid::Color::Grey;
                }
            }

            virtual void OnPress(uint8_t velocity) override
            {
                if (m_owner->m_internal->m_sceneState.m_shift)
                {
                    m_owner->m_internal->ClearGesture(m_gesture);
                }
                else
                {
                    m_owner->m_internal->m_squiggleBoyState.SelectGesture(m_gesture, true);
                }
            }

            virtual void OnRelease() override
            {
                m_owner->m_internal->m_squiggleBoyState.SelectGesture(m_gesture, false);
            }
        };

        // Cell that shows gesture affectation status when a gesture is selected,
        // otherwise behaves like a ShiftedCell
        //
        struct GestureAwareSelectorCell : SmartGrid::Cell
        {
            TheNonagonSquiggleBoyWrldBldr* m_owner;
            std::shared_ptr<SmartGrid::Cell> m_main;
            std::shared_ptr<SmartGrid::Cell> m_shifted;
            size_t m_ordinal;

            GestureAwareSelectorCell(
                TheNonagonSquiggleBoyWrldBldr* owner,
                std::shared_ptr<SmartGrid::Cell> main,
                std::shared_ptr<SmartGrid::Cell> shifted,
                size_t ordinal)
                : m_owner(owner)
                , m_main(main)
                , m_shifted(shifted)
                , m_ordinal(ordinal)
            {
            }

            virtual SmartGrid::Color GetColor() override
            {
                if (!m_owner->m_internal->m_squiggleBoyState.m_selectedGesture.IsZero())
                {
                    // When gestures are selected, show if any affects this bank for the current track
                    //
                    size_t currentTrack = 0;
                    if (m_ordinal < SquiggleBoyWithEncoderBank::x_numVoiceBanks)
                    {
                        currentTrack = m_owner->m_internal->m_squiggleBoyState.GetCurrentTrack();
                    }
                    
                    BitSet16 affectingGestures = m_owner->m_internal->m_squiggleBoy.GetGesturesAffectingBankForTrack(m_ordinal, currentTrack);
                    BitSet16 selectedAndAffecting = affectingGestures.Intersect(m_owner->m_internal->m_squiggleBoyState.m_selectedGesture);
                    
                    SmartGrid::Color color = m_owner->m_internal->m_squiggleBoy.GetSelectorColorNoDim(m_ordinal);
                    if (!selectedAndAffecting.IsZero())
                    {
                        return color;
                    }
                    else
                    {
                        return color.Dim();
                    }
                }

                // Otherwise, behave like a ShiftedCell
                //
                if (m_owner->m_auxFocus)
                {
                    return m_shifted ? m_shifted->GetColor() : SmartGrid::Color::Off;
                }
                else
                {
                    return m_main ? m_main->GetColor() : SmartGrid::Color::Off;
                }
            }

            virtual void OnPress(uint8_t velocity) override
            {
                if (m_owner->m_auxFocus)
                {
                    if (m_shifted)
                    {
                        m_shifted->OnPress(velocity);
                    }
                }
                else
                {
                    if (m_main)
                    {
                        m_main->OnPress(velocity);
                    }
                }
            }

            virtual void OnRelease() override
            {
                if (m_owner->m_auxFocus)
                {
                    if (m_shifted)
                    {
                        m_shifted->OnRelease();
                    }
                }
                else
                {
                    if (m_main)
                    {
                        m_main->OnRelease();
                    }
                }
            }
        };

        struct GestureVisibleCell : SmartGrid::Cell
        {
            TheNonagonSquiggleBoyWrldBldr* m_owner;
            std::shared_ptr<SmartGrid::Cell> m_main;
            std::shared_ptr<SmartGrid::Cell> m_gesture;

            GestureVisibleCell(
                TheNonagonSquiggleBoyWrldBldr* owner,
                std::shared_ptr<SmartGrid::Cell> main,
                std::shared_ptr<SmartGrid::Cell> gesture)
                : m_owner(owner)
                , m_main(main)
                , m_gesture(gesture)
            {
            }

            bool ShouldShowGesture()
            {
                return m_owner->m_auxFocus || !m_owner->m_internal->m_squiggleBoyState.m_selectedGesture.IsZero();
            }

            virtual SmartGrid::Color GetColor() override
            {
                if (ShouldShowGesture())
                {
                    return m_gesture ? m_gesture->GetColor() : SmartGrid::Color::Off;
                }
                else
                {
                    return m_main ? m_main->GetColor() : SmartGrid::Color::Off;
                }
            }

            virtual void OnPress(uint8_t velocity) override
            {
                if (ShouldShowGesture())
                {
                    if (m_gesture)
                    {
                        m_gesture->OnPress(velocity);
                    }
                }
                else
                {
                    if (m_main)
                    {
                        m_main->OnPress(velocity);
                    }
                }
            }

            virtual void OnRelease() override
            {
                if (ShouldShowGesture())
                {
                    if (m_gesture)
                    {
                        m_gesture->OnRelease();
                    }
                }
                else
                {
                    if (m_main)
                    {
                        m_main->OnRelease();
                    }
                }
            }
        };

        AuxGrid(TheNonagonSquiggleBoyWrldBldr* owner)
            : m_owner(owner)
        {
            // Row 0: Main = scene selectors, Aux = gesture selectors (visible when auxFocus OR numGesturesHeld > 0)
            //
            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                std::shared_ptr<SmartGrid::Cell> gestureCell = std::make_shared<WrldBldrGestureSelectorCell>(owner, i);
                std::shared_ptr<SmartGrid::Cell> mainCell = std::make_shared<TheNonagonSquiggleBoyInternal::SceneSelectorCell>(m_owner->m_internal, i);
                Put(i, 0, new GestureVisibleCell(m_owner, mainCell, gestureCell));
            }

            // Row 1: Main = grid mode selectors (0-3) + display mode (7), Aux = gesture selectors
            //
            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                std::shared_ptr<SmartGrid::Cell> gestureCell = std::make_shared<WrldBldrGestureSelectorCell>(owner, i + SmartGrid::x_baseGridSize);
                std::shared_ptr<SmartGrid::Cell> mainCell = nullptr;
                if (i < static_cast<size_t>(GridsMode::NumGrids))
                {
                    mainCell = std::make_shared<SetGridsModeCell>(m_owner, static_cast<GridsMode>(i));
                }
                else if (i == SmartGrid::x_baseGridSize - 1)
                {
                    mainCell = std::make_shared<SetDisplayModeCell>(m_owner, DisplayMode::Visualizer);
                }

                Put(i, 1, new GestureVisibleCell(m_owner, mainCell, gestureCell));
            }

            // Rows 2-3: Main = bank selectors, Aux = mutes and trio selectors (same x positions as original, y+2)
            //
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
                size_t ordinal = i + SquiggleBoyWithEncoderBank::x_numVoiceBanks;
                std::shared_ptr<SmartGrid::Cell> auxCell = nullptr;

                // Check if this position has a mute cell (y=2 corresponds to original y=0)
                //
                for (size_t trio = 0; trio < TheNonagonInternal::x_numTrios; ++trio)
                {
                    int xPos = 2 + 2 * trio;
                    if (static_cast<int>(i) == xPos)
                    {
                        auxCell = std::shared_ptr<SmartGrid::Cell>(m_owner->m_internal->m_nonagon.MakeMuteCell(static_cast<TheNonagonSmartGrid::Trio>(trio), 0));
                    }
                    else if (static_cast<int>(i) == xPos + 1)
                    {
                        auxCell = std::shared_ptr<SmartGrid::Cell>(m_owner->m_internal->m_nonagon.MakeMuteCell(static_cast<TheNonagonSmartGrid::Trio>(trio), 1));
                    }
                }

                Put(i, 2, new GestureAwareSelectorCell(
                    m_owner,
                    GetShared(i, 2),
                    auxCell,
                    ordinal));
            }

            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                std::shared_ptr<SmartGrid::Cell> auxCell = nullptr;

                // Check if this position has a trio selector or mute cell (y=3 corresponds to original y=1)
                //
                for (size_t trio = 0; trio < TheNonagonInternal::x_numTrios; ++trio)
                {
                    int xPos = 2 + 2 * trio;
                    if (static_cast<int>(i) == xPos)
                    {
                        auxCell = std::make_shared<SetActiveTrioCell>(m_owner, static_cast<TheNonagonSmartGrid::Trio>(trio));
                    }
                    else if (static_cast<int>(i) == xPos + 1)
                    {
                        auxCell = std::shared_ptr<SmartGrid::Cell>(m_owner->m_internal->m_nonagon.MakeMuteCell(static_cast<TheNonagonSmartGrid::Trio>(trio), 2));
                    }
                }

                size_t ordinal = (i < SquiggleBoyWithEncoderBank::x_numVoiceBanks) ? i : 255;
                Put(i, 3, new GestureAwareSelectorCell(
                    m_owner,
                    GetShared(i, 3),
                    auxCell,
                    ordinal));
            }

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

    void ProcessSample()
    {
        ProcessMessages(SampleTimer::GetAbsTimeUs());
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