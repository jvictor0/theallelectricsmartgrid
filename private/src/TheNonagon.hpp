#pragma once
#include "SmartGrid.hpp"
#include "TheoryOfTime.hpp"
#include "LameJuis.hpp"
#include "plugin.hpp"
#include "StateSaver.hpp"

struct TheNonagonInternal
{
    static constexpr size_t x_numTimeBits = 6;
    struct Input
    {
        MusicalTimeWithClock::Input m_theoryOfTimeInput;
        LameJuisInternal::Input m_lameJuisInput;
    };
    
    MusicalTimeWithClock m_theoryOfTime;
    LameJuisInternal m_lameJuis;

    struct TimeBit
    {
        bool m_bit;
    };

    TimeBit m_time[x_numTimeBits];

    bool* TimeBit(size_t i)
    {
        return &m_time[i].m_bit;
    }

    void SetTimeBits(Input& input)
    {
        for (size_t i = 0; i < x_numTimeBits; ++i)
        {
            m_time[i].m_bit = m_theoryOfTime.m_musicalTime.m_gate[x_numTimeBits - i];
            input.m_lameJuisInput.m_inputBitInput[i].m_value = m_time[i].m_bit;
        }
    }

    bool* OutBit(size_t i)
    {
        return &m_lameJuis.m_operations[i].m_gate;
    }

    void Process(float dt, Input& input)
    {
        m_theoryOfTime.Process(dt, input.m_theoryOfTimeInput);
        if (m_theoryOfTime.m_musicalTime.m_change)
        {
            SetTimeBits(input);
            m_lameJuis.Process(input.m_lameJuisInput);
        }                
    }
};

struct TheNonagonSmartGrid
{
    TheNonagonInternal::Input m_state;
    TheNonagonInternal m_nonagon;
    SmartGrid::GridUnion m_grids;
    SmartGrid::SmartGrid m_smartGrid;

    StateSaver m_stateSaver;

    json_t* ToJSON()
    {
        return m_stateSaver.ToJSON();
    }

    void SetFromJSON(json_t* jin)
    {
        m_stateSaver.SetFromJSON(jin);
    }
    
    SmartGrid::Cell* TimeBitCell(size_t ix)
    {
        return new SmartGrid::StateCell<bool>(
                        SmartGrid::Color::Off /*offColor*/,
                        SmartGrid::Color::White /*onColor*/,
                        m_nonagon.TimeBit(ix),
                        true,
                        false,
                        SmartGrid::StateCell<bool>::Mode::ShowOnly);
    }

    SmartGrid::Cell* OutBitCell(size_t ix)
    {
        return new SmartGrid::StateCell<bool>(
                        SmartGrid::Color::Off /*offColor*/,
                        SmartGrid::Color::White /*onColor*/,
                        m_nonagon.OutBit(ix),
                        true,
                        false,
                        SmartGrid::StateCell<bool>::Mode::ShowOnly);
    }    
    
    enum class Trio : int
    {
        Fire = 0,
        Earth = 1,
        Water = 2,
        NumTrios = 3
    };

    static SmartGrid::Color TrioColor(Trio t)
    {
        switch (t)
        {
            case Trio::Fire: return SmartGrid::Color::Red;
            case Trio::Earth: return SmartGrid::Color::Green;
            case Trio::Water: return SmartGrid::Color::Blue;
            case Trio::NumTrios: return SmartGrid::Color::Dim;
        }
    }
    
    struct TheoryOfTimeTopologyPage : public SmartGrid::CompositeGrid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;
        TheoryOfTimeTopologyPage(TheNonagonSmartGrid* owner)
            : SmartGrid::CompositeGrid(SmartGrid::x_gridSize, SmartGrid::x_gridSize)
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
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

