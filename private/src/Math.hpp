#pragma once

#include <complex>
#include <cmath>
#include "BasicWaveTable.hpp"

struct Math
{
    static Math s_instance;
    BasicWaveTable m_cosTable;
    std::complex<float> m_rootOfUnity[BasicWaveTable::x_tableSize];

    Math()
    {
        // Initialize cosine wavetable
        //
        for (size_t i = 0; i < BasicWaveTable::x_tableSize; ++i)
        {
            m_cosTable.Write(i, cosf(2.0f * M_PI * i / BasicWaveTable::x_tableSize));
        }

        // Initialize root of unity table
        //
        for (size_t i = 0; i < BasicWaveTable::x_tableSize; ++i)
        {
            m_rootOfUnity[i] = std::complex<float>(cosf(2.0f * M_PI * i / BasicWaveTable::x_tableSize), sinf(2.0f * M_PI * i / BasicWaveTable::x_tableSize));
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

    static std::complex<float> Polar(float mag, double phase)
    {
        return std::complex<float>(mag * Cos(phase), mag * Sin(phase));
    }

    static float Hann(size_t index)
    {
        return 0.5f * (1.0f - s_instance.m_cosTable.m_table[index]);
    }

    static void Hann(BasicWaveTable& table)
    {
        for (size_t i = 0; i < BasicWaveTable::x_tableSize; ++i)
        {
            float window = Hann(i);
            table.m_table[i] *= window;
        }
    }
};

inline Math Math::s_instance;

