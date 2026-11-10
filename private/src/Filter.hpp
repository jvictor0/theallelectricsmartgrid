#pragma once

#include "AsyncLogger.hpp"
#include "QuadUtils.hpp"
#include "SampleTimer.hpp"
#include "TransferFunction.hpp"
#include <atomic>
#include <cmath>
#include <complex>

template <typename Number, size_t Size>
struct OPLowPassFilterGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr Number x_maxCutoff = static_cast<Number>(0.499);
    
    Vector m_alpha;
    Vector m_output;

    OPLowPassFilterGeneric()
        : m_alpha(Vector::Splat(0))
        , m_output(Vector::Splat(0))
    {
    }

    Vector Process(const Vector& input)
    {
        m_output = Vector::FusedMultiplyAdd(m_alpha, input, (Vector::Splat(1) - m_alpha) * m_output);
        return m_output;
    }

    void SetAlphaFromNatFreq(Number cyclesPerSample)
    {
        m_alpha = AlphaFromNatFreq(Vector::Splat(cyclesPerSample));
    }

    void SetAlphaFromNatFreq(const Vector& cyclesPerSample)
    {
        m_alpha = AlphaFromNatFreq(cyclesPerSample);
    }

    void SetAlphaFromNatFreqApprox(Number cyclesPerSample)
    {
        m_alpha = AlphaFromNatFreqApprox(Vector::Splat(cyclesPerSample));
    }

    void SetAlphaFromNatFreqApprox(const Vector& cyclesPerSample)
    {
        m_alpha = AlphaFromNatFreqApprox(cyclesPerSample);
    }

    // Radians per sample, with the cutoff clamped to x_maxCutoff.
    //
    static Vector OmegaFromNatFreq(const Vector& cyclesPerSample)
    {
        Vector clamped = Vector::Min(Vector::Splat(x_maxCutoff), cyclesPerSample);
        for (size_t i = 0; i < Size; ++i)
        {
            assert(clamped[i] > 0);
        }

        return clamped * static_cast<Number>(2 * M_PI);
    }

    static Vector AlphaFromNatFreq(const Vector& cyclesPerSample)
    {
        return Vector::Splat(1) - (-OmegaFromNatFreq(cyclesPerSample)).Exp();
    }

    // Approximate 1 - exp(-omega) directly, preserving tiny slew coefficients.
    // Use a fourth-order series for omega <= 0.05. Above that, a sixth-order
    // series at omega/8 and three doublings cover the full clamped range.
    // Relative alpha error is below 6e-7 in float and 1.5e-7 in double;
    // equivalent decay-rate error is below 3e-6 and 1.1e-6, respectively.
    // The polynomial arithmetic is SIMD, including for double state.
    //
    static Vector AlphaFromNatFreqApprox(const Vector& cyclesPerSample)
    {
        Vector omega = OmegaFromNatFreq(cyclesPerSample);
        if (omega == Vector::Min(omega, Vector::Splat(static_cast<Number>(0.05))))
        {
            Vector p = Vector::Splat(static_cast<Number>(-1.0 / 24.0));
            p = p * omega + static_cast<Number>(1.0 / 6.0);
            p = p * omega - static_cast<Number>(0.5);
            p = p * omega + static_cast<Number>(1);
            return omega * p;
        }

        Vector x = omega * static_cast<Number>(0.125);
        Vector p = Vector::Splat(static_cast<Number>(-1.0 / 720.0));
        p = p * x + static_cast<Number>(1.0 / 120.0);
        p = p * x - static_cast<Number>(1.0 / 24.0);
        p = p * x + static_cast<Number>(1.0 / 6.0);
        p = p * x - static_cast<Number>(0.5);
        p = p * x + static_cast<Number>(1);
        Vector alpha = x * p;
        for (size_t i = 0; i < 3; ++i)
        {
            alpha = alpha * (Vector::Splat(2) - alpha);
        }

        return alpha;
    }

    // Phase-distance decay has no Nyquist cutoff. Keep the accurate small
    // coefficient path, and use the exponential for large phase steps.
    //
    static Vector AlphaFromPhaseTravel(const Vector& cycles)
    {
        for (size_t lane = 0; lane < Size; ++lane)
        {
            assert(std::isfinite(cycles[lane]) && cycles[lane] >= 0);
        }

        Vector safe = Vector::Max(cycles, Vector::Splat(static_cast<Number>(1e-20)));
        Vector small = AlphaFromNatFreqApprox(safe);
        if (cycles == Vector::Min(cycles, Vector::Splat(x_maxCutoff)))
        {
            return Vector::IfLess(Vector(), cycles, small, Vector());
        }

        for (size_t lane = 0; lane < Size; ++lane)
        {
            if (cycles[lane] > x_maxCutoff)
            {
                small[lane] = -std::expm1(static_cast<Number>(-2 * M_PI) * cycles[lane]);
            }
            else if (cycles[lane] == 0)
            {
                small[lane] = 0;
            }
        }

        return small;
    }

    static std::complex<float> TransferFunction(float alpha, float freq)
    {
        // Calculate normalized frequency (omega)
        //
        float omega = 2.0f * M_PI * freq;
        float cosOmega = std::cos(omega);
        float sinOmega = std::sin(omega);
        
        // OPLowPassFilter transfer function: H(z) = alpha / (1 - (1-alpha)z^-1)
        // H(e^jω) = alpha / (1 - (1-alpha)e^-jω)
        //         = alpha / (1 - (1-alpha)(cos(ω) - j*sin(ω)))
        //         = alpha / ((1 - (1-alpha)cos(ω)) + j*(1-alpha)sin(ω))
        //
        float den_real = 1.0f - (1.0f - alpha) * cosOmega;
        float den_imag = (1.0f - alpha) * sinOmega;
        
        std::complex<float> numerator(alpha, 0.0f);
        std::complex<float> denominator(den_real, den_imag);
        return numerator / denominator;
    }

    // Compute group delay of a one-pole low-pass filter in samples.
    // alpha: filter coefficient (0 < alpha <= 1)
    // normalizedFreq: frequency in cycles per sample (0 to 0.5)
    //
    // For H(z) = alpha / (1 - (1-alpha)*z^-1), the group delay is:
    // τ(ω) = a1*(a1 - cos(ω)) / (1 - 2*a1*cos(ω) + a1²)
    // where a1 = 1 - alpha
    //
    static float GroupDelay(float alpha, float normalizedFreq)
    {
        float a1 = 1.0f - alpha;
        float cosOmega = Math::Cos2pi(normalizedFreq);
        float denominator = 1.0f - 2.0f * a1 * cosOmega + a1 * a1;

        if (denominator < 1e-10f)
        {
            return 0.0f;
        }

        return a1 * (a1 - cosOmega) / denominator;
    }

    // Compute phase delay of a one-pole low-pass filter in samples.
    // This is the delay term that should be used to keep comb tuning stable.
    //
    static float PhaseDelay(float alpha, float normalizedFreq)
    {
        float safeAlpha = std::max(1e-6f, std::min(1.0f, alpha));
        float safeFreq = std::max(0.0f, std::min(0.5f, normalizedFreq));
        float a1 = 1.0f - safeAlpha;
        float omega = 2.0f * static_cast<float>(M_PI) * safeFreq;

        if (omega < 1e-6f)
        {
            // lim_{w->0} phaseDelay(w) = (1 - alpha) / alpha
            //
            return a1 / safeAlpha;
        }

        float sinOmega = Math::Sin2pi(safeFreq);
        float cosOmega = Math::Cos2pi(safeFreq);
        float denominatorReal = 1.0f - a1 * cosOmega;
        float denominatorImag = a1 * sinOmega;
        float phaseMagnitude = std::atan2(denominatorImag, denominatorReal);
        return phaseMagnitude / omega;
    }

    static float FrequencyResponse(float alpha, float freq)
    {
        return std::abs(TransferFunction(alpha, freq));
    }
};

