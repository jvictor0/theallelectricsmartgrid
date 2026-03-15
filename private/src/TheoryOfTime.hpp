#pragma once

#include "Tick2Phasor.hpp"
#include "PolyXFader.hpp"
#include "CircleTracker.hpp"
#include "ScopeWriter.hpp"
#include "PhaseUtils.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "MessageOut.hpp"
#include "PLL.hpp"
#include "SampleTimer.hpp"
#include <atomic>

struct TheoryOfTimeBase;

struct TimeLoop
{
    static constexpr size_t x_numControlSamples = SampleTimer::x_controlFrameRate + 1;

    int m_index;
    int m_parentIndex[x_numControlSamples];
    int m_parentMult[x_numControlSamples];
    int m_loopSize[x_numControlSamples];
    int m_externalLoopMult[x_numControlSamples];

    bool m_gate[x_numControlSamples];
    bool m_gateChanged[x_numControlSamples];
    bool m_top[x_numControlSamples];
    bool m_topIndependent[x_numControlSamples];
    int m_position[x_numControlSamples];
    int m_prevPosition[x_numControlSamples];

    double m_phasor[x_numControlSamples];
    double m_phasorIndependent[x_numControlSamples];
    int64_t m_globalWinding[x_numControlSamples];
    double m_glueWinding[x_numControlSamples];
    bool m_ascending[x_numControlSamples];

    TheoryOfTimeBase* m_owner;

    struct Input
    {
        int m_parentIndex;
        int m_parentMult;

        Input()
        {
            m_parentIndex = 0;
            m_parentMult = 2;
        }
    };

    void Preprocess(size_t j)
    {
        size_t prev = j - 1;
        m_parentIndex[j] = m_parentIndex[prev];
        m_parentMult[j] = m_parentMult[prev];
        m_loopSize[j] = m_loopSize[prev];
        m_externalLoopMult[j] = m_externalLoopMult[prev];
        m_gate[j] = m_gate[prev];
        m_gateChanged[j] = m_gateChanged[prev];
        m_top[j] = m_top[prev];
        m_topIndependent[j] = m_topIndependent[prev];
        m_position[j] = m_position[prev];
        m_prevPosition[j] = m_prevPosition[prev];
        m_phasor[j] = m_phasor[prev];
        m_phasorIndependent[j] = m_phasorIndependent[prev];
        m_globalWinding[j] = m_globalWinding[prev];
        m_glueWinding[j] = m_glueWinding[prev];
        m_ascending[j] = m_ascending[prev];
    }

    bool ProcessDirectly(size_t j, double phasor, double phasorIndependent, int64_t globalWinding)
    {
        double prevPhasor = m_phasor[j];
        int64_t prevWinding = m_globalWinding[j];
        m_ascending[j] = prevWinding < globalWinding || (prevWinding == globalWinding && prevPhasor < phasor);

        m_phasor[j] = phasor;
        m_phasorIndependent[j] = phasorIndependent;

        bool prevGate = m_gate[j];
        int prevPos = m_position[j];
        m_prevPosition[j] = prevPos;
        m_position[j] = static_cast<int>(std::floor(phasor * m_loopSize[j]));
        if (std::abs(m_position[j] - m_prevPosition[j]) > 1)
        {
            m_prevPosition[j] = (m_loopSize[j] + m_position[j] - (m_ascending[j] ? 1 : -1)) % m_loopSize[j];
        }

        m_gate[j] = m_position[j] < m_loopSize[j] / 2;
        m_gateChanged[j] = prevGate != m_gate[j];
        m_top[j] = (m_position[j] == 0 && m_prevPosition[j] == m_loopSize[j] - 1 && m_ascending[j]) || 
                (m_position[j] == m_loopSize[j] - 1 && m_prevPosition[j] == 0 && !m_ascending[j]);
        m_topIndependent[j] = TopIndependent(j, m_loopSize[j]);
        m_globalWinding[j] = globalWinding;
        return m_prevPosition[j] != m_position[j];
    }

    void Process(size_t j)
    {
        SetMembersFromParent(j);
        m_globalWinding[j] = MonodromyNumber(j, -1, false);
    }

