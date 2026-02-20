#pragma once

#include "Filter.hpp"
#include "DebugMeter.hpp"
#include <atomic>
#include <cmath>
#include <complex>

struct LadderFilterLP
{
    static constexpr float x_maxCutoff = 0.499f;

    struct UIState
    {
        std::atomic<float> m_alpha;
        std::atomic<float> m_feedback;

        UIState()
            : m_alpha(0.0f)
            , m_feedback(0.0f)
        {
        }

        float FrequencyResponse(float freq)
        {
            return LadderFilterLP::FrequencyResponse(m_alpha.load(), m_feedback.load(), freq);
        }

        std::complex<float> TransferFunction(float freq)
        {
            return LadderFilterLP::TransferFunction(m_alpha.load(), m_feedback.load(), freq);
        }
    };

    OPLowPassFilter m_stage1;
    OPLowPassFilter m_stage2;
    OPLowPassFilter m_stage3;
    OPLowPassFilter m_stage4;

    TanhSaturator<true> m_saturator;
    TanhSaturator<false> m_fbSaturator;

    float m_cutoff;
    float m_feedback;

    float m_input;
    float m_output;
    float m_kEff;

    // Debug meters: input, feedback input, output
    //
    static constexpr bool x_debugMetersOn = false;
    DebugMeter<x_debugMetersOn> m_meterInput;
    DebugMeter<x_debugMetersOn> m_meterFbInput;
    DebugMeter<x_debugMetersOn> m_meterOutput;

    // Debug meters: before and after saturation for each stage
    //
    DebugMeter<x_debugMetersOn> m_meterStage1Pre;
    DebugMeter<x_debugMetersOn> m_meterStage1Post;
    DebugMeter<x_debugMetersOn> m_meterStage2Pre;
    DebugMeter<x_debugMetersOn> m_meterStage2Post;
    DebugMeter<x_debugMetersOn> m_meterStage3Pre;
    DebugMeter<x_debugMetersOn> m_meterStage3Post;
    DebugMeter<x_debugMetersOn> m_meterStage4Pre;
    DebugMeter<x_debugMetersOn> m_meterStage4Post;

    LadderFilterLP()
        : m_cutoff(0.1f)
        , m_feedback(0.0f)
        , m_input(0.0f)
        , m_output(0.0f)
        , m_kEff(0.0f)
    {
        m_saturator.SetInputGain(0.5f);
        m_fbSaturator.SetInputGain(1.0f);
    }

    void PopulateUIState(UIState* uiState)
    {
        uiState->m_alpha.store(m_stage4.m_alpha);
        uiState->m_feedback.store(m_kEff);
    }

    float Process(float input)
    {
        m_meterInput.Process(input);

        // Calculate effective Nyquist gain including saturation small-signal gain
        //
        float smallSignalGain = m_saturator.DerivativeZero();
        float perStageGain = (m_stage4.m_alpha / (2.0f - m_stage4.m_alpha)) * smallSignalGain;
        float Cpi = std::pow(perStageGain, 4.0f);
        float kSafe = 0.9f / (Cpi + 1e-8f);
        m_kEff = std::min(m_feedback, kSafe);

        // Apply resonance feedback from LP output
        //
        float feedbackInput = input - (m_kEff * m_stage4.m_output);
        m_input = feedbackInput;
        m_meterFbInput.Process(feedbackInput);

        // Process through the 4-pole ladder with saturation after each stage
        //
        float stage1Pre = m_stage1.Process(feedbackInput);
        m_meterStage1Pre.Process(stage1Pre);
        float stage1Out = m_saturator.Process(stage1Pre);
        m_meterStage1Post.Process(stage1Out);

        float stage2Pre = m_stage2.Process(stage1Out);
        m_meterStage2Pre.Process(stage2Pre);
        float stage2Out = m_saturator.Process(stage2Pre);
        m_meterStage2Post.Process(stage2Out);

        float stage3Pre = m_stage3.Process(stage2Out);
        m_meterStage3Pre.Process(stage3Pre);
        float stage3Out = m_saturator.Process(stage3Pre);
        m_meterStage3Post.Process(stage3Out);

        float stage4Pre = m_stage4.Process(stage3Out);
        m_meterStage4Pre.Process(stage4Pre);
        float stage4Out = m_saturator.Process(stage4Pre);
        m_meterStage4Post.Process(stage4Out);

        // Output gain compensation to counter resonance-induced passband drop
        // Maintains approximately unity DC gain as resonance increases
        //
        m_output = stage4Out * (1.0f + m_kEff);
        m_meterOutput.Process(m_output);
        return m_output;
    }

