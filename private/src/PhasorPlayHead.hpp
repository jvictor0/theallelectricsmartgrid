#pragma once

#include "MultiChannelNumber.hpp"
#include "PhaseUtils.hpp"
#include "TheoryOfTime.hpp"
#include <algorithm>
#include <cmath>

// Maps each channel's Theory of Time loop phase to a normalized position in
// [0, 1): speed and length scale the phase, which wraps within the length and
// is offset by the start.
//
template <size_t Size>
struct PhasorPlayHeadGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    using DoubleVector = MultiChannelNumber<double, Size>;
    using IndexVector = MultiChannelNumber<int, Size>;

    struct Input
    {
        TheoryOfTime* m_theoryOfTime;
        Vector m_start;
        Vector m_length;
        Vector m_speed;
        IndexVector m_loopIndex;

        Input()
            : m_theoryOfTime(nullptr)
            , m_start(Vector::Splat(0))
            , m_length(Vector::Splat(1))
            , m_speed(Vector::Splat(1))
            , m_loopIndex(IndexVector::Splat(0))
        {
        }
    };

    // Channels with a non-positive length read position 0.
    //
    Vector Process(Input const& input)
    {
        if (!input.m_theoryOfTime)
        {
            return Vector::Splat(0);
        }

        TheoryOfTime* tot = input.m_theoryOfTime;

        DoubleVector phase;
        for (size_t i = 0; i < Size; ++i)
        {
            size_t loopIx = static_cast<size_t>(std::max(0, input.m_loopIndex[i]));
            phase[i] = tot->GetPhase(loopIx, PhaseDomain::Modulated);
        }

        DoubleVector length = input.m_length.template Convert<double>();
        DoubleVector loopScaled = phase * input.m_speed.template Convert<double>() * length;
        DoubleVector wrapped = loopScaled - length * (loopScaled / length).Floor() + input.m_start.template Convert<double>();
        DoubleVector fractional = wrapped - wrapped.Floor();
        DoubleVector zero = DoubleVector::Splat(0);
        return DoubleVector::IfLess(zero, length, fractional, zero).template Convert<float>();
    }
};

using PhasorPlayHead = PhasorPlayHeadGeneric<1>;
using TrioPhasorPlayHead = PhasorPlayHeadGeneric<3>;
