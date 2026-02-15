#pragma once

#include "DCServo.hpp"
#include "Filter.hpp"
#include "Math.hpp"
#include "PhaseUtils.hpp"
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdio>

struct LinearStateVariableFilter
{
    static constexpr float x_maxCutoff = 0.499f;
    static constexpr float x_qMin = 0.5f;
    static constexpr float x_qMax = 5.5f;

    struct UIState
    {
        std::atomic<float> m_g;
        std::atomic<float> m_k;

        UIState()
            : m_g(0.0f)
            , m_k(0.0f)
        {
        }

        float LowPassFrequencyResponse(float freq)
        {
            return LinearStateVariableFilter::LowPassFrequencyResponse(m_g.load(), m_k.load(), freq);
        }

        float HighPassFrequencyResponse(float freq)
        {
            return LinearStateVariableFilter::HighPassFrequencyResponse(m_g.load(), m_k.load(), freq);
        }

        float BandPassFrequencyResponse(float freq)
        {
            return LinearStateVariableFilter::BandPassFrequencyResponse(m_g.load(), m_k.load(), freq);
        }
    };

    float m_cutoff;
    PhaseUtils::ExpParam m_resonance;

    float m_g;
    float m_k;
    float m_a1;
    float m_a2;
    float m_a3;

    float m_ic1eq;
    float m_ic2eq;

    float m_input;
    float m_lowPass;
    float m_highPass;
    float m_bandPass;
    float m_notch;

    LinearStateVariableFilter()
        : m_cutoff(0.1f)
        , m_resonance(20.0f)
        , m_g(0.0f)
        , m_k(0.0f)
        , m_a1(0.0f)
        , m_a2(0.0f)
        , m_a3(0.0f)
        , m_ic1eq(0.0f)
        , m_ic2eq(0.0f)
        , m_input(0.0f)
        , m_lowPass(0.0f)
        , m_highPass(0.0f)
        , m_bandPass(0.0f)
        , m_notch(0.0f)
    {
        m_resonance = PhaseUtils::ExpParam(x_qMin, x_qMax);
        SetCutoff(m_cutoff);
        SetResonance(0.0f);
    }

    void Process(float input)
    {
        m_input = input;

        // Trapezoidal SVF implementation
        // Based on the TPT (topology-preserving transform) structure
        //
        float v3 = input - m_ic2eq;
        float v1 = m_a1 * m_ic1eq + m_a2 * v3;
        float v2 = m_ic2eq + m_a2 * m_ic1eq + m_a3 * v3;

        m_ic1eq = 2.0f * v1 - m_ic1eq;
        m_ic2eq = 2.0f * v2 - m_ic2eq;

        m_lowPass = v2;
        m_bandPass = v1;
        m_highPass = input - m_k * v1 - v2;
        m_notch = m_lowPass + m_highPass;
    }

    float GetLowPass() const
    {
        return m_lowPass;
    }

    float GetHighPass() const
    {
        return m_highPass;
    }

    float GetBandPass() const
    {
        return m_bandPass;
    }

    float GetNotch() const
    {
        return m_notch;
    }

    void SetCutoff(float cutoff)
    {
        m_cutoff = std::min(x_maxCutoff, cutoff);
        m_g = Math::TanPi(m_cutoff);
        UpdateCoefficients();
    }

    void SetResonance(float resonance)
    {
        // k = 1/Q, where Q ranges from x_qMin to x_qMax
        // resonance 0 -> Q = x_qMin (Butterworth-ish), k = 1/x_qMin
        // resonance 1 -> Q = x_qMax (high resonance), k = 1/x_qMax
        //
        float expResonance = m_resonance.Update(resonance);        
        m_k = 1.0f / expResonance;
        UpdateCoefficients();
    }

    void UpdateCoefficients()
    {
        m_a1 = 1.0f / (1.0f + m_g * (m_g + m_k));
        m_a2 = m_g * m_a1;
        m_a3 = m_g * m_a2;
    }