    void SetSaturationGain(float gain)
    {
        m_saturator.SetInputGain(gain);
    }

    void SetCutoff(float cutoff)
    {
        m_cutoff = std::min(x_maxCutoff, cutoff);
        float c = Math::Cos2pi(m_cutoff);

        // Solve for the -3dB point: |H|² = 2^(-1/4) per stage
        // Equation: (1-k)α² + 2k(1-c)α - 2k(1-c) = 0
        //
        static constexpr float k = 0.84089641525f;
        float oneMinusC = 1.0f - c;
        float kOneMinusC = k * oneMinusC;
        float discriminant = kOneMinusC * (kOneMinusC + 2.0f * (1.0f - k));

        float alpha = (-kOneMinusC + std::sqrt(discriminant)) / (1.0f - k);
        SetAlphaDirect(alpha);
    }

    void SetAlphaDirect(float alpha)
    {
        m_stage1.m_alpha = alpha;
        m_stage2.m_alpha = alpha;
        m_stage3.m_alpha = alpha;
        m_stage4.m_alpha = alpha;
    }

    void SetResonance(float resonance)
    {
        // Calculate feedback amount based on resonance
        // Higher resonance = more feedback = more pronounced filter character
        //
        m_feedback = resonance * 4.0f;
    }

    void Reset()
    {
        m_input = 0.0f;
        m_stage1.m_output = 0.0f;
        m_stage2.m_output = 0.0f;
        m_stage3.m_output = 0.0f;
        m_stage4.m_output = 0.0f;

        m_meterInput.Reset();
        m_meterFbInput.Reset();
        m_meterOutput.Reset();
        m_meterStage1Pre.Reset();
        m_meterStage1Post.Reset();
        m_meterStage2Pre.Reset();
        m_meterStage2Post.Reset();
        m_meterStage3Pre.Reset();
        m_meterStage3Post.Reset();
        m_meterStage4Pre.Reset();
        m_meterStage4Post.Reset();
    }

    void DebugPrint()
    {
        if (!x_debugMetersOn)
        {
            return;
        }

        float smallSignalGain = m_saturator.DerivativeZero();
        float satInputGain = m_saturator.m_inputGain;
        float perStageGain = (m_stage4.m_alpha / (2.0f - m_stage4.m_alpha)) * smallSignalGain;
        float Cpi = std::pow(perStageGain, 4.0f);

        printf("=== LadderFilterLP Debug ===\n");
        printf("Cutoff: %.6f  Alpha: %.6f\n", m_cutoff, m_stage1.m_alpha);
        printf("Feedback: %.6f  kEff: %.6f\n", m_feedback, m_kEff);
        printf("Sat InputGain: %.6f  SmallSigGain: %.6f\n", satInputGain, smallSignalGain);
        printf("PerStageGain: %.6f  Cpi: %.6f\n", perStageGain, Cpi);
        printf("\n");

        m_meterInput.Print("Input          ");
        m_meterFbInput.Print("FB Input       ");
        m_meterOutput.Print("Output         ");
        printf("\n");

        m_meterStage1Pre.Print("Stage1 Pre-Sat ");
        m_meterStage1Post.Print("Stage1 Post-Sat");
        m_meterStage2Pre.Print("Stage2 Pre-Sat ");
        m_meterStage2Post.Print("Stage2 Post-Sat");
        m_meterStage3Pre.Print("Stage3 Pre-Sat ");
        m_meterStage3Post.Print("Stage3 Post-Sat");
        m_meterStage4Pre.Print("Stage4 Pre-Sat ");
        m_meterStage4Post.Print("Stage4 Post-Sat");
        printf("============================\n");
    }

