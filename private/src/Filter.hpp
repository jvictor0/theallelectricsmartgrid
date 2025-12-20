#pragma once

#include "QuadUtils.hpp"
#include "AsyncLogger.hpp"
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

        float omega = 2.0f * M_PI * cyclesPerSample;   // radians per sample
        m_alpha = 1.0f - std::exp(-omega);
        // float rc = 1.0f / (2.0f * M_PI * cyclesPerSample);
        // m_alpha = 1.0f / (rc + 1.0f);
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

    static float FrequencyResponse(float alpha, float freq)
    {
        return std::abs(TransferFunction(alpha, freq));
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
    {
    }

    TanhSaturator(float gain)
        : m_inputGain(1.0)
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
        float y = x * (27 + x * x) / (27 + 9 * x * x);
        return std::min(1.0f, std::max(-1.0f, y));
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
    {
    }

    ATanSaturator(float gain)
        : m_inputGain(1.0)
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
