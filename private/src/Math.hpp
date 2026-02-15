#pragma once

#include <complex>
#include <cmath>
#include "BasicWaveTable.hpp"

template<size_t Bits>
struct MathGeneric
{
    inline static MathGeneric s_instance;
    static constexpr size_t x_tableSize = BasicWaveTableGeneric<Bits>::x_tableSize;
    BasicWaveTableGeneric<Bits> m_cosTable;
    std::complex<float> m_rootOfUnity[x_tableSize];

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
};

typedef MathGeneric<10> Math;
typedef MathGeneric<12> Math4096;