    static std::complex<float> TransferFunction(float alpha, float feedback, float freq)
    {
        // Calculate normalized frequency (omega)
        //
        float omega = 2.0f * M_PI * freq;
        float cosOmega = std::cos(omega);
        float sinOmega = std::sin(omega);
        
        // Single stage complex frequency response for OPLowPassFilter
        // H(z) = alpha / (1 - (1-alpha)z^-1)
        // H(e^jω) = alpha / (1 - (1-alpha)e^-jω)
        //         = alpha / (1 - (1-alpha)(cos(ω) - j*sin(ω)))
        //         = alpha / ((1 - (1-alpha)cos(ω)) + j*(1-alpha)sin(ω))
        //
        float realPart = 1.0f - (1.0f - alpha) * cosOmega;
        float imagPart = (1.0f - alpha) * sinOmega;
        
        std::complex<float> singleStage(alpha, 0.0f);
        std::complex<float> singleStageDen(realPart, imagPart);
        singleStage = singleStage / singleStageDen;
        
        // Four cascaded stages: H^4
        //
        std::complex<float> fourStage = singleStage * singleStage * singleStage * singleStage;
        
        // Apply feedback with one-sample delay: z^(-1) = e^(-jω)
        // H_total = H^4 / (1 + feedback * z^(-1) * H^4)
        //
        std::complex<float> zInv(cosOmega, -sinOmega);
        std::complex<float> feedbackTerm(1.0f, 0.0f);
        feedbackTerm = feedbackTerm + feedback * zInv * fourStage;
        std::complex<float> totalTransfer = fourStage / feedbackTerm;
        
        // Apply gain compensation (1 + feedback) to maintain unity DC gain
        //
        return totalTransfer * (1.0f + feedback);
    }

    static float FrequencyResponse(float alpha, float feedback, float freq)
    {
        return std::abs(TransferFunction(alpha, feedback, freq));
    }
};

// TPT (Topology Preserving Transform) one-pole integrator for ZDF filters
// Uses g = tan(π * cutoff) for proper frequency response
//
struct TPTOnePole
{
    float m_g;
    float m_G;
    float m_state;

    TPTOnePole()
        : m_g(0.0f)
        , m_G(0.0f)
        , m_state(0.0f)
    {
    }

    void SetCutoff(float cutoff)
    {
        m_g = std::tan(static_cast<float>(M_PI) * cutoff);
        m_G = m_g / (1.0f + m_g);
    }

    // Process with immediate state update
    //
    float Process(float input)
    {
        float v = m_G * (input - m_state);
        float y = v + m_state;
        m_state = y + v;
        return y;
    }

    // Get output without updating state (for ZDF solving)
    //
    float GetOutput(float input) const
    {
        return m_G * input + (1.0f - m_G) * m_state;
    }

    // Update state after ZDF solution
    //
    void UpdateState(float input)
    {
        float v = m_G * (input - m_state);
        float y = v + m_state;
        m_state = y + v;
    }
};

// Zero Delay Feedback Ladder Filter (4-pole lowpass)
// Solves the feedback equation implicitly for zero-delay response
//
struct LadderFilterLPZDF
{
    static constexpr float x_maxCutoff = 0.499f;

    struct UIState
    {
        std::atomic<float> m_alpha;
        std::atomic<float> m_feedback;

        UIState()
            : m_alpha(0.0f)
            , m_feedback(0.0f)
        {
        }

        float FrequencyResponse(float freq)
        {
            return LadderFilterLPZDF::FrequencyResponse(m_alpha.load(), m_feedback.load(), freq);
        }

        std::complex<float> TransferFunction(float freq)
        {
            return LadderFilterLPZDF::TransferFunction(m_alpha.load(), m_feedback.load(), freq);
        }
    };

    TPTOnePole m_stage1;
    TPTOnePole m_stage2;
    TPTOnePole m_stage3;
    TPTOnePole m_stage4;

    TanhSaturator<true> m_saturator;

    float m_cutoff;
    float m_feedback;
    float m_kEff;

    float m_input;
    float m_output;