    void SetMembersFromParent(size_t j)
    {
        TimeLoop* parent = GetParent(j);
        assert(parent);
        m_position[j] = parent->m_position[j] % m_loopSize[j];
        m_prevPosition[j] = parent->m_prevPosition[j] % m_loopSize[j];
        m_ascending[j] = parent->m_ascending[j];

        m_gate[j] = m_position[j] < m_loopSize[j] / 2;
        m_gateChanged[j] = GetMaster()->m_position[j] / (m_loopSize[j] / 2) != GetMaster()->m_prevPosition[j] / (m_loopSize[j] / 2);
        m_top[j] = (m_position[j] == 0 && m_prevPosition[j] == m_loopSize[j] - 1 && m_ascending[j]) || 
                (m_position[j] == m_loopSize[j] - 1 && m_prevPosition[j] == 0 && !m_ascending[j]) || 
                parent->m_top[j];
    }

    void SetLoopSize(int loopSize, size_t j)
    {
        if (GetParent(j))
        {   
            int externalLoopMult = GetMaster()->m_loopSize[j] / loopSize;  
            bool oldReverseTop = m_top[j] && !m_ascending[j];       
            m_loopSize[j] = loopSize;
            
            SetMembersFromParent(j);

            int64_t winding = MonodromyNumber(j, -1, false);
            double oldWinding = m_glueWinding[j] + m_globalWinding[j];
            int windingOffset = 0;
            
            if (m_top[j] && !m_ascending[j])
            {
                windingOffset = 1;
            }

            if (oldReverseTop)
            {
                oldWinding = oldWinding + 1;
            }

            m_glueWinding[j] = static_cast<double>(oldWinding * externalLoopMult) / m_externalLoopMult[j] - winding - windingOffset;
            m_globalWinding[j] = winding;
            m_externalLoopMult[j] = externalLoopMult;
        }
        else
        {
            int oldLoopSize = m_loopSize[j];
            m_position[j] = m_position[j] * loopSize / oldLoopSize;
            m_loopSize[j] = loopSize;
            m_prevPosition[j] = (m_position[j] + (m_ascending[j] ? 1 : -1)) % m_loopSize[j];
            m_externalLoopMult[j] = 1;
        }
    }

    void ProcessPhasorIndependent(size_t j)
    {
        TimeLoop* parent = GetParent(j);
        m_phasorIndependent[j] = parent->m_phasorIndependent[j] * m_parentMult[j];
        m_phasorIndependent[j] = m_phasorIndependent[j] - std::floor(m_phasorIndependent[j]);
        m_topIndependent[j] = TopIndependent(j, m_loopSize[j]);
    }

    void ProcessPhasorDependent(size_t j)
    {
        TimeLoop* parent = GetParent(j);
        double phasor = parent->m_phasor[j] * m_parentMult[j];
        phasor = phasor - static_cast<double>(parent->m_position[j] / m_loopSize[j]);
        m_phasor[j] = phasor;
    }

    void Stop()
    {
        for (size_t j = 0; j < x_numControlSamples; ++j)
        {
            m_gate[j] = false;
            m_gateChanged[j] = (j == 0);
            m_top[j] = (j == 0);
            m_topIndependent[j] = (j == 0);
            m_position[j] = 0;
            m_prevPosition[j] = 0;
            m_phasor[j] = 0;
            m_phasorIndependent[j] = 0;
            m_globalWinding[j] = 0;
            m_glueWinding[j] = 0;
            m_ascending[j] = false;
            m_parentIndex[j] = m_index + 1;
            m_parentMult[j] = 1;
            m_loopSize[j] = 1;
            m_externalLoopMult[j] = 1;
        }
    }

    bool HandleInput(size_t j, const Input& input)
    {
        TimeLoop* parent = GetParent(j);
        if (parent && parent->m_top[j])
        {
            bool result = false;
            if (input.m_parentMult != m_parentMult[j])
            {
                m_parentMult[j] = input.m_parentMult;
                result = true;
            }

            if (input.m_parentIndex != m_parentIndex[j])
            {
                int oldParentIndex = m_parentIndex[j];
                m_parentIndex[j] = input.m_parentIndex;
                if (parent->m_top[j])
                {
                    result = true;
                }
                else
                {
                    m_parentIndex[j] = oldParentIndex;
                }
            }            

            return result;
        }

        return false;
    }

