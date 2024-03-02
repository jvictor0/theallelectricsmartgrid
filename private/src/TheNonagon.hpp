#pragma once
#include "SmartGrid.hpp"
#include "GridJnct.hpp"
#include "TheoryOfTime.hpp"
#include "LameJuis.hpp"
#include "plugin.hpp"
#include "StateSaver.hpp"
#include "TimbreAndMutes.hpp"
#include "MultiPhasorGate.hpp"
#include "PercentileSequencer.hpp"
#include "TrioOctaveSwitches.hpp"

struct TheNonagonInternal
{
    static constexpr size_t x_numTimeBits = 6;
    static constexpr size_t x_numVoices = 9;
    static constexpr size_t x_numTrios = 3;
    static constexpr size_t x_voicesPerTrio = 3;
    static constexpr size_t x_numAuxFaders = 16;

    struct Input
    {
        MusicalTimeWithClock::Input m_theoryOfTimeInput;
        LameJuisInternal::Input m_lameJuisInput;
        TimbreAndMute::Input m_timbreAndMuteInput;
        MultiPhasorGateInternal::Input m_multiPhasorGateInput;
        PercentileSequencerInternal::Input m_percentileSequencerInput;
        TrioOctaveSwitches::Input m_trioOctaveSwitchesInput;
        bool m_mute[x_numVoices];
        bool m_running;
        bool m_recording;
        
        float m_auxFaders[x_numAuxFaders];

        Input()
            : m_running(false)
            , m_recording(false)
        {
            for (size_t i = 0; i < x_numAuxFaders; ++i)
            {
                m_auxFaders[i] = 0;
            }

            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_mute[i] = false;
            }
        }
    };
    
    MusicalTimeWithClock m_theoryOfTime;
    LameJuisInternal m_lameJuis;
    TimbreAndMute m_timbreAndMute;
    MultiPhasorGateInternal m_multiPhasorGate;
    PercentileSequencerInternal m_percentileSequencer;
    bool m_muted[x_numVoices];

    TheNonagonInternal()
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_muted[i] = false;
        }
    }
    
    struct Output
    {
        bool m_gate[x_numVoices];
        float m_phasor[x_numVoices];
        float m_voltPerOct[x_numVoices];
        float m_timbre[x_numVoices];
        float m_auxFaders[x_numAuxFaders];
        bool m_recording;

        Output()
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_gate[i] = false;
                m_voltPerOct[i] = 0;
                m_timbre[i] = 0;
                m_phasor[i] = 0;
            }

            m_recording = false;
        }
    };

    Output m_output;

    struct TimeBit
    {
        bool m_bit;
    };

    TimeBit m_time[x_numTimeBits];

    bool* TimeBit(size_t i)
    {
        return &m_time[i].m_bit;
    }

    void SetTheoryOfTimeInput(Input& input)
    {
        input.m_theoryOfTimeInput.m_input.m_running = input.m_running;
    }

    void SetLameJuisInput(Input& input)
    {
        for (size_t i = 0; i < x_numTimeBits; ++i)
        {
            m_time[i].m_bit = m_theoryOfTime.m_musicalTime.m_gate[x_numTimeBits - i];
            input.m_lameJuisInput.m_inputBitInput[i].m_value = m_time[i].m_bit;
        }

        for (size_t i = 0; i < x_numTrios; ++i)
        {
            for (size_t j = 0; j < x_voicesPerTrio; ++j)
            {
                float percentile = m_percentileSequencer.GetVal(i, j);
                input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_percentiles[j] = percentile;
            }
        }
    }

    bool* OutBit(size_t i)
    {
        return &m_lameJuis.m_operations[i].m_gate;
    }

    void SetTimbreAndMuteInputs(Input& input)
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            bool trig = m_lameJuis.m_outputs[i / x_voicesPerTrio].m_trigger[i % x_voicesPerTrio];
            input.m_timbreAndMuteInput.m_input[i].m_trigIn = trig;
        }

        for (size_t i = 0; i < LameJuisInternal::x_numOperations; ++i)
        {
            input.m_timbreAndMuteInput.m_on[i] = m_lameJuis.m_operations[i].m_gate;
        }
    }

    void SetMultiPhasorGateInputs(Input& input)
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            input.m_multiPhasorGateInput.m_trigs[i] = m_theoryOfTime.m_musicalTime.m_anyChange && m_timbreAndMute.m_voices[i].m_trig;
        }

        for (size_t i = 0; i < x_numTimeBits + 1; ++i)
        {
            input.m_multiPhasorGateInput.m_phasors[i] = m_theoryOfTime.m_musicalTime.m_bits[x_numTimeBits - i].m_pos;
            if (i < x_numTimeBits)
            {
                for (size_t j = 0; j < x_numVoices; ++j)
                {                    
                    bool coMute = m_lameJuis.m_outputs[j / x_voicesPerTrio].m_coMuteState.m_coMutes[i];
                    input.m_multiPhasorGateInput.m_phasorSelector[j][i] = !coMute;
                }
            }
        }

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            int voiceClock = input.m_percentileSequencerInput.m_clockSelect[i / x_voicesPerTrio];
            input.m_multiPhasorGateInput.m_phasorSelector[i][voiceClock] = true;  
        }
    }

    void SetPercentileSequencerInputs(Input& input)
    {
        for (size_t i = 0; i < x_numTimeBits + 1; ++i)
        {            
            input.m_percentileSequencerInput.m_clocks[i] = m_theoryOfTime.m_musicalTime.m_change[x_numTimeBits - i];
        }
    }    

    void SetOutputs(Input& input)
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            if (m_output.m_gate[i] != m_multiPhasorGate.m_gate[i])
            {                                
                if (m_multiPhasorGate.m_gate[i])
                {
                    if (!input.m_mute[i])
                    {
                        m_output.m_gate[i] = true;
                        m_muted[i] = false;
                        float preOctave = m_lameJuis.m_outputs[i / x_voicesPerTrio].m_pitch[i % x_voicesPerTrio].m_pitch;
                        m_output.m_voltPerOct[i] = input.m_trioOctaveSwitchesInput.Octavize(preOctave, i);
                        m_output.m_timbre[i] = m_timbreAndMute.m_voices[i].m_timbre;
                    }
                    else
                    {
                        m_muted[i] = true;
                    }
                }
                else
                {
                    m_output.m_gate[i] = false;
                }
            }

            m_output.m_phasor[i] = m_muted[i] ? 0 : m_multiPhasorGate.m_phasorOut[i];
        }

        for (size_t i = 0; i < x_numAuxFaders; ++i)
        {
            m_output.m_auxFaders[i] = input.m_auxFaders[i];
        }

        m_output.m_recording = input.m_recording;
    }

    void Process(float dt, Input& input)
    {
        SetTheoryOfTimeInput(input);
        m_theoryOfTime.Process(dt, input.m_theoryOfTimeInput);
        if (m_theoryOfTime.m_musicalTime.m_anyChange)
        {
            SetPercentileSequencerInputs(input);
            m_percentileSequencer.Process(input.m_percentileSequencerInput);
            
            SetLameJuisInput(input);
            m_lameJuis.Process(input.m_lameJuisInput);
            
            SetTimbreAndMuteInputs(input);
            m_timbreAndMute.Process(input.m_timbreAndMuteInput);
        }

        SetMultiPhasorGateInputs(input);
        m_multiPhasorGate.Process(input.m_multiPhasorGateInput);
        SetOutputs(input);
    }
};

