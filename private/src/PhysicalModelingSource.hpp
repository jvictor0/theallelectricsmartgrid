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

// One sample per call for every channel. Process takes the amplitude
// envelope from the caller; ProcessWithAHD runs the source's own AHD.
//
template <size_t Size>
struct PhysicalModelingSourceGeneric : PhysicalModelingSourceBase
{
    using Vector = MultiChannelNumber<float, Size>;

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
        return Process(input, modOut);
    }

    void PopulateUIState(UIState* uiState, size_t channel) const
    {
        uiState->m_mainSVFG.store(m_mainSVF.m_g[channel]);
        uiState->m_mainSVFK.store(m_mainSVF.m_k[channel]);
        uiState->m_mainSVFMorph.store(m_mainSVFMorph[channel]);
        m_combFilter.PopulateUIState(&uiState->m_combFilter, channel);
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
