#pragma once

#include "SampledTransferFunction.hpp"
#include <type_traits>

// Adapt a coefficient-only generated record and a selected static power kernel
// to the existing sampled history. No complex response or virtual snapshot is
// required. Evaluation uses the generated filter's chosen arithmetic precision.
//
template<typename Filter, auto MagnitudeSquared>
struct GeneratedResponseSnapshot : Filter::Snapshot
{
    using Coefficients = typename Filter::Snapshot;
    using Frequency = ResponseFrequencyGeneric<MultiChannelNumber<double, Filter::x_numChannels>>;
    using ResponseVector = MultiChannelNumber<float, Filter::x_numChannels>;
    static constexpr size_t x_numLanes = Filter::x_numChannels;

    GeneratedResponseSnapshot() = default;

    GeneratedResponseSnapshot(const Coefficients& coefficients)
        : Coefficients(coefficients)
    {
    }

    template<typename Value>
    static Value FrequencyValue(const MultiChannelNumber<double, x_numLanes>& source)
    {
        if constexpr (std::is_arithmetic_v<Value>)
        {
            static_assert(x_numLanes == 1);
            return static_cast<Value>(source[0]);
        }
        else
        {
            return source.template Convert<typename Filter::NumberType>();
        }
    }

    ResponseVector MagnitudeSquaredResponses(const Frequency& frequency) const
    {
        using Value = decltype(typename Filter::Frequency{}.m_sinHalf);
        const typename Filter::Frequency prepared{
            FrequencyValue<Value>(frequency.m_cyclesPerSample),
            FrequencyValue<Value>(frequency.m_sinHalf),
            FrequencyValue<Value>(frequency.m_cosHalf),
            {FrequencyValue<Value>(frequency.m_zInverse.m_real), FrequencyValue<Value>(frequency.m_zInverse.m_imag)}};
        const auto power = MagnitudeSquared(static_cast<const Coefficients&>(*this), prepared);
        if constexpr (std::is_arithmetic_v<decltype(power)>)
        {
            return ResponseVector::Splat(static_cast<float>(power));
        }
        else
        {
            return power.template Convert<float>();
        }
    }

    float MagnitudeSquaredResponse(const ResponseFrequency& frequency, size_t lane) const
    {
        assert(lane < x_numLanes);
        return MagnitudeSquaredResponses(frequency.template Broadcast<MultiChannelNumber<double, x_numLanes>>())[lane];
    }
};

// Adapt a generated processor to a composable response snapshot. Its audio
// outputs are determined by its Lean export; choosing a response here changes
// only the UI view. Handwritten machines can own child snapshots.
//
template<typename Filter, auto MagnitudeSquared>
struct GeneratedFilterView : Filter
{
    using Snapshot = GeneratedResponseSnapshot<Filter, MagnitudeSquared>;
    using UIState = TransferFunctionGeneric<Snapshot>;

    Snapshot MakeSnapshot() const
    {
        return Filter::MakeSnapshot();
    }

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }
};
