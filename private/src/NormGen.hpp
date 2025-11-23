#pragma once
#include <random>

struct RGen
{
    static std::random_device s_rd;
    static std::mt19937 s_gen;
    std::normal_distribution<float> m_norm;
    std::uniform_real_distribution<float> m_uni;

    RGen()
        : m_norm(0, 1)
        , m_uni(0, 1)
    {
    }
    
    float NormGen()
    {
        return m_norm(s_gen);
    }

    float NormGen(float mu, float sigma)
    {
        return m_norm(s_gen) * sigma + mu;
    }

    float UniGen()
    {
        return m_uni(s_gen);
    }

    float UniGenRange(float min, float max)
    {
        return UniGen() * (max - min) + min;
    }

    size_t RangeGen(size_t max)
    {
        return static_cast<size_t>(UniGen() * max);
    }
};

inline std::random_device RGen::s_rd;
inline std::mt19937 RGen::s_gen(s_rd());