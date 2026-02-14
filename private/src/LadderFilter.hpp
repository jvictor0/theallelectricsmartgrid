#pragma once

#include "Filter.hpp"
#include <cmath>
#include <complex>

struct LadderFilterLP
{
    static constexpr float x_maxCutoff = 0.499f;

    OPLowPassFilter m_stage1;
    OPLowPassFilter m_stage2;
    OPLowPassFilter m_stage3;
    OPLowPassFilter m_stage4;

    TanhSaturator<true> m_feedbackSaturator;

    float m_cutoff;
    float m_feedback;

    float m_input;
    float m_output;
    float m_kEff;

    LadderFilterLP()
        : m_cutoff(0.1f)
        , m_feedback(0.0f)
        , m_output(0.0f)
    {
        m_feedbackSaturator.SetInputGain(0.5f);
    }

    float Process(float input)
    {
        float Cpi = std::pow(m_stage4.m_alpha / (2.0f - m_stage4.m_alpha), 4.0f);
        float kSafe = 0.9f / (Cpi + 1e-8f);
        m_kEff = std::min(m_feedback, kSafe);

        // Apply resonance feedback from LP output
        //
        float feedbackInput = input - m_kEff * m_stage4.m_output;

        feedbackInput = m_feedbackSaturator.Process(feedbackInput);
        m_input = feedbackInput;

        // Process through the 4-pole ladder
        //
        float stage1Out = m_stage1.Process(feedbackInput);
        float stage2Out = m_stage2.Process(stage1Out);
        float stage3Out = m_stage3.Process(stage2Out);
        float stage4Out = m_stage4.Process(stage3Out);

        // Output gain compensation to counter resonance-induced passband drop
        // Maintains approximately unity DC gain as resonance increases
        //
        m_output = stage4Out * (1.0f + m_kEff);
        return m_output;
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

struct LadderFilterHP
{
    static constexpr float x_maxCutoff = 0.35f;

    OPLowPassFilter m_stage1;
    OPLowPassFilter m_stage2;
    OPLowPassFilter m_stage3;
    OPLowPassFilter m_stage4;

    TanhSaturator<true> m_feedbackSaturator;

    float m_cutoff;
    float m_feedback;

    float m_input;
    float m_output;
    float m_hpOutput;
    float m_kEff;

    LadderFilterHP()
        : m_cutoff(0.1f)
        , m_feedback(0.0f)
        , m_output(0.0f)
        , m_hpOutput(0.0f)
    {
        m_feedbackSaturator.SetInputGain(0.5f);
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

        // Calculate safe feedback limit based on LP gain at Nyquist (same as LP filter)
        //
        float Cpi = std::pow(m_stage4.m_alpha / (2.0f - m_stage4.m_alpha), 4.0f);
        float kSafe = 0.9f / (Cpi + 1e-8f);
        m_kEff = std::min(m_feedback, kSafe);

        // Apply resonance feedback from LP output (same structure as LP filter)
        // This puts resonance at the LP cutoff frequency for this alpha
        //
        float feedbackInput = input - m_kEff * m_stage4.m_output;

        feedbackInput = m_feedbackSaturator.Process(feedbackInput);
        m_input = feedbackInput;

        // Process through the 4-pole ladder
        //
        float stage1Out = m_stage1.Process(feedbackInput);
        float stage2Out = m_stage2.Process(stage1Out);
        float stage3Out = m_stage3.Process(stage2Out);
        m_stage4.Process(stage3Out);

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

