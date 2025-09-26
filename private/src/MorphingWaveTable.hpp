#pragma once

#include "AdaptiveWaveTable.hpp"
#include "FixedAllocator.hpp"
#include "WaveTable.hpp"

struct MorphingWaveTable
{
    const WaveTable* m_base;
    AdaptiveWaveTable* m_left;
    AdaptiveWaveTable* m_right;

    MorphingWaveTable()
        : m_base(nullptr)
        , m_left(nullptr)
        , m_right(nullptr)
    {
        m_base = &WaveTable::GetCosine();
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

    float Evaluate(float phase, float freq, float maxFreq, float baseBlend, float blend)
    {
        float base = m_base->Evaluate(phase);
        float left = m_left->Evaluate(phase, freq, maxFreq);
        float right = m_right->Evaluate(phase, freq, maxFreq);
        return base * (1 - baseBlend) + baseBlend * (left * (1 - blend) + right * blend);
    }

    float StartValue(float baseBlend, float blend) const
    {
        float base = m_base->StartValue();
        float left = m_left->StartValue();
        float right = m_right->StartValue();
        return base * (1 - baseBlend) + baseBlend * (left * (1 - blend) + right * blend);

    }

    float CenterValue(float baseBlend, float blend) const
    {
        float base = m_base->CenterValue();
        float left = m_left->CenterValue();
        float right = m_right->CenterValue();
        return base * (1 - baseBlend) + baseBlend * (left * (1 - blend) + right * blend);
    }
};