    int MonodromyNumber(size_t j, int resetIndex, bool external)
    {
        if (m_index == resetIndex)
        {
            return external && !m_gate[j] ? 1 : 0;
        }
        else if (!GetParent(j))
        {
            return GlobalWinding() * (external ? 2 : 1) + (external && !m_gate[j] ? 1 : 0);
        }
        else
        {
            TimeLoop* parent = GetParent(j);
            int monodromyRelParent = parent->m_position[j] / (external ? m_loopSize[j] / 2 : m_loopSize[j]);
            int parentMonodromyNumber = parent->MonodromyNumber(j, resetIndex, false);
            int mult = m_parentMult[j] * (external ? 2 : 1);
            int result = monodromyRelParent + mult * parentMonodromyNumber;
            return result;
        }
    }   

    TimeLoop()
        : m_index(0)
        , m_owner(nullptr)
    {
        for (size_t j = 0; j < x_numControlSamples; ++j)
        {
            m_gate[j] = false;
            m_gateChanged[j] = false;
            m_top[j] = true;
            m_topIndependent[j] = true;
            m_position[j] = 0;
            m_prevPosition[j] = 0;
            m_phasor[j] = 0;
            m_phasorIndependent[j] = 0;
            m_globalWinding[j] = 0;
            m_glueWinding[j] = 0;
            m_ascending[j] = false;
            m_parentIndex[j] = 0;
            m_parentMult[j] = 1;
            m_loopSize[j] = 1;
            m_externalLoopMult[j] = 1;
        }
    }

    double GetUnwoundPhasor(size_t j)
    {
        return m_phasor[j] + static_cast<double>(m_globalWinding[j]) + m_glueWinding[j];
    }

    bool AnyGateChanged() const
    {
        for (size_t j = 0; j < x_numControlSamples - 1; ++j)
        {
            if (m_gateChanged[j])
            {
                return true;
            }
        }

        return false;
    }

    TimeLoop* GetParent(size_t j);
    TimeLoop* GetMaster();

    int GlobalWinding();
    bool TopIndependent(size_t j, int loopSize);
};

struct TheoryOfTimeBase
{
    static constexpr int x_numLoops = 6;
    static constexpr int x_masterLoop = x_numLoops - 1;
    static constexpr size_t x_microBlockSize = SampleTimer::x_controlFrameRate;
    static constexpr size_t x_microBlockBufferSize = x_microBlockSize + 1;
    TimeLoop m_loops[x_numLoops];
    bool m_anyChange[x_microBlockBufferSize];
    bool m_running;

    int m_positionIndependent[x_microBlockBufferSize];
    int m_prevPositionIndependent[x_microBlockBufferSize];
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
        for (size_t j = 0; j < x_microBlockBufferSize; ++j)
        {
            m_anyChange[j] = false;
            m_positionIndependent[j] = 0;
            m_prevPositionIndependent[j] = 0;
        }

