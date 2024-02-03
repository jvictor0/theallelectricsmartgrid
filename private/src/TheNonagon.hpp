#pragma once
#include "SmartGrid.hpp"
#include "TheoryOfTime.hpp"
#include "plugin.hpp"

struct TheNonagonInternal
{
    static constexpr size_t x_numTimeBits = 6;
    struct Input
    {
        MusicalTimeWithClock::Input m_theoryOfTimeInput;
    };
    
    MusicalTimeWithClock m_theoryOfTime;

    void Process(float dt, Input& input)
    {
        m_theoryOfTime.Process(dt, input.m_theoryOfTimeInput);
    }
};

struct TheNonagonSmartGrid
{
    TheNonagonInternal::Input m_state;
    TheNonagonInternal m_nonagon;
    SmartGrid::GridUnion m_grids;
    SmartGrid::SmartGrid m_smartGrid;

    struct TheoryOfTimeTopologyPage : public SmartGrid::CompositeGrid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheoryOfTimeTopologyPage(TheNonagonSmartGrid* owner)
            : SmartGrid::CompositeGrid(SmartGrid::x_gridSize, SmartGrid::x_gridSize)
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
        {
            InitGrid();
        }
        
        void InitGrid()
        {
            for (size_t i = 1; i < 1 + TheNonagonInternal::x_numTimeBits; ++i)
            {
                size_t xPos = SmartGrid::x_gridSize - i - 1;
                if (i != 1)
                {
                    for (size_t mult = 2; mult <= 5; ++mult)
                    {
                        Put(
                            xPos,
                            SmartGrid::x_gridSize - mult + 1,
                            new SmartGrid::StateCell<size_t>(
                                SmartGrid::Color::Fuscia /*offColor*/,
                                SmartGrid::Color::White /*onColor*/,
                                &m_state->m_theoryOfTimeInput.m_input.m_input[i].m_mult,
                                mult,
                                1,
                                SmartGrid::StateCell<size_t>::Mode::Toggle));
                    }

                    Put(
                        xPos,
                        1,
                        new SmartGrid::StateCell<bool>(
                            SmartGrid::Color::Ocean /*offColor*/,
                            SmartGrid::Color::White /*onColor*/,
                            &m_state->m_theoryOfTimeInput.m_input.m_input[i].m_pingPong,
                            true,
                            false,
                            SmartGrid::StateCell<bool>::Mode::Toggle));                    
                }

                if (2 < i)
                {
                    Put(
                        xPos,
                        3,
                        new SmartGrid::StateCell<size_t>(
                            SmartGrid::Color::Ocean /*offColor*/,
                            SmartGrid::Color::White /*onColor*/,
                            &m_state->m_theoryOfTimeInput.m_input.m_input[i].m_parentIx,
                            i - 2,
                            i - 1,
                            SmartGrid::StateCell<size_t>::Mode::Toggle));
                }

                Put(
                    xPos,
                    0,
                    new SmartGrid::StateCell<bool>(
                        SmartGrid::Color::Off /*offColor*/,
                        SmartGrid::Color::White /*onColor*/,
                        &m_nonagon->m_theoryOfTime.m_musicalTime.m_gate[i],
                        true,
                        false,
                        SmartGrid::StateCell<bool>::Mode::ShowOnly));

            }
        }        
    };

    TheNonagonSmartGrid()
        : m_smartGrid(&m_grids)
    {
        InitGrid();
    }
    
    void InitGrid()
    {
        m_grids.AddGrid(new TheoryOfTimeTopologyPage(this));
        m_grids.AddMenu(new SmartGrid::Grid());

        m_smartGrid.m_state.m_midiSwitchInput.m_numChannels = 1;
        m_smartGrid.m_state.m_midiSwitchInput.m_channels[0] = 0;
    }

    void Process(float dt, uint64_t frame)
    {
        m_smartGrid.BeforeModuleProcess(dt, frame);
        m_nonagon.Process(dt, m_state);
        m_smartGrid.AfterModuleProcess(frame);
    }
};

struct TheNonagon : Module
{
    TheNonagonSmartGrid m_nonagon;

    TheNonagon()
    {
        config(0, 0, 0, 0);
    }

    void process(const ProcessArgs &args) override
    {
        m_nonagon.Process(args.sampleTime, args.frame);
    }
};

struct TheNonagonWidget : ModuleWidget
{
    TheNonagonWidget(TheNonagon* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/TheNonagon.svg")));
    }
};
    
