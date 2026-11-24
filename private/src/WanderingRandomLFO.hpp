#pragma once

#include "MultiChannelNumber.hpp"
#include "WanderingRandomLFO32.hpp"
#include "WanderingRandomLFO64.hpp"
#include "WanderingRandomLFOFramed32.hpp"
#include "WanderingRandomLFOFramed64.hpp"
#include "WanderingRandomLFOFramedInit32.hpp"
#include "WanderingRandomLFOFramedInit64.hpp"
#include "WanderingRandomLFOFramedState.hpp"
#include "WanderingRandomLFOInit32.hpp"
#include "WanderingRandomLFOInit64.hpp"
#include "WanderingRandomLFOState.hpp"
#include <cstdint>
#include <type_traits>

template<typename Number, bool Framed>
struct WanderingRandomLFOGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);

    using State = std::conditional_t<Framed, WanderingRandomLFOFramedState<Number>,
        WanderingRandomLFOState<Number>>;
    using Kernel = std::conditional_t<Framed,
        std::conditional_t<std::is_same_v<Number, float>, WanderingRandomLFOFramed32, WanderingRandomLFOFramed64>,
        std::conditional_t<std::is_same_v<Number, float>, WanderingRandomLFO32, WanderingRandomLFO64>>;
    using Initializer = std::conditional_t<Framed,
        std::conditional_t<std::is_same_v<Number, float>, WanderingRandomLFOFramedInit32, WanderingRandomLFOFramedInit64>,
        std::conditional_t<std::is_same_v<Number, float>, WanderingRandomLFOInit32, WanderingRandomLFOInit64>>;

    State m_state{};

    WanderingRandomLFOGeneric() = default;

    WanderingRandomLFOGeneric(double lambdaSamples, std::uint64_t seed)
    {
        Initialize(lambdaSamples, seed);
    }

    void Initialize(double lambdaSamples, std::uint64_t seed)
    {
        Initializer::Process(m_state, {lambdaSamples, seed});
    }

    Number Process()
    {
        return Kernel::Process(m_state);
    }
};

template<typename Number, size_t Size, bool Framed>
struct PackedWanderingRandomLFO : WanderingRandomLFOGeneric<Number, Framed>
{
    MultiChannelNumber<Number, Size> NextSpline()
    {
        return MultiChannelNumber<Number, Size>::Splat(this->Process());
    }
};
