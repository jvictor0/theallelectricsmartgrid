#pragma once

#include "SquiggleBoyQuadReverbEverySample.hpp"
#include "SquiggleBoyQuadReverbFramed.hpp"
#include "SquiggleBoyQuadReverbInitEverySample.hpp"
#include "SquiggleBoyQuadReverbInitFramed.hpp"
#include "SquiggleBoyQuadReverbTuning.hpp"
#include "TPTBaseWidthGeneric.hpp"
#include <cassert>
#include <type_traits>

template<typename Number, bool Framed = true>
struct SquiggleBoyQuadReverb
{
    using Vector = MultiChannelNumber<Number, 4>;
    using ProcessKernel = std::conditional_t<Framed, SquiggleBoyQuadReverbFramed<Number, 4>,
        SquiggleBoyQuadReverbEverySample<Number, 4>>;
    using Initialize = std::conditional_t<Framed, SquiggleBoyQuadReverbInitFramed<Number, 4>,
        SquiggleBoyQuadReverbInitEverySample<Number, 4>>;
    using Tune = SquiggleBoyQuadReverbTuning<Number, 4>;
    using State = typename ProcessKernel::State;
    using LogicalSpec = typename Tune::Input;
    using DampingFilter = TPTBaseWidthGeneric<Number, 4>;
    using DampingUIState = typename DampingFilter::UIState;

    static constexpr size_t x_delayCapacity = decltype(State::m_delay)::x_capacity;

    State m_state{};

    explicit SquiggleBoyQuadReverb(const LogicalSpec& spec)
    {
        CheckSpec(spec);
        Initialize::Process(m_state, spec);
    }

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        CheckSpec(spec);
        Tune::Process(m_state, spec);
    }

    Vector Process(const Vector& input)
    {
        return ProcessKernel::Process(m_state, typename ProcessKernel::Input{input});
    }

    typename DampingFilter::Snapshot MakeDampingSnapshot() const
    {
        const auto& snapshot = m_state.m_damping.m_snapshot;
        return {{snapshot.m_highPass.m_g}, {snapshot.m_lowPass.m_g}};
    }

    static void CheckSpec([[maybe_unused]] const LogicalSpec& spec)
    {
#ifndef NDEBUG
        for (size_t lane = 0; lane < Vector::x_numChannels; ++lane)
        {
            assert(spec.m_delaySamples[lane] > 0);
        }
#endif
    }
};