        for (int i = 0; i < x_numLoops; ++i)
        {
            m_loops[i].m_owner = this;
            m_loops[i].m_index = i;
            for (size_t k = 0; k < x_microBlockBufferSize; ++k)
            {
                m_loops[i].m_parentIndex[k] = i + 1;
            }
        }
    }
    
    bool AnyChangeInMicroBlock() const
    {
        for (size_t j = 0; j < x_microBlockSize; ++j)
        {
            if (m_anyChange[j])
            {
                return true;
            }
        }

        return false;
    }

    void Preprocess(size_t j)
    {
        size_t prev = j - 1;
        m_positionIndependent[j] = m_positionIndependent[prev];
        m_prevPositionIndependent[j] = m_prevPositionIndependent[prev];
        m_anyChange[j] = false;
        for (int i = 0; i < x_numLoops; ++i)
        {
            m_loops[i].Preprocess(j);
        }
    }

    // Copies slot 8 (first sample of next micro block, computed in previous block) into slot 0.
    // Called at the start of each micro block so the Nonagon runs with the correct sample 0.
    //
    void RolloverMicroblockBuffer()
    {
        constexpr size_t x_rolloverSlot = x_microBlockBufferSize - 1;
        m_positionIndependent[0] = m_positionIndependent[x_rolloverSlot];
        m_prevPositionIndependent[0] = m_prevPositionIndependent[x_rolloverSlot];
        m_anyChange[0] = m_anyChange[x_rolloverSlot];        

        for (int i = 0; i < x_numLoops; ++i)
        {
            TimeLoop& loop = m_loops[i];
            loop.m_parentIndex[0] = loop.m_parentIndex[x_rolloverSlot];
            loop.m_parentMult[0] = loop.m_parentMult[x_rolloverSlot];
            loop.m_loopSize[0] = loop.m_loopSize[x_rolloverSlot];
            loop.m_externalLoopMult[0] = loop.m_externalLoopMult[x_rolloverSlot];
            loop.m_gate[0] = loop.m_gate[x_rolloverSlot];
            loop.m_gateChanged[0] = loop.m_gateChanged[x_rolloverSlot];
            loop.m_top[0] = loop.m_top[x_rolloverSlot];
            loop.m_topIndependent[0] = loop.m_topIndependent[x_rolloverSlot];
            loop.m_position[0] = loop.m_position[x_rolloverSlot];
            loop.m_prevPosition[0] = loop.m_prevPosition[x_rolloverSlot];
            loop.m_phasor[0] = loop.m_phasor[x_rolloverSlot];
            loop.m_phasorIndependent[0] = loop.m_phasorIndependent[x_rolloverSlot];
            loop.m_globalWinding[0] = loop.m_globalWinding[x_rolloverSlot];
            loop.m_glueWinding[0] = loop.m_glueWinding[x_rolloverSlot];
            loop.m_ascending[0] = loop.m_ascending[x_rolloverSlot];
        }
    }

    void SetLoopSizes(size_t j)
    {
        int loopSizes[x_numLoops] = {0};
        for (int i = 0; i < x_numLoops; ++i)
        {
            loopSizes[i] = 1;
        }

        for (int i = 0; i < x_numLoops; ++i)
        {
            if (m_loops[i].GetParent(j))
            {
                loopSizes[m_loops[i].m_parentIndex[j]] = std::lcm(loopSizes[m_loops[i].m_parentIndex[j]], loopSizes[i] * m_loops[i].m_parentMult[j]);
            }
        }

        for (int i = x_numLoops - 2; i >= 0; --i)
        {
            loopSizes[i] = loopSizes[m_loops[i].m_parentIndex[j]] / m_loops[i].m_parentMult[j];
        }

        for (int i = x_numLoops - 1; i >= 0; --i)
        {
            m_loops[i].SetLoopSize(2 * loopSizes[i], j);
        }
    }

    void ProcessRunning(size_t j, Input& input)
    {
        bool startedRunning = false;
        if (!m_running)
        {
            m_running = true;
            startedRunning = true;
            for (int i = x_numLoops - 2; i >= 0; --i)
            {
                m_loops[i].m_parentIndex[j] = input.m_input[i].m_parentIndex;
                m_loops[i].m_parentMult[j] = input.m_input[i].m_parentMult;
                m_globalPhase.Reset(0);
            }

            SetLoopSizes(j);

            for (int i = 0; i < x_numLoops; ++i)
            {
                m_loops[i].m_position[j] = m_loops[i].m_loopSize[j] - 1;                
                m_loops[i].m_gate[j] = false;
            }

            m_positionIndependent[j] = GetMasterLoop()->m_loopSize[j] - 1;
        }

        std::ignore = startedRunning;

        m_prevPositionIndependent[j] = m_positionIndependent[j];
        m_positionIndependent[j] = static_cast<int>(std::floor(input.m_phasor * GetMasterLoop()->m_loopSize[j]));

        m_anyChange[j] = false;
        double directPhasor = input.m_phasor + input.m_phaseOffset;
        directPhasor = directPhasor - std::floor(directPhasor);
        m_globalPhase.Process(directPhasor);
        m_anyChange[j] = GetMasterLoop()->ProcessDirectly(j, directPhasor, input.m_phasor, m_globalPhase.m_winding);

        if (m_anyChange[j])
        {
            for (int i = x_numLoops - 2; i >= 0; --i)
            {
                m_loops[i].Process(j);
            }

            bool setLoopSizes = false;
            for (int i = x_numLoops - 2; i >= 0; --i)
            {
                if (m_loops[i].HandleInput(j, input.m_input[i]))
                {
                    setLoopSizes = true;
                }
            }

            if (setLoopSizes)
            {
                SetLoopSizes(j);
            }
        }

        for (int i = x_numLoops - 2; i >= 0; --i)
        {
            if (!m_anyChange[j])
            {
                m_loops[i].m_gateChanged[j] = false;
                m_loops[i].m_top[j] = false;
            }

            m_loops[i].ProcessPhasorIndependent(j);
            m_loops[i].ProcessPhasorDependent(j);
        }
    }

    void Process(size_t j, Input& input)
    {
        if (input.m_running)
        {
            ProcessRunning(j, input);
        }
        else if (m_running)
        {
            m_running = false;
            m_anyChange[j] = true;
            for (int i = 0; i < x_numLoops; ++i)
            {
                m_loops[i].Stop();
            }
        }
        else
        {
            m_anyChange[j] = false;
        }
    }

    TimeLoop* GetMasterLoop()
    {
        return &m_loops[x_masterLoop];
    }

    double GetPhasorIndependent(size_t j) const
    {
        return GetDirectPhasor(j, x_masterLoop);
    }
    
    double GetIndirectPhasor(size_t j) const
    {
        return GetIndirectPhasor(j, x_masterLoop);
    }
    
    bool GetTopIndependent(size_t j) const
    {
        return GetDirectTop(j, x_masterLoop);
    }

    int GetLoopInternalMultiplier(size_t j, int loopIndex)
    {
        return GetMasterLoop()->m_loopSize[j] / std::max(1, m_loops[loopIndex].m_loopSize[j] / 2);
    }

    int GetLoopExternalMultiplier(size_t j, int loopIndex)
    {
        return GetMasterLoop()->m_loopSize[j] / std::max(1, m_loops[loopIndex].m_loopSize[j]);
    }

    double GetDirectPhasor(size_t j, size_t loopIndex) const
    {
        if (loopIndex >= x_numLoops || j >= x_microBlockSize)
        {
            return 0.0;
        }

        return m_loops[loopIndex].m_phasorIndependent[j];
    }

    bool GetDirectTop(size_t j, size_t loopIndex) const
    {
        if (loopIndex >= x_numLoops || j >= x_microBlockSize)
        {
            return false;
        }

        return m_loops[loopIndex].m_topIndependent[j];
    }

    double GetIndirectPhasor(size_t j, size_t loopIndex) const
    {
        if (loopIndex >= x_numLoops || j >= x_microBlockSize)
        {
            return 0.0;
        }

        return m_loops[loopIndex].m_phasor[j];
    }

    bool GetIndirectTop(size_t j, size_t loopIndex) const
    {
        if (loopIndex >= x_numLoops || j >= x_microBlockSize)
        {
            return false;
        }

        return m_loops[loopIndex].m_top[j];
    }

    double InterpolateMicroBlock(const double values[], float samplePosition) const
    {
        if (samplePosition <= 0.0f)
        {
            return values[0];
        }

        if (samplePosition >= static_cast<float>(x_microBlockSize - 1))
        {
            return values[x_microBlockSize - 1];
        }

        size_t sampleFloor = static_cast<size_t>(std::floor(samplePosition));
        size_t sampleCeil = std::min<size_t>(sampleFloor + 1, x_microBlockSize - 1);
        float frac = samplePosition - static_cast<float>(sampleFloor);
        return static_cast<double>(1.0f - frac) * values[sampleFloor] + static_cast<double>(frac) * values[sampleCeil];
    }

    double GetInterpolatedDirectPhasor(size_t j, float samplePosition) const
    {
        if (j >= x_numLoops)
        {
            return 0.0;
        }

        return InterpolateMicroBlock(m_loops[j].m_phasorIndependent, samplePosition);
    }

    double GetInterpolatedIndirectPhasor(size_t j, float samplePosition) const
    {
        if (j >= x_numLoops)
        {
            return 0.0;
        }

        return InterpolateMicroBlock(m_loops[j].m_phasor, samplePosition);
    }
};

