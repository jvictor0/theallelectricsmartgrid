#pragma once

#include "Filter.hpp"
#include "NaiveReuppingLadderShape.hpp"
#include "ConservativeReuppingLadderShape.hpp"
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
    using ConservativeReuppingLadderShape = ConservativeReuppingLadderShapeGeneric<Size>;
    using SVF2PoleShape = SVF2PoleShapeGeneric<Size>;

    struct Snapshot : PowerResponseSnapshot<Size>
    {
        using Base = PowerResponseSnapshot<Size>;
        using typename Base::Frequency;
        using typename Base::ResponseVector;
        FilterMachine m_filterMachine = FilterMachine::NaiveReuppingLadder;
        typename NaiveReuppingLadderShape::Snapshot m_naiveReuppingLadder;
        typename ConservativeReuppingLadderShape::Snapshot m_conservativeReuppingLadder;
        typename SVF2PoleShape::Snapshot m_svf;

        Vector m_srrRate = Vector::Splat(1);

        ResponseVector PreSrrPower(const Frequency& frequency) const
        {
            switch (m_filterMachine)
            {
                case FilterMachine::NaiveReuppingLadder:
                    return m_naiveReuppingLadder.m_lp.MagnitudeSquaredResponses(frequency);
                case FilterMachine::ConservativeReuppingLadder:
                    return m_conservativeReuppingLadder.m_lp.MagnitudeSquaredResponses(frequency);
                case FilterMachine::SVF2Pole:
                default:
                    return m_svf.m_lp.MagnitudeSquaredResponses(frequency);
            }
        }

        ResponseVector PostSrrPower(const Frequency& frequency) const
        {
            switch (m_filterMachine)
            {
                case FilterMachine::NaiveReuppingLadder:
                    return m_naiveReuppingLadder.m_hp.MagnitudeSquaredResponses(frequency);
                case FilterMachine::ConservativeReuppingLadder:
                    return m_conservativeReuppingLadder.m_hp.MagnitudeSquaredResponses(frequency);
                case FilterMachine::SVF2Pole:
                default:
                    return m_svf.m_hp.MagnitudeSquaredResponses(frequency);
            }
        }

        ResponseVector MagnitudeSquaredResponses(const Frequency& frequency) const override
        {
            return PreSrrPower(frequency) * PostSrrPower(frequency);
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
            snapshot.m_srrRate = m_naiveReuppingLadder.m_sampleRateReducer.m_freq;
        }
        else if (machine == FilterMachine::ConservativeReuppingLadder)
        {
            snapshot.m_conservativeReuppingLadder = m_conservativeReuppingLadder.MakeSnapshot();
            snapshot.m_srrRate = m_conservativeReuppingLadder.m_sampleRateReducer.m_freq;
        }
        else
        {
            snapshot.m_svf = m_svf2Pole.MakeSnapshot();
            snapshot.m_srrRate = m_svf2Pole.m_sampleRateReducer.m_freq;
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
    ConservativeReuppingLadderShape m_conservativeReuppingLadder;
    SVF2PoleShape m_svf2Pole;

    Vector m_output;

    ScopeWriterHolderGeneric<Size> m_scopeWriter;

    struct Input
    {
        SourceMachine m_sourceMachine;
        FilterMachine m_filterMachine;

        typename NaiveReuppingLadderShape::Input m_naiveReuppingLadderInput;
        typename ConservativeReuppingLadderShape::Input m_conservativeReuppingLadderInput;
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

            case FilterMachine::ConservativeReuppingLadder:
            {
                m_output = m_conservativeReuppingLadder.Process(input.m_conservativeReuppingLadderInput, dcBlocked);
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
    void WriteScopes(const SampleTopGeneric<Size>& top)
    {
        m_scopeWriter.Write(m_output);
        m_scopeWriter.RecordStarts(top);
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

            case FilterMachine::ConservativeReuppingLadder:
            {
                m_conservativeReuppingLadder.SetEncoderParams(encoders, input.m_conservativeReuppingLadderInput, track, filterBaseFreq);
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
