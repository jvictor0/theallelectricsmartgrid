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
#include "NonagonPanner.hpp"
#include "Trig.hpp"
#include "ClockSelectCell.hpp"
#include "GangedRandomLFO.hpp"
#include "Slew.hpp"

struct TheNonagonInternal
{
    static constexpr size_t x_numTimeBits = 6;
    static constexpr size_t x_numVoices = 9;
    static constexpr size_t x_numTrios = 3;
    static constexpr size_t x_voicesPerTrio = 3;
    static constexpr size_t x_numGridIds = 16;
    static constexpr size_t x_numExtraTimbres = 3;

    struct Input
    {
        MusicalTimeWithClock::Input m_theoryOfTimeInput;
        LameJuisInternal::Input m_lameJuisInput;
        TimbreAndMute::Input m_timbreAndMuteInput;
        MultiPhasorGateInternal::Input m_multiPhasorGateInput;
        NonagonIndexArp::Input m_arpInput;
        TrioOctaveSwitches::Input m_trioOctaveSwitchesInput;

        int m_indexArpIntervalSelect[x_numVoices];
        int m_indexArpOffsetSelect[x_numVoices];
        int m_indexArpMotiveIxSelect[x_numVoices];
        
        bool m_mute[x_numVoices];
        float m_extraTimbre[x_numTrios][x_numExtraTimbres];
        bool m_running;
        bool m_recording;
        
        Input()
            : m_timbreAndMuteInput(m_mute)
            , m_running(false)
            , m_recording(false)
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_mute[i] = false;
            }

