#pragma once

#include "AsyncLogger.hpp"
#include "QuadUtils.hpp"
#include "TransferFunction.hpp"
#include <atomic>
#include <cmath>
#include <complex>

struct OPLowPassFilter
{
    static constexpr float x_maxCutoff = 0.499f;
    
    float m_alpha;
    float m_output;

    OPLowPassFilter()
        : m_alpha(0.0f)
        , m_output(0.0f)
    {
    }

    float Process(float input)
    {
        m_output = m_alpha * input + (1.0f - m_alpha) * m_output;
        return m_output;
    }

    void SetAlphaFromNatFreq(float cyclesPerSample)
    {
        cyclesPerSample = std::min(x_maxCutoff, cyclesPerSample);
        assert(cyclesPerSample > 0);
        m_alpha = AlphaFromNatFreq(cyclesPerSample);
    }

    static float AlphaFromNatFreq(float cyclesPerSample)
    {
        cyclesPerSample = std::min(x_maxCutoff, cyclesPerSample);
        assert(cyclesPerSample > 0);

        float omega = 2.0f * M_PI * cyclesPerSample;   // radians per sample
        return 1.0f - std::exp(-omega);
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

struct OPLowPassFilterDouble
{
    static constexpr double x_maxCutoff = 0.499;
    
    double m_alpha;
    double m_output;

    OPLowPassFilterDouble()
        : m_alpha(0.0)
        , m_output(0.0)
    {
    }

    double Process(double input)
    {
        m_output = m_alpha * input + (1.0 - m_alpha) * m_output;
        return m_output;
    }

    void SetAlphaFromNatFreq(double cyclesPerSample)
    {
        cyclesPerSample = std::min(x_maxCutoff, cyclesPerSample);
        assert(cyclesPerSample > 0);

        double omega = 2.0 * M_PI * cyclesPerSample;
        m_alpha = 1.0 - std::exp(-omega);
    }
};

template<size_t Size>
struct BulkFilter
{   
    static constexpr float x_maxCutoff = 0.499f;
    float m_alpha;
    alignas(16) float m_target[Size];
    alignas(16) float m_output[Size];

    BulkFilter()
        : m_alpha(0.0f)
        , m_target{}
        , m_output{0.0f}
    {
        SetAlphaFromNatFreq(500.0 / 48000.0);

        assert(reinterpret_cast<uintptr_t>(m_output) % 16 == 0);
        assert(reinterpret_cast<uintptr_t>(m_target) % 16 == 0);

        memset(m_output, 0, Size * sizeof(float));
        memset(m_target, 0, Size * sizeof(float));
    }

    void SetAlphaFromNatFreq(float cyclesPerSample)
    {
        cyclesPerSample = std::min(x_maxCutoff, cyclesPerSample);
        assert(cyclesPerSample > 0);

        float omega = 2.0f * M_PI * cyclesPerSample;   // radians per sample
        m_alpha = 1.0f - std::exp(-omega);
        // float rc = 1.0f / (2.0f * M_PI * cyclesPerSample);
        // m_alpha = 1.0f / (rc + 1.0f);
    }  

    void LoadTarget(size_t n, size_t offset, float* target)
    {
        memcpy(m_target + offset, target, n * sizeof(float));
    }

    void Process(size_t n)
    {
        float* target = static_cast<float*>(__builtin_assume_aligned(m_target, 16));
        float* output = static_cast<float*>(__builtin_assume_aligned(m_output, 16));
        float alpha = m_alpha;
        float oneMinusAlpha = 1.0f - m_alpha;

        n = ((n + 3) / 4) * 4;

        //#pragma clang loop vectorize(enable) interleave(enable)
        for (size_t i = 0; i < n; ++i)
        {
            output[i] = alpha * target[i] + oneMinusAlpha * output[i];
        }
    }
};

struct OPHighPassFilter
{
    static constexpr float x_maxCutoff = 0.499f;
    
    float m_alpha;
    float m_output;
    float m_prevInput;

    OPHighPassFilter()
        : m_alpha(0.0f)
        , m_output(0.0f)
        , m_prevInput(0.0f)
    {
    }

    float Process(float input)
    {
        float output = m_alpha * (m_output + input - m_prevInput);
        m_prevInput = input;
        m_output = output;
        return output;
    }

    void SetAlphaFromNatFreq(float cyclesPerSample)
    {
        cyclesPerSample = std::min(x_maxCutoff, cyclesPerSample);
        assert(cyclesPerSample > 0);

        float omega = 2.0f * M_PI * cyclesPerSample;   // radians per sample
        m_alpha = std::exp(-omega);
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

// Damping filter: HP + LP in series. UI state implements TransferFunction for visualization.
//
struct DampingFilter
{
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

struct OPBaseWidthFilter
{
    OPHighPassFilter m_highPassFilter;
    OPLowPassFilter m_lowPassFilter;

    OPBaseWidthFilter()
    {
    }

    void SetBaseWidth(float base, float width)
    {
        m_highPassFilter.SetAlphaFromNatFreq(base);
        m_lowPassFilter.SetAlphaFromNatFreq(base * width);
    }

    float Process(float input)
    {
        float highPassOutput = m_highPassFilter.Process(input);
        float lowPassOutput = m_lowPassFilter.Process(highPassOutput);
        return lowPassOutput;
    }
};

struct QuadOPLowPassFilter
{
    OPLowPassFilter m_filters[4];
    QuadFloat m_output;

    QuadFloat Process(const QuadFloat& input)
    {
        for (int i = 0; i < 4; ++i)
        {
            m_output[i] = m_filters[i].Process(input[i]);
        }
        
        return m_output;
    }

    void SetAlphaFromNatFreq(float cyclesPerSample)
    {
        m_filters[0].SetAlphaFromNatFreq(cyclesPerSample);
        for (int i = 1; i < 4; ++i)
        {
            m_filters[i].m_alpha = m_filters[0].m_alpha;
        }
    }

    void SetAlphaFromNatFreq(QuadFloat cyclesPerSample)
    {
        for (int i = 0; i < 4; ++i)
        {
            m_filters[i].SetAlphaFromNatFreq(cyclesPerSample[i]);
        }
    }
};

struct QuadOPHighPassFilter
{
    OPHighPassFilter m_filters[4];
    QuadFloat m_output;

    QuadFloat Process(const QuadFloat& input)
    {
        for (int i = 0; i < 4; ++i)
        {
            m_output[i] = m_filters[i].Process(input[i]);
        }
        
        return m_output;
    }

    void SetAlphaFromNatFreq(float cyclesPerSample)
    {
        m_filters[0].SetAlphaFromNatFreq(cyclesPerSample);
        for (int i = 1; i < 4; ++i)
        {
            m_filters[i].m_alpha = m_filters[0].m_alpha;
        }
    }

    void SetAlphaFromNatFreq(QuadFloat cyclesPerSample)
    {
        for (int i = 0; i < 4; ++i)
        {
            m_filters[i].SetAlphaFromNatFreq(cyclesPerSample[i]);
        }
    }
};

struct QuadOPBaseWidthFilter
{
    OPBaseWidthFilter m_filters[4];
    QuadFloat m_output;

    QuadFloat Process(const QuadFloat& input)
    {
        for (int i = 0; i < 4; ++i)
        {
            m_output[i] = m_filters[i].Process(input[i]);
        }
        
        return m_output;
    }

    void SetBaseWidth(float base, float width)
    {
        m_filters[0].SetBaseWidth(base, width);
        for (int i = 1; i < 4; ++i)
        {
            m_filters[i].m_highPassFilter.m_alpha = m_filters[0].m_highPassFilter.m_alpha;
            m_filters[i].m_lowPassFilter.m_alpha = m_filters[0].m_lowPassFilter.m_alpha;
        }
    }

    void SetBaseWidth(QuadFloat base, QuadFloat width)
    {
        for (int i = 0; i < 4; ++i)
        {
            m_filters[i].SetBaseWidth(base[i], width[i]);
        }
    }
};

template<bool Normalize>
struct TanhSaturator
{
    float m_inputGain;
    float m_tanhGain;

    TanhSaturator()
        : m_inputGain(1.0f)
        , m_tanhGain(0.7615941559557649f)
    {
    }

    TanhSaturator(float gain)
        : m_inputGain(1.0)
        , m_tanhGain(0.7615941559557649f)
    {
        SetInputGain(gain);
    }

    void SetInputGain(float gain)
    {
        if (gain != m_inputGain)
        {
            m_inputGain = gain;
            m_tanhGain = Tanh(m_inputGain);
        }
    }

    float Tanh(float x)
    {
        float y = x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
        return std::min(1.0f, std::max(-1.0f, y));
    }

    // Derivative of Process w.r.t. input
    // Non-normalized: d/dx[Tanh(gain*x)] = gain * sech²(gain*x)
    // Normalized: d/dx[Tanh(gain*x)/tanhGain] = gain * sech²(gain*x) / tanhGain
    //
    float Derivative(float input)
    {
        float scaledInput = m_inputGain * input;
        if (1.0 <= std::fabs(Tanh(scaledInput)))
        {
            return 0.0f;
        }

        // Rational tanh and its derivative sech^2 as a rational function
        // y = x * (27 + x^2) / (27 + 9*x^2)
        // dy/dx = [ (27 + x^2) + 2x^2 ] / (27 + 9*x^2) - x*(27 + x^2)*18x / (27 + 9*x^2)^2
        //        = (27 + 3x^2) / (27 + 9x^2) - [18x^2 (27 + x^2)] / (27 + 9x^2)^2

        float x = scaledInput;
        float x2 = x * x;
        float denom = 27.0f + 9.0f * x2;

        // Derivative of the numerator
        float num1 = 27.0f + 3.0f * x2;
        float num2 = 18.0f * x2 * (27.0f + x2);

        float numerator = num1 * denom - num2;
        float denominator = denom * denom;

        float sech2 = numerator / denominator;

        if (Normalize)
        {
            return m_inputGain * sech2 / m_tanhGain;
        }
        else
        {
            return m_inputGain * sech2;
        }
    }

    float DerivativeZero()
    {
        if (Normalize)
        {
            return m_inputGain / m_tanhGain;
        }
        else
        {
            return m_inputGain;
        }
    }

    float Process(float input)
    {
        float scaledInput = m_inputGain * input;
        float output = Tanh(scaledInput);
        return Normalize ? output / m_tanhGain : output;
    }

    QuadFloat Process(const QuadFloat& input)
    {
        QuadFloat output;
        for (int i = 0; i < 4; ++i)
        {
            output[i] = Process(input[i]);
        }

        return output;
    }
};

template<bool Normalize>
struct ATanSaturator
{
    float m_inputGain;
    float m_atanGain;

    ATanSaturator()
        : m_inputGain(1.0f)
        , m_atanGain(0.0f)
    {
    }

    ATanSaturator(float gain)
        : m_inputGain(1.0)
        , m_atanGain(0.0f)
    {
        SetInputGain(gain);
    }

    float Saturate(float input)
    {
        return std::atan(input * M_PI / 2) / (M_PI / 2);
    }

    void SetInputGain(float gain)
    {
        if (gain != m_inputGain)
        {
            m_inputGain = gain;
            m_atanGain = Saturate(m_inputGain);
        }
    }

    float Process(float input)
    {
        float scaledInput = m_inputGain * input;
        float output = Saturate(scaledInput);
        return Normalize ? output / m_atanGain : output;
    }

    QuadFloat Process(const QuadFloat& input)
    {
        QuadFloat output;
        for (int i = 0; i < 4; ++i)
        {
            output[i] = Process(input[i]);
        }

        return output;
    }
};
