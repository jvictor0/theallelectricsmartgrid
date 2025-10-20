#pragma once

#include "AdaptiveWaveTable.hpp"
#include "FixedAllocator.hpp"
#include "WaveTable.hpp"

struct MorphingWaveTable
{
    AdaptiveWaveTable* m_left;
    AdaptiveWaveTable* m_right;

    MorphingWaveTable()
        : m_left(nullptr)
        , m_right(nullptr)
    {
    }
    
    AdaptiveWaveTable* SetLeft(AdaptiveWaveTable* left)
    {
        AdaptiveWaveTable* old = m_left;
        m_left = left;
        return old;
    }
    
    AdaptiveWaveTable* SetRight(AdaptiveWaveTable* right)
    {
        AdaptiveWaveTable* old = m_right;
        m_right = right;
        return old;
    }

    float Evaluate(float phase, float freq, float maxFreq, float blend)
    {
        float left = m_left->Evaluate(phase, freq, maxFreq);
        float right = m_right->Evaluate(phase, freq, maxFreq);
        return left * (1 - blend) + right * blend;
    }

    float StartValue(float blend, float freq, float maxFreq)
    {
        return Evaluate(0, freq, maxFreq, blend);
    }

    float CenterValue(float blend, float freq, float maxFreq)
    {
        return Evaluate(0.5, freq, maxFreq, blend);
    }
};

