#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

struct PitchShiftQuantizer
{
    static constexpr size_t x_numNumerators = 16;
    int m_numerators[x_numNumerators];
    size_t m_numNumerators;

    PitchShiftQuantizer()
    {
        ClearNumerators();
    }

    void ClearNumerators()
    {
        m_numNumerators = 0;
        AddNumerator(1);
    }

    void AddNumerator(int numerator)
    {
        m_numerators[m_numNumerators] = numerator;
        m_numNumerators++;
    }

    void FinalizeNumerators()
    {
        std::sort(m_numerators, m_numerators + m_numNumerators);
    }

    float QuantizeNoInvert(float pitch)
    {
        float bestQuantized = pitch;
        float bestDistance = std::numeric_limits<float>::max();
        
        float absPitch = std::abs(pitch);
        if (absPitch < 1e-8f)
        {
            return 0.0f;
        }

        float logD = std::log2(static_cast<float>(2));
        
        // Try each numerator
        //
        for (size_t i = 0; i < m_numNumerators; ++i)
        {
            float n = static_cast<float>(m_numerators[i]);
            
            // Compute ideal k: we want n/d^k ≈ pitch, so k ≈ log_d(n/pitch)
            //
            float kIdeal = std::log2(n / absPitch) / logD;
            int kCenter = static_cast<int>(std::round(kIdeal));
            
            // Try k values around the ideal
            //
            for (int kOffset = -1; kOffset <= 1; ++kOffset)
            {
                int k = kCenter + kOffset;
                float denomPower = std::pow(static_cast<float>(2), static_cast<float>(k));
                float candidate = n / denomPower;
                
                // Preserve sign of original pitch
                //
                if (pitch < 0.0f)
                {
                    candidate = -candidate;
                }

                float distance = std::abs(pitch - candidate);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestQuantized = candidate;
                }
            }
        }
        
        return bestQuantized;
    }

    float Quantize(float pitch)
    {
        float quantized = QuantizeNoInvert(pitch);
        float quantizedInvert = 1.0 / QuantizeNoInvert(1.0 / pitch);
        return std::abs(quantized - pitch) < std::abs(quantizedInvert - pitch) ? quantized : quantizedInvert;
    }
};