    void Reset()
    {
        m_ic1eq = 0.0f;
        m_ic2eq = 0.0f;
        m_input = 0.0f;
        m_lowPass = 0.0f;
        m_highPass = 0.0f;
        m_bandPass = 0.0f;
        m_notch = 0.0f;
    }

    void PopulateUIState(UIState* uiState)
    {
        uiState->m_g.store(m_g);
        uiState->m_k.store(m_k);
    }

    static std::complex<float> LowPassTransferFunction(float g, float k, float freq)
    {
        // Calculate normalized frequency (omega)
        //
        float omega = 2.0f * static_cast<float>(M_PI) * freq;
        std::complex<float> z(std::cos(omega), std::sin(omega));
        std::complex<float> zInv = std::complex<float>(1.0f, 0.0f) / z;

        // Bilinear transform: s = (1 - z^-1) / (1 + z^-1) * 2 / T
        // For normalized frequency, T = 1, so s = (1 - z^-1) / (1 + z^-1) * 2
        // But since we use g = tan(pi*fc), we work directly with g
        //
        std::complex<float> num = g * g;
        std::complex<float> s = (std::complex<float>(1.0f, 0.0f) - zInv) / (std::complex<float>(1.0f, 0.0f) + zInv);
        s = s / g;
        std::complex<float> den = s * s + k * s + std::complex<float>(1.0f, 0.0f);

        return std::complex<float>(1.0f, 0.0f) / den;
    }

    static std::complex<float> HighPassTransferFunction(float g, float k, float freq)
    {
        float omega = 2.0f * static_cast<float>(M_PI) * freq;
        std::complex<float> z(std::cos(omega), std::sin(omega));
        std::complex<float> zInv = std::complex<float>(1.0f, 0.0f) / z;

        std::complex<float> s = (std::complex<float>(1.0f, 0.0f) - zInv) / (std::complex<float>(1.0f, 0.0f) + zInv);
        s = s / g;
        std::complex<float> num = s * s;
        std::complex<float> den = s * s + k * s + std::complex<float>(1.0f, 0.0f);

        return num / den;
    }

    static std::complex<float> BandPassTransferFunction(float g, float k, float freq)
    {
        float omega = 2.0f * static_cast<float>(M_PI) * freq;
        std::complex<float> z(std::cos(omega), std::sin(omega));
        std::complex<float> zInv = std::complex<float>(1.0f, 0.0f) / z;

        std::complex<float> s = (std::complex<float>(1.0f, 0.0f) - zInv) / (std::complex<float>(1.0f, 0.0f) + zInv);
        s = s / g;
        std::complex<float> num = s;
        std::complex<float> den = s * s + k * s + std::complex<float>(1.0f, 0.0f);

        return num / den;
    }

    static float LowPassFrequencyResponse(float g, float k, float freq)
    {
        return std::abs(LowPassTransferFunction(g, k, freq));
    }

    static float HighPassFrequencyResponse(float g, float k, float freq)
    {
        return std::abs(HighPassTransferFunction(g, k, freq));
    }

    static float BandPassFrequencyResponse(float g, float k, float freq)
    {
        return std::abs(BandPassTransferFunction(g, k, freq));
    }
};

// 4-pole highpass filter by cascading two LinearStateVariableFilters
//
struct LinearSVF4PoleHighPass
{
    static constexpr float x_maxCutoff = LinearStateVariableFilter::x_maxCutoff;
    static constexpr float x_qMin = LinearStateVariableFilter::x_qMin;
    static constexpr float x_qMax = LinearStateVariableFilter::x_qMax;

    struct UIState
    {
        std::atomic<float> m_g;
        std::atomic<float> m_k;

        UIState()
            : m_g(0.0f)
            , m_k(0.0f)
        {
        }

        float FrequencyResponse(float freq)
        {
            float g = m_g.load();
            float k = m_k.load();

            // Cascade two 2-pole highpass responses
            //
            float hp2pole = LinearStateVariableFilter::HighPassFrequencyResponse(g, k, freq);
            return hp2pole * hp2pole;
        }
    };

