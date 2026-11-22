#pragma once

#include "MultiChannelBool.hpp"

template<size_t Size>
struct SampleTopGeneric
{
    using OffsetVector = MultiChannelNumber<double, Size>;

    MultiChannelBool<Size> m_triggered;
    // Offset in engine samples from the sample carrying this event.
    //
    OffsetVector m_offset;

    template<class Number>
    static SampleTopGeneric Advance(MultiChannelNumber<Number, Size>& phase, const MultiChannelNumber<Number, Size>& frequency)
    {
        using PhaseVector = MultiChannelNumber<Number, Size>;
        const auto next = phase + frequency;
        const auto wrapped = next - next.Floor();
        const auto crossed = next.Load() >= static_cast<Number>(1);
        phase = PhaseVector::FromVector(crossed ? wrapped.Load() : next.Load());
        return Interpolate(crossed, -wrapped.template Convert<double>(), frequency.template Convert<double>());
    }

    static SampleTopGeneric FromPhases(const OffsetVector& previous, const OffsetVector& current)
    {
        const auto previousCycle = previous.Floor();
        const auto currentCycle = current.Floor();
        const auto crossed = currentCycle.Load() != previousCycle.Load();
        const auto boundary = currentCycle + OffsetVector::IfLess(current, previous, OffsetVector::Splat(1), OffsetVector{});
        return Interpolate(crossed, boundary - current, current - previous);
    }

    template<class ComparisonVector>
    static SampleTopGeneric Interpolate(ComparisonVector crossed, const OffsetVector& distance, const OffsetVector& increment)
    {
        SampleTopGeneric result{MultiChannelBool<Size>::FromComparison(crossed), {}};
        if (result.m_triggered.ToBits() != 0)
        {
            using Comparison = decltype(distance.Load() != 0.0);
            const auto event = __builtin_convertvector(crossed, Comparison);
            const auto numerator = event ? distance.Load() : typename OffsetVector::Vector{};
            const auto denominator = event ? increment.Load() : OffsetVector::Splat(1).Load();
            result.m_offset = OffsetVector::FromVector(numerator / denominator);
        }

        return result;
    }
};

using SampleTop = SampleTopGeneric<1>;
