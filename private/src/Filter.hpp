#pragma once

#include "QuadUtils.hpp"

struct OPLowPassFilter
{
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
        float rc = 1.0f / (2.0f * M_PI * cyclesPerSample);
        m_alpha = 1.0f / (rc + 1.0f);
    }                             
};

struct OPHighPassFilter
{
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

    TanhSaturator()
        : m_inputGain(1.0f)
    {
    }

    float Process(float input)
    {
        float scaledInput = m_inputGain * input;
        float output = std::tanh(scaledInput);
        return Normalize ? output / std::tanh(m_inputGain) : output;
    }
};
