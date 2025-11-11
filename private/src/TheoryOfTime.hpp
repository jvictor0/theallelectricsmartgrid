#pragma once

#include "Tick2Phasor.hpp"
#include "PolyXFader.hpp"
#include "CircleTracker.hpp"
#include <atomic>

struct TheoryOfTimeBase;

struct TimeLoop
{
    int m_index;
    int m_parentIndex;
    int m_parentMult;
    bool m_pingPong;
    bool m_gate;
    bool m_gateChanged;
    bool m_top;
    bool m_topIndependent;
    int m_position;
    int m_prevPosition;
    int m_loopSize;
    float m_phasor;
    float m_phasorIndependent;

    TheoryOfTimeBase* m_owner;

    struct Input
    {
        int m_parentIndex;
        int m_parentMult;
        bool m_pingPong;

        Input()
        {
            m_parentIndex = 0;
            m_parentMult = 1;
            m_pingPong = false;
        }
    };

    bool ProcessDirectly(float phasor, float phasorIndependent)
    {
        m_phasor = phasor;
        m_phasorIndependent = phasorIndependent;
        bool oldGate = m_gate;
        m_prevPosition = m_position;
        m_position = floor(phasor * m_loopSize);
        m_gate = m_position < m_loopSize / 2;
        m_gateChanged = oldGate != m_gate;
        m_top = (m_position == 0 && m_prevPosition == m_loopSize - 1) || (m_position == m_loopSize - 1 && m_prevPosition == 0);
        m_topIndependent = TopIndependent(m_loopSize);
        return m_prevPosition != m_position;
    }

    bool IsPingPonging()
    {
        return m_pingPong && GetParent()->m_position / m_loopSize == m_parentMult - 1;
    }

    void Process()
    {
        assert(GetParent());
        int position = GetParent()->m_position % m_loopSize;
        if (IsPingPonging())
        {
            position = m_loopSize - position - 1;
        }

        m_prevPosition = m_position;
        m_position = position;
        m_gate = m_position < m_loopSize / 2;
        m_gateChanged = GetMaster()->m_position / (m_loopSize / 2) != GetMaster()->m_prevPosition / (m_loopSize / 2);
        m_top = (m_position == 0 && m_prevPosition == m_loopSize - 1) || (m_position == m_loopSize - 1 && m_prevPosition == 0) || GetParent()->m_top;
    }

    void ProcessPhasor()
    {
        float phasor = GetParent()->m_phasor * m_parentMult;
        phasor = phasor - floor(phasor);
        if (IsPingPonging())
        {
            phasor = 1 - phasor;
        }

        m_phasor = phasor;
        m_topIndependent = TopIndependent(m_loopSize);

        m_phasorIndependent = GetParent()->m_phasorIndependent * m_parentMult;
        m_phasorIndependent = m_phasorIndependent - floor(m_phasorIndependent);
        if (IsPingPonging())
        {
            m_phasorIndependent = 1 - m_phasorIndependent;
        }
    }

    void Stop()
    {
        m_gate = false;
        m_gateChanged = true;
        m_top = true;
        m_position = 0;
        m_prevPosition = 0;
        m_phasor = 0;
    }

    bool HandleInput(const Input& input)
    {
        if (GetParent()->m_top)
        {
            bool result = false;
            if (input.m_pingPong != m_pingPong || input.m_parentMult != m_parentMult)
            {
                m_pingPong = input.m_pingPong;
                m_parentMult = input.m_parentMult;
                result = true;
            }

            if (input.m_parentIndex != m_parentIndex)
            {
                int oldParentIndex = m_parentIndex;
                m_parentIndex = input.m_parentIndex;
                if (GetParent()->m_top)
                {
                    result = true;
                }
                else
                {
                    m_parentIndex = oldParentIndex;
                }
            }            

            return result;
        }

        return false;
    }

    int MonodromyNumber(int resetIndex, bool external)
    {
        if (m_index == resetIndex)
        {
            return external && !m_gate ? 1 : 0;
        }
        else if (!GetParent())
        {
            return GlobalWinding() * (external ? 2 : 1) + (external && !m_gate ? 1 : 0);
        }
        else if (!IsPingPonging())
        {
            int monodromyRelParent = GetParent()->m_position / (external ? m_loopSize / 2 : m_loopSize);
            int parentMonodromyNumber = GetParent()->MonodromyNumber(resetIndex, false);
            int mult = (m_pingPong ? m_parentMult - 2 : m_parentMult) * (external ? 2 : 1);
            int result = monodromyRelParent + mult * parentMonodromyNumber;
            return result;
        }
        else
        {
            int monodromyRelParent;
            if (external)
            {
                monodromyRelParent = m_gate ? 2 * m_parentMult - 4 : 2 * m_parentMult - 3;
            }
            else
            {
                monodromyRelParent = m_parentMult - 2;
            }

            int parentMonodromyNumber = GetParent()->MonodromyNumber(resetIndex, false);
            int result = monodromyRelParent + (m_parentMult - 2) * parentMonodromyNumber * (external ? 2 : 1);
            return result;
        }
    }   

