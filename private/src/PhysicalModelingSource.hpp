#pragma once
#include "SampleTop.hpp"

#include "AHD.hpp"
#include "BitCrush.hpp"
#include "TunedDecayComb.hpp"
#include "Math.hpp"
#include "MultiChannelNumber.hpp"
#include "Noise.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "ScopeWriter.hpp"
#include "SmartGridOneEncoders.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "StateVariableFilter.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>

// Physical modeling source machine.
//
// Signal chain:
//   White Noise -> 2-pole SVF (with morph)
//   -> AHD envelope (inverted, exponential) -> Sample Rate Reducer
//   -> Tuned decay comb with phase-compensated two-pole low-pass damping
//
// One sample per call for every channel. Process takes the amplitude
// envelope from the caller; ProcessWithAHD runs the source's own AHD.
//
template <size_t Size>
struct PhysicalModelingSourceGeneric
{
    using Vector = MultiChannelNumber<float, Size>;

    struct Snapshot : FilterResponseSnapshot
    {
        typename LinearStateVariableFilterGeneric<float, Size>::Snapshot m_mainSVF;
        TunedDecayComb::Snapshot m_combFilter;

        std::complex<float> ComplexResponse(const ResponseFrequency& frequency, size_t) const override
        {
            return m_mainSVF.ComplexResponse(frequency, 0) * m_combFilter.ComplexResponse(frequency, 0);
        }

        float MagnitudeResponse(const ResponseFrequency& frequency, size_t) const override
        {
            return m_mainSVF.MagnitudeResponse(frequency, 0) * m_combFilter.MagnitudeResponse(frequency, 0);
        }
    };

    using UIState = TransferFunctionGeneric<Snapshot>;

