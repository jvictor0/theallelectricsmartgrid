#pragma once

#include "Filter.hpp"
#include "Ladder4PoleShape.hpp"
#include "MultiChannelNumber.hpp"
#include "SampleTimer.hpp"
#include "SampleTop.hpp"
#include "ScopeWriter.hpp"
#include "SmartGridOneEncoders.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "SquiggleBoyShapeBase.hpp"
#include "SVF2PoleShape.hpp"
#include "VoiceMachineEnums.hpp"

// Encapsulates filter machine selection and dispatch logic. Contains all
// filter machines and handles Process dispatch and encoder wiring. One sample
// per call for every channel; channel i is voice track * Size + i.
//
template <size_t Size>
struct SquiggleBoyShapeGeneric : SquiggleBoyShapeBase
{
    using Vector = MultiChannelNumber<float, Size>;
    using SourceMachine = VoiceMachine::SourceMachine;
    using FilterMachine = VoiceMachine::FilterMachine;
    using Ladder4PoleShape = Ladder4PoleShapeGeneric<Size>;
    using SVF2PoleShape = SVF2PoleShapeGeneric<Size>;

    // DC blocker before the filter machines (5 Hz)
    //
    OPHighPassFilterGeneric<float, Size> m_dcBlocker;

    // Filter machines
    //
    Ladder4PoleShape m_ladder4Pole;
    SVF2PoleShape m_svf2Pole;

    Vector m_output;

    ScopeWriterHolder m_scopeWriter[Size];

    struct Input
    {
        SourceMachine m_sourceMachine;
        FilterMachine m_filterMachine;

        typename Ladder4PoleShape::Input m_ladder4PoleInput;
        typename SVF2PoleShape::Input m_svf2PoleInput;

        Input()
            : m_sourceMachine(SourceMachine::DualWaveShapingVCO)
            , m_filterMachine(FilterMachine::Ladder4Pole)
        {
        }
    };

    SquiggleBoyShapeGeneric()
        : m_output(Vector::Splat(0))
    {
        m_dcBlocker.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(5.0));
    }

    void SetScopeWriters(ScopeWriter* scopeWriter, size_t track)
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_scopeWriter[i] = ScopeWriterHolder(scopeWriter, track * Size + i, static_cast<size_t>(SmartGridOne::AudioScopes::PostFilter));
        }
    }

    Vector Process(Input& input, const Vector& x)
    {
        Vector dcBlocked = m_dcBlocker.Process(x);
        switch (input.m_filterMachine)
        {
            case FilterMachine::Ladder4Pole:
            {
                m_output = m_ladder4Pole.Process(input.m_ladder4PoleInput, dcBlocked);
                break;
            }

            case FilterMachine::SVF2Pole:
            default:
            {
                m_output = m_svf2Pole.Process(input.m_svf2PoleInput, dcBlocked);
                break;
            }
        }

        return m_output;
    }

    // Writes the post-filter scopes, starting a cycle at each source top.
    //
    void WriteScopes(const SampleTop* top)
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_scopeWriter[i].Write(m_output[i]);
            if (top[i])
            {
                m_scopeWriter[i].RecordStart(top[i].m_offset);
            }
        }
    }

    // Called from SetEncoderParameters to set the shape encoder params.
    // Dispatches to the active filter machine.
    //
    void SetEncoderParams(
        SmartGridOneEncoders& encoders,
        Input& input,
        size_t track,
        const Vector& baseFreq)
    {
        // Thru has no pitch, so the filters track a fixed 80 Hz.
        //
        Vector filterBaseFreq = baseFreq;
        if (input.m_sourceMachine == SourceMachine::Thru)
        {
            filterBaseFreq = Vector::Splat(SampleTimer::HzToCycPerSamp(80.0));
        }

        switch (input.m_filterMachine)
        {
            case FilterMachine::Ladder4Pole:
            {
                m_ladder4Pole.SetEncoderParams(encoders, input.m_ladder4PoleInput, track, filterBaseFreq);
                break;
            }

            case FilterMachine::SVF2Pole:
            default:
            {
                m_svf2Pole.SetEncoderParams(encoders, input.m_svf2PoleInput, track, filterBaseFreq);
                break;
            }
        }
    }

    void DebugPrint()
    {
        m_ladder4Pole.m_lpLadder.DebugPrint();
    }
};

using SquiggleBoyShape = SquiggleBoyShapeGeneric<1>;
using TrioSquiggleBoyShape = SquiggleBoyShapeGeneric<3>;
