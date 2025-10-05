#pragma once

#include "Filter.hpp"
#include <cmath>

struct LadderFilter
{
    static constexpr float x_minCutoff = 0.001f;
    static constexpr float x_maxCutoff = 0.499f;

    OPLowPassFilter m_stage1;
    OPLowPassFilter m_stage2;
    OPLowPassFilter m_stage3;
    OPLowPassFilter m_stage4;

    TanhSaturator<true> m_feedbackSaturator;
    TanhSaturator<true> m_outputSaturator;

    float m_cutoff;
    float m_feedback;

    LadderFilter()
        : m_cutoff(0.1f)
        , m_feedback(0.0f)
    {
        m_feedbackSaturator.SetInputGain(0.5f);
        m_outputSaturator.SetInputGain(0.5f);
    }

    float Process(float input)
    {
        float Cpi = std::pow(m_stage4.m_alpha / (2.0f - m_stage4.m_alpha), 4.0f);
        float kSafe = 0.9f / (Cpi + 1e-8f);
        float kEff  = std::min(m_feedback, kSafe);

        // Apply resonance feedback
        //
        float feedbackInput = input - kEff * m_stage4.m_output;

        feedbackInput = m_feedbackSaturator.Process(feedbackInput);

        // Process through the 4-pole ladder
        //
        float stage1Out = m_stage1.Process(feedbackInput);
        float stage2Out = m_stage2.Process(stage1Out);
        float stage3Out = m_stage3.Process(stage2Out);
        float stage4Out = m_stage4.Process(stage3Out);

        // Output gain compensation to counter resonance-induced passband drop
        // Maintains approximately unity DC gain as resonance increases
        //
        float compensatedOut = stage4Out * (1.0f + kEff);

        return m_outputSaturator.Process(compensatedOut);
    }

    void SetCutoff(float cutoff)
    {
        m_cutoff = (cutoff < x_minCutoff) ? x_minCutoff : (cutoff > x_maxCutoff) ? x_maxCutoff : cutoff;

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
        m_stage1.m_output = 0.0f;
        m_stage2.m_output = 0.0f;
        m_stage3.m_output = 0.0f;
        m_stage4.m_output = 0.0f;
    }

    static float FrequencyResponse(float alpha, float feedback, float freq)
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
        float denominator = realPart * realPart + imagPart * imagPart;
        
        // Complex H = (alpha * realPart + j * alpha * imagPart) / denominator
        // |H| = alpha / sqrt(denominator)
        //
        float singleStageMagnitude = alpha / std::sqrt(denominator);
        float singleStagePhase = std::atan2(imagPart, realPart);
        
        // Four cascaded stages: magnitude^4, phase * 4
        //
        float fourStageMagnitude = singleStageMagnitude * singleStageMagnitude * singleStageMagnitude * singleStageMagnitude;
        float fourStagePhase = singleStagePhase * 4.0f;
        
        // Apply feedback: H_total = H^4 / (1 + feedback * H^4)
        // This requires complex arithmetic
        //
        float feedbackReal = 1.0f + feedback * fourStageMagnitude * std::cos(fourStagePhase);
        float feedbackImag = feedback * fourStageMagnitude * std::sin(fourStagePhase);
        float feedbackDenominator = feedbackReal * feedbackReal + feedbackImag * feedbackImag;
        
        float totalMagnitude = fourStageMagnitude / std::sqrt(feedbackDenominator);
        
        // Apply gain compensation (1 + feedback) to maintain unity DC gain
        //
        return totalMagnitude * (1.0f + feedback);
    }
}; 