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
        // Apply resonance feedback
        //
        float feedbackInput = input - m_feedback * m_stage4.m_output;

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
        float compensatedOut = stage4Out * (1.0f + m_feedback);

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
}; 