#pragma once
#include <random>

struct RGen
{
    std::default_random_engine m_gen;
    std::normal_distribution<float> m_norm;
    std::uniform_real_distribution<float> m_uni;

    RGen()
        : m_norm(0, 1)
        , m_uni(0, 1)
    {
    }
    
    float NormGen()
    {
        return m_norm(m_gen);
    }

    float UniGen()
    {
        return m_uni(m_gen);
    }

    size_t RangeGen(size_t max)
    {
        return static_cast<size_t>(UniGen() * max);
    }
};
