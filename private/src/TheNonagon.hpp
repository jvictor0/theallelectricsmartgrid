#pragma once

#include "SmartGrid.hpp"
#include "GridJnct.hpp"
#include "TheoryOfTime.hpp"
#include "LameJuis.hpp"
#include "plugin.hpp"
#include "StateSaver.hpp"
#include "MultiPhasorGate.hpp"
#include "PercentileSequencer.hpp"
#include "TrioOctaveSwitches.hpp"
#include "Trig.hpp"
#include "ClockSelectCell.hpp"
#include "GangedRandomLFO.hpp"
#include "Slew.hpp"
#include "Tick2Phasor.hpp"
#include "MessageOut.hpp"
#include "PhaseUtils.hpp"

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
        TheoryOfTime::Input m_theoryOfTimeInput;
        LameJuisInternal::Input m_lameJuisInput;
        MultiPhasorGateInternal::NonagonTrigLogic m_trigLogic;
        MultiPhasorGateInternal::Input m_multiPhasorGateInput;
        NonagonIndexArp::Input m_arpInput;
        TrioOctaveSwitches::Input m_trioOctaveSwitchesInput;

        int m_indexArpIntervalSelect[x_numVoices];
        int m_indexArpOffsetSelect[x_numVoices];
        int m_indexArpMotiveIxSelect[x_numVoices];
        
        float m_extraTimbre[x_numTrios][x_numExtraTimbres];
        bool m_running;
        bool m_shift;
        
        Input()
            : m_running(false)
            , m_shift(false)
        {
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

    struct UIState
    {
        TheoryOfTime::UIState m_theoryOfTimeUIState;
        std::atomic<int> m_loopMultiplier[x_numTrios];
        std::atomic<bool> m_gate[x_numVoices];
        std::atomic<bool> m_muted[x_numVoices];
        std::atomic<float> m_cyclesPerSamplePitch[x_numVoices];

        int GetLoopMultiplier(int trioIndex)
        {
            return m_loopMultiplier[trioIndex].load();
        }

        void SetLoopMultiplier(int trioIndex, int value)
        {
            m_loopMultiplier[trioIndex].store(value);
        }

        void SetGate(size_t i, bool gate)
        {
            m_gate[i].store(gate);
        }

        void SetMuted(size_t i, bool muted)
        {
            m_muted[i].store(muted);
        }

        void SetCyclesPerSamplePitch(size_t i, float cyclesPerSample)
        {
            m_cyclesPerSamplePitch[i].store(cyclesPerSample);
        }
    };
    
    NonagonNoteWriter m_noteWriter;

    TheoryOfTime m_theoryOfTime;
    LameJuisInternal m_lameJuis;
    MultiPhasorGateInternal m_multiPhasorGate;
    NonagonIndexArp m_indexArp;

    TheNonagonInternal()
    {
    }
    
    struct Output
    {
        bool m_gate[x_numVoices];
        float m_phasor[x_numVoices];
        float m_voltPerOct[x_numVoices];
        float m_gridId[x_numGridIds];
        float m_extraTimbreTrg[x_numVoices][x_numExtraTimbres];
        float m_extraTimbre[x_numVoices][x_numExtraTimbres];
        FixedSlew m_extraTimbreSlew[x_numVoices][x_numExtraTimbres];
        float m_totPhasors[x_numTimeBits];
        bool m_totTop[x_numTimeBits];

        Output()
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_gate[i] = false;
                m_voltPerOct[i] = 0;
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
                m_totTop[i] = false;
            }
        }
    };

    Output m_output;
    SmartGrid::MessageOutBuffer* m_messageOutBuffer;

    struct TimeBit
    {
        bool m_bit;
    };

    void SetTheoryOfTimeInput(Input& input)
    {
        input.m_theoryOfTimeInput.m_running = input.m_running || m_multiPhasorGate.m_anyGate;
    }

    void SetLameJuisInput(Input& input)
    {
        for (size_t i = 0; i < x_numTimeBits; ++i)
        {
            input.m_lameJuisInput.m_inputBitInput[i].m_value = m_theoryOfTime.m_loops[i].m_gate;
        }

        for (size_t i = 0; i < x_numTrios; ++i)
        {
            for (size_t j = 0; j < x_voicesPerTrio; ++j)
            {
                if (input.m_arpInput.m_input[i * x_voicesPerTrio + j].m_read
                    || m_indexArp.m_arp[i * x_voicesPerTrio + j].m_triggered)
                {
                    input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_usePercentile[j] = input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_harmonic[0];
                    input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_harmonic[j] = input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_harmonic[0];
                }

                input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_toQuantize[j] = m_indexArp.m_arp[i * x_voicesPerTrio + j].m_output * 4;
                input.m_lameJuisInput.m_outputInput[i].m_coMuteInput.m_percentiles[j] = m_indexArp.m_arp[i * x_voicesPerTrio + j].m_output;
            }
        }
    }

    bool* OutBit(size_t i)
    {
        return &m_lameJuis.m_operations[i].m_gate;
    }

    void SetMultiPhasorGateInputs(Input& input)
    {
        input.m_trigLogic.m_running = input.m_running;
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            bool pitchChanged = m_lameJuis.m_outputs[i / x_voicesPerTrio].m_trigger[i % x_voicesPerTrio];
            bool anyChange = m_theoryOfTime.m_anyChange;
            input.m_trigLogic.m_pitchChanged[i] = anyChange && pitchChanged;
            input.m_trigLogic.m_earlyMuted[i] = m_lameJuis.m_outputs[i / x_voicesPerTrio].m_pitch[i % x_voicesPerTrio].m_isMuted;
            input.m_trigLogic.m_subTrigger[i] = m_indexArp.m_arp[i].m_triggered && anyChange;
        }

        input.m_trigLogic.SetInput(input.m_multiPhasorGateInput);

        input.m_multiPhasorGateInput.m_phasor = m_theoryOfTime.GetMasterLoop()->m_phasor;

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            int denom = 1;
            int voiceClock = input.m_arpInput.m_clockSelect[i / x_voicesPerTrio];
            if (voiceClock >= 0)
            {
                denom = m_theoryOfTime.GetLoopInternalMultiplier(voiceClock);
            }

            for (size_t j = 0; j < x_numTimeBits; ++j)
            {                    
                bool coMute = m_lameJuis.m_outputs[i / x_voicesPerTrio].m_coMuteState.m_coMutes[j];
                if (!coMute)
                {
                    denom = std::lcm(denom, m_theoryOfTime.GetLoopInternalMultiplier(j));
                }
            }

            input.m_multiPhasorGateInput.m_phasorDenominator[i] = denom;
        }
    }

    void SetIndexArpInputs(Input& input)
    {
        for (size_t j = 0; j < x_numVoices; ++j)
        {
            input.m_arpInput.m_input[j].m_read = false;
        }

        for (size_t i = 0; i < x_numTimeBits; ++i)
        {            
            bool ticked = m_theoryOfTime.m_loops[i].m_gateChanged;
            input.m_arpInput.m_clocks[i] = ticked;
            if (m_theoryOfTime.m_anyChange)
            {
                for (size_t j = 0; j < x_numVoices; ++j)
                {                    
                    bool coMute = m_lameJuis.m_outputs[j / x_voicesPerTrio].m_coMuteState.m_coMutes[i];
                    if (!coMute && ticked)
                    {
                        input.m_arpInput.m_input[j].m_read = true;
                    }
                }
            }
        }

        if (m_theoryOfTime.m_anyChange)
        {
            for (size_t j = 0; j < x_numTrios; ++j)
            {                    
                if (input.m_arpInput.m_clockSelect[j] < 0)
                {
                    input.m_arpInput.m_totalIndex[j] = 0;
                }
                else if (m_theoryOfTime.m_loops[input.m_arpInput.m_clockSelect[j]].m_gateChanged)
                {
                    input.m_arpInput.m_totalIndex[j] = m_theoryOfTime.MonodromyNumber(
                        input.m_arpInput.m_clockSelect[j],
                        input.m_arpInput.m_resetSelect[j]);
                }
            }
        }
    }

    void SetOutputs(Input& input)
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            if (m_multiPhasorGate.m_adspControl[i].m_trig)
            {                                
                m_output.m_gate[i] = true;
                size_t pitchIx = input.m_trigLogic.m_unisonMaster[i / x_voicesPerTrio] == -1 ? i : input.m_trigLogic.m_unisonMaster[i / x_voicesPerTrio];
                auto& result = m_lameJuis.m_outputs[i / x_voicesPerTrio].m_pitch[pitchIx % x_voicesPerTrio];
                float preOctave = result.m_pitch;
                m_output.m_voltPerOct[i] = input.m_trioOctaveSwitchesInput.Octavize(preOctave, i);
                    
                for (size_t j = 0; j < x_numExtraTimbres; ++j)
                {
                    m_output.m_extraTimbreTrg[i][j] = result.Timbre(j);
                }

                NonagonNoteWriter::EventData eventData;
                eventData.m_voiceIx = i;
                eventData.m_voltPerOct = m_output.m_voltPerOct[i];
                eventData.m_startPosition = m_theoryOfTime.m_phasorIndependent;
                for (size_t j = 0; j < x_numExtraTimbres; ++j)
                {
                    eventData.m_timbre[j] = m_output.m_extraTimbre[i][j];
                }

                m_noteWriter.RecordNote(eventData);
            }
            else if (!m_multiPhasorGate.m_gate[i])
            {
                if (m_output.m_gate[i])
                {
                    m_noteWriter.RecordNoteEnd(i, m_theoryOfTime.m_phasorIndependent);
                }
                
                m_output.m_gate[i] = false;
            }

            m_output.m_phasor[i] = m_multiPhasorGate.m_adspControl[i].m_phasor;

            for (size_t j = 0; j < x_numExtraTimbres; ++j)
            {
                m_output.m_extraTimbre[i][j] = m_output.m_extraTimbreSlew[i][j].Process(m_output.m_extraTimbreTrg[i][j]);
            }
        }

        for (size_t i = 0; i < x_numTimeBits; ++i)
        {
            m_output.m_totPhasors[i] = m_theoryOfTime.m_loops[i].m_phasor;
            m_output.m_totTop[i] = m_theoryOfTime.m_loops[i].m_topIndependent;
        }
    }

    void Process(Input& input)
    {
        SetTheoryOfTimeInput(input);
        m_theoryOfTime.Process(input.m_theoryOfTimeInput);

        if (m_theoryOfTime.m_topIndependent)
        {
            m_noteWriter.RecordStartIndex();
        }

        if (m_theoryOfTime.m_anyChange)
        {
            SetIndexArpInputs(input);
            m_indexArp.Process(input.m_arpInput);
            
            SetLameJuisInput(input);
            m_lameJuis.Process(input.m_lameJuisInput);
        }

        if (m_theoryOfTime.m_running)
        {
            SetMultiPhasorGateInputs(input);
            m_multiPhasorGate.Process(input.m_multiPhasorGateInput);
        }
        else
        {
            m_multiPhasorGate.Reset();
        }

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

    JSON ToJSON()
    {
        return m_stateSaver.ToJSON();
    }    

    void FromJSON(JSON jin)
    {
        m_stateSaver.SetFromJSON(jin);
    }
    
    SmartGrid::Cell* TimeBitCell(size_t ix)
    {
        return new SmartGrid::StateCell<bool>(
                        SmartGrid::Color::Off /*offColor*/,
                        SmartGrid::Color::White /*onColor*/,
                        &m_nonagon.m_theoryOfTime.m_loops[ix].m_gate,
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
        Water = 0,
        Earth = 1,
        Fire = 2,
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
            int xPos = (i == 0) ? x : x + 1;
            int yPos = (i < 2) ? y - 1 : y;
            grid->Put(xPos, yPos, MakeMuteCell(t, i));
        }
    }

    SmartGrid::Cell* MakeMuteCell(Trio t, size_t voiceOffset)
    {
        size_t voiceIx = static_cast<size_t>(t) * TheNonagonInternal::x_voicesPerTrio + voiceOffset;
        return new SmartGrid::StateCell<bool, SmartGrid::BoolFlash>(
            SmartGrid::Color::White.Dim(),
            VoiceColor(voiceIx),
            SmartGrid::Color::Grey,
            TrioColor(t),
            &m_state.m_trigLogic.m_mute[voiceIx],
            SmartGrid::BoolFlash(&m_nonagon.m_multiPhasorGate.m_preGate[voiceIx]),
            false,
            true,
            SmartGrid::StateCell<bool, SmartGrid::BoolFlash>::Mode::Toggle);
    }
    
    struct TheoryOfTimeTopologyPage : public SmartGrid::CompositeGrid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;
        bool m_isPan;
        TheoryOfTimeBase::Input* m_timeState;
        
        TheoryOfTimeTopologyPage(TheNonagonSmartGrid* owner, bool isPan)
            : SmartGrid::CompositeGrid()
            , m_state(&owner->m_state)
            , m_nonagon(&owner->m_nonagon)
            , m_owner(owner)
            , m_isPan(isPan)
        {
            SetColors(SmartGrid::Color::Fuscia, SmartGrid::Color::Fuscia.Dim());
            m_timeState = &m_state->m_theoryOfTimeInput;

            InitGrid();
        }
        
        void InitGrid()
        {
            for (size_t i = 0; i < TheNonagonInternal::x_numTimeBits - 1; ++i)
            {
                size_t xPos = i;

                for (size_t mult = 2; mult <= 5; ++mult)
                {
                    Put(
                        xPos,
                        mult - 2,
                        new SmartGrid::StateCell<int>(
                            SmartGrid::Color::Fuscia /*offColor*/,
                            SmartGrid::Color::White /*onColor*/,
                            &m_timeState->m_input[i].m_parentMult,
                            mult,
                            1,
                            SmartGrid::StateCell<int>::Mode::Toggle));
                }
                
                Put(xPos, 6, new SmartGrid::StateCell<bool>(
                        SmartGrid::Color::Ocean /*offColor*/,
                        SmartGrid::Color::White /*onColor*/,
                        &m_timeState->m_input[i].m_pingPong,
                        true,
                        false,
                        SmartGrid::StateCell<bool>::Mode::Toggle));
                
                m_owner->m_stateSaver.Insert(
                    "TheoryOfTimeMult", i, &m_timeState->m_input[i].m_parentMult);
                m_owner->m_stateSaver.Insert(
                    "TheoryOfTimePingPong", i, &m_timeState->m_input[i].m_pingPong);
                
                if (i < TheNonagonInternal::x_numTimeBits - 2)
                {
                    Put(xPos, 4, new SmartGrid::StateCell<int>(
                            SmartGrid::Color::Ocean /*offColor*/,
                            SmartGrid::Color::White /*onColor*/,
                            &m_timeState->m_input[i].m_parentIndex,
                            i + 2,
                            i + 1,
                            SmartGrid::StateCell<int>::Mode::Toggle));
                    m_owner->m_stateSaver.Insert(
                        "TheoryOfTimeParentIx", i, &m_timeState->m_input[i].m_parentIndex);
                }
                
                Put(xPos, 7, m_owner->TimeBitCell(i));
            }

            Put(6, 7, m_owner->TimeBitCell(TheNonagonInternal::x_numTimeBits));            
        }
    };

    void SetFrequency(float voct)
    {
        m_state.m_theoryOfTimeInput.m_freq = pow(2, voct) / 128;
    }

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
            for (size_t i = 0; i < TheNonagonInternal::x_numTimeBits; ++i)
            {
                Put(i, 7, m_owner->TimeBitCell(i));
                if (i < TheNonagonInternal::x_numTimeBits - 2)
                {
                    Put(i, 6, new SmartGrid::StateCell<int>(
                            SmartGrid::Color::Ocean /*offColor*/,
                            SmartGrid::Color::White /*onColor*/,
                            &m_state->m_theoryOfTimeInput.m_input[i].m_parentIndex,
                            i + 2,
                            i + 1,
                            SmartGrid::StateCell<int>::Mode::Toggle));
                }
                    
                for (Trio t : {Trio::Fire, Trio::Earth, Trio::Water})
                {
                    size_t tId = static_cast<size_t>(t);
                    size_t coMuteY = tId * 2;
                    size_t seqY = coMuteY + 1;

                    Put(i, coMuteY, new SmartGrid::StateCell<bool>(
                            SmartGrid::Color::White /*offColor*/,
                            TrioColor(t) /*onColor*/,                            
                            &m_state->m_lameJuisInput.m_outputInput[tId].m_coMuteInput.m_coMutes[i],
                            true,
                            false,
                            SmartGrid::StateCell<bool>::Mode::Toggle));
                    m_owner->m_stateSaver.Insert(
                        "LameJuisCoMute", i, tId, &m_state->m_lameJuisInput.m_outputInput[tId].m_coMuteInput.m_coMutes[i]);

                    Put(i, seqY, m_owner->MkClockSelectCell(t, i));

                    m_owner->PlaceMutes(t, 6, seqY, this);
                }                                
            }

            if (m_owner->m_isStandalone)
            {
                Put(7, 7, m_owner->StartStopCell());
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

    struct LameJuisRHSPage : public SmartGrid::Grid
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
        LameJuisRHSPage(TheNonagonSmartGrid* owner)
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
                        "LameJuisRHS", i, j, &m_state->m_lameJuisInput.m_operationInput[i].m_direct[j]);
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
    

    struct SheafViewCell : public SmartGrid::Cell
    {
        int m_x;
        int m_y;
        Trio m_trio;
        TheNonagonSmartGrid* m_owner;

        SheafViewCell(int x, int y, Trio trio, TheNonagonSmartGrid* owner)
            : m_x(x)
            , m_y(y)
            , m_trio(trio)
            , m_owner(owner)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            size_t outputId = static_cast<size_t>(m_trio);
            LameJuisInternal::GridSheafView::CellInfo cellInfo = m_owner->m_nonagon.m_lameJuis.GetCellInfo(outputId, static_cast<uint8_t>(m_x), static_cast<uint8_t>(m_y));

            size_t rotateOffset = cellInfo.m_localHarmonicPosition.RotateAvoidOffset(outputId);
            SmartGrid::Color baseColor = SmartGrid::Color(
                cellInfo.m_localHarmonicPosition.Timbre256((rotateOffset + 0) % TheNonagonInternal::x_numTrios),
                cellInfo.m_localHarmonicPosition.Timbre256((rotateOffset + 1) % TheNonagonInternal::x_numTrios),
                cellInfo.m_localHarmonicPosition.Timbre256((rotateOffset + 2) % TheNonagonInternal::x_numTrios));
            
            baseColor = baseColor.Interpolate(TrioColor(m_trio), 0.3);

            if (cellInfo.m_isMuted)
            {
                return SmartGrid::Color::Off;
            }
            else if (!cellInfo.m_isCurrentSlice)
            {
                 return baseColor.AdjustBrightness(0.20);
            }
            else
            {
                for (size_t i = 0; i < TheNonagonInternal::x_voicesPerTrio; ++i)
                {
                    size_t voiceIndex = outputId * TheNonagonInternal::x_voicesPerTrio + i;
                    if (cellInfo.m_isPlaying[i] && m_owner->m_nonagon.m_output.m_gate[voiceIndex])
                    {
                        return baseColor;
                    }
                }

                return baseColor.AdjustBrightness(0.66);
            }
        }

        virtual void OnPress(uint8_t velocity) override
        {
            m_owner->m_nonagon.m_lameJuis.ToggleSheafMute(static_cast<size_t>(m_trio), m_x, m_y);
        }
    };

    struct SheafViewGrid : public SmartGrid::Grid
    {
        TheNonagonInternal::Input* m_state;
        TheNonagonInternal* m_nonagon;
        TheNonagonSmartGrid* m_owner;
        Trio m_trio;
        
        SheafViewGrid(TheNonagonSmartGrid* owner, Trio t)
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
                    Put(i, j, new SheafViewCell(SmartGrid::x_baseGridSize - i - 1, SmartGrid::x_baseGridSize - j - 1, m_trio, m_owner));
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
                    result = TrioColor(m_trio);
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
                
                for (size_t j = 0; j < SmartGrid::x_baseGridSize; ++j)
                {
                    Put(j, yPos, new RhythmCell(
                            &m_state->m_arpInput.m_input[voice],
                            &m_nonagon->m_indexArp.m_arp[voice],
                            j,
                            &m_state->m_shift,
                            m_trio));
                    m_owner->m_stateSaver.Insert(
                        "IndexArpRhythm", voice, j, &m_state->m_arpInput.m_input[voice].m_rhythm[j]);
                }

                m_owner->m_stateSaver.Insert(
                    "IndexArpRhythmLength", voice, &m_state->m_arpInput.m_input[voice].m_rhythmLength);

            }

            if (m_owner->m_isStandalone)
            {
                Put(7, 7, new SmartGrid::StateCell<bool>(
                        SmartGrid::Color::White.Dim() /*offColor*/,
                        SmartGrid::Color::White /*onColor*/,
                        &m_owner->m_state.m_shift,
                        true,
                        false,
                        SmartGrid::StateCell<bool>::Mode::Momentary));
            }

            for (int i = -3; i < 3; ++i)
            {
                Put(5 + i, 6, new SmartGrid::StateCell<int>(
                        TrioCompanionColor(m_trio),
                        SmartGrid::Color::White /*onColor*/,
                        &m_state->m_trioOctaveSwitchesInput.m_octave[tId],
                        i,
                        0,
                        SmartGrid::StateCell<int>::Mode::SetOnly));
            }

            for (size_t i = 0; i < 4; ++i)
            {
                TrioOctaveSwitches::Spread spread = static_cast<TrioOctaveSwitches::Spread>(i);
                Put(3 + i, 7, new SmartGrid::StateCell<TrioOctaveSwitches::Spread>(
                        TrioCompanionColor(m_trio),
                        SmartGrid::Color::White /*onColor*/,
                        &m_state->m_trioOctaveSwitchesInput.m_spread[tId],
                        spread,
                        spread,
                        SmartGrid::StateCell<TrioOctaveSwitches::Spread>::Mode::SetOnly));
            }

            for (size_t i = 0; i < TheNonagonInternal::x_numTrios; ++i)
            {
                for (size_t j = 0; j <= i; ++j)
                {
                    Put(j, 3 + i, new SmartGrid::StateCell<bool>(
                            TrioColor(static_cast<Trio>(i)).Dim() /*offColor*/,
                            TrioColor(static_cast<Trio>(i)) /*onColor*/,
                            &m_state->m_trigLogic.m_interrupt[i][j],
                            true,
                            false,
                            SmartGrid::StateCell<bool>::Mode::Toggle));
                }
            }

            Put(0, 6, new SmartGrid::StateCell<bool>(
                    SmartGrid::Color::Fuscia.Dim() /*offColor*/,
                    SmartGrid::Color::Fuscia /*onColor*/,
                    &m_state->m_lameJuisInput.m_outputInput[tId].m_coMuteInput.m_harmonic[0],
                    true,
                    false,
                    SmartGrid::StateCell<bool>::Mode::Toggle));

            Put(0, 7, new SmartGrid::StateCell<bool>(
                    SmartGrid::Color::White.Dim() /*offColor*/,
                    SmartGrid::Color::White /*onColor*/,
                    &m_state->m_trigLogic.m_trigOnPitchChanged[tId],
                    true,
                    false,
                    SmartGrid::StateCell<bool>::Mode::Toggle));

            Put(1, 7, new SmartGrid::StateCell<bool>(
                    SmartGrid::Color::White.Dim() /*offColor*/,
                    SmartGrid::Color::White /*onColor*/,
                    &m_state->m_trigLogic.m_trigOnSubTrigger[tId],
                    true,
                    false,
                    SmartGrid::StateCell<bool>::Mode::Toggle));

            for (size_t i = 0; i < TheNonagonInternal::x_voicesPerTrio; ++i)
            {
                Put(7, 3 + i, new SmartGrid::StateCell<int>(
                        SmartGrid::Color::Orange.Dim() /*offColor*/,
                        SmartGrid::Color::Orange /*onColor*/,
                        &m_state->m_trigLogic.m_unisonMaster[tId],
                        TheNonagonInternal::x_voicesPerTrio * tId + i,
                        -1,
                        SmartGrid::StateCell<int>::Mode::Toggle));
            }
                        
            m_owner->m_stateSaver.Insert(
                "TrioOctave", tId, &m_state->m_trioOctaveSwitchesInput.m_octave[tId]);
            m_owner->m_stateSaver.Insert(
                "TrioSpread", tId, &m_state->m_trioOctaveSwitchesInput.m_spread[tId]);            

            m_owner->m_stateSaver.Insert(
                "IndexArpClockSelect", tId, &m_state->m_arpInput.m_clockSelect[tId]);
            m_owner->m_stateSaver.Insert(
                "IndexArpResetSelect", tId, &m_state->m_arpInput.m_resetSelect[tId]);

            m_owner->m_stateSaver.Insert(
                "UnisonMaster", tId, &m_state->m_trigLogic.m_unisonMaster[tId]);

            m_owner->m_stateSaver.Insert(
                "IndexArpHarmonicMode", tId, &m_state->m_lameJuisInput.m_outputInput[tId].m_coMuteInput.m_harmonic[0]);
            m_owner->m_stateSaver.Insert(
                "TrigOnPitchChanged", tId, &m_state->m_trigLogic.m_trigOnPitchChanged[tId]);
            m_owner->m_stateSaver.Insert(
                "TrigOnSubTrigger", tId, &m_state->m_trigLogic.m_trigOnSubTrigger[tId]);
        }
    };

    size_t m_theoryOfTimeTopologyGridId;
    SmartGrid::Grid* m_theoryOfTimeTopologyGrid;
    size_t m_lameJuisCoMuteGridId;
    SmartGrid::Grid* m_lameJuisCoMuteGrid;
    size_t m_lameJuisMatrixGridId;
    SmartGrid::Grid* m_lameJuisMatrixGrid;
    size_t m_lameJuisRHSGridId;
    SmartGrid::Grid* m_lameJuisRHSGrid;
    size_t m_lameJuisIntervalGridId;
    SmartGrid::Grid* m_lameJuisIntervalGrid;
    size_t m_indexArpFireGridId;
    SmartGrid::Grid* m_indexArpFireGrid;
    size_t m_indexArpEarthGridId;
    SmartGrid::Grid* m_indexArpEarthGrid;
    size_t m_indexArpWaterGridId;
    SmartGrid::Grid* m_indexArpWaterGrid;
    size_t m_sheafViewGridFireGridId;
    SmartGrid::Grid* m_sheafViewGridFireGrid;
    size_t m_sheafViewGridEarthGridId;
    SmartGrid::Grid* m_sheafViewGridEarthGrid;
    size_t m_sheafViewGridWaterGridId;
    SmartGrid::Grid* m_sheafViewGridWaterGrid;

    bool m_isStandalone;

    SmartGrid::MessageOutBuffer* m_messageOutBuffer;
    
    TheNonagonSmartGrid(bool isStandalone)
        : m_isStandalone(isStandalone)
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
        for (size_t i = 0; i < TheNonagonInternal::x_numVoices; ++i)
        {
            m_numClockSelectsHeld[i] = 0;
            m_numClockSelectsMaxHeld[i] = 0;
            m_stateSaver.Insert("Mute", i, &m_state.m_trigLogic.m_mute[i]);                                
        }
    }
    
    void InitGrid()
    {
        // TheoryOfTime
        //
        m_theoryOfTimeTopologyGrid = new TheoryOfTimeTopologyPage(this, false /*isPan*/);
        m_theoryOfTimeTopologyGridId = m_gridHolder.AddGrid(m_theoryOfTimeTopologyGrid);
        
        // LaMeJuIS
        //
        m_lameJuisCoMuteGrid = new LameJuisCoMutePage(this);
        m_lameJuisCoMuteGridId = m_gridHolder.AddGrid(m_lameJuisCoMuteGrid);
        
        m_lameJuisMatrixGrid = new LameJuisMatrixPage(this);
        m_lameJuisMatrixGridId = m_gridHolder.AddGrid(m_lameJuisMatrixGrid);
        
        m_lameJuisRHSGrid = new LameJuisRHSPage(this);
        m_lameJuisRHSGridId = m_gridHolder.AddGrid(m_lameJuisRHSGrid);
        
        m_lameJuisIntervalGrid = new LameJuisIntervalPage(this);
        m_lameJuisIntervalGridId = m_gridHolder.AddGrid(m_lameJuisIntervalGrid);
 
        // Sequencers
        //
        m_indexArpFireGrid = new IndexArpPage(this, Trio::Fire);
        m_indexArpFireGridId = m_gridHolder.AddGrid(m_indexArpFireGrid);
        
        m_indexArpEarthGrid = new IndexArpPage(this, Trio::Earth);
        m_indexArpEarthGridId = m_gridHolder.AddGrid(m_indexArpEarthGrid);
        
        m_indexArpWaterGrid = new IndexArpPage(this, Trio::Water);
        m_indexArpWaterGridId = m_gridHolder.AddGrid(m_indexArpWaterGrid);

        // Palettes
        //
        m_sheafViewGridFireGrid = new SheafViewGrid(this, Trio::Fire);
        m_sheafViewGridFireGridId = m_gridHolder.AddGrid(m_sheafViewGridFireGrid);
        
        m_sheafViewGridEarthGrid = new SheafViewGrid(this, Trio::Earth);
        m_sheafViewGridEarthGridId = m_gridHolder.AddGrid(m_sheafViewGridEarthGrid);
        
        m_sheafViewGridWaterGrid = new SheafViewGrid(this, Trio::Water);
        m_sheafViewGridWaterGridId = m_gridHolder.AddGrid(m_sheafViewGridWaterGrid);
         
        m_nonagon.m_lameJuis.AddSheafMuteState(&m_stateSaver);

        for (size_t i = 0; i < TheNonagonInternal::x_numTrios; ++i)
        {
            for (size_t j = 0; j <= i; ++j)
            {
                m_stateSaver.Insert("Interrupt", i, j, &m_state.m_trigLogic.m_interrupt[i][j]);
            }
        }
    }

    void RemoveGridIds()
    {
        m_theoryOfTimeTopologyGrid->RemoveGridId();
        m_lameJuisCoMuteGrid->RemoveGridId();
        m_lameJuisMatrixGrid->RemoveGridId();
        m_lameJuisRHSGrid->RemoveGridId();
        m_lameJuisIntervalGrid->RemoveGridId();
        m_indexArpFireGrid->RemoveGridId();
        m_indexArpEarthGrid->RemoveGridId();
        m_indexArpWaterGrid->RemoveGridId();
        m_sheafViewGridFireGrid->RemoveGridId();
        m_sheafViewGridEarthGrid->RemoveGridId();
        m_sheafViewGridWaterGrid->RemoveGridId();
    }

    void SetupMonoScopeWriter(ScopeWriter* scopeWriter)
    {
        m_nonagon.m_theoryOfTime.SetupMonoScopeWriter(scopeWriter);
    }

    void SetupMessageOutBuffer(SmartGrid::MessageOutBuffer* messageOutBuffer)
    {
        m_messageOutBuffer = messageOutBuffer;
        m_nonagon.m_messageOutBuffer = m_messageOutBuffer;
        m_nonagon.m_theoryOfTime.SetupMessageOutBuffer(m_messageOutBuffer);
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

    void CopyToScene(int scene)
    {
        m_stateSaver.CopyToScene(scene);
    }

    void SetLeftScene(int scene)
    {
        m_stateSaverState.SetLeftScene(scene);
    }

    void SetRightScene(int scene)
    {
        m_stateSaverState.SetRightScene(scene);
    }

    void SetBlendFactor(float blendFactor)
    {
        m_stateSaverState.m_blend = blendFactor;
    }

    bool IsSceneActive(size_t sceneIx)
    {
        if (m_stateSaverState.m_blend == 0)
        {
            return sceneIx == static_cast<size_t>(m_stateSaverState.m_left);
        }
        else if (m_stateSaverState.m_blend == 1)
        {
            return sceneIx == static_cast<size_t>(m_stateSaverState.m_right);
        }
        else
        {
            return sceneIx == static_cast<size_t>(m_stateSaverState.m_left) || sceneIx == static_cast<size_t>(m_stateSaverState.m_right);
        }
    }

    void RevertToDefault(bool allScenes)
    {
        for (size_t s = 0; s < 8; ++s)
        {
            if (!allScenes && !IsSceneActive(s))
            {
                continue;
            }

            m_stateSaver.RevertToDefaultForScene(s);
        }
    }

    void PopulateUIState(TheNonagonInternal::UIState* uiState)
    {
        m_nonagon.m_noteWriter.SetCurPosition(m_nonagon.m_theoryOfTime.m_phasorIndependent);
        m_nonagon.m_theoryOfTime.PopulateUIState(&uiState->m_theoryOfTimeUIState);
        for (size_t i = 0; i < TheNonagonInternal::x_numTrios; ++i)
        {
            uiState->SetLoopMultiplier(i, m_state.m_multiPhasorGateInput.m_phasorDenominator[i * TheNonagonInternal::x_voicesPerTrio]);
        }

        for (size_t i = 0; i < TheNonagonInternal::x_numVoices; ++i)
        {
            uiState->SetGate(i, m_nonagon.m_multiPhasorGate.m_gate[i]);
            uiState->SetMuted(i, m_state.m_trigLogic.m_mute[i]);
            float cyclesPerSample = PhaseUtils::VOctToNatural(m_nonagon.m_output.m_voltPerOct[i], 1.0 / 48000.0);
            uiState->SetCyclesPerSamplePitch(i, cyclesPerSample);
        }
    }
    
    void ProcessSample(float dt)
    {
        m_stateSaver.Process(m_stateSaverState);
        m_gridHolder.Process(dt);
        m_nonagon.Process(m_state);
    }

    void ProcessFrame(TheNonagonInternal::UIState* uiState)
    {
        PopulateUIState(uiState);
    }
};
