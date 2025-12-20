#pragma once

#include "Filter.hpp"
#include <cmath>
#include <complex>

struct LadderFilter
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

    LadderFilter()
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

        // Apply resonance feedback
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

    float GetHighPassOutput() const
    {
        float hpOut = m_input - 4 * m_stage1.m_output + 6 * m_stage2.m_output - 4 * m_stage3.m_output + m_stage4.m_output;
        return hpOut;
    }

    float ProcessHP(float input)
    {
        Process(input);
        return GetHighPassOutput();
    }

    void SetCutoff(float cutoff)
    {
        m_cutoff = std::min(x_maxCutoff, cutoff);

        // Set all stages to the same cutoff frequency
        //
        m_stage1.SetAlphaFromNatFreq(m_cutoff);
        m_stage2.m_alpha = m_stage1.m_alpha;
        m_stage3.m_alpha = m_stage2.m_alpha;
        m_stage4.m_alpha = m_stage3.m_alpha;
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
        
        // Apply feedback: H_total = H^4 / (1 + feedback * H^4)
        //
        std::complex<float> feedbackTerm(1.0f, 0.0f);
        feedbackTerm = feedbackTerm + feedback * fourStage;
        std::complex<float> totalTransfer = fourStage / feedbackTerm;
        
        // Apply gain compensation (1 + feedback) to maintain unity DC gain
        //
        return totalTransfer * (1.0f + feedback);
    }

    static std::complex<float> TransferFunctionHP(float alpha, float feedback, float freq)
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
        
        std::complex<float> oneMinusSingleStage = std::complex<float>(1.0f, 0.0f) - singleStage;
        std::complex<float> oneMinusSingleStageQuad = oneMinusSingleStage * oneMinusSingleStage * oneMinusSingleStage * oneMinusSingleStage;

        std::complex<float> fourStage = singleStage * singleStage * singleStage * singleStage;
        return oneMinusSingleStageQuad / (std::complex<float>(1.0f, 0.0f) + feedback * fourStage);
    }

    static float FrequencyResponse(float alpha, float feedback, float freq)
    {
        return std::abs(TransferFunction(alpha, feedback, freq));
    }

    static float FrequencyResponseHP(float alpha, float feedback, float freq)
    {
        return std::abs(TransferFunctionHP(alpha, feedback, freq));
    }
}; 