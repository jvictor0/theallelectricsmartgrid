#pragma once

#include "Tick2Phasor.hpp"

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
    int m_position;
    int m_prevPosition;
    int m_loopSize;
    float m_phasor;

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

    bool ProcessDirectly(float phasor)
    {
        m_phasor = phasor;
        bool oldGate = m_gate;
        m_prevPosition = m_position;
        m_position = floor(phasor * m_loopSize);
        m_gate = m_position < m_loopSize / 2;
        m_gateChanged = oldGate != m_gate;
        m_top = (m_position == 0 && m_prevPosition == m_loopSize - 1) || (m_position == m_loopSize - 1 && m_prevPosition == 0);
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

        m_position = position;
        m_prevPosition = m_position;
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
};

struct TheoryOfTimeBase
{
    static constexpr int x_numLoops = 6;
    TimeLoop m_loops[x_numLoops];
    int m_globalWinding;
    bool m_anyChange;
    bool m_running;

    float m_phasor;
    bool m_top;

    struct Input
    {
        float m_phasor;
        float m_phaseOffset;
        bool m_top;
        TimeLoop::Input m_input[x_numLoops];
        bool m_running;

        Input()
        {
            m_phasor = 0;
            m_phaseOffset = 0;
            m_top = false;
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
        m_globalWinding = 0;
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
            }

            SetLoopSizes();
        }

        m_phasor = input.m_phasor;
        m_top = input.m_top;
        if (m_top)
        {
            ++m_globalWinding;
        }

        m_anyChange = false;
        float directPhasor = input.m_phasor + input.m_phaseOffset;
        directPhasor = directPhasor - floor(directPhasor);
        m_anyChange = GetMasterLoop()->ProcessDirectly(directPhasor);
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
            m_globalWinding = 0;
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
    return m_owner->m_globalWinding;
}

struct TheoryOfTime : public TheoryOfTimeBase
{
    Tick2Phasor m_tick2Phasor;

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
        {
        }
        
        ClockMode m_clockMode;
        float m_freq;        
        float m_timeIn;
        Tick2Phasor::Input m_tick2PhasorInput;
    };

    int MonodromyNumber(int clockIx, int resetIx)
    {
        return m_loops[clockIx].MonodromyNumber(resetIx, true);
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
                input.m_top = 1.0f <= input.m_phasor;
                input.m_phasor = input.m_phasor - floor(input.m_phasor);
            }
            else if (input.m_clockMode == ClockMode::External)
            {
                input.m_top = std::abs(input.m_timeIn - input.m_phasor) > 0.5f;
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
            input.m_top = std::abs(m_tick2Phasor.m_output - input.m_phasor) > 0.5f;
            input.m_phasor = m_tick2Phasor.m_output;
        }

        TheoryOfTimeBase::Process(input);
    }
};