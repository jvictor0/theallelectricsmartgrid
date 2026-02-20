#pragma once

#include "AdaptiveWaveTable.hpp"
#include "Primes.hpp"

struct MultiplicativeSpectra
{
    float m_coefficients[Primes::x_max];

    MultiplicativeSpectra()
        : m_coefficients{}
    {
        for (int i = 0; i < Primes::x_max; ++i)
        {
            m_coefficients[i] = 0;
        }
    }

    void Saw()
    {
        for (int i = 1; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            m_coefficients[i] = 1.0f / i;
        }
    }

    float GetCoefficient(int n)
    {
        Primes::Iterator itr = Primes::s_primes.Begin(n);
        float result = 1;
        while (!itr.Done())
        {
            result *= m_coefficients[itr.Get()];
            itr.Next();
        }
     
        return result;
    }
};