    // Cached coefficients for feedback solving
    //
    float m_G;
    float m_G2;
    float m_G3;
    float m_G4;
    float m_oneMinusG;

    // Debug meters
    //
    static constexpr bool x_debugMetersOn = true;
    DebugMeter<x_debugMetersOn> m_meterInput;
    DebugMeter<x_debugMetersOn> m_meterFbInput;
    DebugMeter<x_debugMetersOn> m_meterOutput;
    DebugMeter<x_debugMetersOn> m_meterStage1Pre;
    DebugMeter<x_debugMetersOn> m_meterStage1Post;
    DebugMeter<x_debugMetersOn> m_meterStage2Pre;
    DebugMeter<x_debugMetersOn> m_meterStage2Post;
    DebugMeter<x_debugMetersOn> m_meterStage3Pre;
    DebugMeter<x_debugMetersOn> m_meterStage3Post;
    DebugMeter<x_debugMetersOn> m_meterStage4Pre;
    DebugMeter<x_debugMetersOn> m_meterStage4Post;

    LadderFilterLPZDF()
        : m_cutoff(0.1f)
        , m_feedback(0.0f)
        , m_kEff(0.0f)
        , m_input(0.0f)
        , m_output(0.0f)
        , m_G(0.0f)
        , m_G2(0.0f)
        , m_G3(0.0f)
        , m_G4(0.0f)
        , m_oneMinusG(1.0f)
    {
        m_saturator.SetInputGain(0.5f);
        SetCutoff(0.1f);
    }

    void PopulateUIState(UIState* uiState)
    {
        uiState->m_alpha.store(m_G);
        uiState->m_feedback.store(m_kEff);
    }

    float Process(float input)
    {
        m_meterInput.Process(input);
        m_input = input;

        // Calculate effective feedback with saturation compensation
        //
        float smallSignalGain = m_saturator.DerivativeZero();
        float effectiveG4 = m_G4 * std::pow(smallSignalGain, 4.0f);
        float kSafe = 0.95f / (effectiveG4 + 1e-8f);
        m_kEff = std::min(m_feedback, kSafe);

        // Compute state contribution: S = (1-G)*(G³s1 + G²s2 + Gs3 + s4)
        //
        float S = m_oneMinusG * (
            m_G3 * m_stage1.m_state +
            m_G2 * m_stage2.m_state +
            m_G  * m_stage3.m_state +
                   m_stage4.m_state
        );

        // Solve for y4: y4 = (G⁴*input + S) / (1 + k*G⁴)
        // This is the ZDF magic - we compute output without delay
        //
        float y4Linear = (m_G4 * input + S) / (1.0f + m_kEff * m_G4);

        // Compute feedback input
        //
        float fbInput = input - m_kEff * y4Linear;
        m_meterFbInput.Process(fbInput);

        // Now process each stage with saturation and update states
        //
        float stage1Pre = m_stage1.GetOutput(fbInput);
        m_meterStage1Pre.Process(stage1Pre);
        float stage1Out = m_saturator.Process(stage1Pre);
        m_meterStage1Post.Process(stage1Out);
        m_stage1.UpdateState(fbInput);

        float stage2Pre = m_stage2.GetOutput(stage1Out);
        m_meterStage2Pre.Process(stage2Pre);
        float stage2Out = m_saturator.Process(stage2Pre);
        m_meterStage2Post.Process(stage2Out);
        m_stage2.UpdateState(stage1Out);

        float stage3Pre = m_stage3.GetOutput(stage2Out);
        m_meterStage3Pre.Process(stage3Pre);
        float stage3Out = m_saturator.Process(stage3Pre);
        m_meterStage3Post.Process(stage3Out);
        m_stage3.UpdateState(stage2Out);

        float stage4Pre = m_stage4.GetOutput(stage3Out);
        m_meterStage4Pre.Process(stage4Pre);
        float stage4Out = m_saturator.Process(stage4Pre);
        m_meterStage4Post.Process(stage4Out);
        m_stage4.UpdateState(stage3Out);

        // Output with gain compensation
        //
        m_output = stage4Out * (1.0f + m_kEff);
        m_meterOutput.Process(m_output);
        return m_output;
    }