using OPLowPassFilter = OPLowPassFilterGeneric<float, 1>;
using OPLowPassFilterDouble = OPLowPassFilterGeneric<double, 1>;
using QuadOPLowPassFilter = OPLowPassFilterGeneric<float, 4>;

template <typename Number, size_t Size>
struct OPHighPassFilterGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    
    Vector m_alpha;
    Vector m_output;
    Vector m_prevInput;

    OPHighPassFilterGeneric()
        : m_alpha(Vector::Splat(0))
        , m_output(Vector::Splat(0))
        , m_prevInput(Vector::Splat(0))
    {
    }

    Vector Process(const Vector& input)
    {
        Vector output = m_alpha * (m_output + input - m_prevInput);
        m_prevInput = input;
        m_output = output;
        return output;
    }

    void SetAlphaFromNatFreq(Number cyclesPerSample)
    {
        m_alpha = AlphaFromNatFreq(Vector::Splat(cyclesPerSample));
    }

    void SetAlphaFromNatFreq(const Vector& cyclesPerSample)
    {
        m_alpha = AlphaFromNatFreq(cyclesPerSample);
    }

    static Vector AlphaFromNatFreq(const Vector& cyclesPerSample)
    {
        return (-OPLowPassFilterGeneric<Number, Size>::OmegaFromNatFreq(cyclesPerSample)).Exp();
    }

    static std::complex<float> TransferFunction(float alpha, float freq)
    {
        // Calculate normalized frequency (omega)
        //
        float omega = 2.0f * M_PI * freq;
        float cosOmega = std::cos(omega);
        float sinOmega = std::sin(omega);
        
        // OPHighPassFilter transfer function: H(z) = alpha * (1 - z^-1) / (1 - alpha * z^-1)
        // H(e^jω) = alpha * (1 - e^-jω) / (1 - alpha * e^-jω)
        //         = alpha * (1 - (cos(ω) - j*sin(ω))) / (1 - alpha * (cos(ω) - j*sin(ω)))
        //         = alpha * ((1 - cos(ω)) + j*sin(ω)) / ((1 - alpha*cos(ω)) + j*alpha*sin(ω))
        //
        float num_real = alpha * (1.0f - cosOmega);
        float num_imag = alpha * sinOmega;
        
        float den_real = 1.0f - alpha * cosOmega;
        float den_imag = alpha * sinOmega;
        
        std::complex<float> numerator(num_real, num_imag);
        std::complex<float> denominator(den_real, den_imag);
        return numerator / denominator;
    }

    static float FrequencyResponse(float alpha, float freq)
    {
        return std::abs(TransferFunction(alpha, freq));
    }
};

