#pragma once

#include "AdaptiveWaveTable.hpp"
#include "QuadUtils.hpp"

struct OLA
{
    static constexpr size_t x_bits = 12;
    static constexpr size_t x_hopDenom = 4;
    typedef BasicWaveTableGeneric<x_bits> Buffer;
    typedef DiscreteFourierTransformGeneric<x_bits> DFT;
    static constexpr size_t x_tableSize = Buffer::x_tableSize;
    static constexpr size_t x_maxComponents = DFT::x_maxComponents;
    static constexpr size_t x_N = x_tableSize;
    static constexpr size_t x_H = x_N / x_hopDenom;

    size_t m_index;
    Buffer m_buffer;

    OLA()
        : m_index(0)
    {
    }

    float Process()
    {
        float result = m_buffer.m_table[m_index];
        m_buffer.m_table[m_index] = 0.0f;
        ++m_index;
        if (m_index == x_tableSize)
        {
            m_index = 0;
        }

        return result;
    }

    void Write(DFT& dft)
    {
        Buffer buffer;
        dft.InverseTransform(buffer, x_maxComponents);
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            size_t index = (m_index + i) % x_tableSize;
            m_buffer.m_table[index] += buffer.m_table[i];
        }
    }
};

struct QuadBuffer
{
    OLA::Buffer m_buffers[4];
};

struct QuadDFT
{
    OLA::DFT m_dfts[4];

    void AddComponent(size_t componentIndex, std::complex<float> value, QuadFloat distribution)
    {
        if (componentIndex == 0 || OLA::x_maxComponents <= componentIndex)
        {
            return;
        }

        for (int i = 0; i < 4; ++i)
        {
            m_dfts[i].m_components[componentIndex] += value * distribution[i];
        }
    }

    void WriteWindowedPartial(float magnitude, float phase, float exactFrequency, QuadFloat distribution)
    {
        for (int i = 0; i < 4; ++i)
        {
            m_dfts[i].WriteWindowedPartial(phase, magnitude * distribution[i], exactFrequency);
        }
    }
};

struct QuadOLA
{
    OLA m_olas[4];

    QuadFloat Process()
    {
        return QuadFloat(
            m_olas[0].Process(),
            m_olas[1].Process(),
            m_olas[2].Process(),
            m_olas[3].Process());
    }

    void Write(QuadDFT& dft)
    {
        for (int i = 0; i < 4; ++i)
        {
            m_olas[i].Write(dft.m_dfts[i]);
        }
    }
};
