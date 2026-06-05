#pragma once

#include <complex>
#include <cmath>
#include "BasicWaveTable.hpp"

template<size_t Bits>
struct MathGeneric
{
    inline static MathGeneric s_instance;
    static constexpr size_t x_tableSize = BasicWaveTableGeneric<Bits>::x_tableSize;
    static constexpr int x_hannKernelRadius = 8;
    static constexpr size_t x_hannKernelTableSize = x_tableSize;
    static constexpr float x_hannKernelMinOffset = -static_cast<float>(x_hannKernelRadius);
    static constexpr float x_hannKernelWidth = 2.0f * static_cast<float>(x_hannKernelRadius) + 1.0f;
    BasicWaveTableGeneric<Bits> m_cosTable;
    std::complex<float> m_rootOfUnity[x_tableSize];
    std::complex<float> m_hannKernel[x_hannKernelTableSize + 1];

    MathGeneric()
    {
        // Initialize cosine wavetable
        //
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_cosTable.Write(i, cosf(2.0f * M_PI * i / x_tableSize));
        }

        // Initialize root of unity table
        //
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_rootOfUnity[i] = std::complex<float>(cosf(2.0f * M_PI * i / x_tableSize), sinf(2.0f * M_PI * i / x_tableSize));
        }

        // Initialize Hann kernel table for windowed partial writes
        //
        for (size_t i = 0; i <= x_hannKernelTableSize; ++i)
        {
            float offset = x_hannKernelMinOffset + x_hannKernelWidth * static_cast<float>(i) / static_cast<float>(x_hannKernelTableSize);
            m_hannKernel[i] = ComputeHannKernel(offset);
        }
    }

    static std::complex<float> RootOfUnityByIndex(size_t index)
    {
        return s_instance.m_rootOfUnity[index];
    }

    static float Sin2pi(float x)
    {
        float phase = x - std::floor(x);
        phase += 0.75f;
        phase -= std::floor(phase);
        return s_instance.m_cosTable.Evaluate(phase);
    }

    static float Sin(float x)
    {
        return Sin2pi(x / (2.0f * M_PI));
    }

    static float Cos2pi(float x)
    {
        float phase = x - std::floor(x);
        return s_instance.m_cosTable.Evaluate(phase);
    }

    static float Cos(float x)
    {
        return Cos2pi(x / (2.0f * M_PI));
    }

    static float Tan2pi(float x)
    {
        float s = Sin2pi(x);
        float c = Cos2pi(x);
        return s / c;
    }

    static float TanPi(float x)
    {
        return Tan2pi(x / 2.0f);
    }

    static float Tan(float x)
    {
        return Tan2pi(x / (2.0f * static_cast<float>(M_PI)));
    }

    static std::complex<float> Polar(float mag, double phase)
    {
        return std::complex<float>(mag * Cos(phase), mag * Sin(phase));
    }

    static std::complex<float> Polar2pi(float mag, double phase)
    {
        return std::complex<float>(mag * Cos2pi(phase), mag * Sin2pi(phase));
    }

    static float Hann(size_t index)
    {
        return 0.5f * (1.0f - s_instance.m_cosTable.m_table[index]);
    }

    static void Hann(BasicWaveTableGeneric<Bits>& table)
    {
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            float window = Hann(i);
            table.m_table[i] *= window;
        }
    }

    static std::complex<float> ComputeRectKernel(float offset)
    {
        float denominator = sinf(static_cast<float>(M_PI) * offset / static_cast<float>(x_tableSize));
        if (std::abs(denominator) < 1e-6f)
        {
            if (std::abs(offset) < 1e-6f)
            {
                return std::complex<float>(1.0f, 0.0f);
            }

            return std::complex<float>(0.0f, 0.0f);
        }

        float magnitude = sinf(static_cast<float>(M_PI) * offset) / (static_cast<float>(x_tableSize) * denominator);
        float phase = static_cast<float>(M_PI) * offset * static_cast<float>(x_tableSize - 1) / static_cast<float>(x_tableSize);
        return std::complex<float>(magnitude * cosf(phase), magnitude * sinf(phase));
    }

    static std::complex<float> ComputeHannKernel(float offset)
    {
        return 0.5f * ComputeRectKernel(offset) - 0.25f * ComputeRectKernel(offset + 1.0f) - 0.25f * ComputeRectKernel(offset - 1.0f);
    }

    static std::complex<float> HannKernel(float offset)
    {
        float pos = (offset - x_hannKernelMinOffset) * static_cast<float>(x_hannKernelTableSize) / x_hannKernelWidth;
        if (pos <= 0.0f)
        {
            return s_instance.m_hannKernel[0];
        }

        if (static_cast<float>(x_hannKernelTableSize) <= pos)
        {
            return s_instance.m_hannKernel[x_hannKernelTableSize];
        }

        size_t index = static_cast<size_t>(std::floor(pos));
        float frac = pos - static_cast<float>(index);
        return s_instance.m_hannKernel[index] * (1.0f - frac) + s_instance.m_hannKernel[index + 1] * frac;
    }
};

typedef MathGeneric<10> Math;
typedef MathGeneric<12> Math4096;