using OPHighPassFilter = OPHighPassFilterGeneric<float, 1>;
using QuadOPHighPassFilter = OPHighPassFilterGeneric<float, 4>;

// Damping filter: HP + LP in series. UI state implements TransferFunction for visualization.
//
struct DampingFilter
{
    // Base frequency range of the delay and reverb damping filters, in Hz, so
    // the damping keeps its physical frequencies at any engine rate.
    //
    static constexpr double x_minBaseHz = SampleTimer::CycPerHostSampToHz(1.0 / 2048.0);
    static constexpr double x_maxBaseHz = SampleTimer::CycPerHostSampToHz(0.5);

    struct UIState : TransferFunction
    {
        std::atomic<float> m_hpAlpha;
        std::atomic<float> m_lpAlpha;

        UIState()
            : m_hpAlpha(0.0f)
            , m_lpAlpha(0.0f)
        {
        }

        float FrequencyResponse(float freq) const override
        {
            return OPLowPassFilter::FrequencyResponse(m_lpAlpha.load(), freq)
                * OPHighPassFilter::FrequencyResponse(m_hpAlpha.load(), freq);
        }

        std::complex<float> TransferFunctionValue(float freq) const override
        {
            return OPLowPassFilter::TransferFunction(m_lpAlpha.load(), freq)
                * OPHighPassFilter::TransferFunction(m_hpAlpha.load(), freq);
        }
    };
};

template <typename Number, size_t Size>
struct OPBaseWidthFilterGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;

    OPHighPassFilterGeneric<Number, Size> m_highPassFilter;
    OPLowPassFilterGeneric<Number, Size> m_lowPassFilter;

    void SetBaseWidth(Number base, Number width)
    {
        m_highPassFilter.SetAlphaFromNatFreq(base);
        m_lowPassFilter.SetAlphaFromNatFreq(base * width);
    }

    void SetBaseWidth(const Vector& base, const Vector& width)
    {
        m_highPassFilter.SetAlphaFromNatFreq(base);
        m_lowPassFilter.SetAlphaFromNatFreq(base * width);
    }

    Vector Process(const Vector& input)
    {
        Vector highPassOutput = m_highPassFilter.Process(input);
        return m_lowPassFilter.Process(highPassOutput);
    }
};

using OPBaseWidthFilter = OPBaseWidthFilterGeneric<float, 1>;
using QuadOPBaseWidthFilter = OPBaseWidthFilterGeneric<float, 4>;
