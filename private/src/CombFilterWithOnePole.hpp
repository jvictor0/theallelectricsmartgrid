#pragma once

#include "DelayLine.hpp"
#include "Filter.hpp"
#include "Math.hpp"
#include "PhaseUtils.hpp"
#include "Slew.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>

// Comb filter with a one-pole low-pass filter in its feedback loop.
// Compensates for the one-pole's group delay to keep the fundamental frequency in tune.
//
// Signal flow:
//   output = input + feedback * OnePole(delayLine.read(compensatedDelay))
//   delayLine.write(output)
//
// The one-pole's group delay at the fundamental frequency would detune the comb.
// We compute the group delay at the target frequency and adjust delay time to compensate.
//
// Usage:
//   Call SetParams() once per block, then call Process() for each sample.
//   SetParams() computes compensated delay; Process() slews toward it.
//
struct CombFilterWithOnePole
{
    static constexpr size_t x_maxDelaySamples = 1 << 14;
    static constexpr float x_minDampingTimeSeconds = 0.001f;
    static constexpr float x_maxDampingTimeSeconds = 5.0f;

    DelayLine<x_maxDelaySamples> m_delayLine;
    OPLowPassFilter m_damping;
    PhaseUtils::ExpParam m_dampingTime;

    // Internal slews for smooth parameter changes
    //
    ParamSlew m_feedbackSlew;
    ParamSlew m_compensatedDelaySlew;
    ParamSlew m_dampingAlphaSlew;

    float m_output;

    struct UIState : TransferFunction
    {
        std::atomic<float> m_compensatedDelay;
        std::atomic<float> m_feedback;
        std::atomic<float> m_alpha;

        UIState()
            : m_compensatedDelay(100.0f)
            , m_feedback(0.0f)
            , m_alpha(0.5f)
        {
        }

        std::complex<float> TransferFunctionValue(float normalizedFreq) const override
        {
            float freq = std::max(0.0f, std::min(0.5f, normalizedFreq));
            float omega = 2.0f * static_cast<float>(M_PI) * freq;
            float delaySamples = m_compensatedDelay.load();
            float delayPhase = -omega * delaySamples;
            std::complex<float> zDelay(Math::Cos(delayPhase), Math::Sin(delayPhase));
            std::complex<float> dampingTransfer = OPLowPassFilter::TransferFunction(m_alpha.load(), freq);
            float feedback = m_feedback.load();
            std::complex<float> denominator =
                std::complex<float>(1.0f, 0.0f) - feedback * dampingTransfer * zDelay;
            return std::complex<float>(1.0f, 0.0f) / denominator;
        }

        float FrequencyResponse(float normalizedFreq) const override
        {
            return std::abs(TransferFunctionValue(normalizedFreq));
        }
    };

    CombFilterWithOnePole()
        : m_dampingTime(x_minDampingTimeSeconds, x_maxDampingTimeSeconds)
        , m_output(0.0f)
    {
    }

    // Construct with oversample factor for proper slew timing
    //
    CombFilterWithOnePole(float relativeSampleRate)
        : m_dampingTime(x_minDampingTimeSeconds, x_maxDampingTimeSeconds)
        , m_feedbackSlew(relativeSampleRate)
        , m_compensatedDelaySlew(relativeSampleRate)
        , m_output(0.0f)
    {
    }

    // Set all parameters (call once per block, outside sample loop).
    // Computes compensated delay and stores target values for slewing.
    //
    // combFreq: normalized frequency (freq / sampleRate)
    // alpha: one-pole filter coefficient (0 < alpha <= 1)
    //
    void SetParams(float combFreq, float alpha)
    {
        combFreq = std::max(1e-6f, combFreq);
        if (m_feedbackSlew.m_target < 0.0f)
        {
            combFreq *= 2.0;
        }

        alpha = std::max(1e-6f, std::min(1.0f, alpha));

        // Set one-pole alpha directly
        //
        m_dampingAlphaSlew.Update(alpha);

        // Compute compensated delay
        // Base delay for comb at target frequency
        //
        float baseDelaySamples = 1.0f / combFreq;

        // Get one-pole phase delay at fundamental
        //
        float phaseDelaySamples = OPLowPassFilter::PhaseDelay(alpha, combFreq);

        // Subtract phase-delay compensation
        //
        float compensatedDelay = baseDelaySamples - phaseDelaySamples;

        // Clamp to valid range
        //
        compensatedDelay = std::max(1.0f, std::min(static_cast<float>(x_maxDelaySamples - 1), compensatedDelay));

        // Update slew target
        //
        m_compensatedDelaySlew.Update(compensatedDelay);
    }

    // Set damping time from a single [0, 1] knob.
    // Knob mapping:
    //   [0, 0.5): negative polarity
    //   (0.5, 1]: positive polarity
    // The absolute mapped value drives T60 exponentially from 1 ms to 5 s.
    //
    void SetDampingTime(float dampingTimeKnob, float sampleRate, float oversample)
    {
        float clampedKnob = std::max(0.0f, std::min(1.0f, dampingTimeKnob));
        float safeSampleRate = std::max(1.0f, sampleRate);
        float safeOversample = std::max(1.0f, oversample);

        float signedKnob = 0.0f;
        if (clampedKnob < 0.5f)
        {
            float doubled = clampedKnob * 2.0f;
            signedKnob = -(1.0f - doubled);
        }
        else
        {
            signedKnob = (clampedKnob - 0.5f) * 2.0f;
        }

        float dampingAmount = std::fabs(signedKnob);
        float t60 = m_dampingTime.Update(dampingAmount);

        float decayPerSample = std::pow(10.0f, -3.0f / (t60 * safeSampleRate * safeOversample));
        float blend = std::min(1.0f, std::max(0.0f, std::fabs(clampedKnob - 0.5f) * 50.0f));
        float feedback = 0.0f;
        if (signedKnob < 0.0f)
        {
            feedback = -decayPerSample * blend;
        }
        else if (signedKnob > 0.0f)
        {
            feedback = decayPerSample * blend;
        }

        m_feedbackSlew.Update(feedback);
    }

    float Process(float input)
    {
        // Process slews
        //
        float feedback = m_feedbackSlew.Process();
        float delaySamples = m_compensatedDelaySlew.Process();
        m_damping.m_alpha = m_dampingAlphaSlew.Process();

        // Read from delay line using XFader for smooth crossfade
        //
        float delayed = m_delayLine.Read(delaySamples);

        // Process through one-pole damping filter
        //
        float dampedOut = m_damping.Process(delayed);

        m_output = input + feedback * dampedOut;

        // Write to delay line
        //
        m_delayLine.Write(m_output);

        return m_output;
    }

    void Reset()
    {
        m_damping.m_output = 0.0f;
        m_output = 0.0f;

        for (size_t i = 0; i < x_maxDelaySamples; ++i)
        {
            m_delayLine.m_delayLine[i] = 0.0f;
        }

        m_delayLine.m_index = 0;

        // Reset slews to their current target values
        //
        m_feedbackSlew.m_filter.m_output = m_feedbackSlew.m_target;
        m_compensatedDelaySlew.m_filter.m_output = m_compensatedDelaySlew.m_target;
    }

    void PopulateUIState(UIState* uiState) const
    {
        uiState->m_compensatedDelay.store(m_compensatedDelaySlew.m_filter.m_output);
        uiState->m_feedback.store(m_feedbackSlew.m_filter.m_output);
        uiState->m_alpha.store(m_damping.m_alpha);
    }
};