    TimeLoop* GetParent();
    TimeLoop* GetMaster();

    int GlobalWinding();
    bool TopIndependent(int loopSize);
};

struct TheoryOfTimeBase
{
    static constexpr int x_numLoops = 6;
    TimeLoop m_loops[x_numLoops];
    bool m_anyChange;
    bool m_running;

    float m_phasorIndependent;
    bool m_topIndependent;
    int m_positionIndependent;
    int m_prevPositionIndependent;
    CircleTracker m_globalPhase;

    struct Input
    {
        float m_phasor;
        float m_phaseOffset;
        bool m_phasorTop;
        TimeLoop::Input m_input[x_numLoops];
        bool m_running;

        Input()
        {
            m_phasor = 0;
            m_phaseOffset = 0;
            m_phasorTop = false;
            m_running = false;
            for (int i = 0; i < x_numLoops; ++i)
            {
                m_input[i].m_parentIndex = i + 1;
            }
        }
    };

    TheoryOfTimeBase()
    {
        m_running = false;
        m_anyChange = false;
        for (int i = 0; i < x_numLoops; ++i)
        {
            m_loops[i].m_owner = this;
            m_loops[i].m_index = i;
            m_loops[i].m_parentIndex = i + 1;
        }
    }
    
    void SetLoopSizes()
    {
        for (int i = 0; i < x_numLoops; ++i)
        {
            m_loops[i].m_loopSize = 1;
        }

        for (int i = 0; i < x_numLoops; ++i)
        {
            if (m_loops[i].GetParent())
            {
                m_loops[i].GetParent()->m_loopSize = std::lcm(m_loops[i].GetParent()->m_loopSize, m_loops[i].m_loopSize * m_loops[i].m_parentMult);
            }
        }

        for (int i = 0; i < x_numLoops; ++i)
        {
            m_loops[i].m_loopSize *= 2;
        }
    }

    void ProcessRunning(Input& input)
    {
        if (!m_running)
        {
            m_running = true;
            for (int i = x_numLoops - 2; i >= 0; --i)
            {
                m_loops[i].m_parentIndex = input.m_input[i].m_parentIndex;
                m_loops[i].m_parentMult = input.m_input[i].m_parentMult;
                m_loops[i].m_pingPong = input.m_input[i].m_pingPong;
                m_globalPhase.Reset(0);
            }

            SetLoopSizes();

            for (int i = 0; i < x_numLoops; ++i)
            {
                m_loops[i].m_position = m_loops[i].m_loopSize - 1;
            }

            m_positionIndependent = GetMasterLoop()->m_loopSize - 1;
        }               

        m_phasorIndependent = input.m_phasor;
        m_topIndependent = input.m_phasorTop;
        m_prevPositionIndependent = m_positionIndependent;
        m_positionIndependent = floor(m_phasorIndependent * GetMasterLoop()->m_loopSize);

        m_anyChange = false;
        float directPhasor = input.m_phasor + input.m_phaseOffset;
        directPhasor = directPhasor - floor(directPhasor);
        m_globalPhase.Process(directPhasor);
        m_anyChange = GetMasterLoop()->ProcessDirectly(directPhasor, m_phasorIndependent);
        if (m_anyChange)
        {
            bool resetLoopSize = false;
            for (int i = x_numLoops - 2; i >= 0; --i)
            {
                m_loops[i].Process();
                if (m_loops[i].HandleInput(input.m_input[i]))
                {
                    resetLoopSize = true;
                }
            }

            if (resetLoopSize)
            {
                SetLoopSizes();
            }
        }

        for (int i = x_numLoops - 2; i >= 0; --i)
        {
            if (!m_anyChange)
            {
                m_loops[i].m_gateChanged = false;
                m_loops[i].m_top = false;
            }

            m_loops[i].ProcessPhasor();
        }
    }

    void Process(Input& input)
    {
        if (input.m_running)
        {
            ProcessRunning(input);
        }
        else if (m_running)
        {
            m_running = false;
            m_anyChange = true;
            for (int i = 0; i < x_numLoops; ++i)
            {
                m_loops[i].Stop();
            }
        }
        else
        {
            m_anyChange = false;
        }
    }

    TimeLoop* GetMasterLoop()
    {
        return &m_loops[x_numLoops - 1];
    }

    int GetLoopMultiplier(int loopIndex)
    {
        return GetMasterLoop()->m_loopSize / (m_loops[loopIndex].m_loopSize / 2);
    }
};

inline TimeLoop* TimeLoop::GetParent()
{
    if (m_parentIndex < 0 || TheoryOfTimeBase::x_numLoops <= m_parentIndex)
    {
        return nullptr;
    }

    return &m_owner->m_loops[m_parentIndex];
}

inline TimeLoop* TimeLoop::GetMaster()
{
    return m_owner->GetMasterLoop();
}

inline int TimeLoop::GlobalWinding()
{
    return m_owner->m_globalPhase.m_winding;
}

