#pragma once

#include "Tick2Phasor.hpp"
#include "PolyXFader.hpp"
#include "CircleTracker.hpp"
#include "ScopeWriter.hpp"
#include "PhaseUtils.hpp"
#include "SmartGridOneScopeEnums.hpp"
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
    double m_phasor;
    double m_phasorIndependent;
    int64_t m_globalWinding;

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

    bool ProcessDirectly(double phasor, double phasorIndependent, int64_t globalWinding)
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
        m_globalWinding = globalWinding;
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
        m_globalWinding = MonodromyNumber(-1, false);
    }

    void SetLoopSize(int loopSize)
    {
        m_loopSize = loopSize;
        m_prevPosition = GetParent()->m_prevPosition % loopSize;
        m_position = GetParent()->m_position % loopSize;
        m_globalWinding = MonodromyNumber(-1, false);
    }

    void ProcessPhasor()
    {
        double phasor = GetParent()->m_phasor * m_parentMult;
        phasor = phasor - GetParent()->m_position / m_loopSize;
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

    TimeLoop()
        : m_index(0)
        , m_parentIndex(0)
        , m_parentMult(1)
        , m_pingPong(false)
        , m_gate(false)
        , m_gateChanged(false)
        , m_top(true)
        , m_topIndependent(true)
        , m_position(0)
        , m_prevPosition(0)
        , m_loopSize(1)
        , m_phasor(0)
        , m_phasorIndependent(0)
        , m_globalWinding(0)
        , m_owner(nullptr)
    {
    }

    double GetUnwoundPhasor()
    {
        return m_phasor + static_cast<double>(m_globalWinding);
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

    double m_phasorIndependent;
    bool m_topIndependent;
    int m_positionIndependent;
    int m_prevPositionIndependent;
    CircleTracker m_globalPhase;

    struct Input
    {
        double m_phasor;
        double m_phaseOffset;
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
        int loopSizes[x_numLoops] = {0};
        for (int i = 0; i < x_numLoops; ++i)
        {
            loopSizes[i] = 1;
        }

        for (int i = 0; i < x_numLoops; ++i)
        {
            if (m_loops[i].GetParent())
            {
                loopSizes[m_loops[i].m_parentIndex] = std::lcm(loopSizes[m_loops[i].m_parentIndex], loopSizes[i] * m_loops[i].m_parentMult);
            }
        }

        for (int i = x_numLoops - 1; i >= 0; --i)
        {
            m_loops[i].SetLoopSize(2 * loopSizes[i]);
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
        double directPhasor = input.m_phasor + input.m_phaseOffset;
        directPhasor = directPhasor - floor(directPhasor);
        m_globalPhase.Process(directPhasor);
        m_anyChange = GetMasterLoop()->ProcessDirectly(directPhasor, m_phasorIndependent, m_globalPhase.m_winding);
        if (m_anyChange)
        {
            for (int i = x_numLoops - 2; i >= 0; --i)
            {
                m_loops[i].Process();
                if (m_loops[i].HandleInput(input.m_input[i]))
                {
                    SetLoopSizes();
                }
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

    int GetLoopInternalMultiplier(int loopIndex)
    {
        return GetMasterLoop()->m_loopSize / (m_loops[loopIndex].m_loopSize / 2);
    }

    int GetLoopExternalMultiplier(int loopIndex)
    {
        return GetMasterLoop()->m_loopSize / m_loops[loopIndex].m_loopSize;
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
    double m_masterLoopSamples;

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

            m_modIndex.SetBaseByCenter(1.0 / 16);

            float paramFilterFreq = 0.3 / 48000.0;
            m_freqFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoSkewFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoMultFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoShapeFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoCenterFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoSlopeFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoIndexFilter.SetAlphaFromNatFreq(paramFilterFreq);
        }

        OPLowPassFilter m_freqFilter;
        OPLowPassFilter m_lfoSkewFilter;
        OPLowPassFilter m_lfoMultFilter;
        OPLowPassFilter m_lfoShapeFilter;
        OPLowPassFilter m_lfoCenterFilter;
        OPLowPassFilter m_lfoSlopeFilter;
        OPLowPassFilter m_lfoIndexFilter;
        
        ClockMode m_clockMode;
        double m_freq;        
        double m_timeIn;
        Tick2Phasor::Input m_tick2PhasorInput;
        PolyXFaderInternal::Input m_phaseModLFOInput;
        double m_phasorValues[x_numLoops];
        bool m_phasorTops[x_numLoops];
        PhaseUtils::ZeroedExpParam m_modIndex;
        PhaseUtils::ExpParam m_lfoMult;
    };

    struct UIState
    {
        std::atomic<int> m_timeYModAmount;

        double GetTimeYModAmount()
        {
            return m_timeYModAmount.load();
        }

        void SetTimeYModAmount(double value)
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
            input.m_phaseModLFOInput.m_externalWeights[i] = static_cast<double>(m_loops[i].m_loopSize) / GetMasterLoop()->m_loopSize;
        }

        input.m_phaseModLFOInput.m_mult = input.m_lfoMult.m_expParam;
        m_phaseModLFO.Process(input.m_phaseModLFOInput);
        input.m_phaseOffset = - 2 * input.m_modIndex.m_expParam * m_phaseModLFO.m_output;
    }

    void SetupMonoScopeWriter(ScopeWriter* scopeWriter)
    {
        m_scopeWriter = ScopeWriterHolder(scopeWriter, 0, static_cast<size_t>(SmartGridOne::MonoScopes::TheoryOfTime));
    }

    TheoryOfTime()
    {
    }

    double LoopSamples(int loopIndex)
    {
        return m_masterLoopSamples / GetLoopExternalMultiplier(loopIndex);
    }

    double LoopSamplesFraction(int loopIndex, double fraction)
    {
        return LoopSamples(loopIndex) * (m_loops[loopIndex].GetUnwoundPhasor() - fraction);
    }

    double PhasorUnwoundSamples(int loopIndex)
    {
        return m_loops[loopIndex].GetUnwoundPhasor() * LoopSamples(loopIndex);
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
                m_masterLoopSamples = 1.0 / input.m_freq;
            }
            else if (input.m_clockMode == ClockMode::External)
            {
                input.m_phasorTop = std::abs(input.m_timeIn - input.m_phasor) > 0.5f;
                m_masterLoopSamples = 1.0 / std::max<double>(std::abs(input.m_phasor - input.m_timeIn), 1.0e-12);
                input.m_phasor = input.m_timeIn;
            }
        }
        else
        {
            input.m_phasor = 0;
            m_tick2Phasor.m_reset = true;
            m_masterLoopSamples = 1.0 / input.m_freq;
        }
        
        if (input.m_clockMode == ClockMode::Tick2Phasor)
        {
            m_tick2Phasor.Process(input.m_tick2PhasorInput);
            input.m_phasorTop = std::abs(m_tick2Phasor.m_output - input.m_phasor) > 0.5f;
            m_masterLoopSamples = 1.0 / std::max<double>(m_tick2Phasor.m_phaseIncrement, 1.0e-12);
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