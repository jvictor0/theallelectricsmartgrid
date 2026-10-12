#pragma once
#include "SampleTop.hpp"

#include "AHD.hpp"
#include "BitCrush.hpp"
#include "TunedDecayComb.hpp"
#include "Math.hpp"
#include "Noise.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "SmartGridOneEncoders.hpp"
#include "Slew.hpp"
#include "StateVariableFilter.hpp"
#include <algorithm>
#include <atomic>
#include <complex>

// Physical modeling source machine.
//
// Signal chain:
//   White Noise -> 2-pole SVF (with morph)
//   -> AHD envelope (inverted, exponential) -> Sample Rate Reducer
//   -> Tuned decay comb with phase-compensated two-pole low-pass damping
//
struct PhysicalModelingSource
{
    static constexpr size_t x_oversample = 4;
    static constexpr size_t x_uBlockSize = SampleTimer::x_controlFrameRate * x_oversample;

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

    // Noise source
    //
    WhiteNoise m_noise;

    // Sample rate reducer
    //
    SampleRateReducer m_sampleRateReducer;

    // Main SVF (pre-envelope)
    //
    LinearStateVariableFilter m_mainSVF;

    // AHD envelope for amplitude modulation
    //
    AHD m_ahd;

    // Tuned decay comb with phase-compensated two-pole low-pass damping
    //
    TunedDecayComb m_combFilter;

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
            : m_baseFreq(PhaseUtils::VOctToNatural(0.0, 1.0 / 48000.0))
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
        : m_noise(42)
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

        // Update exact comb coefficients outside the oversampled loop.
        //
        m_combFilter.SetParams(
            input.m_baseFreq / x_oversample,
            input.m_combFeedback,
            input.m_combDamp,
            48000.0 * x_oversample);

        for (size_t i = 0; i < x_uBlockSize; ++i)
        {
            input.m_ahdInput.m_samplePosition = static_cast<float>(i) / x_oversample;
            input.m_ahdInputSetter.Process(input.m_ahdInput);
            float ahdEnv = m_ahd.Process(input.m_ahdInput);

            // Process slews
            //
            float sampleRateReducerFreq = m_sampleRateReducerFreqSlew.Process();
            float mainSVFCutoff = m_mainSVFCutoffSlew.Process();
            float mainSVFResonance = m_mainSVFResonanceSlew.Process();
            float mainSVFMorph = m_mainSVFMorphSlew.Process();

            // 1. Generate white noise
            //
            float noise = static_cast<float>(m_noise.Generate());

            // 3. Main SVF processing (morph inside SVF: blend 0=LP, 1=HP, equal power)
            // Adjust cutoff for oversample rate
            //
            float adjustedMainCutoff = std::min(0.499f / x_oversample, mainSVFCutoff / x_oversample);
            m_mainSVF.SetCutoff(adjustedMainCutoff);
            m_mainSVF.SetResonance(mainSVFResonance);
            m_mainSVF.Process(noise);
            float svfOut = m_mainSVF.GetMorphedOutput(mainSVFMorph);

            // 4. AHD envelope amplitude modulation
            // The envelope is inverted (like amp section): at 0 amplitude, output is at full
            // Apply exponential curve for natural modulation
            // Use slewed envelope for smooth oversampled modulation
            //
            float envValue = PhaseUtils::ZeroedExpParam::Compute(10.0f, ahdEnv);
            float envelopedSignal = svfOut * envValue;

            // 2. Sample rate reduction
            // Adjust for oversample rate
            //
            m_sampleRateReducer.SetFreq(sampleRateReducerFreq / x_oversample);
            float prefiltered = m_sampleRateReducer.Process(envelopedSignal);

            // 5. Tuned decay comb with phase-compensated two-pole low-pass damping
            //
            m_output = m_combFilter.Process(prefiltered);
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
        uiState->m_mainSVFG.store(m_mainSVF.m_g);
        uiState->m_mainSVFK.store(m_mainSVF.m_k);
        uiState->m_mainSVFMorph.store(m_mainSVFMorphSlew.m_filter.m_output);
        m_combFilter.PopulateUIState(&uiState->m_combFilter);
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
        input.m_mainSVFCutoff.Update(encoders.GetValueNoSlew(Param::PMMainSVFCutoff, voiceIx));
        input.m_mainSVFResonance.Update(encoders.GetValueNoSlew(Param::PMMainSVFResonance, voiceIx));
        input.m_mainSVFMorph = encoders.GetValueNoSlew(Param::PMMainSVFMorph, voiceIx);
        input.m_sampleRateReducerFreq.Update(1 - encoders.GetValueNoSlew(Param::PMSampleRateReduction, voiceIx));

        // Row 1: AHD envelope params (inverted like amp section)
        //
        input.SetAHD(
            encoders.GetValueNoSlew(Param::PMAHDAttack, voiceIx),
            encoders.GetValueNoSlew(Param::PMAHDHold, voiceIx),
            encoders.GetValueNoSlew(Param::PMAHDDecay, voiceIx),
            encoders.GetValueNoSlew(Param::PMAmplitude, voiceIx));

        // Row 2: Comb filter params
        // Feedback knob is mapped in the comb filter to signed damping time
        //
        input.m_combFeedback = encoders.GetValue(Param::PMCombFeedback, voiceIx);

        // Comb damping params
        //
        input.m_combDamp = encoders.GetValueNoSlew(Param::PMCombDampenCutoff, voiceIx);
    }
};