inline TimeLoop* TimeLoop::GetParent(size_t j)
{
    if (m_parentIndex[j] < 0 || TheoryOfTimeBase::x_numLoops <= m_parentIndex[j])
    {
        return nullptr;
    }

    return &m_owner->m_loops[m_parentIndex[j]];
}

inline TimeLoop* TimeLoop::GetMaster()
{
    return m_owner->GetMasterLoop();
}

inline int TimeLoop::GlobalWinding()
{
    return m_owner->m_globalPhase.m_winding;
}

inline bool TimeLoop::TopIndependent(size_t j, int loopSize)
{
    int pos = m_owner->m_positionIndependent[j] % loopSize;
    int prevPos = m_owner->m_prevPositionIndependent[j] % loopSize;
    TimeLoop* parent = GetParent(j);
    return (pos == 0 && prevPos == loopSize - 1) || (pos == loopSize - 1 && prevPos == 0) || (parent && parent->m_topIndependent[j]);
}

struct TheoryOfTime : public TheoryOfTimeBase
{
    Tick2Phasor m_tick2Phasor;
    Phasor2Tick m_phasor2Tick;
    SmartGrid::MessageOutBuffer* m_messageOutBuffer;
    PolyXFaderInternal m_phaseModLFO;
    ScopeWriterHolder m_scopeWriter;
    double m_masterLoopSamples;
    PLL m_pll;

