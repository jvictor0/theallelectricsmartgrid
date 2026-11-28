#pragma once

#include "Filter.hpp"
#include "PolyXFaderMono.hpp"
#include "PolyXFaderMorph.hpp"
#include "PolyXFaderQuad.hpp"
#include "PolyXFaderTrio.hpp"
#include "PolyXFaderWaveform.hpp"
#include "TheoryOfTimeBase.hpp"
#include "Tuning.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <tuple>
#include <type_traits>

// A snapshot of one clock domain. The multiplier ceilings depend only on
// nominal tempo and loop ratios.
//
struct PolyXFaderClockInput
{
    double m_masterPhase = 0.0;
    double m_phaseTravel = 0.0;
    std::array<double, TheoryOfTimeBase::x_numLoops> m_cycleRatios{};
    float m_minWaveMultLimit = std::numeric_limits<float>::infinity();
    std::array<float, TheoryOfTimeBase::x_numLoops> m_waveMultLimits{};

    static float LimitForRatio(double nominalBudget, int64_t ratio)
    {
        assert(ratio > 0 && nominalBudget > 0.0);
        const double limit = nominalBudget / static_cast<double>(ratio);
        float rounded = static_cast<float>(limit);
        if (static_cast<double>(rounded) > limit)
        {
            // The next lower positive float keeps the cap from rounding up.
            //
            uint32_t bits;
            static_assert(sizeof(bits) == sizeof(rounded));
            std::memcpy(&bits, &rounded, sizeof(bits));
            --bits;
            std::memcpy(&rounded, &bits, sizeof(rounded));
        }

        return rounded;
    }
};

// Crossfades the shared Theory of Time loops for Size voices at once.
//
template <size_t Size>
struct PolyXFaderGeneric
{
    static_assert(Size == 1 || Size == 3 || Size == 4);
    using Vector = MultiChannelNumber<float, Size>;
    using DoubleVector = MultiChannelNumber<double, Size>;
    using Core = std::conditional_t<Size == 1, PolyXFaderMono,
        std::conditional_t<Size == 3, PolyXFaderTrio, PolyXFaderQuad>>;
    using Input = typename Core::Input;
    static constexpr float x_maxMappedMult = 16.0f;
    static constexpr size_t x_maxSize = std::tuple_size_v<decltype(Input::m_cycleRatio)>;
    static_assert(x_maxSize == TheoryOfTimeBase::x_numLoops);
    static constexpr double x_maxSlewCycles =
        Tuning::x_maxTempoHz * TheoryOfTimeBase::x_maxNominalCycleRatio / SampleTimer::x_sampleRate;
    static constexpr double x_maxWaveCycles = x_maxSlewCycles;
    static constexpr size_t x_slewDoublings = SampleTimer::x_oversampleBits < 2 ? 2 - SampleTimer::x_oversampleBits : 0;
    static constexpr double x_slewStepScale = 1.0 / (size_t{1} << x_slewDoublings);
    static_assert(x_maxSlewCycles * x_slewStepScale <= 3125.0 / 65536.0);

    static Input MakeInput()
    {
        Input input{};
        input.m_maxSlewCycles = x_maxSlewCycles;
        input.m_slewDoublings = x_slewDoublings;
        input.m_slewStepScale = x_slewStepScale;
        input.m_amplitude = Vector::Splat(1);
        input.m_externalWeights.fill(Vector::Splat(1));
        return input;
    }

