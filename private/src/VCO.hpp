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
    SampleTopGeneric<Size> m_top;

    VCOGeneric()
        : m_output(Vector::Splat(0))
        , m_phase(Vector::Splat(0))
    {
    }

    Vector Process(const Vector& freq)
    {
        m_top = SampleTopGeneric<Size>::Advance(m_phase, freq);
        m_output = Math::Sin2pi(m_phase);
        return m_output;
    }
};

using VCO = VCOGeneric<1>;
using TrioVCO = VCOGeneric<3>;
