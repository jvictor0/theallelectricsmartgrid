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
#include "SmartGridOneEncoders.hpp"
#include "Slew.hpp"
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
struct PhysicalModelingSourceBase
{
    struct UIState : TransferFunction
    {
        std::atomic<float> m_mainSVFG;
        std::atomic<float> m_mainSVFK;
        std::atomic<float> m_mainSVFMorph;
        TunedDecayComb::UIState m_combFilter;

        UIState()
            : m_mainSVFG(0.0f)
            , m_mainSVFK(0.0f)
            , m_mainSVFMorph(0.5f)
        {
        }

        std::complex<float> TransferFunctionValue(float normalizedFreq) const override
        {
            float freq = std::max(0.0f, std::min(0.5f, normalizedFreq));
            float g = m_mainSVFG.load();
            float k = m_mainSVFK.load();
            auto weights = LinearStateVariableFilter::GetMorphWeights(m_mainSVFMorph.load());
            std::complex<float> svfTransfer =
                weights.m_lowPass * LinearStateVariableFilter::LowPassTransferFunction(g, k, freq) +
                weights.m_bandPass * LinearStateVariableFilter::BandPassTransferFunction(g, k, freq) +
                weights.m_highPass * LinearStateVariableFilter::HighPassTransferFunction(g, k, freq);

            return svfTransfer * m_combFilter.TransferFunctionValue(freq);
        }

        float FrequencyResponse(float normalizedFreq) const override
        {
            return std::abs(TransferFunctionValue(normalizedFreq));
        }
    };
};

// One sample per call for every channel. The amplitude envelope comes from
// the caller.
//
template <size_t Size>
struct PhysicalModelingSourceGeneric : PhysicalModelingSourceBase
{
    using Vector = MultiChannelNumber<float, Size>;

    WhiteNoiseGeneric<Size> m_noise;
    SampleRateReducerGeneric<Size> m_sampleRateReducer;
    LinearStateVariableFilterGeneric<float, Size> m_mainSVF;
    TunedDecayCombGeneric<Size> m_combFilter;
    Vector m_mainSVFMorph;
    Vector m_output;

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

        Input()
            : m_mainSVFCutoff(Vector::Splat(0.001))
            , m_mainSVFResonance(Vector::Splat(0))
            , m_mainSVFMorph(Vector::Splat(0.5))
            , m_sampleRateReducerFreq(Vector::Splat(1))
            , m_envelope(Vector::Splat(0))
        {
        }
    };

    explicit PhysicalModelingSourceGeneric(uint32_t seed)
        : m_noise(seed)
        , m_mainSVFMorph(Vector::Splat(0.5))
        , m_output(Vector::Splat(0))
    {
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
        static constexpr float x_envelopeBase = 10.0f;

        Vector noise = m_noise.Generate();

        // Morph inside the SVF: blend 0 = LP, 1 = HP, equal power.
        //
        m_mainSVF.SetCutoff(input.m_mainSVFCutoff);
        m_mainSVF.SetResonance(input.m_mainSVFResonance);
        m_mainSVF.Process(noise);
        m_mainSVFMorph = input.m_mainSVFMorph;
        Vector svfOut = m_mainSVF.GetMorphedOutput(m_mainSVFMorph);

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

    void PopulateUIState(UIState* uiState, size_t channel) const
    {
        uiState->m_mainSVFG.store(m_mainSVF.m_g[channel]);
        uiState->m_mainSVFK.store(m_mainSVF.m_k[channel]);
        uiState->m_mainSVFMorph.store(m_mainSVFMorph[channel]);
        m_combFilter.PopulateUIState(&uiState->m_combFilter, channel);
    }
};

using TrioPhysicalModelingSource = PhysicalModelingSourceGeneric<3>;

// Mono micro-block shell: owns the AHD and slews the control-rate inputs into
// the core at four times the base rate.
//
struct PhysicalModelingSource : PhysicalModelingSourceBase
{
    static constexpr size_t x_oversample = 4;
    static constexpr size_t x_uBlockSize = SampleTimer::x_controlFrameRate * x_oversample;

    using Core = PhysicalModelingSourceGeneric<1>;

    Core m_core;

    // AHD envelope for amplitude modulation
    //
    AHD m_ahd;

    // Output buffers
    //
    SampleTop m_uBlockTop[SampleTimer::x_controlFrameRate];
    float m_uBlockOutput[x_uBlockSize];

    float m_output;
    SampleTop m_top;

    // Slewed parameters for oversampled processing
    // The comb crossfades parameter changes internally
    //
    ParamSlew m_sampleRateReducerFreqSlew;
    ParamSlew m_mainSVFCutoffSlew;
    ParamSlew m_mainSVFResonanceSlew;
    ParamSlew m_mainSVFMorphSlew;

    struct Input
    {
        float m_baseFreq;

        // Row 0: Main SVF params + sample rate reduction
        //
        PhaseUtils::ExpParam m_mainSVFCutoff;
        PhaseUtils::ZeroedExpParam m_mainSVFResonance;
        float m_mainSVFMorph;
        PhaseUtils::ExpParam m_sampleRateReducerFreq;

        // Row 1: AHD envelope (inverted like amp section)
        // InputSetter allows 100 µs minimum attack and 500 µs minimum decay
        //
        AHD::SlewedInputSetter m_ahdInputSetter;
        AHD::Input m_ahdInput;

        // Row 2: Comb filter params
        // Feedback knob: [0, 1], mapped to signed damping-time feedback in comb filter
        //
        float m_combFeedback;

        // Comb damping params
        // Prototype -3 dB cutoff: exponential 2x to 128x pitch, default 16x
        // Variable Q retains a unity peak and exact stationary fundamental decay
        // Clamped below processing Nyquist; only exact DECAY center bypasses
        //
        float m_combDamp;

