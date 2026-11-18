#pragma once

#include "MultiChannelComplex.hpp"
#include "Tuning.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <type_traits>

template<typename Value>
struct ResponseFrequencyTypes
{
    using Complex = MultiChannelComplex<Value, 1>;
};

template<typename Number, size_t Size>
struct ResponseFrequencyTypes<MultiChannelNumber<Number, Size>>
{
    using Complex = MultiChannelComplex<Number, Size>;
};

// Prepared frequency data shared by handwritten and generated responses.
// Value is normally a MultiChannelNumber; scalar instantiations also support
// native codegen comparisons. The layout mirrors DSP.ResponseFrequency.
//
template<typename Value>
struct ResponseFrequencyGeneric
{
    using Complex = typename ResponseFrequencyTypes<Value>::Complex;

    Value m_cyclesPerSample;
    Value m_sinHalf;
    Value m_cosHalf;
    Complex m_zInverse;

    template<typename Target>
    static Target BroadcastValue(double value)
    {
        if constexpr (std::is_arithmetic_v<Target>)
        {
            return static_cast<Target>(value);
        }
        else
        {
            return Target::Splat(value);
        }
    }

    static ResponseFrequencyGeneric CyclesPerSample(double frequency)
    {
        assert(std::isfinite(frequency) && frequency >= 0 && frequency <= 0.5);
        double s;
        double c;
        if (frequency == 0)
        {
            s = 0;
            c = 1;
        }
        else if (frequency == 0.5)
        {
            s = 1;
            c = 0;
        }
        else
        {
            const auto halfPhase = MultiChannelNumber<double, 1>::Splat(frequency * 0.5);
            s = Math4096::Sin2pi(halfPhase)[0];
            c = Math4096::Cos2pi(halfPhase)[0];
        }

        return {BroadcastValue<Value>(frequency), BroadcastValue<Value>(s), BroadcastValue<Value>(c),
            {BroadcastValue<Value>(1 - 2 * s * s), BroadcastValue<Value>(frequency == 0 || frequency == 0.5 ? 0 : -2 * s * c)}};
    }

    static ResponseFrequencyGeneric Hz(double frequency, double sampleRate)
    {
        assert(sampleRate > 0);
        return CyclesPerSample(std::clamp(frequency / sampleRate, 0.0, 0.5));
    }

    static ResponseFrequencyGeneric VOct(double frequency, double sampleRate)
    {
        return Hz(Tuning::x_middleCHz * std::exp2(frequency), sampleRate);
    }

    // Widen a shared one-lane frequency once, before evaluating a snapshot cohort.
    //
    template<typename Target>
    ResponseFrequencyGeneric<Target> Broadcast() const
    {
        return {BroadcastValue<Target>(m_cyclesPerSample), BroadcastValue<Target>(m_sinHalf),
            BroadcastValue<Target>(m_cosHalf), {BroadcastValue<Target>(m_zInverse.m_real),
            BroadcastValue<Target>(m_zInverse.m_imag)}};
    }

    ResponseFrequencyGeneric<MultiChannelNumber<double, 1>> Channel(size_t lane) const
    {
        assert(lane < Value::x_numChannels);
        return {m_cyclesPerSample[lane], m_sinHalf[lane], m_cosHalf[lane],
            {m_zInverse.m_real[lane], m_zInverse.m_imag[lane]}};
    }
};

using ResponseFrequency = ResponseFrequencyGeneric<MultiChannelNumber<double, 1>>;