    void SetSaturationGain(float gain)
    {
        m_saturator.SetInputGain(gain);
    }

    void SetCutoff(float cutoff)
    {
        m_cutoff = std::min(x_maxCutoff, cutoff);

        m_stage1.SetCutoff(m_cutoff);
        m_stage2.SetCutoff(m_cutoff);
        m_stage3.SetCutoff(m_cutoff);
        m_stage4.SetCutoff(m_cutoff);

        // Cache powers of G for feedback solving
        //
        m_G = m_stage1.m_G;
        m_G2 = m_G * m_G;
        m_G3 = m_G2 * m_G;
        m_G4 = m_G3 * m_G;
        m_oneMinusG = 1.0f - m_G;
    }

    void SetResonance(float resonance)
    {
        m_feedback = resonance * 4.0f;
    }

    void Reset()
    {
        m_input = 0.0f;
        m_output = 0.0f;
        m_stage1.m_state = 0.0f;
        m_stage2.m_state = 0.0f;
        m_stage3.m_state = 0.0f;
        m_stage4.m_state = 0.0f;

        m_meterInput.Reset();
        m_meterFbInput.Reset();
        m_meterOutput.Reset();
        m_meterStage1Pre.Reset();
        m_meterStage1Post.Reset();
        m_meterStage2Pre.Reset();
        m_meterStage2Post.Reset();
        m_meterStage3Pre.Reset();
        m_meterStage3Post.Reset();
        m_meterStage4Pre.Reset();
        m_meterStage4Post.Reset();
    }

    void DebugPrint()
    {
        float smallSignalGain = m_saturator.DerivativeZero();
        float satInputGain = m_saturator.m_inputGain;

        printf("=== LadderFilterLPZDF Debug ===\n");
        printf("Cutoff: %.6f  g: %.6f  G: %.6f\n", m_cutoff, m_stage1.m_g, m_G);
        printf("Feedback: %.6f  kEff: %.6f\n", m_feedback, m_kEff);
        printf("Sat InputGain: %.6f  SmallSigGain: %.6f\n", satInputGain, smallSignalGain);
        printf("G^4: %.6f  1+k*G^4: %.6f\n", m_G4, 1.0f + m_kEff * m_G4);
        printf("\n");

        m_meterInput.Print("Input          ");
        m_meterFbInput.Print("FB Input       ");
        m_meterOutput.Print("Output         ");
        printf("\n");

        m_meterStage1Pre.Print("Stage1 Pre-Sat ");
        m_meterStage1Post.Print("Stage1 Post-Sat");
        m_meterStage2Pre.Print("Stage2 Pre-Sat ");
        m_meterStage2Post.Print("Stage2 Post-Sat");
        m_meterStage3Pre.Print("Stage3 Pre-Sat ");
        m_meterStage3Post.Print("Stage3 Post-Sat");
        m_meterStage4Pre.Print("Stage4 Pre-Sat ");
        m_meterStage4Post.Print("Stage4 Post-Sat");
        printf("================================\n");
    }

    // ZDF transfer function uses G instead of alpha
    // G = g/(1+g) where g = tan(π*cutoff)
    //
    static std::complex<float> TransferFunction(float G, float feedback, float freq)
    {
        float omega = 2.0f * static_cast<float>(M_PI) * freq;
        float cosOmega = std::cos(omega);
        float sinOmega = std::sin(omega);

        // TPT one-pole transfer function:
        // H(z) = G * (1 + z^-1) / (1 + (2G-1)*z^-1)
        // This is the bilinear-transformed integrator
        //
        std::complex<float> zInv(cosOmega, -sinOmega);
        std::complex<float> num = G * (1.0f + zInv);
        std::complex<float> den = 1.0f + (2.0f * G - 1.0f) * zInv;
        std::complex<float> singleStage = num / den;

        // Four cascaded stages
        //
        std::complex<float> fourStage = singleStage * singleStage * singleStage * singleStage;

        // ZDF feedback (no delay in feedback path)
        // H_total = H^4 / (1 + feedback * H^4)
        //
        std::complex<float> feedbackTerm = 1.0f + feedback * fourStage;
        std::complex<float> totalTransfer = fourStage / feedbackTerm;

        // Gain compensation
        //
        return totalTransfer * (1.0f + feedback);
    }