                    Put(xPos, 1, new SmartGrid::StateCell<bool>(
                            SmartGrid::Color::Ocean /*offColor*/,
                            SmartGrid::Color::White /*onColor*/,
                            &m_state->m_theoryOfTimeInput.m_input.m_input[i].m_pingPong,
                            true,
                            false,
                            SmartGrid::StateCell<bool>::Mode::Toggle));
                    
                    m_owner->m_stateSaver.Insert(
                        "TheoryOfTimeMult", i, &m_state->m_theoryOfTimeInput.m_input.m_input[i].m_mult);
                    m_owner->m_stateSaver.Insert(
                        "TheoryOfTimePingPong", i, &m_state->m_theoryOfTimeInput.m_input.m_input[i].m_pingPong);
                }

                if (2 < i)
                {
                    Put(xPos, 3, new SmartGrid::StateCell<size_t>(
                            SmartGrid::Color::Ocean /*offColor*/,
                            SmartGrid::Color::White /*onColor*/,
                            &m_state->m_theoryOfTimeInput.m_input.m_input[i].m_parentIx,
                            i - 2,
                            i - 1,
                            SmartGrid::StateCell<size_t>::Mode::Toggle));
                    m_owner->m_stateSaver.Insert(
                        "TheoryOfTimeParentIx", i, &m_state->m_theoryOfTimeInput.m_input.m_input[i].m_parentIx);
                }

                Put(xPos, 0, m_owner->TimeBitCell(TheNonagonInternal::x_numTimeBits - i));
            }

            AddGrid(0, 0, new SmartGrid::Fader(
                        &m_state->m_theoryOfTimeInput.m_freq,
                        SmartGrid::x_gridSize,
                        SmartGrid::ColorScheme::Whites,
                        1.0/64 /*minHz*/,
                        2.0 /*maxHz*/,
                        0.1 /*minChangeSpeed*/,
                        100  /*maxChangeSpeed*/,
                        true /*pressureSensitive*/,
                        SmartGrid::Fader::Structure::Exponential));

            m_owner->m_stateSaver.Insert(
                "TheoryOfTimeFreq", &m_state->m_theoryOfTimeInput.m_freq);
        }        
    };

    struct LameJuisCoMutePage : public SmartGrid::Grid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;        
        LameJuisCoMutePage(TheNonagonSmartGrid* owner)
            : SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize)
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
        {
            InitGrid();
        }
        
        void InitGrid()
        {
            for (size_t i = 0; i < TheNonagonInternal::x_numTimeBits; ++i)
            {
                Put(i, 0, m_owner->TimeBitCell(i));
                for (Trio t : {Trio::Fire, Trio::Earth, Trio::Water})
                {
                    size_t tId = static_cast<size_t>(t);
                    size_t coMuteY = 3 + tId * 2;
                    Put(i, coMuteY, new SmartGrid::StateCell<bool>(
                            TrioColor(t),
                            SmartGrid::Color::White /*onColor*/,
                            &m_state->m_lameJuisInput.m_outputInput[tId].m_coMuteInput.m_coMutes[i],
                            true,
                            false,
                            SmartGrid::StateCell<bool>::Mode::Toggle));
                    m_owner->m_stateSaver.Insert(
                        "LameJuisCoMute", i, tId, &m_state->m_lameJuisInput.m_outputInput[tId].m_coMuteInput.m_coMutes[i]);
                }
            }
        }
    };

    static SmartGrid::Color EquationColor(size_t i)
    {
        if (i == 0)
        {
            return SmartGrid::Color::Ocean;
        }
        else if (i == 1)
        {
            return SmartGrid::Color::Magenta;
        }
        else
        {
            return SmartGrid::Color::SeaGreen;
        }
    }
    
    SmartGrid::Cell* EquationOutputSwitch(size_t i)
    {
        SmartGrid::ColorScheme colorScheme(std::vector<SmartGrid::Color>({
                    EquationColor(0), EquationColor(1), EquationColor(2)}));
        return new SmartGrid::CycleCell<LameJuisInternal::LogicOperation::SwitchVal>(
            colorScheme,
            &m_state.m_lameJuisInput.m_operationInput[i].m_switch);
    }
    
    struct LameJuisMatrixPage : public SmartGrid::Grid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;        
        LameJuisMatrixPage(TheNonagonSmartGrid* owner)
            : SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize)
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
        {
            InitGrid();
        }        
        
        void InitGrid()
        {
            SmartGrid::ColorScheme colorScheme(std::vector<SmartGrid::Color>({
                        SmartGrid::Color::Yellow, SmartGrid::Color::Dim, SmartGrid::Color::Fuscia}));
            for (size_t i = 0; i < TheNonagonInternal::x_numTimeBits; ++i)
            {
                Put(i, 1, m_owner->TimeBitCell(i));

                for (size_t j = 0; j < LameJuisInternal::x_numOperations; ++j)
                {
                    Put(i, j + 2, new SmartGrid::CycleCell<LameJuisInternal::MatrixSwitch>(
                            colorScheme,
                            &m_state->m_lameJuisInput.m_operationInput[j].m_elements[i]));
                    m_owner->m_stateSaver.Insert(
                        "LameJuisMatrixSwitch", i, j, &m_state->m_lameJuisInput.m_operationInput[j].m_elements[i]);
                }

                Put(6, i + 2, m_owner->OutBitCell(i));
                Put(7, i + 2, m_owner->EquationOutputSwitch(i));
                m_owner->m_stateSaver.Insert(
                    "LameJuisEquationOutputSwitch", i, &m_state->m_lameJuisInput.m_operationInput[i].m_switch);
            }
        }
    };

    struct LameJuisLHSPage : public SmartGrid::Grid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;        
        LameJuisLHSPage(TheNonagonSmartGrid* owner)
            : SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize)
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
        {
            InitGrid();
        }        
        
        void InitGrid()
        {
            for (size_t i = 0; i < LameJuisInternal::x_numOperations; ++i)
            {
                for (size_t j = 0; j < TheNonagonInternal::x_numTimeBits + 1; ++j)
                {
                    Put(j, i + 2, new SmartGrid::StateCell<bool, SmartGrid::Flash<size_t>>(
                            SmartGrid::Color::Dim,
                            SmartGrid::Color::DimPurple,
                            SmartGrid::Color::Yellow,
                            SmartGrid::Color::Purple,
                            &m_state->m_lameJuisInput.m_operationInput[i].m_direct[j],
                            SmartGrid::Flash<size_t>(&m_nonagon->m_lameJuis.m_operations[i].m_countHigh, j),
                            true,
                            false,
                            SmartGrid::StateCell<bool, SmartGrid::Flash<size_t>>::Mode::Toggle));
                    m_owner->m_stateSaver.Insert(
                        "LameJuisLHS", i, j, &m_state->m_lameJuisInput.m_operationInput[i].m_direct[j]);
                }

                Put(7, i + 2, m_owner->EquationOutputSwitch(i));                
            }
        }
    };

    struct LameJuisIntervalPage : public SmartGrid::Grid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;        
        LameJuisIntervalPage(TheNonagonSmartGrid* owner)
            : SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize)
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
        {
            InitGrid();
        }        
        
        void InitGrid()
        {
            for (size_t i = 0; i < LameJuisInternal::x_numAccumulators; ++i)
            {
                for (size_t j = 0; j < static_cast<size_t>(LameJuisInternal::Accumulator::Interval::NumIntervals); ++j)
                {
                    size_t xPos = 2 * i + (j == 0 || 6 < j ? 1 : 0);
                    size_t yPos = j == 0 ? 2 : (7 - (j - 1) % 6);
                    Put(xPos, yPos, new SmartGrid::StateCell<LameJuisInternal::Accumulator::Interval>(
                            EquationColor(i),
                            SmartGrid::Color::White,
                            &m_state->m_lameJuisInput.m_accumulatorInput[i].m_interval,
                            static_cast<LameJuisInternal::Accumulator::Interval>(j),
                            LameJuisInternal::Accumulator::Interval::Off,
                            SmartGrid::StateCell<LameJuisInternal::Accumulator::Interval>::Mode::SetOnly));
                    m_owner->m_stateSaver.Insert(
                        "LameJuisInterval", i, &m_state->m_lameJuisInput.m_accumulatorInput[i].m_interval);
                }
            }
        }
    };
    
    Trio m_activeTrio;
    
    struct Menu : public SmartGrid::Grid
    {
        virtual ~Menu()
        {
        }
        
        enum class Page : size_t
        {
            TheoryOfTime = 0,
            LaMeJuIS = 1,
            Sequencer = 2,
            Articulation = 3,
            Faders = 4,
            NumPages = 5
        };

        struct SubPageCell : public SmartGrid::Cell
        {
            virtual ~SubPageCell()
            {
            }

            SubPageCell(
                Menu* owner,
                size_t subPage)
                : m_owner(owner)
                , m_subPage(subPage)
            {
            }

            virtual void OnPress(uint8_t) override
            {
                m_owner->SetSubPage(m_subPage);
            }

            virtual SmartGrid::Color GetColor() override
            {
                if (m_owner->SubPage() == m_subPage)
                {
                    return SmartGrid::Color::White;
                }
                else
                {
                    return m_owner->GetSubPageColor(m_subPage);
                }
            }
            
            Menu* m_owner;
            size_t m_subPage;
        };

        struct PageCell : public SmartGrid::Cell
        {
            virtual ~PageCell()
            {
            }

            PageCell(
                Menu* owner,
                Page page)
                : m_owner(owner)
                , m_page(page)
            {
            }

            virtual void OnPress(uint8_t) override
            {
                m_owner->SetPage(m_page);
            }

            virtual SmartGrid::Color GetColor() override
            {
                if (m_owner->GetPage() == m_page)
                {
                    return SmartGrid::Color::White;
                }
                else
                {
                    return SmartGrid::Color::Dim;
                }
            }
            
            Menu* m_owner;
            Page m_page;
        };
            
        Menu(Trio* activeTrio, size_t ix, SmartGrid::GridUnion* owner, TheNonagonSmartGrid* smartGrid)
            : Grid(
                SmartGrid::x_gridSize - x_subPages,
                SmartGrid::x_gridSize - static_cast<size_t>(Page::NumPages),
                SmartGrid::x_gridSize + 1,
                SmartGrid::x_gridSize + 1)
            , m_currentPage(&owner->m_pageIx[ix])
            , m_activeTrio(activeTrio)
            , m_owner(owner)
        {
            for (size_t i = 0; i < static_cast<size_t>(Page::NumPages); ++i)
            {
                m_subPage[i] = 0;
                smartGrid->m_stateSaver.Insert("SubPage", ix, i, &m_subPage[i]);
            }

            for (size_t i = 0; i < x_subPages; ++i)
            {
                Put(SmartGrid::x_gridSize - x_subPages + i, SmartGrid::x_gridSize, new SubPageCell(this, i));                    
            }

            for (size_t i = 0; i < static_cast<size_t>(Page::NumPages); ++i)
            {
                Put(SmartGrid::x_gridSize, SmartGrid::x_gridSize - 1 - i, new PageCell(this, static_cast<Page>(i)));
            }

            smartGrid->m_stateSaver.Insert("CurPage", ix, m_currentPage);
        }
        
        static constexpr size_t x_subPages = 4;
        
        static size_t PageId(Page p, size_t subPage)
        {
            return x_subPages * static_cast<size_t>(p) + subPage;
        }

        SmartGrid::Color GetSubPageColor(size_t subPage)
        {
            if (ActiveTrioAffected(GetPage(), subPage))
            {
                return TrioColor(static_cast<Trio>(subPage));
            }
            else
            {
                return SmartGrid::Color::Dim;
            }
        }
        
        size_t SubPage(Page p)
        {
            return m_subPage[static_cast<size_t>(GetPage())];
        }

        size_t SubPage()
        {
            return SubPage(GetPage());
        }

        Page GetPage()
        {
            return static_cast<Page>(*m_currentPage / x_subPages);
        }
        
        size_t PageId()
        {
            return PageId(GetPage(), SubPage(GetPage()));
        }
        
        bool ActiveTrioAffected(Page p, size_t subPage)
        {
            switch (p)
            {
                case Page::TheoryOfTime:
                case Page::LaMeJuIS:
                case Page::Faders:
                {
                    return false;
                }
                case Page::Sequencer:
                case Page::Articulation:
                {
                    return subPage < static_cast<size_t>(Trio::NumTrios);
                }
                default:
                {
                    return false;
                }
            }
        }

        void SetActiveTrio()
        {
            for (Page p : {Page::Sequencer, Page::Articulation})
            {
                if (m_subPage[static_cast<size_t>(p)] < static_cast<size_t>(Trio::NumTrios))
                {
                    m_subPage[static_cast<size_t>(p)] = static_cast<size_t>(*m_activeTrio);
                }
            }
        }

        void SetPage(Page newPage)
        {
            *m_currentPage = PageId(newPage, SubPage(newPage));
        }
        
        void SetSubPage(size_t subPage)
        {
            m_subPage[static_cast<size_t>(GetPage())] = subPage;
            if (ActiveTrioAffected(GetPage(), subPage))
            {
                *m_activeTrio = static_cast<Trio>(subPage);

                for (std::shared_ptr<SmartGrid::Grid>& grid : m_owner->m_menuGrids)
                {
                    static_cast<Menu*>(grid.get())->SetActiveTrio();
                }
            }

            *m_currentPage = PageId();
        }

        size_t m_subPage[static_cast<size_t>(Page::NumPages)];
        size_t* m_currentPage;
        Trio* m_activeTrio;
        SmartGrid::GridUnion* m_owner;
    };

    TheNonagonSmartGrid()
        : m_smartGrid(&m_grids)
    {
        InitState();
        InitGrid();
    }

    void InitState()
    {
        for (size_t i = 0; i < LameJuisInternal::x_numInputs; ++i)
        {
            m_state.m_lameJuisInput.m_inputBitInput[i].m_connected = true;
        }
        
        for (size_t i = 0; i < LameJuisInternal::x_numOperations; ++i)
        {
            m_state.m_lameJuisInput.m_operationInput[i].m_operator = LameJuisInternal::LogicOperation::Operator::Direct;
        }
    }
    
    void InitGrid()
    {
        // TheoryOfTime
        //
        m_grids.AddGrid(new TheoryOfTimeTopologyPage(this));
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));

        // LaMeJuIS
        //
        m_grids.AddGrid(new LameJuisCoMutePage(this));
        m_grids.AddGrid(new LameJuisMatrixPage(this));
        m_grids.AddGrid(new LameJuisLHSPage(this));
        m_grids.AddGrid(new LameJuisIntervalPage(this));

        // Sequencers
        //
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));
        
        // Articulation
        //
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));

        // Faders
        //
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));
        m_grids.AddGrid(new SmartGrid::Grid(SmartGrid::x_gridSize, SmartGrid::x_gridSize));
        
        m_grids.AddMenu(new Menu(&m_activeTrio, 0, &m_grids, this));
        m_stateSaver.Insert("ActiveTrio", &m_activeTrio);

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

    json_t* dataToJson() override
    {
        return m_nonagon.ToJSON();
    }

    void dataFromJson(json_t* rootJ) override
    {
        m_nonagon.SetFromJSON(rootJ);
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
    