    LinearStateVariableFilter m_stage1;
    LinearStateVariableFilter m_stage2;

    float m_output;

    LinearSVF4PoleHighPass()
        : m_output(0.0f)
    {
    }

    void Process(float input)
    {
        m_stage1.Process(input);
        m_stage2.Process(m_stage1.GetHighPass());
        m_output = m_stage2.GetHighPass();
    }

    float GetOutput() const
    {
        return m_output;
    }

    void SetCutoff(float cutoff)
    {
        m_stage1.SetCutoff(cutoff);
        m_stage2.SetCutoff(cutoff);
    }

    void SetResonance(float resonance)
    {
        m_stage1.SetResonance(resonance);
        m_stage2.SetResonance(resonance);
    }

    void Reset()
    {
        m_stage1.Reset();
        m_stage2.Reset();
        m_output = 0.0f;
    }

    void PopulateUIState(UIState* uiState)
    {
        uiState->m_g.store(m_stage1.m_g);
        uiState->m_k.store(m_stage1.m_k);
    }

    static std::complex<float> TransferFunction(float g, float k, float freq)
    {
        // Cascade two 2-pole highpass transfer functions
        //
        std::complex<float> hp2pole = LinearStateVariableFilter::HighPassTransferFunction(g, k, freq);
        return hp2pole * hp2pole;
    }

    static float FrequencyResponse(float g, float k, float freq)
    {
        return std::abs(TransferFunction(g, k, freq));
    }
};

// Nonlinear TPT/ZDF SVF with saturation ONLY on the summer (hp drive)
// 1D Newton solve for v3 (saturated highpass output)
//
// Topology:
//   u  = input - R * v1 - v2       (linear summer)
//   v3 = Sat(u)                    (ONLY nonlinearity)
//   v1 = g * v3 + s1               (bp)
//   v2 = g * v1 + s2               (lp)
//
struct NonlinearStateVariableFilter
{
    static constexpr float x_maxCutoff = 0.499f;
    static constexpr float x_qMin = 0.5f;
    static constexpr float x_qMax = 10.5f;
    static constexpr int x_newtonIterations = 6;
    static constexpr float x_newtonEps = 1e-8f;
    static constexpr float x_newtonDamp = 1.0f;

    // DEBUG: Trace interval (48000 * 4 = 192000 samples = 1 second at 4x oversampling)
    //
    static constexpr size_t x_debugTraceInterval = 192000;

    struct UIState
    {
        std::atomic<float> m_g;
        std::atomic<float> m_k;

        UIState()
            : m_g(0.0f)
            , m_k(0.0f)
        {
        }

        float LowPassFrequencyResponse(float freq)
        {
            // Use linear approximation for UI display
            //
            return LinearStateVariableFilter::LowPassFrequencyResponse(m_g.load(), m_k.load(), freq);
        }

        float HighPassFrequencyResponse(float freq)
        {
            return LinearStateVariableFilter::HighPassFrequencyResponse(m_g.load(), m_k.load(), freq);
        }

        float BandPassFrequencyResponse(float freq)
        {
            return LinearStateVariableFilter::BandPassFrequencyResponse(m_g.load(), m_k.load(), freq);
        }
    };

    // DEBUG: DC tracking with very low cutoff one-poles
    // At 192kHz, alpha=0.000001 gives ~5 second time constant
    //
    struct DebugDCTracker
    {
        float m_state;
        static constexpr float x_alpha = 0.000001f;

        DebugDCTracker() : m_state(0.0f) {}

        float Process(float input)
        {
            m_state += x_alpha * (input - m_state);
            return m_state;
        }
    };

    float m_cutoff;
    PhaseUtils::ExpParam m_resonance;

    float m_g;
    float m_R;

    // Integrator states with DC servo
    //
    DCServo m_ic1;
    DCServo m_ic2;

