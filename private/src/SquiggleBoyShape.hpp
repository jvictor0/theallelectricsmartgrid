#pragma once

#include "Filter.hpp"
#include "NaiveReuppingLadderShape.hpp"
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
    using NaiveReuppingLadderShape = NaiveReuppingLadderShapeGeneric<Size>;
    using SVF2PoleShape = SVF2PoleShapeGeneric<Size>;

    struct Snapshot : PowerResponseSnapshot<Size>
    {
        using Base = PowerResponseSnapshot<Size>;
        using typename Base::Frequency;
        using typename Base::ResponseVector;
        FilterMachine m_filterMachine = FilterMachine::NaiveReuppingLadder;
        typename NaiveReuppingLadderShape::Snapshot m_naiveReuppingLadder;
        typename SVF2PoleShape::Snapshot m_svf;

        ResponseVector MagnitudeSquaredResponses(const Frequency& frequency) const override
        {
            return m_filterMachine == FilterMachine::NaiveReuppingLadder
                ? m_naiveReuppingLadder.MagnitudeSquaredResponses(frequency) : m_svf.MagnitudeSquaredResponses(frequency);
        }
    };

    using UIState = TransferFunctionGeneric<Snapshot>;

    Snapshot MakeSnapshot(FilterMachine machine) const
    {
        Snapshot snapshot;
        snapshot.m_filterMachine = machine;
        if (machine == FilterMachine::NaiveReuppingLadder)
        {
            snapshot.m_naiveReuppingLadder = m_naiveReuppingLadder.MakeSnapshot();
        }
        else
        {
            snapshot.m_svf = m_svf2Pole.MakeSnapshot();
        }

        return snapshot;
    }

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer, FilterMachine machine) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot(machine);
        });
    }

    // DC blocker before the filter machines (5 Hz)
    //
    OPHighPassFilterGeneric<float, Size> m_dcBlocker;

    // Filter machines
    //
    NaiveReuppingLadderShape m_naiveReuppingLadder;
    SVF2PoleShape m_svf2Pole;

    Vector m_output;

    ScopeWriterHolderGeneric<Size> m_scopeWriter;

    struct Input
    {
        SourceMachine m_sourceMachine;
        FilterMachine m_filterMachine;

        typename NaiveReuppingLadderShape::Input m_naiveReuppingLadderInput;
        typename SVF2PoleShape::Input m_svf2PoleInput;

        Input()
            : m_sourceMachine(SourceMachine::DualWaveShapingVCO)
            , m_filterMachine(FilterMachine::NaiveReuppingLadder)
        {
        }
    };

    SquiggleBoyShapeGeneric()
        : m_output(Vector::Splat(0))
    {
        m_dcBlocker.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(5.0));
    }

    void SetScopeWriters(ScopeWriterInput<Size>* scopeWriter, size_t track)
    {
        m_scopeWriter = ScopeWriterHolderGeneric<Size>(scopeWriter,
            SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::PostFilter), track));
    }

    Vector Process(Input& input, const Vector& x)
    {
        Vector dcBlocked = m_dcBlocker.Process(x);
        switch (input.m_filterMachine)
        {
            case FilterMachine::NaiveReuppingLadder:
            {
                m_output = m_naiveReuppingLadder.Process(input.m_naiveReuppingLadderInput, dcBlocked);
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
        m_scopeWriter.Write(m_output);
        for (size_t i = 0; i < Size; ++i)
        {
            if (top[i])
            {
                m_scopeWriter.RecordStartAtVoice(i, top[i].m_offset);
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
            case FilterMachine::NaiveReuppingLadder:
            {
                m_naiveReuppingLadder.SetEncoderParams(encoders, input.m_naiveReuppingLadderInput, track, filterBaseFreq);
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

};

using SquiggleBoyShape = SquiggleBoyShapeGeneric<1>;
using TrioSquiggleBoyShape = SquiggleBoyShapeGeneric<3>;
