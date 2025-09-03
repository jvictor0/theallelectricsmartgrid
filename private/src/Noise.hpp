#pragma once

#include <random>
#include <cmath>

// White noise generator using Box-Muller transform for Gaussian distribution
//
struct WhiteNoise
{
    WhiteNoise(unsigned int seed = 0)
        : m_generator(seed)
        , m_distribution(0.0, 1.0)
    {
    }

    double Generate()
    {
        double u1 = m_distribution(m_generator);
        double u2 = m_distribution(m_generator);
        
        // Box-Muller transform
        //
        double z0 = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
        
        // Normalize to [-1, 1] range using zero normalization
        // Scale by a factor to fit most of the Gaussian distribution within bounds
        // Using 3.0 as scaling factor covers ~99.7% of the distribution
        //
        z0 = z0 / 3.0;
        
        return z0;
    }

    void SetSeed(unsigned int seed)
    {
        m_generator.seed(seed);
    }

    std::mt19937 m_generator;
    std::uniform_real_distribution<double> m_distribution;
};

// Pink noise generator using Voss-McCartney algorithm
//
struct PinkNoise
{
    PinkNoise(unsigned int seed = 0)
        : m_generator(seed)
        , m_distribution(0.0, 1.0)
        , m_whiteValues()
        , m_pinkValue(0.0)
        , m_key(0)
        , m_maxKey(0x7f)
    {
        // Initialize white noise values
        //
        for (int i = 0; i < 7; ++i)
        {
            m_whiteValues[i] = m_distribution(m_generator);
        }
        
        m_pinkValue = 0.0;
        m_key = 0;
    }

    double Generate()
    {
        int lastKey = m_key;
        
        m_key++;
        if (m_key > m_maxKey)
        {
            m_key = 1;
        }
        
        int diff = lastKey ^ m_key;
        double sum = 0.0;
        
        for (int i = 0; i < 7; ++i)
        {
            if (diff & (1 << i))
            {
                m_whiteValues[i] = m_distribution(m_generator);
            }
            sum += m_whiteValues[i];
        }
        
        // Normalize to [-1, 1] range with proper centering
        // Raw sum has mean 3.5 and range [0, 7]
        // Center by subtracting mean, then scale to [-1, 1]
        //
        m_pinkValue = ((sum - 3.5) / 3.5);
        return m_pinkValue;
    }

    void SetSeed(unsigned int seed)
    {
        m_generator.seed(seed);
        m_key = 0;
        // Reinitialize white noise values
        //
        for (int i = 0; i < 7; ++i)
        {
            m_whiteValues[i] = m_distribution(m_generator);
        }
    }

    std::mt19937 m_generator;
    std::uniform_real_distribution<double> m_distribution;
    double m_whiteValues[7];
    double m_pinkValue;
    int m_key;
    int m_maxKey;
}; 