struct TheNonagonSmartGrid
{
    TheNonagonInternal::Input m_state;
    TheNonagonInternal m_nonagon;
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

    SmartGrid::Cell* StartStopCell()
    {
        return new SmartGrid::StateCell<bool>(
                        SmartGrid::Color::White /*offColor*/,
                        SmartGrid::Color::Green /*onColor*/,
                        &m_state.m_running,
                        true,
                        false,
                        SmartGrid::StateCell<bool>::Mode::Toggle);
    }

    SmartGrid::Cell* RecordingCell()
    {
        return new SmartGrid::StateCell<bool>(
                        SmartGrid::Color::White /*offColor*/,
                        SmartGrid::Color::Red /*onColor*/,
                        &m_state.m_recording,
                        true,
                        false,
                        SmartGrid::StateCell<bool>::Mode::Toggle);
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

    static SmartGrid::Color TrioCompanionColor(Trio t)
    {
        switch (t)
        {
            case Trio::Fire: return SmartGrid::Color::Orange;
            case Trio::Earth: return SmartGrid::Color::Yellow;
            case Trio::Water: return SmartGrid::Color::Indigo;
            case Trio::NumTrios: return SmartGrid::Color::Dim;
        }
    }

    static SmartGrid::Color TrioCompanionDimColor(Trio t)
    {
        switch (t)
        {
            case Trio::Fire: return SmartGrid::Color::DimOrange;
            case Trio::Earth: return SmartGrid::Color::DimYellow;
            case Trio::Water: return SmartGrid::Color::DimIndigo;
            case Trio::NumTrios: return SmartGrid::Color::Dim;
        }
    }

    static SmartGrid::ColorScheme TrioColorScheme(Trio t)
    {
        switch (t)
        {
            case Trio::Fire: return SmartGrid::ColorScheme::Reds;
            case Trio::Earth: return SmartGrid::ColorScheme::Greens;
            case Trio::Water: return SmartGrid::ColorScheme::Blues;
            case Trio::NumTrios: return SmartGrid::ColorScheme::Whites;
        }
    }

    static SmartGrid::Color VoiceColor(Trio t, size_t relVoice)
    {
        return TrioColorScheme(t)[relVoice];
    }

    static SmartGrid::Color VoiceColor(size_t absVoice)
    {
        return VoiceColor(static_cast<Trio>(absVoice / TheNonagonInternal::x_voicesPerTrio), absVoice % TheNonagonInternal::x_voicesPerTrio);
    }

    size_t m_numPercentileSeqClockSelectsHeld[TheNonagonInternal::x_numVoices];
    
    SmartGrid::Cell* PercentileSeqClockSelectCell(Trio trio, size_t clockIx)
    {
        size_t trioIx = static_cast<size_t>(trio);
        return new PercentileSequencerClockSelectCell(
            &m_state.m_percentileSequencerInput,
            trioIx,
            &m_numPercentileSeqClockSelectsHeld[trioIx],
            clockIx,
            SmartGrid::ColorScheme::Hues(TrioColor(trio))[0],
            SmartGrid::Color::White,
            SmartGrid::Color::Grey);
    }

    void PlaceMutes(Trio t, int x, int y, SmartGrid::Grid* grid)
    {
        for (size_t i = 0; i < TheNonagonInternal::x_voicesPerTrio; ++i)
        {
            size_t voiceIx = static_cast<size_t>(t) * TheNonagonInternal::x_voicesPerTrio + i;
            int xPos = (i == 0) ? x : x + 1;
            int yPos = (i < 2) ? y + 1 : y;
            grid->Put(xPos, yPos, new SmartGrid::StateCell<bool, SmartGrid::BoolFlash>(
                          SmartGrid::Color::Dim,
                          VoiceColor(t, i),
                          SmartGrid::Color::Grey,
                          TrioColor(t),
                          &m_state.m_mute[static_cast<size_t>(t) * TheNonagonInternal::x_voicesPerTrio + i],
                          SmartGrid::BoolFlash(&m_nonagon.m_multiPhasorGate.m_gate[voiceIx]),
                          false,
                          true,
                          SmartGrid::StateCell<bool, SmartGrid::BoolFlash>::Mode::Toggle));
        }
    }
    
    struct TheoryOfTimeTopologyPage : public SmartGrid::CompositeGrid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;
        TheoryOfTimeTopologyPage(TheNonagonSmartGrid* owner)
            : SmartGrid::CompositeGrid()
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
                        1.0/128 /*minHz*/,
                        8.0 /*maxHz*/,
                        0.02 /*minChangeSpeed*/,
                        100  /*maxChangeSpeed*/,
                        true /*pressureSensitive*/,
                        SmartGrid::Fader::Structure::Exponential));

            m_owner->m_stateSaver.Insert(
                "TheoryOfTimeFreq", &m_state->m_theoryOfTimeInput.m_freq);

            for (Trio t : {Trio::Fire, Trio::Earth, Trio::Water})
            {
                size_t muteY = 6 - static_cast<size_t>(t) * 2;
                m_owner->PlaceMutes(t, 6, muteY, this);
            }
            
            Put(6, 1, m_owner->StartStopCell());
            Put(7, 1, m_owner->RecordingCell());
        }        
    };

    struct TheoryOfTimeSwingAndSwaggerPage : public SmartGrid::CompositeGrid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;
        bool m_swing;
        TheoryOfTimeSwingAndSwaggerPage(TheNonagonSmartGrid* owner, bool swing)
            : SmartGrid::CompositeGrid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
            , m_swing(swing)
        {
            if (m_swing)
            {
                SetColors(SmartGrid::Color::Orange, SmartGrid::Color::DimOrange);
            }
            else
            {
                SetColors(SmartGrid::Color::Yellow, SmartGrid::Color::DimYellow);
            }
            
            InitGrid();
        }
        
        void InitGrid()
        {
            AddGrid(0, 0, new SmartGrid::Fader(
                        &m_state->m_theoryOfTimeInput.m_input.m_rand,
                        SmartGrid::x_gridSize,
                        SmartGrid::ColorScheme::Whites,
                        0 /*min*/,
                        1.0 /*max*/,
                        0.1 /*minChangeSpeed*/,
                        100  /*maxChangeSpeed*/,
                        true /*pressureSensitive*/,
                        SmartGrid::Fader::Structure::Linear));
            AddGrid(SmartGrid::x_gridSize - 1, 0, new SmartGrid::Fader(
                        &m_state->m_theoryOfTimeInput.m_input.m_globalHomotopy,
                        SmartGrid::x_gridSize,
                        SmartGrid::ColorScheme::Whites,
                        0 /*min*/,
                        1.0 /*max*/,
                        0.1 /*minChangeSpeed*/,
                        100  /*maxChangeSpeed*/,
                        true /*pressureSensitive*/,
                        SmartGrid::Fader::Structure::Linear));

            if (m_swing)
            {
                m_owner->m_stateSaver.Insert("TheoryOfTimeSwingSwaggerRand", &m_state->m_theoryOfTimeInput.m_input.m_rand);
                m_owner->m_stateSaver.Insert("TheoryOfTimeSwingSwaggerGlobalHomotopy", &m_state->m_theoryOfTimeInput.m_input.m_globalHomotopy);
            }

            for (size_t i = 1; i < 1 + TheNonagonInternal::x_numTimeBits; ++i)
            {
                size_t xPos = SmartGrid::x_gridSize - i - 1;
                float* state;
                if (m_swing)
                {
                    state = &m_state->m_theoryOfTimeInput.m_input.m_input[i].m_swing;
                }
                else
                {
                    state = &m_state->m_theoryOfTimeInput.m_input.m_input[i].m_swagger;
                }

                m_owner->m_stateSaver.Insert("TheoryOfTimeSwingSwagger", i, m_swing, state);
                
                AddGrid(xPos, 0, new SmartGrid::Fader(
                            state,
                            SmartGrid::x_gridSize,
                            m_swing ? SmartGrid::ColorScheme::OrangeHues : SmartGrid::ColorScheme::YellowHues,
                            -1 /*min*/,
                            1 /*max*/,
                            0.1 /*minChangeSpeed*/,
                            100  /*maxChangeSpeed*/,
                            true /*pressureSensitive*/,
                            SmartGrid::Fader::Structure::Bipolar));
            }
        }
    };

    struct LameJuisCoMutePage : public SmartGrid::Grid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;        
        LameJuisCoMutePage(TheNonagonSmartGrid* owner)
            : SmartGrid::Grid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
        {
            InitGrid();
        }
        
        void InitGrid()
        {
            for (size_t i = 0; i < TheNonagonInternal::x_numTimeBits + 1; ++i)
            {
                if (i < TheNonagonInternal::x_numTimeBits)
                {
                    Put(i, 0, m_owner->TimeBitCell(i));
                    size_t theoryIx = TheNonagonInternal::x_numTimeBits - i;
                    Put(i, 1, new SmartGrid::StateCell<size_t>(
                            SmartGrid::Color::Ocean /*offColor*/,
                            SmartGrid::Color::White /*onColor*/,
                            &m_state->m_theoryOfTimeInput.m_input.m_input[theoryIx].m_parentIx,
                            theoryIx - 2,
                            theoryIx - 1,
                            SmartGrid::StateCell<size_t>::Mode::Toggle));

                }
                    
                for (Trio t : {Trio::Fire, Trio::Earth, Trio::Water})
                {
                    size_t tId = static_cast<size_t>(t);
                    size_t coMuteY = 7 - tId * 2;
                    size_t seqY = coMuteY - 1;
                    if (i < TheNonagonInternal::x_numTimeBits)
                    {
                        Put(i, coMuteY, new SmartGrid::StateCell<bool>(
                                SmartGrid::Color::White /*offColor*/,
                                TrioColor(t) /*onColor*/,                            
                                &m_state->m_lameJuisInput.m_outputInput[tId].m_coMuteInput.m_coMutes[i],
                                true,
                                false,
                                SmartGrid::StateCell<bool>::Mode::Toggle));
                        m_owner->m_stateSaver.Insert(
                            "LameJuisCoMute", i, tId, &m_state->m_lameJuisInput.m_outputInput[tId].m_coMuteInput.m_coMutes[i]);
                    }

                    Put(i, seqY, m_owner->PercentileSeqClockSelectCell(t, i));

                    m_owner->PlaceMutes(t, 6, seqY, this);
                }                                
            }

            Put(6, 1, m_owner->StartStopCell());
            Put(7, 1, m_owner->RecordingCell());
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
            return SmartGrid::Color::Indigo;
        }
        else
        {
            return SmartGrid::Color::SeaGreen;
        }
    }
    
    SmartGrid::Cell* EquationOutputSwitch(size_t i)
    {
        SmartGrid::ColorScheme colorScheme(std::vector<SmartGrid::Color>({
                    EquationColor(2), EquationColor(1), EquationColor(0)}));
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
            : SmartGrid::Grid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
        {
            SetColors(SmartGrid::Color::Ocean, SmartGrid::Color::DimOcean);
            InitGrid();
        }        
        
        void InitGrid()
        {
            SmartGrid::ColorScheme colorScheme(std::vector<SmartGrid::Color>({
                        SmartGrid::Color::Yellow, SmartGrid::Color::Dim, SmartGrid::Color::Ocean}));
            SmartGrid::ColorScheme dimColorScheme(std::vector<SmartGrid::Color>({
                        SmartGrid::Color::DimYellow, SmartGrid::Color::Dim, SmartGrid::Color::DimOcean}));
            for (size_t i = 0; i < TheNonagonInternal::x_numTimeBits; ++i)
            {
                Put(i, 1, m_owner->TimeBitCell(i));

                for (size_t j = 0; j < LameJuisInternal::x_numOperations; ++j)
                {
                    Put(i, j + 2, new SmartGrid::CycleCell<LameJuisInternal::MatrixSwitch, SmartGrid::BoolFlash>(
                            dimColorScheme,
                            colorScheme,
                            &m_state->m_lameJuisInput.m_operationInput[j].m_elements[i],
                            SmartGrid::BoolFlash(&m_nonagon->m_lameJuis.m_operations[j].m_highParticipant[i])));
                    m_owner->m_stateSaver.Insert(
                        "LameJuisMatrixSwitch", i, j, &m_state->m_lameJuisInput.m_operationInput[j].m_elements[i]);
                }

                Put(6, i + 2, m_owner->OutBitCell(i));
                Put(7, i + 2, m_owner->EquationOutputSwitch(i));
                m_owner->m_stateSaver.Insert(
                    "LameJuisEquationOutputSwitch", i, &m_state->m_lameJuisInput.m_operationInput[i].m_switch);
            }

            Put(6, 1, m_owner->StartStopCell());
            Put(7, 1, m_owner->RecordingCell());
        }
    };

    struct LameJuisLHSPage : public SmartGrid::Grid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;        
        LameJuisLHSPage(TheNonagonSmartGrid* owner)
            : SmartGrid::Grid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
        {
            SetColors(SmartGrid::Color::Indigo, SmartGrid::Color::DimIndigo);
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
                            SmartGrid::Color::DimIndigo,
                            SmartGrid::Color::Yellow,
                            SmartGrid::Color::Indigo,
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

            Put(6, 1, m_owner->StartStopCell());
            Put(7, 1, m_owner->RecordingCell());
        }
    };

    struct LameJuisIntervalPage : public SmartGrid::Grid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;        
        LameJuisIntervalPage(TheNonagonSmartGrid* owner)
            : SmartGrid::Grid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
        {
            SetColors(SmartGrid::Color::SeaGreen, SmartGrid::Color::DimSeaGreen);
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

            Put(6, 1, m_owner->StartStopCell());
            Put(7, 1, m_owner->RecordingCell());
        }
    };

    struct PercentileSequencerSubPage : public SmartGrid::CompositeGrid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;
        Trio m_trio;
        PercentileSequencerSubPage(TheNonagonSmartGrid* owner, Trio t)
            : SmartGrid::CompositeGrid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
            , m_trio(t)
        {
            SetColors(TrioColor(m_trio), SmartGrid::ColorScheme::Hues(TrioColor(m_trio))[0]);
            InitGrid();
        }        
        
        void InitGrid()
        {
            size_t trioIx = static_cast<size_t>(m_trio);
            for (size_t i = 0; i < TheNonagonInternal::x_numTimeBits + 1; ++i)
            {
                size_t coMuteY = 1;
                size_t seqY = 0;
                if (i < TheNonagonInternal::x_numTimeBits)
                {
                    Put(i, coMuteY, new SmartGrid::StateCell<bool>(
                            SmartGrid::Color::White /*offColor*/,
                            TrioColor(m_trio) /*onColor*/,                            
                            &m_state->m_lameJuisInput.m_outputInput[trioIx].m_coMuteInput.m_coMutes[i],
                            true,
                            false,
                            SmartGrid::StateCell<bool>::Mode::Toggle));
                }
                
                Put(i, seqY, m_owner->PercentileSeqClockSelectCell(m_trio, i));
            }
                
            for (size_t i = 0; i < PercentileSequencerInternal::x_numSteps; ++i)
            {
                for (size_t j = 0; j < 2; ++j)
                {
                    Put(i, 3 - j, new SmartGrid::StateCell<size_t, SmartGrid::Flash<size_t>>(
                            VoiceColor(m_trio, 2 * j) /*offColor*/,
                            SmartGrid::Color::Grey,
                            SmartGrid::Color::White,
                            SmartGrid::Color::White,
                            &m_state->m_percentileSequencerInput.m_input[trioIx].m_selectorIn[j].m_seqLen,
                            SmartGrid::Flash<size_t>(&m_nonagon->m_percentileSequencer.m_sequencer[trioIx].m_stepSelector[j].m_curStep, i),
                            i,
                            0,
                            SmartGrid::StateCell<size_t, SmartGrid::Flash<size_t>>::Mode::SetOnly));
                }
                
                AddGrid(i, 4, new SmartGrid::Fader(
                            &m_state->m_percentileSequencerInput.m_input[trioIx].m_sequence[i],
                            SmartGrid::x_gridSize - 4,
                            SmartGrid::ColorScheme::Hues(TrioColor(m_trio)),
                            0 /*min*/,
                            1.0 /*max*/,
                            0.1 /*minChangeSpeed*/,
                            100  /*maxChangeSpeed*/,
                            true /*pressureSensitive*/,
                            SmartGrid::Fader::Structure::Linear));
                m_owner->m_stateSaver.Insert(
                    "SequencerValue", trioIx, i, &m_state->m_percentileSequencerInput.m_input[trioIx].m_sequence[i]);            

            }

            for (size_t i = 0; i < 2; ++i)
            {
                m_owner->m_stateSaver.Insert(
                    "PercentileSequenceLen", trioIx, i, &m_state->m_percentileSequencerInput.m_input[trioIx].m_selectorIn[i].m_seqLen);
            }

            m_owner->PlaceMutes(m_trio, 6, 0, this);

            m_owner->m_stateSaver.Insert(
                "PercentileSequenceClockSelect", trioIx, &m_state->m_percentileSequencerInput.m_clockSelect[trioIx]);
            m_owner->m_stateSaver.Insert(
                "PercentileSequenceResetSelect", trioIx, &m_state->m_percentileSequencerInput.m_resetSelect[trioIx]);
        }
    };

    struct PercentileSequencerPages : public SmartGrid::GridSwitcher
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;
        PercentileSequencerPages(TheNonagonSmartGrid* owner)
            : GridSwitcher(nullptr)
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
        {
        }        

        virtual size_t GetGridId() override
        {
            switch (m_owner->m_activeTrio)
            {
                case Trio::Fire: return m_owner->m_percentileSequencerFireGridId;
                case Trio::Earth: return m_owner->m_percentileSequencerEarthGridId;
                case Trio::Water: return m_owner->m_percentileSequencerWaterGridId;
                default: return m_owner->m_percentileSequencerFireGridId;
            }
        }
    };

    struct PercentileSequencerFaderPage : public SmartGrid::CompositeGrid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;        
        PercentileSequencerFaderPage(TheNonagonSmartGrid* owner)
            : SmartGrid::CompositeGrid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
        {
            InitGrid();
        }

        void InitGrid()
        {
            for (Trio t : {Trio::Fire, Trio::Earth, Trio::Water})
            {
                size_t tId = static_cast<size_t>(t);
                size_t xPos = 2 * tId;
                AddGrid(xPos, 0, new SmartGrid::Fader(
                            &m_state->m_percentileSequencerInput.m_input[tId].m_sequenceScale,
                            SmartGrid::x_gridSize,
                            SmartGrid::ColorScheme::Hues(TrioColor(t)),
                            0 /*min*/,
                            1.0 /*max*/,
                            0.1 /*minChangeSpeed*/,
                            100  /*maxChangeSpeed*/,
                            true /*pressureSensitive*/,
                            SmartGrid::Fader::Structure::Linear));
                AddGrid(xPos + 1, 0, new SmartGrid::Fader(
                            &m_state->m_percentileSequencerInput.m_input[tId].m_tenorDistance,
                            SmartGrid::x_gridSize,
                            SmartGrid::ColorScheme::Hues(TrioColor(t)),
                            0 /*min*/,
                            1.0 /*max*/,
                            0.1 /*minChangeSpeed*/,
                            100  /*maxChangeSpeed*/,
                            true /*pressureSensitive*/,
                            SmartGrid::Fader::Structure::Linear));
                m_owner->m_stateSaver.Insert(
                    "SequenceScale", tId, &m_state->m_percentileSequencerInput.m_input[tId].m_sequenceScale);
                m_owner->m_stateSaver.Insert(
                    "TenorDistance", tId, &m_state->m_percentileSequencerInput.m_input[tId].m_tenorDistance);
                
                m_owner->PlaceMutes(t, 6, SmartGrid::x_gridSize - 2 - 2 * tId, this);
            }

            Put(6, 1, m_owner->StartStopCell());
            Put(7, 1, m_owner->RecordingCell());
        }
    };
    
    struct TimbreAndMuteSubPage : public SmartGrid::CompositeGrid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;
        Trio m_trio;
        TimbreAndMuteSubPage(TheNonagonSmartGrid* owner, Trio t)
            : SmartGrid::CompositeGrid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
            , m_trio(t)
        {
            SetColors(TrioCompanionColor(m_trio), TrioCompanionDimColor(t));
            InitGrid();
        }        
        
        void InitGrid()
        {
            size_t trioIx = static_cast<size_t>(m_trio);
            for (size_t i = 0; i < LameJuisInternal::x_numOperations; ++i)
            {                
                for (size_t j = 0; j < TheNonagonInternal::x_voicesPerTrio; ++j)
                {
                    size_t voiceIx = trioIx * TheNonagonInternal::x_voicesPerTrio + j;
                    SmartGrid::Color c = VoiceColor(m_trio, j);
                    Put(j, i + 2, new SmartGrid::StateCell<bool, SmartGrid::BoolFlash>(
                            SmartGrid::Color::Dim /*offColor*/,
                            c /*onColor*/,
                            SmartGrid::Color::Grey /*offFlashColor*/,
                            TrioColor(m_trio) /*onFlashColor*/,
                            &m_state->m_timbreAndMuteInput.m_input[voiceIx].m_armed[i],
                            SmartGrid::BoolFlash(&m_nonagon->m_timbreAndMute.m_voices[voiceIx].m_on[i]),
                            true,
                            false,
                            SmartGrid::StateCell<bool, SmartGrid::BoolFlash>::Mode::Toggle));
                    m_owner->m_stateSaver.Insert(
                        "TimbreAndMuteOutBitSelect", i, voiceIx, &m_state->m_timbreAndMuteInput.m_input[voiceIx].m_armed[i]);
                }
            }

            for (size_t i = 0; i < LameJuisInternal::x_numOperations + 1; ++i)
            {
                Put(5, i + 1, new SmartGrid::StateCell<bool>(
                        SmartGrid::Color::Dim /*offColor*/,
                        SmartGrid::ColorScheme::Rainbow[i] /*onColor*/,
                        &m_state->m_timbreAndMuteInput.m_canPassIfOn[trioIx][i],
                        true,
                        false,
                        SmartGrid::StateCell<bool>::Mode::Toggle));
                m_owner->m_stateSaver.Insert(
                    "TimbreAndMuteCountHighSelect", trioIx, i, &m_state->m_timbreAndMuteInput.m_canPassIfOn[trioIx][i]);
            }

            AddGrid(6, 0, new SmartGrid::Fader(
                        &m_state->m_timbreAndMuteInput.m_slew[trioIx],
                        SmartGrid::x_gridSize,
                        SmartGrid::ColorScheme::Hues(TrioColor(m_trio)),
                        0 /*minSlew*/,
                        1.0 /*maxSlew*/,
                        0.1 /*minChangeSpeed*/,
                        100  /*maxChangeSpeed*/,
                        true /*pressureSensitive*/,
                        SmartGrid::Fader::Structure::Linear));            
            AddGrid(7, 0, new SmartGrid::Fader(
                        &m_state->m_timbreAndMuteInput.m_timbreMult[trioIx],
                        SmartGrid::x_gridSize,
                        SmartGrid::ColorScheme::Hues(TrioColor(m_trio)),
                        -1.0 /*minMult*/,
                        1.0 /*maxMult*/,
                        0.1 /*minChangeSpeed*/,
                        100  /*maxChangeSpeed*/,
                        true /*pressureSensitive*/,
                        SmartGrid::Fader::Structure::Bipolar));

            for (int i = -3; i < 3; ++i)
            {
                Put(3, 5 + i, new SmartGrid::StateCell<int>(
                        TrioCompanionColor(m_trio),
                        SmartGrid::Color::White /*onColor*/,
                        &m_state->m_trioOctaveSwitchesInput.m_octave[trioIx],
                        i,
                        0,
                        SmartGrid::StateCell<int>::Mode::SetOnly));
            }

            for (size_t i = 0; i < 4; ++i)
            {
                TrioOctaveSwitches::Spread spread = static_cast<TrioOctaveSwitches::Spread>(i);
                Put(4, 4 + i, new SmartGrid::StateCell<TrioOctaveSwitches::Spread>(
                        TrioCompanionColor(m_trio),
                        SmartGrid::Color::White /*onColor*/,
                        &m_state->m_trioOctaveSwitchesInput.m_spread[trioIx],
                        spread,
                        spread,
                        SmartGrid::StateCell<TrioOctaveSwitches::Spread>::Mode::SetOnly));
            }
            
            m_owner->m_stateSaver.Insert(
                "TrioOctave", trioIx, &m_state->m_trioOctaveSwitchesInput.m_octave[trioIx]);
            m_owner->m_stateSaver.Insert(
                "TrioSpread", trioIx, &m_state->m_trioOctaveSwitchesInput.m_spread[trioIx]);            
            m_owner->m_stateSaver.Insert(
                "TimbreAndMuteSlew", trioIx, &m_state->m_timbreAndMuteInput.m_slew[trioIx]);            
            m_owner->m_stateSaver.Insert(
                "TimbreAndMuteTimbreMult", trioIx, &m_state->m_timbreAndMuteInput.m_timbreMult[trioIx]);
        }
    };

    struct TimbreAndMutePages : public SmartGrid::GridSwitcher
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;
        TimbreAndMutePages(TheNonagonSmartGrid* owner)
            : GridSwitcher(nullptr)
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
        {
        }        

        virtual size_t GetGridId() override
        {
            switch (m_owner->m_activeTrio)
            {
                case Trio::Fire: return m_owner->m_timbreAndMuteFireGridId;
                case Trio::Earth: return m_owner->m_timbreAndMuteEarthGridId;
                case Trio::Water: return m_owner->m_timbreAndMuteWaterGridId;
                default: return m_owner->m_timbreAndMuteFireGridId;
            }
        }
    };

    struct AuxFadersPage : public SmartGrid::CompositeGrid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;
        size_t m_startIx;
        AuxFadersPage(TheNonagonSmartGrid* owner, size_t startIx)
            : SmartGrid::CompositeGrid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
            , m_startIx(startIx)
        {
            InitGrid();
        }

        SmartGrid::Color GetFaderColor(size_t ix)
        {
            switch(ix)
            {
                case 0:
                case 1:
                case 2:
                case 6:
                case 7:
                    return SmartGrid::Color::Fuscia;
                case 3:
                    return SmartGrid::Color::Red;
                case 4:
                    return SmartGrid::Color::Green;
                case 5:
                    return SmartGrid::Color::Blue;
                case 8:
                    return SmartGrid::Color::White;
                case 9:
                    return SmartGrid::Color::Ocean;
                case 10:
                    return SmartGrid::Color::Yellow;
                case 11:
                    return SmartGrid::Color::SeaGreen;
                default:
                    return SmartGrid::Color::Off;
            }
        }

        void InitGrid()
        {
            for (size_t i = m_startIx; i < m_startIx + 8; ++i)
            {
                SmartGrid::Color c = GetFaderColor(i);
                if (c == SmartGrid::Color::Off)
                {
                    continue;
                }

                SmartGrid::Fader::Structure structure = SmartGrid::Fader::Structure::Linear;
                if (i == 8)
                {
                    structure = SmartGrid::Fader::Structure::Bipolar;
                }
                
                AddGrid(i - m_startIx, 0, new SmartGrid::Fader(
                        &m_state->m_auxFaders[i],
                        SmartGrid::x_gridSize,
                        SmartGrid::ColorScheme::Hues(c),
                        0 /*minVal*/,
                        10.0 /*maxVal*/,
                        0.1 /*minChangeSpeed*/,
                        100  /*maxChangeSpeed*/,
                        true /*pressureSensitive*/,
                        structure));
                m_owner->m_stateSaver.Insert("AuxFader", i, &m_state->m_auxFaders[i]);
            }                     
        }            
    };

    size_t m_theoryOfTimeTopologyGridId;
    size_t m_theoryOfTimeSwingGridId;
    size_t m_theoryOfTimeSwaggerGridId;
    size_t m_lameJuisCoMuteGridId;
    size_t m_lameJuisMatrixGridId;
    size_t m_lameJuisLHSGridId;
    size_t m_lameJuisIntervalGridId;
    size_t m_percentileSequencerFireGridId;
    size_t m_percentileSequencerEarthGridId;
    size_t m_percentileSequencerWaterGridId;
    size_t m_percentileSequencerPagesGridId;
    size_t m_percentileSequencerFaderGridId;
    size_t m_timbreAndMuteFireGridId;
    size_t m_timbreAndMuteEarthGridId;
    size_t m_timbreAndMuteWaterGridId;
    size_t m_timbreAndMutePagesGridId;
    size_t m_auxFadersPage1GridId;
    size_t m_auxFadersPage2GridId;    
    
    struct TheNonagonGridSwitcher : public SmartGrid::MenuGridSwitcher
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;
        TheNonagonGridSwitcher(TheNonagonSmartGrid* owner)
            : m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
        {
            InitGrid();
        }        

        void InitGrid()
        {
            GetMenuGrid()->AddMenuRow(SmartGrid::MenuButtonRow::RowPos::Bottom);
            GetMenuGrid()->AddMenuRow(SmartGrid::MenuButtonRow::RowPos::SubBottom);
            GetMenuGrid()->AddMenuRow(SmartGrid::MenuButtonRow::RowPos::Right, false, 0, 3);

            // Theory Of Time
            //
            GetMenuGrid()->SetGridId(                
                SmartGrid::MenuButtonRow::RowPos::Bottom,
                0,
                m_owner->m_theoryOfTimeTopologyGridId);
            GetMenuGrid()->SetGridId(                
                SmartGrid::MenuButtonRow::RowPos::Bottom,
                2,
                m_owner->m_theoryOfTimeSwingGridId);
            GetMenuGrid()->SetGridId(                
                SmartGrid::MenuButtonRow::RowPos::Bottom,
                3,
                m_owner->m_theoryOfTimeSwaggerGridId);
            
            // LaMeJuIS
            //
            GetMenuGrid()->SetGridId(                
                SmartGrid::MenuButtonRow::RowPos::SubBottom,
                0,
                m_owner->m_lameJuisCoMuteGridId);
            GetMenuGrid()->SetGridId(                
                SmartGrid::MenuButtonRow::RowPos::SubBottom,
                1,
                m_owner->m_lameJuisMatrixGridId);
            GetMenuGrid()->SetGridId(                
                SmartGrid::MenuButtonRow::RowPos::SubBottom,
                2,
                m_owner->m_lameJuisLHSGridId);
            GetMenuGrid()->SetGridId(                
                SmartGrid::MenuButtonRow::RowPos::SubBottom,
                3,
                m_owner->m_lameJuisIntervalGridId);

            
            // Sequencers
            //
            GetMenuGrid()->SetGridId(                
                SmartGrid::MenuButtonRow::RowPos::Bottom,
                4,
                m_owner->m_percentileSequencerPagesGridId);
            GetMenuGrid()->SetGridId(                
                SmartGrid::MenuButtonRow::RowPos::Bottom,
                1,
                m_owner->m_percentileSequencerFaderGridId);
            
            // Articulation
            //
            GetMenuGrid()->SetGridId(                
                SmartGrid::MenuButtonRow::RowPos::Bottom,
                5,
                m_owner->m_timbreAndMutePagesGridId);
            
            // Faders
            //
            GetMenuGrid()->SetGridId(                
                SmartGrid::MenuButtonRow::RowPos::SubBottom,
                4,
                m_owner->m_auxFadersPage1GridId);
            GetMenuGrid()->SetGridId(                
                SmartGrid::MenuButtonRow::RowPos::SubBottom,
                5,
                m_owner->m_auxFadersPage2GridId);

            GetMenuGrid()->Put(SmartGrid::MenuButtonRow::RowPos::Right, 0, new SmartGrid::StateCell<Trio>(
                    SmartGrid::Color::DimBlue,
                    SmartGrid::Color::Blue,
                    &m_owner->m_activeTrio,
                    Trio::Water,
                    Trio::Water,
                    SmartGrid::StateCell<Trio>::Mode::SetOnly));
            GetMenuGrid()->Put(SmartGrid::MenuButtonRow::RowPos::Right, 1, new SmartGrid::StateCell<Trio>(
                    SmartGrid::Color::DimGreen,
                    SmartGrid::Color::Green,
                    &m_owner->m_activeTrio,
                    Trio::Earth,
                    Trio::Earth,
                    SmartGrid::StateCell<Trio>::Mode::SetOnly));
            GetMenuGrid()->Put(SmartGrid::MenuButtonRow::RowPos::Right, 2, new SmartGrid::StateCell<Trio>(
                    SmartGrid::Color::DimRed,
                    SmartGrid::Color::Red,
                    &m_owner->m_activeTrio,
                    Trio::Fire,
                    Trio::Fire,
                    SmartGrid::StateCell<Trio>::Mode::SetOnly));
        }        
    };
        
    Trio m_activeTrio;
    
    TheNonagonSmartGrid()
    {
        InitState();
        InitGrid();
    }

    void InitState()
    {
        for (size_t i = 0; i < LameJuisInternal::x_numAccumulators; ++i)
        {
            m_state.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_polyChans = TheNonagonInternal::x_voicesPerTrio;
        }
        
        for (size_t i = 0; i < LameJuisInternal::x_numInputs; ++i)
        {
            m_state.m_lameJuisInput.m_inputBitInput[i].m_connected = true;
        }
        
        for (size_t i = 0; i < LameJuisInternal::x_numOperations; ++i)
        {
            m_state.m_lameJuisInput.m_operationInput[i].m_operator = LameJuisInternal::LogicOperation::Operator::Direct;
        }

        m_state.m_multiPhasorGateInput.m_numTrigs = TheNonagonInternal::x_numVoices;
        m_state.m_multiPhasorGateInput.m_numPhasors = TheNonagonInternal::x_numTimeBits + 1;
        for (size_t i = 0; i < TheNonagonInternal::x_numVoices; ++i)
        {
            m_state.m_multiPhasorGateInput.m_phasorSelector[i][TheNonagonInternal::x_numTimeBits] = true;
            m_numPercentileSeqClockSelectsHeld[i] = 0;
            m_stateSaver.Insert("Mute", i, &m_state.m_mute[i]);                                
        }        
    }
    
    void InitGrid()
    {
        // TheoryOfTime
        //
        m_theoryOfTimeTopologyGridId = m_smartGrid.AddGrid(new TheoryOfTimeTopologyPage(this));
        m_theoryOfTimeSwingGridId = m_smartGrid.AddGrid(new TheoryOfTimeSwingAndSwaggerPage(this, true));
        m_theoryOfTimeSwaggerGridId = m_smartGrid.AddGrid(new TheoryOfTimeSwingAndSwaggerPage(this, false));
 
        // LaMeJuIS
        //
        m_lameJuisCoMuteGridId = m_smartGrid.AddGrid(new LameJuisCoMutePage(this));
        m_lameJuisMatrixGridId = m_smartGrid.AddGrid(new LameJuisMatrixPage(this));
        m_lameJuisLHSGridId = m_smartGrid.AddGrid(new LameJuisLHSPage(this));
        m_lameJuisIntervalGridId = m_smartGrid.AddGrid(new LameJuisIntervalPage(this));
 
        // Sequencers
        //
        m_percentileSequencerFireGridId = m_smartGrid.AddGrid(new PercentileSequencerSubPage(this, Trio::Fire));
        m_percentileSequencerEarthGridId = m_smartGrid.AddGrid(new PercentileSequencerSubPage(this, Trio::Earth));
        m_percentileSequencerWaterGridId = m_smartGrid.AddGrid(new PercentileSequencerSubPage(this, Trio::Water));
        m_percentileSequencerFaderGridId = m_smartGrid.AddGrid(new PercentileSequencerFaderPage(this));
        m_percentileSequencerPagesGridId = m_smartGrid.AddGrid(new PercentileSequencerPages(this));
         
        // Articulation
        //
        m_timbreAndMuteFireGridId = m_smartGrid.AddGrid(new TimbreAndMuteSubPage(this, Trio::Fire));
        m_timbreAndMuteEarthGridId = m_smartGrid.AddGrid(new TimbreAndMuteSubPage(this, Trio::Earth));
        m_timbreAndMuteWaterGridId = m_smartGrid.AddGrid(new TimbreAndMuteSubPage(this, Trio::Water));
        m_timbreAndMutePagesGridId = m_smartGrid.AddGrid(new TimbreAndMutePages(this));
        
        // Faders
        //
        m_auxFadersPage1GridId = m_smartGrid.AddGrid(new AuxFadersPage(this, 0));
        m_auxFadersPage2GridId = m_smartGrid.AddGrid(new AuxFadersPage(this, 8));
        
        m_smartGrid.AddToplevelGrid(new SmartGrid::GridSwitcher(new TheNonagonGridSwitcher(this)));
        m_smartGrid.AddToplevelGrid(new SmartGrid::GridSwitcher(new TheNonagonGridSwitcher(this)));
        m_stateSaver.Insert("ActiveTrio", &m_activeTrio);

        m_smartGrid.m_state.m_midiSwitchInput.m_numChannels = 2;
        m_smartGrid.m_state.m_midiSwitchInput.m_channels[0] = 0;
        m_smartGrid.m_state.m_midiSwitchInput.m_channels[0] = 1;
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
        config(0, 0, 2 + 12 + 3, 0);

        configOutput(0, "VoltPerOct Output");
        configOutput(1, "Gate Output");
        configOutput(14, "Recording Output");
        configOutput(15, "Timbre Output");
        configOutput(16, "Phasor Output");

        for (size_t i = 0; i < 12; ++i)
        {
            configOutput(i + 2, ("Aux Fader " + std::to_string(i)).c_str());
        }
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
        outputs[0].setChannels(TheNonagonInternal::x_numVoices);
        outputs[1].setChannels(TheNonagonInternal::x_numVoices);
        outputs[15].setChannels(TheNonagonInternal::x_numVoices);
        outputs[16].setChannels(TheNonagonInternal::x_numVoices);
        
        m_nonagon.Process(args.sampleTime, args.frame);

        for (size_t i = 0; i < TheNonagonInternal::x_numVoices; ++i)
        {
            outputs[0].setVoltage(m_nonagon.m_nonagon.m_output.m_voltPerOct[i], i);
            outputs[1].setVoltage(m_nonagon.m_nonagon.m_output.m_gate[i] ? 10 : 0, i);
            outputs[15].setVoltage(m_nonagon.m_nonagon.m_output.m_timbre[i] * 10, i);
            outputs[16].setVoltage(m_nonagon.m_nonagon.m_output.m_phasor[i] * 10, i);
        }

        for (size_t i = 0; i < 12; ++i)
        {
            outputs[i + 2].setVoltage(m_nonagon.m_nonagon.m_output.m_auxFaders[i]);
        }

        outputs[14].setVoltage(m_nonagon.m_nonagon.m_output.m_recording ? 10 : 0);
    }
};

struct TheNonagonWidget : ModuleWidget
{
    TheNonagonWidget(TheNonagon* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/TheNonagon.svg")));

        addOutput(createOutputCentered<PJ301MPort>(Vec(175, 100), module, 0));
        addOutput(createOutputCentered<PJ301MPort>(Vec(125, 100), module, 1));        
        
        for (size_t i = 0; i < 8; ++i)
        {            
            addOutput(createOutputCentered<PJ301MPort>(Vec(25, 100 + 25 * i), module, 2 + i));
        }
        for (size_t i = 0; i < 4; ++i)
        {            
            addOutput(createOutputCentered<PJ301MPort>(Vec(75, 100 + 25 * i), module, 10 + i));
        }

        addOutput(createOutputCentered<PJ301MPort>(Vec(125, 200), module, 14));
        addOutput(createOutputCentered<PJ301MPort>(Vec(175, 200), module, 15));
        addOutput(createOutputCentered<PJ301MPort>(Vec(175, 225), module, 16));                
    }
};
    
