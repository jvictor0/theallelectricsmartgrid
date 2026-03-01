#pragma once

#include "AHD.hpp"
#include "BitCrush.hpp"
#include "CombFilterWithSVF.hpp"
#include "Math.hpp"
#include "Noise.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "Slew.hpp"
#include "StateVariableFilter.hpp"

// Physical modeling source machine.
//
// Signal chain:
//   White Noise -> Sample Rate Reducer -> 2-pole SVF (with morph) 
//   -> AHD envelope (inverted, exponential) amplitude modulation
//   -> Comb Filter with SVF in feedback loop
//
struct PhysicalModelingSource
{
    static constexpr size_t x_oversample = 4;
    static constexpr size_t x_uBlockSize = SampleTimer::x_controlFrameRate * x_oversample;

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

    // Comb filter with SVF in feedback
    //
    CombFilterWithSVF m_combFilter;

    // Output buffers
    //
    bool m_uBlockTop[SampleTimer::x_controlFrameRate];
    float m_uBlockOutput[x_uBlockSize];

    float m_output;
    bool m_top;

    // Slewed parameters for oversampled processing
    //
    ParamSlew m_sampleRateReducerFreqSlew;
    ParamSlew m_mainSVFCutoffSlew;
    ParamSlew m_mainSVFResonanceSlew;
    ParamSlew m_mainSVFMorphSlew;
    ParamSlew m_ahdEnvSlew;
    ParamSlew m_combFreqSlew;
    ParamSlew m_combFeedbackSlew;
    ParamSlew m_combSVFCutoffSlew;
    ParamSlew m_combSVFResonanceSlew;
    ParamSlew m_combSVFMorphSlew;

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
        //
        AHD::InputSetter m_ahdInputSetter;
        AHD::Input m_ahdInput;

        // Row 2: Comb filter params
        //
        PhaseUtils::ExpParam m_combFreq;
        PhaseUtils::ZeroedExpParam m_combFeedback;

        // Row 3: Comb SVF params
        //
        PhaseUtils::ExpParam m_combSVFCutoff;
        PhaseUtils::ZeroedExpParam m_combSVFResonance;
        float m_combSVFMorph;

        Input()
            : m_baseFreq(PhaseUtils::VOctToNatural(0.0, 1.0 / 48000.0))
            , m_mainSVFCutoff(0.001f, 0.499f)
            , m_mainSVFMorph(0.5f)
            , m_sampleRateReducerFreq(1.0f, 2048.0f)
            , m_combFreq(0.001f, 0.25f)
            , m_combSVFCutoff(0.001f, 0.499f)
            , m_combSVFMorph(0.5f)
        {
            m_mainSVFResonance.SetBaseByCenter(0.125);
            m_combFeedback.SetMax(0.99f);
            m_combSVFResonance.SetBaseByCenter(0.125);
        }

        void SetAHD(float attack, float hold, float decay, float amplitude)
        {
            // Inverted AHD like amp section (amplitudePolarity = false)
            //
            m_ahdInputSetter.Set(attack, hold, decay, amplitude, false, m_ahdInput);
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
        , m_ahdEnvSlew(x_oversample)
        , m_combFreqSlew(x_oversample)
        , m_combFeedbackSlew(x_oversample)
        , m_combSVFCutoffSlew(x_oversample)
        , m_combSVFResonanceSlew(x_oversample)
        , m_combSVFMorphSlew(x_oversample)
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
        m_combFreqSlew.Update(input.m_combFreq.m_expParam);
        m_combFeedbackSlew.Update(input.m_combFeedback.m_expParam);
        m_combSVFCutoffSlew.Update(input.m_combSVFCutoff.m_expParam);
        m_combSVFResonanceSlew.Update(input.m_combSVFResonance.m_expParam);
        m_combSVFMorphSlew.Update(input.m_combSVFMorph);

        for (size_t i = 0; i < x_uBlockSize; ++i)
        {
            // Process slews
            //
            float sampleRateReducerFreq = m_sampleRateReducerFreqSlew.Process();
            float mainSVFCutoff = m_mainSVFCutoffSlew.Process();
            float mainSVFResonance = m_mainSVFResonanceSlew.Process();
            float mainSVFMorph = m_mainSVFMorphSlew.Process();
            float combFreq = m_combFreqSlew.Process();
            float combFeedback = m_combFeedbackSlew.Process();
            float combSVFCutoff = m_combSVFCutoffSlew.Process();
            float combSVFResonance = m_combSVFResonanceSlew.Process();
            float combSVFMorph = m_combSVFMorphSlew.Process();

            // 1. Generate white noise
            //
            float noise = static_cast<float>(m_noise.Generate());

            // 2. Sample rate reduction
            // Adjust for oversample rate
            //
            m_sampleRateReducer.SetFreq(sampleRateReducerFreq / x_oversample);
            float reduced = m_sampleRateReducer.Process(noise);

            // 3. Main SVF processing
            // Adjust cutoff for oversample rate
            //
            float adjustedMainCutoff = std::min(0.499f / x_oversample, mainSVFCutoff / x_oversample);
            m_mainSVF.SetCutoff(adjustedMainCutoff);
            m_mainSVF.SetResonance(mainSVFResonance);
            m_mainSVF.Process(reduced);

            // Morph between HP -> BP -> LP
            //
            float svfOut;
            if (mainSVFMorph <= 0.5f)
            {
                float blend = mainSVFMorph * 2.0f;
                svfOut = m_mainSVF.GetHighPass() * (1.0f - blend) + m_mainSVF.GetBandPass() * blend;
            }
            else
            {
                float blend = (mainSVFMorph - 0.5f) * 2.0f;
                svfOut = m_mainSVF.GetBandPass() * (1.0f - blend) + m_mainSVF.GetLowPass() * blend;
            }

            // 4. AHD envelope amplitude modulation (processed at base rate)
            // We read the envelope value directly for smooth modulation
            // The envelope is inverted (like amp section): at 0 amplitude, output is at full
            // Apply exponential curve for natural modulation
            //
            float envValue = PhaseUtils::ZeroedExpParam::Compute(10.0f, m_ahd.m_output);
            float envelopedSignal = svfOut * envValue;

            // 5. Comb filter with SVF in feedback
            // Adjust frequencies for oversample rate
            //
            float adjustedCombFreq = combFreq / x_oversample;
            float adjustedCombSVFCutoff = std::min(0.499f / x_oversample, combSVFCutoff / x_oversample);

            m_combFilter.SetParams(
                adjustedCombFreq,
                combFeedback,
                adjustedCombSVFCutoff,
                combSVFResonance,
                combSVFMorph);

            m_output = m_combFilter.Process(envelopedSignal);
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

    // Process the AHD envelope at control rate
    //
    void ProcessAHDEnvelope(Input& input)
    {
        m_ahd.Process(input.m_ahdInput);
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
};