    // Previous v3 for Newton initial guess
    //
    float m_v3;

    TanhSaturator<true> m_saturator;

    float m_input;
    float m_lowPass;
    float m_highPass;
    float m_bandPass;
    float m_notch;

    // DEBUG: DC trackers and sample counter
    //
    size_t m_debugSampleCount;
    DebugDCTracker m_debugDC_input;
    DebugDCTracker m_debugDC_ic1eq;
    DebugDCTracker m_debugDC_ic2eq;
    DebugDCTracker m_debugDC_v3;
    DebugDCTracker m_debugDC_v1;
    DebugDCTracker m_debugDC_v2;
    DebugDCTracker m_debugDC_u;
    DebugDCTracker m_debugDC_satOut;
    DebugDCTracker m_debugDC_lp;
    DebugDCTracker m_debugDC_bp;
    DebugDCTracker m_debugDC_hp;

    NonlinearStateVariableFilter()
        : m_cutoff(0.1f)
        , m_resonance(x_qMin, x_qMax)
        , m_g(0.0f)
        , m_R(0.0f)
        , m_v3(0.0f)
        , m_input(0.0f)
        , m_lowPass(0.0f)
        , m_highPass(0.0f)
        , m_bandPass(0.0f)
        , m_notch(0.0f)
        , m_debugSampleCount(0)
    {
        m_saturator.SetInputGain(1.0f);

        // DC servo at 5 Hz for 192kHz (4x oversampled from 48kHz)
        //
        m_ic1.SetAlphaFromNatFreq(5.0f / 192000.0f);
        m_ic2.SetAlphaFromNatFreq(5.0f / 192000.0f);
        
        SetCutoff(m_cutoff);
        SetResonance(0.0f);
    }

