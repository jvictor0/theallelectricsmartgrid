#pragma once

#include "Math.hpp"
#include "MultiChannelNumber.hpp"
#include "SampleTop.hpp"

template <size_t Size>
struct VCOGeneric
{
    using Vector = MultiChannelNumber<float, Size>;

    Vector m_output;
    Vector m_phase;
    SampleTop m_top[Size];

    VCOGeneric()
        : m_output(Vector::Splat(0))
        , m_phase(Vector::Splat(0))
    {
    }

    Vector Process(const Vector& freq)
    {
        m_phase += freq;
        Vector wrapped = m_phase - m_phase.Floor();
        for (size_t i = 0; i < Size; ++i)
        {
            m_top[i] = false;
            if (1 <= m_phase[i])
            {
                m_top[i] = SampleTop::FromWrap(wrapped[i], freq[i]);
            }
        }

        m_phase = Vector::IfLess(m_phase, Vector::Splat(1), m_phase, wrapped);
        m_output = Math::Sin2pi(m_phase);
        return m_output;
    }
};

using VCO = VCOGeneric<1>;
using TrioVCO = VCOGeneric<3>;