inline bool TimeLoop::TopIndependent(int loopSize)
{
    int pos = m_owner->m_positionIndependent % loopSize;
    int prevPos = m_owner->m_prevPositionIndependent % loopSize;
    return (pos == 0 && prevPos == loopSize - 1) || (pos == loopSize - 1 && prevPos == 0) || (GetParent() && GetParent()->m_topIndependent);
}

struct TheoryOfTime : public TheoryOfTimeBase
{
    Tick2Phasor m_tick2Phasor;
    PolyXFaderInternal m_phaseModLFO;
    ScopeWriterHolder m_scopeWriter;

    enum class ClockMode : int 
    {
        Internal,
        External,
        Tick2Phasor
    };

    struct Input : public TheoryOfTimeBase::Input
    {
        Input() 
          : m_clockMode(ClockMode::Internal)
          , m_freq(1.0/4)
          , m_timeIn(0)
          , m_lfoMult(1, 16)
        {
            m_phaseModLFOInput.m_size = x_numLoops;
            m_phaseModLFOInput.m_values = m_phasorValues;
            m_phaseModLFOInput.m_top = m_phasorTops;
            m_modIndex = 0;
            m_phaseModLFOInput.m_phaseShift = -0.75;

            m_modIndex.SetBaseByCenter(0.25);
        }
        
        ClockMode m_clockMode;
        float m_freq;        
        float m_timeIn;
        Tick2Phasor::Input m_tick2PhasorInput;
        PolyXFaderInternal::Input m_phaseModLFOInput;
        float m_phasorValues[x_numLoops];
        bool m_phasorTops[x_numLoops];
        PhaseUtils::ZeroedExpParam m_modIndex;
        PhaseUtils::ExpParam m_lfoMult;
    };

    struct UIState
    {
        std::atomic<int> m_timeYModAmount;

        float GetTimeYModAmount()
        {
            return m_timeYModAmount.load();
        }

        void SetTimeYModAmount(float value)
        {
            m_timeYModAmount.store(value);
        }
    };

    void PopulateUIState(UIState* uiState)
    {
        int lfoLoopSize = 1;
        for (int i = 0; i < x_numLoops; ++i)
        {
            if (m_phaseModLFO.m_weights[i] > 0 && m_loops[i].m_loopSize > 0)
            {
                lfoLoopSize = std::lcm(lfoLoopSize, m_loops[i].m_loopSize);
            }
        }

        uiState->m_timeYModAmount.store(GetMasterLoop()->m_loopSize / lfoLoopSize);
    }

    int MonodromyNumber(int clockIx, int resetIx)
    {
        return m_loops[clockIx].MonodromyNumber(resetIx, true);
    }

    void ProcessPhaseModLFO(Input& input)
    {
        for (int i = 0; i < x_numLoops; ++i)
        {
            input.m_phasorValues[i] = m_loops[i].m_phasorIndependent;
            input.m_phasorTops[i] = m_loops[i].m_topIndependent;
            input.m_phaseModLFOInput.m_externalWeights[i] = static_cast<float>(m_loops[i].m_loopSize) / GetMasterLoop()->m_loopSize;
        }

        input.m_phaseModLFOInput.m_mult = input.m_lfoMult.m_expParam;
        m_phaseModLFO.Process(input.m_phaseModLFOInput);
        input.m_phaseOffset = - 2 * input.m_modIndex.m_expParam * m_phaseModLFO.m_output;
    }

    void SetupMonoScopeWriter(ScopeWriter* scopeWriter)
    {
        m_scopeWriter = ScopeWriterHolder(scopeWriter, 0, static_cast<size_t>(SquiggleBoyVoice::MonoScopes::TheoryOfTime));
    }

    TheoryOfTime()
    {
    }

    void Process(Input& input)
    {
        if (input.m_running)
        {
            if (input.m_clockMode == ClockMode::Internal)
            {
                input.m_phasor += input.m_freq;
                input.m_phasorTop = 1.0f <= input.m_phasor;
                input.m_phasor = input.m_phasor - floor(input.m_phasor);
            }
            else if (input.m_clockMode == ClockMode::External)
            {
                input.m_phasorTop = std::abs(input.m_timeIn - input.m_phasor) > 0.5f;
                input.m_phasor = input.m_timeIn;
            }
        }
        else
        {
            input.m_phasor = 0;
            m_tick2Phasor.m_reset = true;
        }
        
        if (input.m_clockMode == ClockMode::Tick2Phasor)
        {
            m_tick2Phasor.Process(input.m_tick2PhasorInput);
            input.m_phasorTop = std::abs(m_tick2Phasor.m_output - input.m_phasor) > 0.5f;
            input.m_phasor = m_tick2Phasor.m_output;
        }

        ProcessPhaseModLFO(input);

        TheoryOfTimeBase::Process(input);

        m_scopeWriter.Write(m_globalPhase.m_phase);
        if (m_phaseModLFO.m_top)
        {
            m_scopeWriter.RecordStart();
        }
    }
};