    Snapshot MakeSnapshot(size_t channel) const
    {
        Snapshot snapshot;
        snapshot.m_mainSVF = m_mainSVF.MakeSnapshot(channel);
        snapshot.m_mainSVF.m_weights = m_mainSVF.GetMorphWeights(m_mainSVFMorph[channel]);
        snapshot.m_combFilter = m_combFilter.MakeSnapshot(channel);
        return snapshot;
    }

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer, size_t channel) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot(channel);
        });
    }

    using Envelope = AHDGeneric<Size>;

    WhiteNoiseGeneric<Size> m_noise;
    SampleRateReducerGeneric<Size> m_sampleRateReducer;
    LinearStateVariableFilterGeneric<float, Size> m_mainSVF;
    TunedDecayCombGeneric<Size> m_combFilter;
    Vector m_mainSVFMorph;
    Vector m_output;

    // Amplitude envelope for ProcessWithAHD
    //
    Envelope m_ahd;
    ScopeWriterHolderGeneric<Size> m_envelopeScopeWriter;

    // Encoder mappings. Cutoff and sample rate reduction are in cycles per
    // host sample; the AHD allows 100 us minimum attack and 500 us minimum
    // decay.
    //
    PhaseUtils::ExpParamGeneric<float, Size> m_mainSVFCutoffParam;
    PhaseUtils::ZeroedExpParamGeneric<float, Size> m_mainSVFResonanceParam;
    PhaseUtils::ExpParamGeneric<float, Size> m_sampleRateReducerFreqParam;
    typename Envelope::InputSetter m_ahdInputSetter;

    // Frequencies are in cycles per sample at the processing rate. The
    // envelope is the AHD output in [0, 1].
    //
    struct Input
    {
        Vector m_mainSVFCutoff;
        Vector m_mainSVFResonance;
        Vector m_mainSVFMorph;
        Vector m_sampleRateReducerFreq;
        Vector m_envelope;

        // Inverted like the amp section; drives m_envelope in ProcessWithAHD.
        //
        typename Envelope::Input m_ahdInput;

        Input()
            : m_mainSVFCutoff(Vector::Splat(0.001))
            , m_mainSVFResonance(Vector::Splat(0))
            , m_mainSVFMorph(Vector::Splat(0.5))
            , m_sampleRateReducerFreq(Vector::Splat(1))
            , m_envelope(Vector::Splat(0))
        {
            for (size_t i = 0; i < Size; ++i)
            {
                m_ahdInput.m_channels[i].m_amplitudePolarity = false;
            }
        }
    };

    explicit PhysicalModelingSourceGeneric(uint32_t seed)
        : m_noise(seed)
        , m_mainSVFMorph(Vector::Splat(0.5))
        , m_output(Vector::Splat(0))
        , m_mainSVFCutoffParam(0.001f, 0.499f)
        // Headroom lets the reducer reach the full-rate bypass.
        //
        , m_sampleRateReducerFreqParam(0.001f, 4.0001f)
        , m_ahdInputSetter(0.0001f, 2.5f, 0.0005f, 2.5f)
    {
        m_mainSVFResonanceParam.SetBaseByCenter(0.125);
    }

    void SetScopeWriters(ScopeWriterInput<Size>* scopeWriter, size_t track)
    {
        m_envelopeScopeWriter = ScopeWriterHolderGeneric<Size>(scopeWriter,
            SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::PhysicalModelingEnvelope), track));
    }

    void SetCombParams(const Vector& normalizedFrequency, const Vector& decayKnob, const Vector& dampKnob, double sampleRate)
    {
        m_combFilter.SetParams(
            normalizedFrequency.template Convert<double>(),
            decayKnob.template Convert<double>(),
            dampKnob.template Convert<double>(),
            sampleRate);
    }

    Vector Process(const Input& input)
    {
        Vector modOut[2];
        return Process(input, modOut);
    }

    Vector Process(const Input& input, Vector (&modOut)[2])
    {
        static constexpr float x_envelopeBase = 10.0f;

        Vector noise = m_noise.Generate();

        // Morph inside the SVF: blend 0 = LP, 1 = HP, equal power.
        //
        m_mainSVF.SetCutoffAndResonance(input.m_mainSVFCutoff, input.m_mainSVFResonance);
        m_mainSVF.Process(noise);
        m_mainSVFMorph = input.m_mainSVFMorph;
        Vector svfOut = m_mainSVF.GetMorphedOutput(m_mainSVFMorph);

        modOut[0] = svfOut * 0.5f + 0.5f;
        modOut[1] = input.m_envelope;

        // The envelope is inverted (like the amp section): at 0 amplitude the
        // output is at full. An exponential curve gives natural modulation.
        //
        Vector envValue = PhaseUtils::ZeroedExpParamGeneric<float, Size>::Evaluate(
            x_envelopeBase,
            std::log2(x_envelopeBase),
            input.m_envelope);

        m_sampleRateReducer.SetFreq(input.m_sampleRateReducerFreq);
        Vector prefiltered = m_sampleRateReducer.Process(svfOut * envValue);

        m_output = m_combFilter.Process(prefiltered);
        return m_output;
    }

    // Runs the AHD into the envelope, then the source.
    //
    Vector ProcessWithAHD(Input& input)
    {
        Vector modOut[2];
        return ProcessWithAHD(input, modOut);
    }

    Vector ProcessWithAHD(Input& input, Vector (&modOut)[2])
    {
        input.m_envelope = m_ahd.Process(input.m_ahdInput);
        m_envelopeScopeWriter.Write(input.m_envelope);
        for (size_t lane = 0; lane < Size; ++lane)
        {
            const auto& envelope = m_ahd.m_channels[lane];
            if (envelope.m_triggered)
            {
                m_envelopeScopeWriter.RecordStartAtVoice(lane, 0.0);
            }

            if (envelope.m_voiceCellCrossed && envelope.m_state == AHD::State::Idle
                && m_envelopeScopeWriter.IsStartedAtVoice(lane))
            {
                m_envelopeScopeWriter.RecordEndAtVoice(lane);
            }
        }

        return Process(input, modOut);
    }

    void PopulateUIState(UIState* uiState, size_t channel) const
    {
        uiState->m_history.Push(MakeSnapshot(channel));
    }

    void PopulateUIState(UIState* uiState) const
    {
        static_assert(Size == 1, "multi-channel sources must name the channel to publish");
        PopulateUIState(uiState, 0);
    }

    void SetEncoderParams(
        SmartGridOneEncoders& encoders,
        Input& input,
        size_t track,
        const Vector& baseFreq)
    {
        using Param = SmartGridOneEncoders::Param;

        // Row 0: Main SVF params + sample rate reduction
        //
        input.m_mainSVFCutoff = m_mainSVFCutoffParam.Update(encoders.GetValue<Size>(Param::PMMainSVFCutoff, track)) / SampleTimer::x_oversample;
        input.m_mainSVFResonance = m_mainSVFResonanceParam.Update(encoders.GetValue<Size>(Param::PMMainSVFResonance, track));
        input.m_mainSVFMorph = encoders.GetValue<Size>(Param::PMMainSVFMorph, track);
        input.m_sampleRateReducerFreq = m_sampleRateReducerFreqParam.Update(
            Vector::Splat(1) - encoders.GetValue<Size>(Param::PMSampleRateReduction, track)) / SampleTimer::x_oversample;

        // Row 1: AHD envelope params (inverted like amp section)
        //
        m_ahdInputSetter.Set(
            encoders.GetValue<Size>(Param::PMAHDAttack, track),
            encoders.GetValue<Size>(Param::PMAHDHold, track),
            encoders.GetValue<Size>(Param::PMAHDDecay, track),
            encoders.GetValue<Size>(Param::PMAmplitude, track),
            false,
            input.m_ahdInput);

        // Row 2: Comb filter params. The feedback knob is mapped in the comb
        // filter to signed damping time; the comb computes exact coefficients
        // from these when its next crossfade starts.
        //
        SetCombParams(
            baseFreq,
            encoders.GetValue<Size>(Param::PMCombFeedback, track),
            encoders.GetValue<Size>(Param::PMCombDampenCutoff, track),
            static_cast<double>(SampleTimer::x_sampleRate));
    }
};

using PhysicalModelingSource = PhysicalModelingSourceGeneric<1>;
using TrioPhysicalModelingSource = PhysicalModelingSourceGeneric<3>;
