#pragma once

#include "QuadUtils.hpp"

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

        float rc = 1.0f / (2.0f * M_PI * cyclesPerSample);
        m_alpha = 1.0f / (rc + 1.0f);
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

        float rc = 1.0f / (2.0f * M_PI * cyclesPerSample);
        m_alpha = 1.0f / (rc + 1.0f);
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

        float rc = 1.0f / (2.0f * M_PI * cyclesPerSample);
        m_alpha = rc / (rc + 1.0f);
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
            m_tanhGain = std::tanh(m_inputGain);
        }
    }

    float Process(float input)
    {
        float scaledInput = m_inputGain * input;
        float output = std::tanh(scaledInput);
        m_tanhGain = std::tanh(m_inputGain);
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