    static float FrequencyResponse(float G, float feedback, float freq)
    {
        return std::abs(TransferFunction(G, feedback, freq));
    }
};

struct LadderFilterHP
{
    static constexpr float x_maxCutoff = 0.35f;

    struct UIState
    {
        std::atomic<float> m_alpha;
        std::atomic<float> m_feedback;

        UIState()
            : m_alpha(0.0f)
            , m_feedback(0.0f)
        {
        }

        float FrequencyResponse(float freq)
        {
            return LadderFilterHP::FrequencyResponse(m_alpha.load(), m_feedback.load(), freq);
        }

        std::complex<float> TransferFunction(float freq)
        {
            return LadderFilterHP::TransferFunction(m_alpha.load(), m_feedback.load(), freq);
        }
    };

    OPLowPassFilter m_stage1;
    OPLowPassFilter m_stage2;
    OPLowPassFilter m_stage3;
    OPLowPassFilter m_stage4;

    TanhSaturator<true> m_saturator;

    float m_cutoff;
    float m_feedback;

    float m_input;
    float m_output;
    float m_hpOutput;
    float m_kEff;

    LadderFilterHP()
        : m_cutoff(0.1f)
        , m_feedback(0.0f)
        , m_input(0.0f)
        , m_output(0.0f)
        , m_hpOutput(0.0f)
        , m_kEff(0.0f)
    {
        m_saturator.SetInputGain(0.5f);
    }

    void PopulateUIState(UIState* uiState)
    {
        uiState->m_alpha.store(m_stage4.m_alpha);
        uiState->m_feedback.store(m_kEff);
    }

    float GetCompensation() const
    {
        float alpha = m_stage1.m_alpha;
        float oneMinusAlpha = 1.0f - alpha;
        float twoMinusAlpha = 2.0f - alpha;
        float singleStageGain = 2.0f * oneMinusAlpha / twoMinusAlpha;
        return 1.0f / (singleStageGain * singleStageGain * singleStageGain * singleStageGain);
    }

    float GetRawHighPassOutput() const
    {
        return m_input - 4 * m_stage1.m_output + 6 * m_stage2.m_output - 4 * m_stage3.m_output + m_stage4.m_output;
    }

    float Process(float input)
    {
        float compensation = GetCompensation();

        // Calculate effective DC gain including saturation small-signal gain
        //
        float smallSignalGain = m_saturator.DerivativeZero();
        float perStageGain = (m_stage4.m_alpha / (2.0f - m_stage4.m_alpha)) * smallSignalGain;
        float Cpi = std::pow(perStageGain, 4.0f);
        float kSafe = 0.9f / (Cpi + 1e-8f);
        m_kEff = std::min(m_feedback, kSafe);

        // Apply resonance feedback from LP output (same structure as LP filter)
        // This puts resonance at the LP cutoff frequency for this alpha
        //
        float feedbackInput = input - m_kEff * m_stage4.m_output;
        m_input = feedbackInput;

        // Process through the 4-pole ladder with saturation after each stage
        //
        float stage1Out = m_saturator.Process(m_stage1.Process(feedbackInput));
        float stage2Out = m_saturator.Process(m_stage2.Process(stage1Out));
        float stage3Out = m_saturator.Process(m_stage3.Process(stage2Out));
        m_saturator.Process(m_stage4.Process(stage3Out));

        // Compute HP output: (1-H)^4 using binomial coefficients
        //
        float hpRaw = GetRawHighPassOutput();

        // Apply Nyquist normalization
        //
        m_hpOutput = hpRaw * compensation;

        // No gain compensation needed for HP with LP-based feedback
        // (HP passband at Nyquist is not attenuated by LP feedback)
        //
        m_output = m_hpOutput;
        return m_output;
    }

    void SetSaturationGain(float gain)
    {
        m_saturator.SetInputGain(gain);
    }