            for (size_t i = 0; i < x_numTrios; ++i)
            {
                for (size_t j = 0; j < x_numExtraTimbres; ++j)
                {
                    m_extraTimbre[i][j] = 0;
                }

                for (size_t j = 0; j < x_voicesPerTrio; ++j)
                {
                    
                }
            }
        }
    };
    
    MusicalTimeWithClock m_theoryOfTime;
    LameJuisInternal m_lameJuis;
    TimbreAndMute m_timbreAndMute;
    MultiPhasorGateInternal m_multiPhasorGate;
    NonagonIndexArp m_indexArp;
    bool m_muted[x_numVoices];
    bool m_gateAck[x_numVoices];
    double m_timer;
    Trig m_timerTrig;

    TheNonagonInternal()
        : m_timer(0)
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_muted[i] = false;
            m_gateAck[i] = false;
        }
    }
    
    struct Output
    {
        bool m_gate[x_numVoices];
        float m_phasor[x_numVoices];
        float m_voltPerOct[x_numVoices];
        float m_timbre[x_numVoices];
        float m_gridId[x_numGridIds];
        float m_extraTimbreTrg[x_numVoices][x_numExtraTimbres];
        float m_extraTimbre[x_numVoices][x_numExtraTimbres];
        FixedSlew m_extraTimbreSlew[x_numVoices][x_numExtraTimbres];
        bool m_recording;
        float m_totPhasors[x_numTimeBits];

        Output()
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_gate[i] = false;
                m_voltPerOct[i] = 0;
                m_timbre[i] = 0;
                m_phasor[i] = 0;

                for (size_t j = 0; j < x_numExtraTimbres; ++j)                
                {
                    m_extraTimbre[i][j] = 0;
                    m_extraTimbreTrg[i][j] = 0;
                }
            }

            for (size_t i = 0; i < x_numTimeBits; ++i)
            {
                m_totPhasors[i] = 0;
            }
            
            m_recording = false;
        }
    };

    Output m_output;

    struct TimeBit
    {
        bool m_bit;
    };

    TimeBit m_time[x_numTimeBits + 1];

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
        m_time[x_numTimeBits].m_bit = m_theoryOfTime.m_musicalTime.m_gate[0];
        for (size_t i = 0; i < x_numTimeBits; ++i)
        {
            m_time[i].m_bit = m_theoryOfTime.m_musicalTime.m_gate[x_numTimeBits - i];
            input.m_lameJuisInput.m_inputBitInput[i].m_value = m_time[i].m_bit;
        }

        for (size_t i = 0; i < x_numTrios; ++i)
        {
            for (size_t j = 0; j < x_voicesPerTrio; ++j)
            {
                if (input.m_arpInput.m_clockSelect[i] >= 0)
                {
                    input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_usePercentile[j] = false;
                    input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_harmonic[j] = false;
                    IndexArp* arp = &m_indexArp.m_committedArp[i * x_voicesPerTrio + j];
                    input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_indexArp[j] = arp;
                    arp = &m_indexArp.m_arp[i * x_voicesPerTrio + j];
                    input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_preIndexArp[j] = arp;
                }
                else
                {
                    input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_usePercentile[j] = true;
                    input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_harmonic[j] = true;
                    float percentile = 1.0 / 2.0 * j;
                    input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_percentiles[j] = percentile;
                }
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
            input.m_timbreAndMuteInput.m_on[i] = m_lameJuis.m_inputs[i].m_value;
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
            input.m_multiPhasorGateInput.m_phasors[i] = m_theoryOfTime.m_musicalTime.m_bits[x_numTimeBits - i].m_pos.Float();
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
            int voiceClock = input.m_arpInput.m_clockSelect[i / x_voicesPerTrio];
            if (voiceClock >= 0)
            {
                input.m_multiPhasorGateInput.m_phasorSelector[i][voiceClock] = true;
            }
        }
    }

    void SetIndexArpInputs(Input& input)
    {
        for (size_t i = 0; i < x_numTimeBits + 1; ++i)
        {            
            input.m_arpInput.m_clocks[i] = m_theoryOfTime.m_musicalTime.m_change[x_numTimeBits - i];
        }

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            const int num = LameJuisInternal::Output::CacheForSingleInputVector<false>::x_octaveBuckets;
            const int intervals[] = {0, num / 7, num * 7 / 24, num * 5 / 12, num * 7 / 12 };
            assert(input.m_indexArpIntervalSelect[i]  < 4);
            assert(input.m_indexArpOffsetSelect[i] < 4);
            assert(input.m_indexArpMotiveIxSelect[i] < 4);
            assert(input.m_indexArpIntervalSelect[i] >= 0);
            assert(input.m_indexArpOffsetSelect[i] >= 0);
            assert(input.m_indexArpMotiveIxSelect[i] >= 0);
            input.m_arpInput.m_input[i].m_interval = intervals[input.m_indexArpIntervalSelect[i] + 1];
            input.m_arpInput.m_input[i].m_offset = intervals[input.m_indexArpOffsetSelect[i]];
            input.m_arpInput.m_input[i].m_motiveInterval = intervals[input.m_indexArpMotiveIxSelect[i]];
        }
    }    

    void SetOutputs(Input& input)
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            if (m_multiPhasorGate.m_gate[i] && !m_gateAck[i])
            {                                
                if (!input.m_mute[i])
                {
                    m_output.m_gate[i] = true;
                    m_muted[i] = false;
                    auto& result = m_lameJuis.m_outputs[i / x_voicesPerTrio].m_pitch[i % x_voicesPerTrio];
                    float preOctave = result.m_pitch;
                    m_output.m_voltPerOct[i] = input.m_trioOctaveSwitchesInput.Octavize(preOctave, i);
                    m_output.m_timbre[i] = m_timbreAndMute.m_voices[i].m_timbre;
                    
                    for (size_t j = 0; j < x_numExtraTimbres; ++j)
                    {
                        m_output.m_extraTimbreTrg[i][j] = result.Timbre(j);
                    }
                }
                else
                {
                    m_muted[i] = true;
                }

                m_gateAck[i] = true;
            }
            else if (!m_multiPhasorGate.m_gate[i])
            {
                m_output.m_gate[i] = false;
                m_gateAck[i] = false;
            }

            m_output.m_phasor[i] = m_muted[i] ? 0 : m_multiPhasorGate.m_phasorOut[i];

            for (size_t j = 0; j < x_numExtraTimbres; ++j)
            {
                m_output.m_extraTimbre[i][j] = m_output.m_extraTimbreSlew[i][j].Process(m_output.m_extraTimbreTrg[i][j]);
            }
        }

        for (size_t i = 0; i < x_numTimeBits; ++i)
        {
            m_output.m_totPhasors[i] = input.m_multiPhasorGateInput.m_phasors[x_numTimeBits - i - 1];
        }

        m_output.m_recording = input.m_recording;
    }

    void ProcessTimer(float dt, Input& input)
    {
        if (m_timerTrig.Process(input.m_running))
        {
            m_timer = 0;
        }

        m_timer += dt;
    }

    float GetTimerFragment(size_t ix)
    {
        double tot = m_timer / (60 * 20);
        if (tot < static_cast<double>(ix) / 16)
        {
            return 0;
        }
        else if (static_cast<double>(ix + 1) / 16 <= tot)
        {
            return 1;
        }
        else
        {
            return (tot - static_cast<double>(ix) / 16) / (1.0 / 16);
        }
    }

    void Process(float dt, Input& input)
    {
        ProcessTimer(dt, input);
        SetTheoryOfTimeInput(input);
        m_theoryOfTime.Process(dt, input.m_theoryOfTimeInput);
        if (m_theoryOfTime.m_musicalTime.m_anyChange)
        {
            SetIndexArpInputs(input);
            m_indexArp.Process(input.m_arpInput);
            
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
    SmartGrid::GridHolder m_gridHolder;

    ScenedStateSaver m_stateSaver;
    ScenedStateSaver::Input m_stateSaverState;

    json_t* m_savedJSON;

    json_t* ToJSON()
    {
        return m_stateSaver.ToJSON();
    }

    void SaveJSON()
    {
        const char* fname = "/Users/joyo/theallelectricsmartgrid/patches/nonagon_dump.json";
        json_dump_file(ToJSON(), fname, 0);
    }

    void SetFromJSON(json_t* jin)
    {
        m_stateSaver.SetFromJSON(jin);
    }

    void LoadJSON()
    {
        const char* fname = "/Users/joyo/theallelectricsmartgrid/patches/nonagon_dump.json";
        json_error_t error;
        SetFromJSON(json_load_file(fname, 0, &error));
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
            case Trio::NumTrios: return SmartGrid::Color::White.Dim();
        }
    }

    static SmartGrid::Color TrioCompanionColor(Trio t)
    {
        switch (t)
        {
            case Trio::Fire: return SmartGrid::Color::Orange;
            case Trio::Earth: return SmartGrid::Color::Yellow;
            case Trio::Water: return SmartGrid::Color::Indigo;
            case Trio::NumTrios: return SmartGrid::Color::White.Dim();
        }
    }

    static SmartGrid::Color TrioCompanionDimColor(Trio t)
    {
        switch (t)
        {
            case Trio::Fire: return SmartGrid::Color::Orange.Dim();
            case Trio::Earth: return SmartGrid::Color::Yellow.Dim();
            case Trio::Water: return SmartGrid::Color::Indigo.Dim();
            case Trio::NumTrios: return SmartGrid::Color::White.Dim();
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

    size_t m_numClockSelectsHeld[TheNonagonInternal::x_numVoices];
    size_t m_numClockSelectsMaxHeld[TheNonagonInternal::x_numVoices];
    bool m_shift;
    
    SmartGrid::Cell* MkClockSelectCell(Trio trio, size_t clockIx)
    {
        size_t trioIx = static_cast<size_t>(trio);
        return new ClockSelectCell(
            &m_state.m_arpInput,
            trioIx,
            &m_numClockSelectsHeld[trioIx],
            &m_numClockSelectsMaxHeld[trioIx],
            clockIx,
            TrioColor(trio).Dim(),
            SmartGrid::Color::White,
            SmartGrid::Color::Grey);
    }

    void PlaceMutes(Trio t, int x, int y, SmartGrid::Grid* grid)
    {
        for (size_t i = 0; i < TheNonagonInternal::x_voicesPerTrio; ++i)
        {
            size_t voiceIx = static_cast<size_t>(t) * TheNonagonInternal::x_voicesPerTrio + i;
            int xPos = (i == 0) ? x : x + 1;
            int yPos = (i < 2) ? y - 1 : y;
            grid->Put(xPos, yPos, new SmartGrid::StateCell<bool, SmartGrid::BoolFlash>(
                          SmartGrid::Color::White.Dim(),
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
        bool m_isPan;
        MusicalTime::Input* m_timeState;
        
        TheoryOfTimeTopologyPage(TheNonagonSmartGrid* owner, bool isPan)
            : SmartGrid::CompositeGrid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
            , m_isPan(isPan)
        {
            SetColors(SmartGrid::Color::Fuscia, SmartGrid::Color::Fuscia.Dim());
            m_timeState = &m_state->m_theoryOfTimeInput.m_input;

            InitGrid();
        }
        
        void InitGrid()
        {
            for (size_t i = 1; i < 1 + TheNonagonInternal::x_numTimeBits; ++i)
            {
                size_t xPos = SmartGrid::x_baseGridSize - i - 2;
                if (i != 1)
                {
                    for (size_t mult = 2; mult <= 5; ++mult)
                    {
                        Put(
                            xPos,
                            mult - 2,
                            new SmartGrid::StateCell<size_t>(
                                SmartGrid::Color::Fuscia /*offColor*/,
                                SmartGrid::Color::White /*onColor*/,
                                &m_timeState->m_input[i].m_mult,
                                mult,
                                1,
                                SmartGrid::StateCell<size_t>::Mode::Toggle));
                    }
                    
                    Put(xPos, 6, new SmartGrid::StateCell<bool>(
                            SmartGrid::Color::Ocean /*offColor*/,
                            SmartGrid::Color::White /*onColor*/,
                            &m_timeState->m_input[i].m_pingPong,
                            true,
                            false,
                            SmartGrid::StateCell<bool>::Mode::Toggle));
                    
                    m_owner->m_stateSaver.Insert(
                        "TheoryOfTimeMult", i, &m_timeState->m_input[i].m_mult);
                    m_owner->m_stateSaver.Insert(
                        "TheoryOfTimePingPong", i, &m_timeState->m_input[i].m_pingPong);
                }
                
                if (2 < i)
                {
                    Put(xPos, 4, new SmartGrid::StateCell<size_t>(
                            SmartGrid::Color::Ocean /*offColor*/,
                            SmartGrid::Color::White /*onColor*/,
                            &m_timeState->m_input[i].m_parentIx,
                            i - 2,
                            i - 1,
                            SmartGrid::StateCell<size_t>::Mode::Toggle));
                    m_owner->m_stateSaver.Insert(
                        "TheoryOfTimeParentIx", i, &m_timeState->m_input[i].m_parentIx);
                }
                
                Put(xPos, 7, m_owner->TimeBitCell(TheNonagonInternal::x_numTimeBits - i));
            }

            Put(6, 7, m_owner->TimeBitCell(TheNonagonInternal::x_numTimeBits));            
        }
    };

    void SetFrequency(float voct)
    {
        m_state.m_theoryOfTimeInput.m_freq = pow(2, voct) / 128;
    }

    struct PhasorSelectorCell : public SmartGrid::Cell
    {
        int m_srcIx;
        PhasorSwitcher* m_trg;
        SmartGrid::Color m_color;

        PhasorSelectorCell(int srcIx, PhasorSwitcher* trg, SmartGrid::Color color)
        {
            m_srcIx = srcIx;
            m_trg = trg;
            m_color = color;
        }
        
        virtual SmartGrid::Color GetColor() override
        {
            if (m_srcIx == m_trg->m_valIx)
            {
                return m_color.Interpolate(SmartGrid::Color::White, m_trg->Get());
            }
            else if (m_srcIx == m_trg->m_desiredIx)
            {
                return SmartGrid::Color::White;
            }
            else
            {
                return m_color;
            }
        }
        
        virtual void OnPress(uint8_t velocity) override
        {
            m_trg->Desire(m_srcIx);
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
                SetColors(SmartGrid::Color::Orange, SmartGrid::Color::Orange.Dim());
            }
            else
            {
                SetColors(SmartGrid::Color::Yellow, SmartGrid::Color::Yellow.Dim());
            }
            
            InitGrid();
        }
        
        void InitGrid()
        {
            AddGrid(0, 0, new SmartGrid::Fader(
                        &m_state->m_theoryOfTimeInput.m_input.m_rand,
                        SmartGrid::x_baseGridSize,
                        SmartGrid::Color::White,
                        0 /*min*/,
                        1.0 /*max*/,
                        0.1 /*minChangeSpeed*/,
                        100  /*maxChangeSpeed*/,
                        true /*pressureSensitive*/,
                        SmartGrid::Fader::Structure::Linear));
            AddGrid(SmartGrid::x_baseGridSize - 1, 0, new SmartGrid::Fader(
                        &m_state->m_theoryOfTimeInput.m_input.m_globalHomotopy,
                        SmartGrid::x_baseGridSize,
                        SmartGrid::Color::White,
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
                size_t xPos = SmartGrid::x_baseGridSize - i - 1;
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
                            SmartGrid::x_baseGridSize,
                            m_swing ? SmartGrid::Color::Orange : SmartGrid::Color::Yellow,
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
                Put(i, 7, m_owner->TimeBitCell(i));
                if (i < TheNonagonInternal::x_numTimeBits - 2)
                {
                    size_t theoryIx = TheNonagonInternal::x_numTimeBits - i;
                    Put(i, 6, new SmartGrid::StateCell<size_t>(
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
                    size_t coMuteY = tId * 2;
                    size_t seqY = coMuteY + 1;
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

                    Put(i, seqY, m_owner->MkClockSelectCell(t, i));

                    m_owner->PlaceMutes(t, 6, seqY, this);
                }                                
            }

            Put(7, 7, m_owner->StartStopCell());
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
            SetColors(SmartGrid::Color::Ocean, SmartGrid::Color::Ocean.Dim());
            InitGrid();
        }        
        
        void InitGrid()
        {
            SmartGrid::ColorScheme colorScheme(std::vector<SmartGrid::Color>({
                        SmartGrid::Color::Yellow, SmartGrid::Color::White.Dim(), SmartGrid::Color::Ocean}));
            SmartGrid::ColorScheme dimColorScheme(std::vector<SmartGrid::Color>({
                        SmartGrid::Color::Yellow.Dim(), SmartGrid::Color::White.Dim(), SmartGrid::Color::Ocean.Dim()}));
            for (size_t i = 0; i < TheNonagonInternal::x_numTimeBits; ++i)
            {
                Put(i, 6, m_owner->TimeBitCell(i));

                for (size_t j = 0; j < LameJuisInternal::x_numOperations; ++j)
                {
                    Put(i, SmartGrid::x_baseGridSize - j - 3, new SmartGrid::CycleCell<LameJuisInternal::MatrixSwitch, SmartGrid::BoolFlash>(
                            dimColorScheme,
                            colorScheme,
                            &m_state->m_lameJuisInput.m_operationInput[j].m_elements[i],
                            SmartGrid::BoolFlash(&m_nonagon->m_lameJuis.m_operations[j].m_highParticipant[i])));
                    m_owner->m_stateSaver.Insert(
                        "LameJuisMatrixSwitch", i, j, &m_state->m_lameJuisInput.m_operationInput[j].m_elements[i]);
                }

                Put(6, SmartGrid::x_baseGridSize - i - 3, m_owner->OutBitCell(i));
                Put(7, SmartGrid::x_baseGridSize - i - 3, m_owner->EquationOutputSwitch(i));
                m_owner->m_stateSaver.Insert(
                    "LameJuisEquationOutputSwitch", i, &m_state->m_lameJuisInput.m_operationInput[i].m_switch);
            }
        }
    };

    struct LameJuisLHSPage : public SmartGrid::Grid
    {
        struct Cell : public SmartGrid::StateCell<bool, SmartGrid::Flash<size_t>>
        {
            int m_i;
            int m_j;
            TheNonagonInternal* m_nonagon;

            Cell(int i, int j, bool* state, TheNonagonInternal* nonagon)
                : SmartGrid::StateCell<bool, SmartGrid::Flash<size_t>>(
                    SmartGrid::Color::White.Dim(),
                    SmartGrid::Color::Indigo.Dim(),
                    SmartGrid::Color::Yellow,
                    SmartGrid::Color::Indigo,
                    state,
                    SmartGrid::Flash<size_t>(&nonagon->m_lameJuis.m_operations[i].m_countHigh, j),
                    true,
                    false,
                    SmartGrid::StateCell<bool, SmartGrid::Flash<size_t>>::Mode::Toggle),
                  m_i(i),
                  m_j(j),
                  m_nonagon(nonagon)
            {
            }

            virtual SmartGrid::Color GetColor() override
            {
                SmartGrid::Color c = SmartGrid::StateCell<bool, SmartGrid::Flash<size_t>>::GetColor();
                if (m_nonagon->m_lameJuis.m_operations[m_i].m_countTotal < static_cast<size_t>(m_j))
                {
                    return c.Dim();
                }

                return c;
            }
        };
        
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;        
        LameJuisLHSPage(TheNonagonSmartGrid* owner)
            : SmartGrid::Grid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
        {
            SetColors(SmartGrid::Color::Indigo, SmartGrid::Color::Indigo.Dim());
            InitGrid();
        }        
        
        void InitGrid()
        {
            for (size_t i = 0; i < LameJuisInternal::x_numOperations; ++i)
            {
                for (size_t j = 0; j < TheNonagonInternal::x_numTimeBits + 1; ++j)
                {
                    Put(j, SmartGrid::x_baseGridSize - i - 3, new Cell(i, j, &m_state->m_lameJuisInput.m_operationInput[i].m_direct[j], m_nonagon));
                    m_owner->m_stateSaver.Insert(
                        "LameJuisLHS", i, j, &m_state->m_lameJuisInput.m_operationInput[i].m_direct[j]);
                }

                Put(7, SmartGrid::x_baseGridSize - i - 3, m_owner->EquationOutputSwitch(i));                
            }
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
            SetColors(SmartGrid::Color::SeaGreen, SmartGrid::Color::SeaGreen.Dim());
            InitGrid();
        }        
        
        void InitGrid()
        {
            for (size_t i = 0; i < LameJuisInternal::x_numAccumulators; ++i)
            {
                for (size_t j = 0; j < static_cast<size_t>(LameJuisInternal::Accumulator::Interval::NumIntervals); ++j)
                {
                    size_t xPos = 2 * i + (j == 0 || 6 < j ? 1 : 0);
                    size_t yPos = j == 0 ? 5 : (j - 1) % 6;
                    Put(xPos, yPos, new SmartGrid::StateCell<LameJuisInternal::Accumulator::Interval>(
                            EquationColor(i),
                            SmartGrid::Color::White,
                            &m_state->m_lameJuisInput.m_accumulatorInput[i].m_interval,
                            static_cast<LameJuisInternal::Accumulator::Interval>(j),
                            LameJuisInternal::Accumulator::Interval::Off,
                            SmartGrid::StateCell<LameJuisInternal::Accumulator::Interval>::Mode::SetOnly));
                }

                m_owner->m_stateSaver.Insert(
                    "LameJuisInterval", i, &m_state->m_lameJuisInput.m_accumulatorInput[i].m_interval);
            }
        }
    };

    static SmartGrid::Color FadeColor(Trio trio, int ord, int ordMax)
    {
        if (ordMax % 2 == 1 && ord == ordMax / 2)
        {
            return TrioColor(trio);
        }
        else
        {
            bool low = ord < ordMax / 2;
            size_t trioIx = static_cast<size_t>(trio);
            Trio o = static_cast<Trio>((low ? trioIx + 2 : trioIx + 1) % 3);
            float perc;
            if (low)
            {
                perc = 0.5 + static_cast<float>(ord) / ordMax;
            }
            else
            {
                perc = 1.0 - static_cast<float>(ord) / (2 * ordMax);
            }
            
            return TrioColor(o).Interpolate(TrioColor(trio), perc);
        }
    }
    
    struct LameJuisSeqPaletteCell : public SmartGrid::Cell
    {
        Trio m_trio;
        LameJuisInternal* m_lameJuis;
        LameJuisInternal::Input* m_state;
        size_t m_ix;

        LameJuisSeqPaletteCell(
            Trio trio,
            LameJuisInternal* lameJuis,
            LameJuisInternal::Input* state,
            size_t ix)
            : m_trio(trio)
            , m_lameJuis(lameJuis)
            , m_state(state)
            , m_ix(ix)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            size_t trioIx = static_cast<size_t>(m_trio);
            LameJuisInternal::SeqPaletteState s = m_lameJuis->GetSeqPaletteState(trioIx, m_ix, m_state->m_inputVector);
            SmartGrid::Color c = BaseColor(s);
            if (!s.m_isCur)
            {
                return c.AdjustBrightness(0.6);
            }
            else
            {
                return c;
            }
        }

        SmartGrid::Color BaseColor(LameJuisInternal::SeqPaletteState s)
        {
            if (s.m_ordMax % 2 == 1 && s.m_ord == s.m_ordMax / 2)
            {
                return TrioColor(m_trio);
            }
            else
            {
                bool low = s.m_ord < s.m_ordMax / 2;
                size_t trioIx = static_cast<size_t>(m_trio);
                Trio o = static_cast<Trio>((low ? trioIx + 2 : trioIx + 1) % 3);
                float perc;
                if (low)
                {
                    perc = 0.5 + static_cast<float>(s.m_ord) / s.m_ordMax;
                }
                else
                {
                    perc = 1.0 - static_cast<float>(s.m_ord) / (2 * s.m_ordMax);
                }

                return TrioColor(o).Interpolate(TrioColor(m_trio), perc);
            }
        }
    };

    struct LameJuisSeqPalettePage : public SmartGrid::Grid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;
        Trio m_trio;
        LameJuisSeqPalettePage(TheNonagonSmartGrid* owner, Trio t)
            : SmartGrid::Grid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
            , m_trio(t)
        {
            SetColors(TrioColor(t), TrioColor(t).Dim());
            InitGrid();
        }

        void InitGrid()
        {
            for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
            {
                for (size_t j = 0; j < SmartGrid::x_baseGridSize; ++j)
                {
                    size_t ix = i * SmartGrid::x_baseGridSize + j;
                    Put(i, j, new LameJuisSeqPaletteCell(
                            m_trio,
                            &m_nonagon->m_lameJuis,
                            &m_state->m_lameJuisInput,
                            ix));
                }
            }
        }
    };
    
    struct IndexArpPage : public SmartGrid::Grid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;
        Trio m_trio;
        IndexArpPage(TheNonagonSmartGrid* owner, Trio t)
            : SmartGrid::Grid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
            , m_trio(t)
        {
            SetColors(TrioColor(t), TrioColor(t).Dim());
            InitGrid();
        }

        struct RhythmCell : public SmartGrid::Cell
        {
            IndexArp::Input* m_state;
            IndexArp* m_arp;
            int m_ix;
            bool* m_shift;
            Trio m_trio;
            SmartGrid::Color m_color;

            RhythmCell(
                IndexArp::Input* state,
                IndexArp* arp,
                int ix,
                bool* shift,
                Trio trio)
                : m_state(state)
                , m_arp(arp)
                , m_ix(ix)
                , m_shift(shift)
                , m_trio(trio)
            {
            }
            
            virtual void OnPress(uint8_t velocity) override
            {
                if (*m_shift)
                {
                    m_state->m_rhythmLength = m_ix + 1;
                }
                else
                {
                    m_state->m_rhythm[m_ix] = !m_state->m_rhythm[m_ix];
                }
            }
            
            virtual SmartGrid::Color GetColor() override
            {
                if (m_state->m_rhythmLength <= m_ix)
                {
                    return SmartGrid::Color::Off;
                }

                SmartGrid::Color result;
                if (m_arp->m_rhythmIndex != m_ix)
                {
                    result = SmartGrid::Color::White;
                }
                else
                {
                    result = FadeColor(m_trio, m_arp->m_index, m_arp->m_size);
                }

                if (!m_state->m_rhythm[m_ix])
                {
                    result = result.Dim();
                }

                return result;
            }            
        };

        void InitGrid()
        {
            size_t tId = static_cast<size_t>(m_trio);
            for (size_t i = 0; i < TheNonagonInternal::x_voicesPerTrio; ++i)
            {
                size_t voice = tId * TheNonagonInternal::x_voicesPerTrio + i;
                size_t yPos = i;

                Put(0, yPos, new SmartGrid::StateCell<bool>(
                        TrioColor(m_trio) /*offColor*/,                            
                        SmartGrid::Color::White /*onColor*/,
                        &m_state->m_arpInput.m_input[voice].m_up,
                        true,
                        false,
                        SmartGrid::StateCell<bool>::Mode::Toggle));
                Put(1, yPos, new SmartGrid::StateCell<bool>(
                        TrioColor(m_trio) /*offColor*/,                            
                        SmartGrid::Color::White /*onColor*/,
                        &m_state->m_arpInput.m_input[voice].m_cycle,
                        true,
                        false,
                        SmartGrid::StateCell<bool>::Mode::Toggle));

                for (size_t j = 0; j < 2; ++j)
                {
                    Put(2 + j, yPos, new SmartGrid::BinaryCell<int>(
                            FadeColor(m_trio, 1, 6) /*offColor*/,                            
                            FadeColor(m_trio, 1, 6).Interpolate(SmartGrid::Color::White, 0.7) /*onColor*/,
                            1 - j,
                            &m_state->m_indexArpOffsetSelect[voice]));
                    Put(4 + j, yPos, new SmartGrid::BinaryCell<int>(
                            FadeColor(m_trio, 2, 6) /*offColor*/,                            
                            FadeColor(m_trio, 2, 6).Interpolate(SmartGrid::Color::White, 0.7) /*onColor*/,
                            1 - j,
                            &m_state->m_indexArpIntervalSelect[voice]));
                    Put(6 + j, yPos, new SmartGrid::BinaryCell<int>(
                            FadeColor(m_trio, 3, 6) /*offColor*/,                            
                            FadeColor(m_trio, 3, 6).Interpolate(SmartGrid::Color::White, 0.7) /*onColor*/,
                            1 - j,
                            &m_state->m_indexArpMotiveIxSelect[voice]));                            
                }

                m_owner->m_stateSaver.Insert(
                    "IndexArpUp", voice, &m_state->m_arpInput.m_input[voice].m_up);
                m_owner->m_stateSaver.Insert(
                    "IndexArpCycle", voice, &m_state->m_arpInput.m_input[voice].m_cycle);
                m_owner->m_stateSaver.Insert(
                    "IndexArpOffset", voice, &m_state->m_indexArpOffsetSelect[voice]);
                m_owner->m_stateSaver.Insert(
                    "IndexArpInterval", voice, &m_state->m_indexArpIntervalSelect[voice]);
                m_owner->m_stateSaver.Insert(
                    "IndexArpMotiveInterval", voice, &m_state->m_indexArpMotiveIxSelect[voice]);
                
                for (size_t j = 0; j < SmartGrid::x_baseGridSize; ++j)
                {
                    Put(j, yPos + 3, new RhythmCell(
                            &m_state->m_arpInput.m_input[voice],
                            &m_nonagon->m_indexArp.m_arp[voice],
                            j,
                            &m_owner->m_shift,
                            m_trio));
                    m_owner->m_stateSaver.Insert(
                        "IndexArpRhythm", voice, j, &m_state->m_arpInput.m_input[voice].m_rhythm[j]);
                }

                m_owner->m_stateSaver.Insert(
                    "IndexArpRhythmLength", voice, &m_state->m_arpInput.m_input[voice].m_rhythmLength);

            }

            Put(7, 7, new SmartGrid::StateCell<bool>(
                SmartGrid::Color::White.Dim() /*offColor*/,
                SmartGrid::Color::White /*onColor*/,
                &m_owner->m_shift,
                true,
                false,
                SmartGrid::StateCell<bool>::Mode::Momentary));


            m_owner->m_stateSaver.Insert(
                "IndexArpClockSelect", tId, &m_state->m_arpInput.m_clockSelect[tId]);
            m_owner->m_stateSaver.Insert(
                "IndexArpResetSelect", tId, &m_state->m_arpInput.m_resetSelect[tId]);
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

        struct CountHighFlash
        {
            Trio m_trio;
            TheNonagonInternal* m_nonagon;
            size_t m_count;
            
            CountHighFlash(Trio t, TheNonagonInternal* nonagon, size_t count)
                : m_trio(t)
                , m_nonagon(nonagon)
                , m_count(count)
            {
            }

            bool IsFlashing()
            {
                size_t trioIx = static_cast<size_t>(m_trio);
                for (size_t i = TheNonagonInternal::x_voicesPerTrio * trioIx; i < TheNonagonInternal::x_voicesPerTrio * (trioIx + 1); ++i)
                {
                    if (m_nonagon->m_timbreAndMute.m_voices[i].m_countHigh == m_count &&
                        m_nonagon->m_multiPhasorGate.m_gate[i])
                    {
                        return true;
                    }
                }

                return false;
            }
        };

        void InitGrid()
        {
            size_t trioIx = static_cast<size_t>(m_trio);
            for (size_t i = 0; i < LameJuisInternal::x_numOperations; ++i)
            {                
                for (size_t j = 0; j < TheNonagonInternal::x_voicesPerTrio; ++j)
                {
                    size_t voiceIx = trioIx * TheNonagonInternal::x_voicesPerTrio + j;
                    SmartGrid::Color c = VoiceColor(m_trio, j);
                    Put(i, j, new SmartGrid::StateCell<bool, SmartGrid::BoolFlash>(
                            SmartGrid::Color::White.Dim() /*offColor*/,
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
                Put(i, 3, new SmartGrid::StateCell<bool, CountHighFlash>(
                        SmartGrid::Color::White.Dim() /*offColor*/,
                        SmartGrid::ColorScheme::Rainbow[i].Dim() /*onColor*/,
                        SmartGrid::Color::Grey /*offFlashColor*/,
                        SmartGrid::ColorScheme::Rainbow[i] /*onFlashColor*/,
                        &m_state->m_timbreAndMuteInput.m_canPassIfOn[trioIx][i],
                        CountHighFlash(m_trio, m_nonagon, i),
                        true,
                        false,
                        SmartGrid::StateCell<bool, CountHighFlash>::Mode::Toggle));
                m_owner->m_stateSaver.Insert(
                    "TimbreAndMuteCountHighSelect", trioIx, i, &m_state->m_timbreAndMuteInput.m_canPassIfOn[trioIx][i]);
            }            
            
            for (int i = -3; i < 3; ++i)
            {
                Put(5 + i, 4, new SmartGrid::StateCell<int>(
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
                Put(4 + i, 5, new SmartGrid::StateCell<TrioOctaveSwitches::Spread>(
                        TrioCompanionColor(m_trio),
                        SmartGrid::Color::White /*onColor*/,
                        &m_state->m_trioOctaveSwitchesInput.m_spread[trioIx],
                        spread,
                        spread,
                        SmartGrid::StateCell<TrioOctaveSwitches::Spread>::Mode::SetOnly));
            }

            SmartGrid::ColorScheme colorScheme(std::vector<SmartGrid::Color>({
                            SmartGrid::Color::White.Dim(),
                            TrioCompanionColor(m_trio),
                            SmartGrid::Color::White}));
            Put(6, 7, new SmartGrid::CycleCell<TimbreAndMute::MonoMode>(
                    colorScheme,
                    &m_state->m_timbreAndMuteInput.m_monoMode[trioIx]));

            m_owner->m_stateSaver.Insert(
                "MonoMode", trioIx, &m_state->m_timbreAndMuteInput.m_monoMode[trioIx]);

            
            m_owner->PlaceMutes(m_trio, 6, 7, this);
            
            m_owner->m_stateSaver.Insert(
                "TrioOctave", trioIx, &m_state->m_trioOctaveSwitchesInput.m_octave[trioIx]);
            m_owner->m_stateSaver.Insert(
                "TrioSpread", trioIx, &m_state->m_trioOctaveSwitchesInput.m_spread[trioIx]);            
        }
    };

    size_t m_theoryOfTimeTopologyGridId;
    size_t m_theoryOfTimeSwingGridId;
    size_t m_theoryOfTimeSwaggerGridId;
    size_t m_lameJuisCoMuteGridId;
    size_t m_lameJuisMatrixGridId;
    size_t m_lameJuisLHSGridId;
    size_t m_lameJuisIntervalGridId;
    size_t m_indexArpFireGridId;
    size_t m_indexArpEarthGridId;
    size_t m_indexArpWaterGridId;
    size_t m_lameJuisSeqPaletteFireGridId;
    size_t m_lameJuisSeqPaletteEarthGridId;
    size_t m_lameJuisSeqPaletteWaterGridId;
    size_t m_timbreAndMuteFireGridId;
    size_t m_timbreAndMuteEarthGridId;
    size_t m_timbreAndMuteWaterGridId;
    
    Trio m_activeTrio;
    
    TheNonagonSmartGrid()
        : m_savedJSON(nullptr)
    {
        InitState();
        InitGrid();
        m_stateSaver.Finalize();
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
            m_numClockSelectsHeld[i] = 0;
            m_numClockSelectsMaxHeld[i] = 0;
            m_shift = false;
            m_stateSaver.Insert("Mute", i, &m_state.m_mute[i]);                                
        }        
    }
    
    void InitGrid()
    {
        // TheoryOfTime
        //
        m_theoryOfTimeTopologyGridId = m_gridHolder.AddGrid(new TheoryOfTimeTopologyPage(this, false /*isPan*/));
        m_theoryOfTimeSwingGridId = m_gridHolder.AddGrid(new TheoryOfTimeSwingAndSwaggerPage(this, true));
        m_theoryOfTimeSwaggerGridId = m_gridHolder.AddGrid(new TheoryOfTimeSwingAndSwaggerPage(this, false));
        
        // LaMeJuIS
        //
        m_lameJuisCoMuteGridId = m_gridHolder.AddGrid(new LameJuisCoMutePage(this));
        m_lameJuisMatrixGridId = m_gridHolder.AddGrid(new LameJuisMatrixPage(this));
        m_lameJuisLHSGridId = m_gridHolder.AddGrid(new LameJuisLHSPage(this));
        m_lameJuisIntervalGridId = m_gridHolder.AddGrid(new LameJuisIntervalPage(this));
 
        // Sequencers
        //
        m_indexArpFireGridId = m_gridHolder.AddGrid(new IndexArpPage(this, Trio::Fire));
        m_indexArpEarthGridId = m_gridHolder.AddGrid(new IndexArpPage(this, Trio::Earth));
        m_indexArpWaterGridId = m_gridHolder.AddGrid(new IndexArpPage(this, Trio::Water));

        // Palettes
        //
        // m_lameJuisSeqPaletteFireGridId = m_gridHolder.AddGrid(new LameJuisSeqPalettePage(this, Trio::Fire));
        // m_lameJuisSeqPaletteEarthGridId = m_gridHolder.AddGrid(new LameJuisSeqPalettePage(this, Trio::Earth));
        // m_lameJuisSeqPaletteWaterGridId = m_gridHolder.AddGrid(new LameJuisSeqPalettePage(this, Trio::Water));
        
        // Articulation
        //
        m_timbreAndMuteFireGridId = m_gridHolder.AddGrid(new TimbreAndMuteSubPage(this, Trio::Fire));
        m_timbreAndMuteEarthGridId = m_gridHolder.AddGrid(new TimbreAndMuteSubPage(this, Trio::Earth));
        m_timbreAndMuteWaterGridId = m_gridHolder.AddGrid(new TimbreAndMuteSubPage(this, Trio::Water));

        m_stateSaver.Insert("ActiveTrio", &m_activeTrio);
    }

    void HandleSceneTrigger(bool shift, int scene)
    {
        if (shift)
        {
            m_stateSaver.CopyToScene(scene);
        }
        else
        {
            m_stateSaverState.ProcessTrigger(scene);
        }
    }
    
    void Process(float dt, uint64_t frame)
    {
        std::ignore = frame;
        m_stateSaver.Process(m_stateSaverState);
        m_gridHolder.Process(dt);
        m_nonagon.Process(dt, m_state);
    }
};

#ifndef IOS_BUILD
struct TheNonagon : Module
{
    TheNonagonSmartGrid m_nonagon;
    Trig m_saveTrig;
    Trig m_loadTrig;
    Trig m_sceneTrig[8];

    TheNonagon()
    {
        config(0, 7, 2 + 12 + 3 + 4 + 3, 0);

        configInput(0, "Save");
        configInput(1, "Load");
        configInput(2, "Radius");
        
        configOutput(0, "VoltPerOct Output");
        configOutput(1, "Gate Output");
        configOutput(14, "Recording Output");
        configOutput(15, "Timbre Output");
        configOutput(16, "Phasor Output");
        configOutput(20, "PanX");
        configOutput(21, "PanY");
        configOutput(22, "PanZ");
        configOutput(23, "Tot Phasor");

        for (size_t i = 0; i < 3; ++i)
        {
            configOutput(i + 17, ("Extra Timbre " + std::to_string(i)).c_str());
        }

        for (size_t i = 0; i < 12; ++i)
        {
            configOutput(i + 2, ("Aux Fader " + std::to_string(i)).c_str());
        }

        configInput(3, "Scene Blend");
        configInput(4, "Scene Select");
        configInput(5, "Scene Shift");

        configInput(6, "Speed");
    }

    json_t* dataToJson() override
    {
        if (m_nonagon.m_savedJSON)
        {
            return json_incref(m_nonagon.m_savedJSON);
        }
        else
        {
            return m_nonagon.ToJSON();
        }
    }

    void SaveJSON()
    {
        json_decref(m_nonagon.m_savedJSON);
        m_nonagon.m_savedJSON = nullptr;
        m_nonagon.m_savedJSON = dataToJson();
    }

    void LoadSavedJSON()
    {
        if (m_nonagon.m_savedJSON)
        {
            m_nonagon.SetFromJSON(m_nonagon.m_savedJSON);
        }
    }

    void dataFromJson(json_t* rootJ) override
    {
        m_nonagon.SetFromJSON(rootJ);
        m_nonagon.m_savedJSON = json_incref(rootJ);
    }

    void HandlesSceneInputs()
    {
        if (m_saveTrig.Process(inputs[0].getVoltage()))
        {
            SaveJSON();
        }
        
        if (m_loadTrig.Process(inputs[1].getVoltage()))
        {
            LoadSavedJSON();
        }
        
        m_nonagon.m_stateSaverState.m_blend = inputs[3].getVoltage() / 10;
        for (size_t i = 0; i < 8; ++i)
        {
            if (m_sceneTrig[i].Process(inputs[4].getVoltage(i)))
            {
                m_nonagon.HandleSceneTrigger(inputs[5].getVoltage() > 0, i);
            }
        }
    }
    
    void process(const ProcessArgs &args) override
    {
        HandlesSceneInputs();
        outputs[0].setChannels(TheNonagonInternal::x_numVoices);
        outputs[1].setChannels(TheNonagonInternal::x_numVoices);
        outputs[15].setChannels(TheNonagonInternal::x_numVoices);
        outputs[16].setChannels(TheNonagonInternal::x_numVoices);
        outputs[17].setChannels(TheNonagonInternal::x_numVoices);
        outputs[18].setChannels(TheNonagonInternal::x_numVoices);
        outputs[19].setChannels(TheNonagonInternal::x_numVoices);
        outputs[20].setChannels(TheNonagonInternal::x_numVoices);
        outputs[21].setChannels(TheNonagonInternal::x_numVoices);
        outputs[22].setChannels(TheNonagonInternal::x_numVoices);
        outputs[23].setChannels(6);
        
        m_nonagon.SetFrequency(inputs[6].getVoltage());
        m_nonagon.Process(args.sampleTime, args.frame);

        for (size_t i = 0; i < TheNonagonInternal::x_numVoices; ++i)
        {
            outputs[0].setVoltage(m_nonagon.m_nonagon.m_output.m_voltPerOct[i], i);
            outputs[1].setVoltage(m_nonagon.m_nonagon.m_output.m_gate[i] ? 10 : 0, i);
            outputs[15].setVoltage(m_nonagon.m_nonagon.m_output.m_timbre[i] * 10, i);
            outputs[16].setVoltage(m_nonagon.m_nonagon.m_output.m_phasor[i] * 10, i);
            for (size_t j = 0; j < 3; ++j)
            {
                outputs[17 + j].setVoltage(m_nonagon.m_nonagon.m_output.m_extraTimbre[i][j] * 10, i);
            }            
        }

        outputs[2].setVoltage(m_nonagon.m_theoryOfTimeTopologyGridId, 0);
        outputs[3].setVoltage(m_nonagon.m_theoryOfTimeSwingGridId);
        outputs[4].setVoltage(m_nonagon.m_theoryOfTimeSwaggerGridId);
        outputs[5].setVoltage(m_nonagon.m_lameJuisCoMuteGridId, 0);
        outputs[6].setVoltage(m_nonagon.m_lameJuisMatrixGridId);
        outputs[7].setVoltage(m_nonagon.m_lameJuisLHSGridId);
        outputs[8].setVoltage(m_nonagon.m_lameJuisIntervalGridId);

        // outputs[9].setChannels(2);
        // outputs[9].setVoltage(m_nonagon.m_state.m_running ? 10 : 0, 0);
        // float pos = m_nonagon.m_nonagon.m_theoryOfTime.m_musicalTime.m_bits[0].m_pos;
        // pos *= 32 * 24;
        // bool clk = pos - floor(pos) < 0.5;
        
        // outputs[9].setVoltage(clk ? 10 : 0, 1);
        outputs[9].setChannels(16);
        for (size_t i = 0; i < 16; ++i)
        {
            outputs[9].setVoltage(SmartGrid::Color(0, std::min<int>(255, 256 * m_nonagon.m_nonagon.GetTimerFragment(i)), 0).ZEncodeFloat() * 10, i);
        }

        outputs[11].setChannels(3);
        outputs[12].setChannels(3);
        outputs[13].setChannels(3);

        outputs[11].setVoltage(m_nonagon.m_lameJuisSeqPaletteFireGridId, 0);
        outputs[11].setVoltage(m_nonagon.m_lameJuisSeqPaletteEarthGridId, 1);
        outputs[11].setVoltage(m_nonagon.m_lameJuisSeqPaletteWaterGridId, 2);
        outputs[12].setVoltage(m_nonagon.m_timbreAndMuteFireGridId, 0);
        outputs[12].setVoltage(m_nonagon.m_timbreAndMuteEarthGridId, 1);
        outputs[12].setVoltage(m_nonagon.m_timbreAndMuteWaterGridId, 2);
        outputs[13].setVoltage(m_nonagon.m_indexArpFireGridId, 0);
        outputs[13].setVoltage(m_nonagon.m_indexArpEarthGridId, 1);
        outputs[13].setVoltage(m_nonagon.m_indexArpWaterGridId, 2);

        outputs[14].setVoltage(m_nonagon.m_nonagon.m_output.m_recording ? 10 : 0);

        for (size_t i = 0; i < 6; ++i)
        {
            outputs[23].setVoltage(10 * m_nonagon.m_nonagon.m_output.m_totPhasors[i], i);
        }
    }
};

struct TheNonagonWidget : ModuleWidget
{
    TheNonagonWidget(TheNonagon* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/TheNonagon.svg")));

        addInput(createInputCentered<PJ301MPort>(Vec(250, 100), module, 0));
        addInput(createInputCentered<PJ301MPort>(Vec(250, 150), module, 1));
        addInput(createInputCentered<PJ301MPort>(Vec(250, 200), module, 2));
        
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

        addOutput(createOutputCentered<PJ301MPort>(Vec(125, 250), module, 17));
        addOutput(createOutputCentered<PJ301MPort>(Vec(125, 275), module, 18));
        addOutput(createOutputCentered<PJ301MPort>(Vec(125, 300), module, 19));                
        addOutput(createOutputCentered<PJ301MPort>(Vec(200, 100), module, 20));
        addOutput(createOutputCentered<PJ301MPort>(Vec(200, 125), module, 21));
        addOutput(createOutputCentered<PJ301MPort>(Vec(200, 150), module, 22));
        addOutput(createOutputCentered<PJ301MPort>(Vec(200, 200), module, 23));

        addInput(createInputCentered<PJ301MPort>(Vec(250, 250), module, 3));
        addInput(createInputCentered<PJ301MPort>(Vec(250, 275), module, 4));
        addInput(createInputCentered<PJ301MPort>(Vec(250, 300), module, 5));
        addInput(createInputCentered<PJ301MPort>(Vec(275, 300), module, 6));
    }
};
#endif
    