    void Process(float input)
    {
        m_input = input;

        const float g = m_g;
        const float R = m_R;

        // Use DC-servoed (AC) values for feedback to prevent DC accumulation
        //
        const float s1 = m_ic1.GetAC();
        const float s2 = m_ic2.GetAC();

        // Solve for v3
        //
        float v3 = m_v3;

        for (int iter = 0; iter < x_newtonIterations; ++iter)
        {
            // Dependent states from current v3 guess
            //
            const float v1 = (g * v3) + s1;
            const float v2 = (g * v1) + s2;

            // Linear summer
            //
            const float u = input - (R * v1) - v2;

            // Saturated summer output
            //
            const float y = m_saturator.Process(u);

            // F(v3) = v3 - Sat(u(v3))
            //
            const float F = v3 - y;

            // Optional early exit
            //
            if (std::fabs(F) < 1e-7f)
            {
                break;
            }

            // dF/dv3 = 1 - Sat'(u) * du/dv3
            //
            // v1 = g*v3 + s1        => dv1/dv3 = g
            // v2 = g*v1 + s2        => dv2/dv3 = g²
            // u  = input - R*v1 - v2 => du/dv3 = -R*g - g²
            //
            // dF/dv3 = 1 + Sat'(u) * (R*g + g²)
            //
            const float dydu = m_saturator.Derivative(u);
            const float dF = 1.0f + dydu * ((R * g) + (g * g));

            // Bail if derivative is tiny
            //
            if (std::fabs(dF) < x_newtonEps)
            {
                break;
            }

            const float step = F / dF;
            v3 -= x_newtonDamp * step;
        }

        // Final states from solved v3 (using DC-blocked version)
        //
        const float v1 = (g * v3) + s1;
        const float v2 = (g * v1) + s2;
        const float u_final = input - (R * v1) - v2;
        const float y_final = m_saturator.Process(u_final);

        // Update integrator states (TPT)
        // Note: we use the AC-coupled s1, s2 in the trapezoidal update formula
        //
        float new_ic1 = (2.0f * v1) - s1;
        float new_ic2 = (2.0f * v2) - s2;

        // DC leak: subtract a tiny fraction to prevent DC accumulation
        // This is a very slow one-pole highpass (cutoff ~ 5 Hz at 192kHz)
        // leak = 1 - 2*pi*fc/fs = 1 - 2*pi*5/192000 ≈ 0.99984
        //
        static constexpr float x_dcLeak = 0.99984f;
        new_ic1 *= x_dcLeak;
        new_ic2 *= x_dcLeak;

        // Set updates the value and the DC servo tracking
        //
        m_ic1.Set(new_ic1);
        m_ic2.Set(new_ic2);

        m_v3 = v3;

        // Store outputs (use DC-blocked v3 for highpass)
        //
        m_highPass = v3;
        m_bandPass = v1;
        m_lowPass = v2;
        m_notch = v2 + v3;

        // DEBUG: Track DC components and print periodically
        //
        m_debugDC_input.Process(input);
        m_debugDC_ic1eq.Process(m_ic1.Get());
        m_debugDC_ic2eq.Process(m_ic2.Get());
        m_debugDC_v3.Process(v3);
        m_debugDC_v1.Process(v1);
        m_debugDC_v2.Process(v2);
        m_debugDC_u.Process(u_final);
        m_debugDC_satOut.Process(y_final);
        m_debugDC_lp.Process(m_lowPass);
        m_debugDC_bp.Process(m_bandPass);
        m_debugDC_hp.Process(m_highPass);

        ++m_debugSampleCount;
        if (m_debugSampleCount >= x_debugTraceInterval)
        {
            m_debugSampleCount = 0;
            printf("=== NonlinearSVF Debug (1s) ===\n");
            printf("  g=%.4f R=%.4f satGain=%.2f\n", m_g, m_R, m_saturator.m_inputGain);
            printf("  DC input:   %+.6f\n", m_debugDC_input.m_state);
            printf("  DC ic1eq:   %+.6f\n", m_debugDC_ic1eq.m_state);
            printf("  DC ic2eq:   %+.6f\n", m_debugDC_ic2eq.m_state);
            printf("  DC v3:      %+.6f\n", m_debugDC_v3.m_state);
            printf("  DC v1(bp):  %+.6f\n", m_debugDC_v1.m_state);
            printf("  DC v2(lp):  %+.6f\n", m_debugDC_v2.m_state);
            printf("  DC u:       %+.6f\n", m_debugDC_u.m_state);
            printf("  DC sat(u):  %+.6f\n", m_debugDC_satOut.m_state);
            printf("  DC hp out:  %+.6f\n", m_debugDC_hp.m_state);
            printf("  DC bp out:  %+.6f\n", m_debugDC_bp.m_state);
            printf("  DC lp out:  %+.6f\n", m_debugDC_lp.m_state);
            printf("  Newton residual: %.2e\n", std::fabs(v3 - y_final));
            printf("\n");
        }
    }

    float GetLowPass() const
    {
        return m_lowPass;
    }

    float GetHighPass() const
    {
        return m_highPass;
    }

    float GetBandPass() const
    {
        return m_bandPass;
    }

    float GetNotch() const
    {
        return m_notch;
    }

    void SetCutoff(float cutoff)
    {
        m_cutoff = std::min(x_maxCutoff, cutoff);
        m_g = Math::TanPi(m_cutoff);
    }

    void SetResonance(float resonance)
    {
        float expResonance = m_resonance.Update(resonance);
        m_R = 1.0f / expResonance;
    }

    void SetSaturationGain(float gain)
    {
        m_saturator.SetInputGain(gain);
    }

    void Reset()
    {
        m_ic1.Reset();
        m_ic2.Reset();
        m_v3 = 0.0f;
        m_input = 0.0f;
        m_lowPass = 0.0f;
        m_highPass = 0.0f;
        m_bandPass = 0.0f;
        m_notch = 0.0f;
    }

    void PopulateUIState(UIState* uiState)
    {
        uiState->m_g.store(m_g);
        uiState->m_k.store(m_R);
    }
};