        Input()
            : m_baseFreq(PhaseUtils::VOctToNatural(0.0, SampleTimer::x_samplePeriod))
            , m_mainSVFCutoff(0.001f, 0.499f)
            , m_mainSVFMorph(0.5f)
            // Headroom lets the parameter slew reach the full-rate bypass.
            //
            , m_sampleRateReducerFreq(0.001f, 4.0001f)
            , m_ahdInputSetter(x_oversample, 0.0001f, 2.5f, 0.0005f, 2.5f)
            , m_combFeedback(0.0f)
            , m_combDamp(0.5f)
        {
            m_mainSVFResonance.SetBaseByCenter(0.125);
            m_ahdInput.m_amplitudePolarity = false;
            m_ahdInput.m_releaseFactor = 1.0f / static_cast<float>(x_oversample);
        }

        void SetAHD(float attack, float hold, float decay, float amplitude)
        {
            m_ahdInputSetter.SetTargets(attack, hold, decay, amplitude);
        }
    };

    PhysicalModelingSource()
        : m_core(42)
        , m_output(0.0f)
        , m_top(false)
        , m_sampleRateReducerFreqSlew(x_oversample)
        , m_mainSVFCutoffSlew(x_oversample)
        , m_mainSVFResonanceSlew(x_oversample)
        , m_mainSVFMorphSlew(x_oversample)
    {
    }

    void ProcessUBlock(Input& input)
    {
        // Update slew targets at start of block
        //
        m_sampleRateReducerFreqSlew.Update(input.m_sampleRateReducerFreq.m_expParam);
        m_mainSVFCutoffSlew.Update(input.m_mainSVFCutoff.m_expParam);
        m_mainSVFResonanceSlew.Update(input.m_mainSVFResonance.m_expParam);
        m_mainSVFMorphSlew.Update(input.m_mainSVFMorph);

        // The comb computes exact coefficients from these when its next
        // crossfade starts.
        //
        m_core.SetCombParams(
            input.m_baseFreq / x_oversample,
            input.m_combFeedback,
            input.m_combDamp,
            static_cast<double>(SampleTimer::x_sampleRate * x_oversample));

        Core::Input coreInput;
        for (size_t i = 0; i < x_uBlockSize; ++i)
        {
            input.m_ahdInput.m_samplePosition = static_cast<float>(i) / x_oversample;
            input.m_ahdInputSetter.Process(input.m_ahdInput);
            coreInput.m_envelope = m_ahd.Process(input.m_ahdInput);

            // Frequencies move to the oversampled rate; the SVF cutoff stays
            // below the base-rate Nyquist.
            //
            coreInput.m_sampleRateReducerFreq = m_sampleRateReducerFreqSlew.Process() / x_oversample;
            coreInput.m_mainSVFCutoff = std::min(0.499f / x_oversample, m_mainSVFCutoffSlew.Process() / x_oversample);
            coreInput.m_mainSVFResonance = m_mainSVFResonanceSlew.Process();
            coreInput.m_mainSVFMorph = m_mainSVFMorphSlew.Process();

            m_output = m_core.Process(coreInput);
            m_uBlockOutput[i] = m_output;

            // Base-rate operations every x_oversample samples
            //
            if ((i + 1) % x_oversample == 0)
            {
                size_t baseIndex = i / x_oversample;

                // For physical modeling, we don't have a clear "top" like an oscillator
                // Use comb filter phase for sync reference
                //
                m_uBlockTop[baseIndex] = false;
            }
        }
    }

    float Process(Input& input)
    {
        if (SampleTimer::IsControlFrame())
        {
            ProcessUBlock(input);
        }

        m_output = m_uBlockOutput[SampleTimer::GetUBlockIndex()];
        m_top = m_uBlockTop[SampleTimer::GetUBlockIndex()];

        return m_output;
    }

    void PopulateUIState(UIState* uiState)
    {
        m_core.PopulateUIState(uiState, 0);
        uiState->m_mainSVFMorph.store(m_mainSVFMorphSlew.m_filter.m_output);
    }

    void SetEncoderParams(
        SmartGridOneEncoders& encoders,
        Input& input,
        size_t voiceIx,
        float baseFreq)
    {
        using Param = SmartGridOneEncoders::Param;

        input.m_baseFreq = baseFreq;

        // Row 0: Main SVF params + sample rate reduction
        //
        input.m_mainSVFCutoff.Update(encoders.GetValue(Param::PMMainSVFCutoff, voiceIx));
        input.m_mainSVFResonance.Update(encoders.GetValue(Param::PMMainSVFResonance, voiceIx));
        input.m_mainSVFMorph = encoders.GetValue(Param::PMMainSVFMorph, voiceIx);
        input.m_sampleRateReducerFreq.Update(1 - encoders.GetValue(Param::PMSampleRateReduction, voiceIx));

        // Row 1: AHD envelope params (inverted like amp section)
        //
        input.SetAHD(
            encoders.GetValue(Param::PMAHDAttack, voiceIx),
            encoders.GetValue(Param::PMAHDHold, voiceIx),
            encoders.GetValue(Param::PMAHDDecay, voiceIx),
            encoders.GetValue(Param::PMAmplitude, voiceIx));

        // Row 2: Comb filter params
        // Feedback knob is mapped in the comb filter to signed damping time
        //
        input.m_combFeedback = encoders.GetValue(Param::PMCombFeedback, voiceIx);

        // Comb damping params
        //
        input.m_combDamp = encoders.GetValue(Param::PMCombDampenCutoff, voiceIx);
    }
};