    enum class ClockMode : int 
    {
        Internal,
        External,
        Tick2Phasor,
        PLL
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
            m_phaseModLFOInput.m_theoryOfTime = nullptr;
            m_phaseModLFOInput.m_useIndirectPhasor = false;
            m_phaseModLFOInput.m_phaseShift = -0.75;

            m_modIndex.SetBaseByCenter(1.0 / 16);

            float paramFilterFreq = 0.1 / 48000.0 * SampleTimer::x_controlFrameRate;
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
        PhaseUtils::ZeroedExpParam m_modIndex;
        PhaseUtils::ExpParam m_lfoMult;
        PLL::Input m_pllInput;
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
            if (m_phaseModLFO.m_weights[i] > 0 && m_loops[i].m_loopSize[0] > 0)
            {
                lfoLoopSize = std::lcm(lfoLoopSize, m_loops[i].m_loopSize[0]);
            }
        }

        uiState->m_timeYModAmount.store(GetMasterLoop()->m_loopSize[0] / lfoLoopSize);
    }

    int MonodromyNumber(size_t j, int clockIx, int resetIx)
    {
        return m_loops[clockIx].MonodromyNumber(j, resetIx, true);
    }

    void ProcessPhaseModLFO(size_t j, Input& input)
    {
        for (int i = 0; i < x_numLoops; ++i)
        {
            input.m_phaseModLFOInput.m_externalWeights[i] = static_cast<double>(m_loops[i].m_loopSize[j]) / GetMasterLoop()->m_loopSize[j];
        }

        input.m_phaseModLFOInput.m_mult = input.m_lfoMult.m_expParam;
        input.m_phaseModLFOInput.m_theoryOfTime = this;
        input.m_phaseModLFOInput.m_useIndirectPhasor = false;
        input.m_phaseModLFOInput.m_samplePosition = static_cast<float>(j);
        m_phaseModLFO.Process(input.m_phaseModLFOInput);
        input.m_phaseOffset = -2 * input.m_modIndex.m_expParam * m_phaseModLFO.m_rawOutput;
    }

    void SetupMonoScopeWriter(ScopeWriter* scopeWriter)
    {
        m_scopeWriter = ScopeWriterHolder(scopeWriter, 0, static_cast<size_t>(SmartGridOne::MonoScopes::TheoryOfTime));
    }

    void SetupMessageOutBuffer(SmartGrid::MessageOutBuffer* messageOutBuffer)
    {
        m_messageOutBuffer = messageOutBuffer;
    }

    TheoryOfTime()
    {
        m_masterLoopSamples = 1.0;
    }

    double LoopSamples(size_t j, int loopIndex)
    {
        return m_masterLoopSamples / GetLoopExternalMultiplier(j, loopIndex);
    }

    double LoopSamplesFraction(size_t j, int loopIndex, double fraction)
    {
        return LoopSamples(j, loopIndex) * (m_loops[loopIndex].GetUnwoundPhasor(j) - fraction);
    }

    double PhasorUnwoundSamples(size_t j, int loopIndex)
    {
        return m_loops[loopIndex].GetUnwoundPhasor(j) * LoopSamples(j, loopIndex);
    }

    void ProcessPLLHit(size_t j, Input& input, int loopIndex)
    {
        int64_t division = static_cast<int64_t>(GetLoopExternalMultiplier(j, loopIndex));
        m_pll.ProcessHit(input.m_pllInput, division);
    }

    void ProcessPLLHit(Input& input, int loopIndex)
    {
        int64_t division = static_cast<int64_t>(GetLoopExternalMultiplier(0, loopIndex));
        m_pll.ProcessHit(input.m_pllInput, division);
    }

    void Preprocess(size_t j)
    {
        TheoryOfTimeBase::Preprocess(j);
    }

    void Process(size_t j, Input& input)
    {
        Preprocess(j);
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
            else if (input.m_clockMode == ClockMode::PLL)
            {
                double prevPhase = m_pll.m_phase;
                for (size_t i = 0; i < SampleTimer::x_controlFrameRate; ++i)
                {
                    m_pll.Process(input.m_pllInput);
                }

                input.m_phasorTop = std::floor(prevPhase) != std::floor(m_pll.m_phase);
                input.m_phasor = m_pll.m_phase - std::floor(m_pll.m_phase);
                m_masterLoopSamples = 1.0 / std::max(m_pll.m_freq, 1.0e-12);
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

        bool wasRunning = m_running;

        ProcessPhaseModLFO(j, input);
        TheoryOfTimeBase::Process(j, input);

        if (wasRunning != m_running)
        {
            if (m_running)
            {
                m_phasor2Tick.UpdateDivisions(input.m_freq);
                m_messageOutBuffer->Push(SmartGrid::MessageOut::Start());
            }
            else
            {
                m_messageOutBuffer->Push(SmartGrid::MessageOut::Stop());
            }
        }

        if (m_running)
        {
            if (m_phasor2Tick.Process(GetMasterLoop()->m_phasorIndependent[0]))
            {
                m_messageOutBuffer->Push(SmartGrid::MessageOut::Clock());
            }
        }

        m_scopeWriter.Write(j, m_globalPhase.m_phase);
        if (m_phaseModLFO.m_top)
        {
            m_scopeWriter.RecordStart(j);
        }
    }

    void PrintState(size_t j)
    {
        INFO("theory of time microblock %d global phase %f loop direct (%f,%f,%f,%f,%f,%f) loop indirect (%f,%f,%f,%f,%f,%f) with position/mult (%d/%d, %d/%d, %d/%d, %d/%d, %d/%d, %d/%d)",
            j,
            m_globalPhase.m_phase,
            m_loops[0].m_phasorIndependent[j],
            m_loops[1].m_phasorIndependent[j],
            m_loops[2].m_phasorIndependent[j],
            m_loops[3].m_phasorIndependent[j],
            m_loops[4].m_phasorIndependent[j],
            m_loops[5].m_phasorIndependent[j],
            m_loops[0].m_phasor[j],
            m_loops[1].m_phasor[j],
            m_loops[2].m_phasor[j],
            m_loops[3].m_phasor[j],
            m_loops[4].m_phasor[j],
            m_loops[5].m_phasor[j],
            m_loops[0].m_position[j],
            m_loops[0].m_parentMult[j],
            m_loops[1].m_position[j],
            m_loops[1].m_parentMult[j],
            m_loops[2].m_position[j],
            m_loops[2].m_parentMult[j],
            m_loops[3].m_position[j],
            m_loops[3].m_parentMult[j],
            m_loops[4].m_position[j],
            m_loops[4].m_parentMult[j],
            m_loops[5].m_position[j],
            m_loops[5].m_parentMult[j]);
    }
};

inline double GetTheoryOfTimePhasor(TheoryOfTimeBase* theoryOfTime, size_t j, bool useIndirectPhasor, float samplePosition)
{
    return useIndirectPhasor
        ? theoryOfTime->GetInterpolatedIndirectPhasor(j, samplePosition)
        : theoryOfTime->GetInterpolatedDirectPhasor(j, samplePosition);
}

inline bool GetTheoryOfTimeTop(TheoryOfTimeBase* theoryOfTime, size_t j, bool useIndirectPhasor, float samplePosition)
{
    size_t sampleIndex = static_cast<size_t>(std::round(std::max<float>(0.0f, std::min<float>(samplePosition, static_cast<float>(TheoryOfTimeBase::x_microBlockSize - 1)))));
    return useIndirectPhasor
        ? theoryOfTime->GetIndirectTop(sampleIndex, j)
        : theoryOfTime->GetDirectTop(sampleIndex, j);
}
