#pragma once

#include "DelayLine.hpp"
#include "StateVariableFilter.hpp"
#include "Math.hpp"
#include <cmath>
#include <complex>

// Comb filter with a state variable filter in its feedback loop.
// Compensates for the SVF's phase response to keep the fundamental frequency in tune.
//
// Signal flow:
//   output = input + feedback * SVF(delayLine.read(compensatedDelay))
//   delayLine.write(output)
//
// The SVF's phase shift at the fundamental frequency would detune the comb.
// We compute the phase at the target frequency and adjust delay time to compensate.
//
struct CombFilterWithSVF
{
    static constexpr size_t x_maxDelaySamples = 1 << 14;
    static constexpr float x_maxCutoff = 0.499f;

    DelayLine<x_maxDelaySamples> m_delayLine;
    LinearStateVariableFilter m_svf;

    float m_combFreq;
    float m_feedback;
    float m_morph;

    float m_output;

    CombFilterWithSVF()
        : m_combFreq(0.01f)
        , m_feedback(0.5f)
        , m_morph(0.5f)
        , m_output(0.0f)
    {
    }

    // Set all parameters at once.
    // combFreq: normalized frequency (freq / sampleRate)
    // feedback: feedback amount [0, 1]
    // svfCutoff: SVF normalized cutoff
    // svfResonance: SVF resonance [0, 1]
    // morph: 0 = highpass, 0.5 = bandpass, 1 = lowpass
    //
    void SetParams(float combFreq, float feedback, float svfCutoff, float svfResonance, float morph)
    {
        m_combFreq = combFreq;
        m_feedback = feedback;
        m_morph = morph;

        m_svf.SetCutoff(std::min(x_maxCutoff, svfCutoff));
        m_svf.SetResonance(svfResonance);
    }

    // Compute phase response of SVF at given normalized frequency.
    // Returns phase in radians.
    //
    float ComputeSVFPhase(float freq) const
    {
        float omega = 2.0f * static_cast<float>(M_PI) * freq;
        std::complex<float> z(std::cos(omega), std::sin(omega));
        std::complex<float> zInv = std::complex<float>(1.0f, 0.0f) / z;

        float g = m_svf.m_g;
        float k = m_svf.m_k;

        std::complex<float> s = (std::complex<float>(1.0f, 0.0f) - zInv) / 
                                 (std::complex<float>(1.0f, 0.0f) + zInv);
        s = s / g;

        std::complex<float> den = s * s + k * s + std::complex<float>(1.0f, 0.0f);

        // Morph between HP, BP, LP
        // morph 0 = HP, morph 0.5 = BP, morph 1 = LP
        //
        std::complex<float> H;
        if (m_morph <= 0.5f)
        {
            // HP to BP blend
            //
            float blend = m_morph * 2.0f;
            std::complex<float> hpNum = s * s;
            std::complex<float> bpNum = s;
            std::complex<float> num = hpNum * (1.0f - blend) + bpNum * blend;
            H = num / den;
        }
        else
        {
            // BP to LP blend
            //
            float blend = (m_morph - 0.5f) * 2.0f;
            std::complex<float> bpNum = s;
            std::complex<float> lpNum = std::complex<float>(1.0f, 0.0f);
            std::complex<float> num = bpNum * (1.0f - blend) + lpNum * blend;
            H = num / den;
        }

        return std::arg(H);
    }

    // Compute the delay in samples compensated for SVF phase shift.
    //
    float ComputeCompensatedDelay() const
    {
        // Base delay for comb at target frequency
        //
        float basedelaySamples = 1.0f / m_combFreq;

        // Get SVF phase at fundamental
        //
        float phaseShift = ComputeSVFPhase(m_combFreq);

        // Phase shift in samples: phase / (2 * pi * f)
        // But phase is already at this frequency, so compensation in samples is:
        // samples = phase / (2 * pi * combFreq)
        //
        float phaseCompensationSamples = phaseShift / (2.0f * static_cast<float>(M_PI) * m_combFreq);

        // Subtract phase compensation (positive phase shift means signal arrives early)
        //
        float compensatedDelay = basedelaySamples - phaseCompensationSamples;

        // Clamp to valid range
        //
        compensatedDelay = std::max(1.0f, std::min(static_cast<float>(x_maxDelaySamples - 1), compensatedDelay));

        return compensatedDelay;
    }

    float Process(float input)
    {
        float delaySamples = ComputeCompensatedDelay();

        // Read from delay line
        //
        float delayed = m_delayLine.Read(delaySamples);

        // Process through SVF
        //
        m_svf.Process(delayed);

        // Get morphed SVF output
        //
        float svfOut;
        if (m_morph <= 0.5f)
        {
            float blend = m_morph * 2.0f;
            svfOut = m_svf.GetHighPass() * (1.0f - blend) + m_svf.GetBandPass() * blend;
        }
        else
        {
            float blend = (m_morph - 0.5f) * 2.0f;
            svfOut = m_svf.GetBandPass() * (1.0f - blend) + m_svf.GetLowPass() * blend;
        }

        // Comb filter: output = input + feedback * filtered_delayed
        //
        m_output = input + m_feedback * svfOut;

        // Write to delay line
        //
        m_delayLine.Write(m_output);

        return m_output;
    }

    void Reset()
    {
        m_svf.Reset();
        m_output = 0.0f;
        for (size_t i = 0; i < x_maxDelaySamples; ++i)
        {
            m_delayLine.m_delayLine[i] = 0.0f;
        }

        m_delayLine.m_index = 0;
    }
};