    static PolyXFaderClockInput MakeClockInput(const TheoryOfTimeBase& time, PhaseDomain phaseDomain)
    {
        PolyXFaderClockInput input;
        const auto& clock = time.GetPhaseClock(phaseDomain);
        input.m_masterPhase = clock.m_phase;

        // The warp producer uses nominal travel, avoiding feedback from its own derivative.
        //
        input.m_phaseTravel = phaseDomain == PhaseDomain::Unmodulated || !time.m_sample.m_running
            ? time.GetUnmodulatedGlobalPhaseIncrement() : std::abs(clock.m_increment);
        const double budget = x_maxWaveCycles * time.m_globalPeriodSamples;
        for (size_t loop = 0; loop < x_maxSize; ++loop)
        {
            const int64_t ratio = time.GetCycleRatio(loop);
            const float limit = PolyXFaderClockInput::LimitForRatio(budget, ratio);
            input.m_cycleRatios[loop] = static_cast<double>(ratio);
            input.m_waveMultLimits[loop] = limit;
            input.m_minWaveMultLimit = std::min(input.m_minWaveMultLimit, limit);
        }

        return input;
    }

    static Vector ComputePhase(const Input& input, const TheoryOfTimeBase& time, PhaseDomain phaseDomain, size_t loop)
    {
        const int64_t ratio = time.GetCycleRatio(loop);
        const double phase = time.GetPhaseClock(phaseDomain).m_phase * static_cast<double>(ratio);
        const float limit = PolyXFaderClockInput::LimitForRatio(x_maxWaveCycles * time.m_globalPeriodSamples, ratio);
        const Vector mult = Vector::Min(input.m_mult, Vector::Splat(limit));
        Vector result;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            result[lane] = PolyXFaderWaveform::Evaluate(
                {input.m_phaseShift[lane], mult[lane], input.m_attackFrac[lane], input.m_shape[lane]}, phase);
        }

        return result;
    }

    static Vector Shape(const Vector& shape, const Vector& in)
    {
        Vector result;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            result[lane] = PolyXFaderMorph::Evaluate({shape[lane]}, in[lane]);
        }

        return result;
    }

    typename Core::State m_dsp{};
    Vector m_amplitude = Vector::Splat(1);
    Vector m_rawOutput;
    Vector m_output;

    PolyXFaderGeneric()
    {
        m_dsp.m_holdAlpha = OPLowPassFilterGeneric<float, 1>::AlphaFromNatFreq(
            static_cast<float>(SampleTimer::HzToCycPerSamp(500.0)))[0];
    }

    // Display metadata uses the weights from the most recently processed sample.
    //
    DoubleVector ComputeLogicalPeriods(const TheoryOfTimeBase& time) const
    {
        int64_t commonCycleRatio[Size]{};
        for (size_t i = 0; i < m_dsp.m_size; ++i)
        {
            const int64_t cycleRatio = time.GetCycleRatio(i);
            for (size_t lane = 0; lane < Size; ++lane)
            {
                if (m_dsp.m_weights[i][lane] != 0)
                {
                    commonCycleRatio[lane] = std::gcd(commonCycleRatio[lane], cycleRatio);
                }
            }
        }

        DoubleVector periods;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            periods[lane] = commonCycleRatio[lane] > 0 ? 1.0 / commonCycleRatio[lane] : 1.0;
        }

        return periods;
    }

    void Process(Input& input, const TheoryOfTimeBase& time, PhaseDomain phaseDomain)
    {
        Process(input, MakeClockInput(time, phaseDomain));
    }

    [[gnu::always_inline]] void Process(Input& input, const PolyXFaderClockInput& clock)
    {
        assert(input.m_size <= x_maxSize);
        if (input.m_size != 0)
        {
            input.m_masterPhase = clock.m_masterPhase;
            input.m_phaseTravel = clock.m_phaseTravel;
            input.m_cycleRatio = clock.m_cycleRatios;
            input.m_minWaveMultLimit = clock.m_minWaveMultLimit;
            input.m_waveMultLimits = clock.m_waveMultLimits;
            m_amplitude = input.m_amplitude;
        }

        Core::Process(m_dsp, input);
        m_rawOutput = m_dsp.m_rawOutput;
        m_output = m_dsp.m_output;
    }
};

using PolyXFaderInternal = PolyXFaderGeneric<1>;
using TrioPolyXFader = PolyXFaderGeneric<3>;
