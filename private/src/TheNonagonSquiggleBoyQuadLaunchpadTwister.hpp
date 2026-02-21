#pragma once

#include "TheNonagonSquiggleBoy.hpp"
#include "InteriorGrid.hpp"
#include "SampleTimer.hpp"

struct TheNonagonSquiggleBoyQuadLaunchpadTwister
{
    struct ToggleGridCell : SmartGrid::Cell
    {
        SmartGrid::Grid* m_grid;
        SmartGrid::Grid* m_replaceGrid;

        SmartGrid::Grid** m_target;

        ToggleGridCell(SmartGrid::Grid* grid, SmartGrid::Grid** target)
            : m_grid(grid)
            , m_target(target)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            return m_replaceGrid == nullptr ? m_grid->GetOffColor() : m_grid->GetOnColor();
        }

        virtual void OnPress(uint8_t velocity) override
        {
            m_replaceGrid = *m_target;
            *m_target = m_grid;
        }

        virtual void OnRelease() override
        {
            *m_target = m_replaceGrid;
            m_replaceGrid = nullptr;
        }
    };

    enum class TopGridsMode : uint8_t
    {
        Matrix,
        Water,
        Earth,
        Fire,
    };

    struct TopLeftGrid : InteriorGridBase
    {
        TheNonagonSquiggleBoyQuadLaunchpadTwister* m_owner;
        TopLeftGrid(TheNonagonSquiggleBoyQuadLaunchpadTwister* owner)
            : m_owner(owner)
        {
            SetGrid(m_owner->m_internal->m_nonagon.m_lameJuisMatrixGrid);

            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                Put(SmartGrid::x_baseGridSize, i, new TheNonagonSquiggleBoyInternal::ActiveTrioIndicatorCell(owner->m_internal));
            }
        }
    };

    struct TopRightGrid : InteriorGridBase
    {
        TheNonagonSquiggleBoyQuadLaunchpadTwister* m_owner;
        TopRightGrid(TheNonagonSquiggleBoyQuadLaunchpadTwister* owner)
            : m_owner(owner)
        {
            SetGrid(m_owner->m_internal->m_nonagon.m_lameJuisRHSGrid);
        }
    };

    struct BottomLeftGrid : InteriorGridBase
    {
        TheNonagonSquiggleBoyQuadLaunchpadTwister* m_owner;
        BottomLeftGrid(TheNonagonSquiggleBoyQuadLaunchpadTwister* owner)
            : m_owner(owner)
        {
            SetGrid(m_owner->m_internal->m_nonagon.m_lameJuisCoMuteGrid);

            for (int i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                Put(i, SmartGrid::x_baseGridSize + 1, new TheNonagonSquiggleBoyInternal::SceneSelectorCell(owner->m_internal, i));
                Put(i, SmartGrid::x_baseGridSize, new TheNonagonSquiggleBoyInternal::GestureSelectorCell(owner->m_internal, i));
            }

            Put(-1, 5, owner->MakeNoiseModeCell());
            Put(-1, 6, owner->MakeRunningCell());
            Put(-1, 7, new TheNonagonSquiggleBoyInternal::RecordCell(owner->m_internal));

            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                Put(i, -1, new TimerCell(&owner->m_internal->m_timer, i));
            }
        }
    };

    struct BottomRightGrid : InteriorGridBase
    {
        TheNonagonSquiggleBoyQuadLaunchpadTwister* m_owner;

        struct TopGridSelect : SmartGrid::Cell
        {
            TheNonagonSquiggleBoyQuadLaunchpadTwister* m_owner;
            TopGridsMode m_mode;
            SmartGrid::Color m_color;

            TopGridSelect(
                TheNonagonSquiggleBoyQuadLaunchpadTwister* owner, 
                TopGridsMode mode,
                SmartGrid::Color color)
                : m_owner(owner)
                , m_mode(mode)
                , m_color(color)
            {
            }

            virtual SmartGrid::Color GetColor() override
            {
                return m_owner->m_topGridsMode == m_mode ? m_color : m_color.Dim();
            }

            virtual void OnPress(uint8_t velocity) override
            {
                m_owner->SetTopGridsMode(m_mode);
            }
        };

        BottomRightGrid(TheNonagonSquiggleBoyQuadLaunchpadTwister* owner)
            : m_owner(owner)
        {
            SetGrid(m_owner->m_internal->m_nonagon.m_theoryOfTimeTopologyGrid);
            Put(SmartGrid::x_baseGridSize, 4, new TopGridSelect(owner, TopGridsMode::Matrix, SmartGrid::Color::Indigo));
            Put(SmartGrid::x_baseGridSize, 5, new TopGridSelect(owner, TopGridsMode::Water, SmartGrid::Color::Blue));
            Put(SmartGrid::x_baseGridSize, 6, new TopGridSelect(owner, TopGridsMode::Earth, SmartGrid::Color::Green));
            Put(SmartGrid::x_baseGridSize, 7, new TopGridSelect(owner, TopGridsMode::Fire, SmartGrid::Color::Red));
            Put(SmartGrid::x_baseGridSize - 1, SmartGrid::x_baseGridSize, new ToggleGridCell(owner->m_internal->m_nonagon.m_lameJuisIntervalGrid, &m_grid));

            Put(-1, 0, new TheNonagonSquiggleBoyInternal::SaveLoadJSONCell(owner->m_internal, true));
            Put(-1, 1, new TheNonagonSquiggleBoyInternal::SaveLoadJSONCell(owner->m_internal, false));

            // Voice banks
            //
            for (size_t i = 0; i < SmartGridOneEncoders::x_numVoiceBanks; ++i)
            {
                Put(i, SmartGrid::x_baseGridSize + 1, new SquiggleBoyWithEncoderBank::SelectorCell(
                    &owner->m_internal->m_squiggleBoy,
                    SmartGridOneEncoders::BankFromOrdinal(i)));
            }

            // Quad banks
            //
            for (size_t i = 0; i < SmartGridOneEncoders::x_numQuadBanks; ++i)
            {
                Put(i, SmartGrid::x_baseGridSize, new SquiggleBoyWithEncoderBank::SelectorCell(
                    &owner->m_internal->m_squiggleBoy,
                    SmartGridOneEncoders::BankFromOrdinal(SmartGridOneEncoders::x_numVoiceBanks + i)));
            }

            // Global banks
            //
            for (size_t i = 0; i < SmartGridOneEncoders::x_numGlobalBanks; ++i)
            {
                Put(SmartGridOneEncoders::x_numQuadBanks + i, SmartGrid::x_baseGridSize, new SquiggleBoyWithEncoderBank::SelectorCell(
                    &owner->m_internal->m_squiggleBoy,
                    SmartGridOneEncoders::BankFromOrdinal(SmartGridOneEncoders::x_numVoiceBanks + SmartGridOneEncoders::x_numQuadBanks + i)));
            }

            Put(SmartGrid::x_baseGridSize - 1, SmartGrid::x_baseGridSize + 1, owner->MakeShiftCell());

            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                Put(i, -1, new TimerCell(&owner->m_internal->m_timer, i + SmartGrid::x_baseGridSize));
            }
        }
    };

    struct UIState
    {
        SmartGrid::SmartBusColor m_colorBus[4];
    };

    TheNonagonSquiggleBoyInternal* m_internal;

    TopLeftGrid* m_topLeftGrid;
    TopRightGrid* m_topRightGrid;
    BottomLeftGrid* m_bottomLeftGrid;
    BottomRightGrid* m_bottomRightGrid;

    TopGridsMode m_topGridsMode;
    SmartGrid::GridHolder m_gridHolder;

    SmartGrid::MessageInBus m_messageBus;
    UIState m_uiState;

    SmartGrid::Cell* MakeShiftCell()
    {
        return m_internal->MakeShiftCell();
    }

    SmartGrid::Cell* MakeRunningCell()
    {
        return m_internal->MakeRunningCell();
    }

    SmartGrid::Cell* MakeNoiseModeCell()
    {
        return m_internal->MakeNoiseModeCell();
    }

    void SetTopGridsMode(TopGridsMode mode)
    {
        m_topGridsMode = mode;

        switch (mode)
        {
            case TopGridsMode::Matrix:
            {
                m_topLeftGrid->SetGrid(m_internal->m_nonagon.m_lameJuisMatrixGrid);
                m_topRightGrid->SetGrid(m_internal->m_nonagon.m_lameJuisRHSGrid);
                break;
            }
            case TopGridsMode::Fire:
            {
                m_topLeftGrid->SetGrid(m_internal->m_nonagon.m_sheafViewGridFireGrid);
                m_topRightGrid->SetGrid(m_internal->m_nonagon.m_indexArpFireGrid);
                m_internal->SetActiveTrio(TheNonagonSmartGrid::Trio::Fire);
                break;
            }
            case TopGridsMode::Earth:
            {
                m_topLeftGrid->SetGrid(m_internal->m_nonagon.m_sheafViewGridEarthGrid);
                m_topRightGrid->SetGrid(m_internal->m_nonagon.m_indexArpEarthGrid);
                m_internal->SetActiveTrio(TheNonagonSmartGrid::Trio::Earth);
                break;
            }
            case TopGridsMode::Water:
            {
                m_topLeftGrid->SetGrid(m_internal->m_nonagon.m_sheafViewGridWaterGrid);
                m_topRightGrid->SetGrid(m_internal->m_nonagon.m_indexArpWaterGrid);
                m_internal->SetActiveTrio(TheNonagonSmartGrid::Trio::Water);
                break;
            }
        }
    }

    enum class Routes : int
    {
        TopLeftGrid = 0,
        TopRightGrid = 1,
        BottomLeftGrid = 2,
        BottomRightGrid = 3,
        Encoder = 4,
        Param7 = 5
    };

    static constexpr size_t x_numLaunchpads = 4;
    static constexpr size_t x_numRoutes = x_numLaunchpads + 2;

    TheNonagonSquiggleBoyQuadLaunchpadTwister(TheNonagonSquiggleBoyInternal* internal)
        : m_internal(internal)
        , m_topGridsMode(TopGridsMode::Matrix)
    {
        m_topLeftGrid = new TopLeftGrid(this);
        m_topRightGrid = new TopRightGrid(this);
        m_bottomLeftGrid = new BottomLeftGrid(this);
        m_bottomRightGrid = new BottomRightGrid(this);

        SetTopGridsMode(TopGridsMode::Matrix);

        m_gridHolder.AddGrid(m_topLeftGrid);
        m_gridHolder.AddGrid(m_topRightGrid);
        m_gridHolder.AddGrid(m_bottomLeftGrid);
        m_gridHolder.AddGrid(m_bottomRightGrid);

        m_messageBus.SetRouteType(static_cast<int>(Routes::TopLeftGrid), SmartGrid::MidiToMessageIn::RouteType::LaunchPad);
        m_messageBus.SetRouteType(static_cast<int>(Routes::TopRightGrid), SmartGrid::MidiToMessageIn::RouteType::LaunchPad);
        m_messageBus.SetRouteType(static_cast<int>(Routes::BottomLeftGrid), SmartGrid::MidiToMessageIn::RouteType::LaunchPad);
        m_messageBus.SetRouteType(static_cast<int>(Routes::BottomRightGrid), SmartGrid::MidiToMessageIn::RouteType::LaunchPad);
        m_messageBus.SetRouteType(static_cast<int>(Routes::Encoder), SmartGrid::MidiToMessageIn::RouteType::Encoder);
        m_messageBus.SetRouteType(static_cast<int>(Routes::Param7), SmartGrid::MidiToMessageIn::RouteType::Param7);
    }

    void WriteUIState(UIState& uiState)
    {
        m_topLeftGrid->OutputToBus(&uiState.m_colorBus[static_cast<int>(Routes::TopLeftGrid)]);
        m_topRightGrid->OutputToBus(&uiState.m_colorBus[static_cast<int>(Routes::TopRightGrid)]);
        m_bottomLeftGrid->OutputToBus(&uiState.m_colorBus[static_cast<int>(Routes::BottomLeftGrid)]);
        m_bottomRightGrid->OutputToBus(&uiState.m_colorBus[static_cast<int>(Routes::BottomRightGrid)]);
    }

    void ProcessMessages()
    {
        m_messageBus.ProcessMessages(this, SampleTimer::GetAbsTimeUs());
    }

    void SendMessage(SmartGrid::MessageIn msg)
    {
        m_messageBus.Push(msg);
    }

    void SendMessage(SmartGrid::BasicMidi msg)
    {
        m_messageBus.Push(msg);
    }

    void Apply(SmartGrid::MessageIn msg)
    {
        Routes route = static_cast<Routes>(msg.m_routeId);
        switch (route)
        {
            case Routes::TopLeftGrid:
            {
                m_topLeftGrid->Apply(msg);
                break;
            }
            case Routes::TopRightGrid:
            {
                m_topRightGrid->Apply(msg);
                break;
            }
            case Routes::BottomLeftGrid:
            {
                m_bottomLeftGrid->Apply(msg);
                break;
            }
            case Routes::BottomRightGrid:
            {
                m_bottomRightGrid->Apply(msg);
                break;
            }
            case Routes::Encoder:
            case Routes::Param7:
            {
                m_internal->Apply(msg);
                break;
            }
        }
    }

    void ProcessSample()
    {
        m_gridHolder.Process(1.0 / 48000.0);
        ProcessMessages();
    }

    void ProcessFrame()
    {
        WriteUIState(m_uiState);
    }

    PadUIGrid MkLaunchPadUIGrid(int routeId)
    {
        return PadUIGrid(routeId, &m_uiState.m_colorBus[routeId], &m_messageBus);
    }
 };