    void SetCutoff(float cutoff)
    {
        m_cutoff = std::min(x_maxCutoff, cutoff);
        
        float c = Math::Cos2pi(m_cutoff);

        // Solve for the -3dB point of the gain-compensated HP response
        // Equation: Aβ² + Bβ + A = 0, where β = 1 - α
        // k = 2^(3/4) is the per-stage magnitude target for -3dB at 4 poles
        //
        static constexpr float k = 1.68179283051f;
        float A = (1.0f - c) - k;
        float B = 2.0f * (1.0f - c) + 2.0f * k * c;

        float discriminant = B * B - 4.0f * A * A;
        float sqrtD = std::sqrt(std::max(0.0f, discriminant));

        // Roots are reciprocals; choose the one in (0, 1)
        // When A < 0 (typical), use (+) branch
        //
        float beta = (-B + sqrtD) / (2.0f * A);
        float alpha = 1.0f - beta;

        alpha = std::clamp(alpha, 0.001f, 0.999f);
        SetAlphaDirect(alpha);
    }

    void SetAlphaDirect(float alpha)
    {
        m_stage1.m_alpha = alpha;
        m_stage2.m_alpha = alpha;
        m_stage3.m_alpha = alpha;
        m_stage4.m_alpha = alpha;
    }

    void SetResonance(float resonance)
    {
        // Calculate feedback amount based on resonance
        // Higher resonance = more feedback = more pronounced filter character
        //
        m_feedback = resonance * 4.0f;
    }

    void Reset()
    {
        m_input = 0.0f;
        m_hpOutput = 0.0f;
        m_stage1.m_output = 0.0f;
        m_stage2.m_output = 0.0f;
        m_stage3.m_output = 0.0f;
        m_stage4.m_output = 0.0f;
    }

    static std::complex<float> TransferFunction(float alpha, float feedback, float freq)
    {
        // Calculate normalized frequency (omega)
        //
        float omega = 2.0f * M_PI * freq;
        float cosOmega = std::cos(omega);
        float sinOmega = std::sin(omega);
        
        // Single stage complex frequency response for OPLowPassFilter
        // H(z) = alpha / (1 - (1-alpha)z^-1)
        //
        float realPart = 1.0f - (1.0f - alpha) * cosOmega;
        float imagPart = (1.0f - alpha) * sinOmega;
        
        std::complex<float> singleStage(alpha, 0.0f);
        std::complex<float> singleStageDen(realPart, imagPart);
        singleStage = singleStage / singleStageDen;
        
        // HP single stage: (1 - H)
        //
        std::complex<float> oneMinusSingleStage = std::complex<float>(1.0f, 0.0f) - singleStage;
        std::complex<float> hpFourStage = oneMinusSingleStage * oneMinusSingleStage * oneMinusSingleStage * oneMinusSingleStage;

        // Normalize for unity gain at Nyquist: multiply by [(2-α)/(2(1-α))]^4
        //
        float oneMinusAlpha = 1.0f - alpha;
        float twoMinusAlpha = 2.0f - alpha;
        float singleStageGain = 2.0f * oneMinusAlpha / twoMinusAlpha;
        float compensation = 1.0f / (singleStageGain * singleStageGain * singleStageGain * singleStageGain);

        std::complex<float> hpNormalized = hpFourStage * compensation;

        // LP four-stage response for feedback
        //
        std::complex<float> lpFourStage = singleStage * singleStage * singleStage * singleStage;

        // Apply LP-based feedback with one-sample delay: z^(-1) = e^(-jω)
        // HP_total = HP_norm / (1 + k * z^(-1) * LP)
        // No gain compensation needed (HP passband not affected by LP feedback)
        //
        std::complex<float> zInv(cosOmega, -sinOmega);
        std::complex<float> feedbackTerm = std::complex<float>(1.0f, 0.0f) + feedback * zInv * lpFourStage;
        std::complex<float> totalTransfer = hpNormalized / feedbackTerm;
        
        return totalTransfer;
    }

    static float FrequencyResponse(float alpha, float feedback, float freq)
    {
        return std::abs(TransferFunction(alpha, feedback, freq));
    }
};

