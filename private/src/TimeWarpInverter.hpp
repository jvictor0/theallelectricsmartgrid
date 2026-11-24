#pragma once

#include "MultiChannelNumber.hpp"
#include "PhaseUtils.hpp"
#include "TimeWarpView.hpp"
#include "../../formal/PhaseTime/generated/TimeWarpInverseRead.hpp"
#include "../../formal/PhaseTime/generated/TimeWarpInverseWrite.hpp"
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

// Each cell owns a fallback traversal and at most one replacement. Starting a
// new replacement reassigns older islands to the fallback. Keeping both fallback
// endpoints prevents a later write in the next cell from changing this traversal.
// See formal/PhaseTime/FixedCell.lean for selection and confinement proofs.
//
template <size_t MaxSamples, size_t FixedResolution = 0>
struct TimeWarpInverter : TimeWarpSource
{
    static constexpr int64_t x_gridLimit = int64_t{1} << 52;
    static_assert(MaxSamples > 0 && MaxSamples <= x_gridLimit);
    static_assert(FixedResolution == 0 || (FixedResolution & (FixedResolution - 1)) == 0);
    static constexpr size_t x_maxSamples = MaxSamples;
    static constexpr size_t x_maxCellPieces = 2;
    static constexpr double x_observationTimeLimit = 0x1p64;
    static constexpr double x_storedTimeLimit = 0x1p66;
    static constexpr float x_deltaLimit = 0x1p67f;
    static constexpr double x_missing = std::numeric_limits<double>::quiet_NaN();
    static_assert(std::atomic<double>::is_always_lock_free);
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<int64_t>::is_always_lock_free);
    static constexpr size_t x_payloadBytes = MaxSamples
        * (3 * sizeof(std::atomic<double>) + 2 * sizeof(std::atomic<float>));

    std::atomic<double> m_writeHeadInverse[MaxSamples];
    std::atomic<double> m_fallbackRightTime[MaxSamples];
    std::atomic<double> m_turnPhase[MaxSamples];
    std::atomic<float> m_turnTime[MaxSamples];
    std::atomic<float> m_currentRightTime[MaxSamples];

    // All shared accesses are sequentially consistent. Readers retry until
    // they see one unchanged even revision. Each scatter section covers at
    // most eight grid edges and their final frontier. The single
    // audio writer does not wait for readers. See PhaseTime/Concurrency.lean
    // and PhaseTime/WriteOptimization.lean.
    //
    std::atomic<uint64_t> m_revision{0};
    uint64_t m_writerRevision = 0;
    std::atomic<double> m_frontierPhase{x_missing};
    std::atomic<double> m_frontierTime{x_missing};

    // Only the first partial entry at the lowest observed cell needs a non-grid
    // left endpoint. Entering through a grid edge retires this exception.
    // The index and phase are published together under the revision counter.
    //
    std::atomic<double> m_seedPhase{x_missing};
    std::atomic<int64_t> m_seedIndex{0};
    size_t m_islandReassignments = 0;
    int64_t m_highestWrittenIndex = 0;
    bool m_hasHighest = false;
    bool m_descending = false;
    bool m_hasForward = false;

    TimeWarpInverter()
        : TimeWarpSource(MaxSamples, FixedResolution == 0 ? 1.0 : static_cast<double>(FixedResolution))
    {
        InitializeCells();
    }

    explicit TimeWarpInverter(double resolution) : TimeWarpSource(MaxSamples, resolution)
    {
        static_assert(FixedResolution == 0, "A fixed-resolution inverter takes no runtime resolution");
        // Scaling by a nonnegative power of two preserves distinct phases in
        // the supported grid range. Scatter divides by the resulting width.
        //
        [[maybe_unused]] int exponent = 0;
        assert(resolution >= 1.0 && std::frexp(resolution, &exponent) == 0.5);
        InitializeCells();
    }

    void InitializeCells()
    {
        for (size_t slot = 0; slot < MaxSamples; ++slot)
        {
            ClearCell(slot);
        }
    }

    static size_t WrapIndex(int64_t index)
    {
        return static_cast<size_t>(PhaseUtils::FloorMod(index, static_cast<int64_t>(MaxSamples)));
    }

    void BeginWrite()
    {
        assert((m_writerRevision & 1) == 0);
        m_revision.store(++m_writerRevision);
    }

    void EndWrite()
    {
        assert((m_writerRevision & 1) != 0);
        m_revision.store(++m_writerRevision);
    }

    void ClearCell(size_t slot)
    {
        m_writeHeadInverse[slot].store(x_missing);
        m_fallbackRightTime[slot].store(x_missing);
        m_turnPhase[slot].store(x_missing);
        m_turnTime[slot].store(0.0f);
        m_currentRightTime[slot].store(static_cast<float>(x_missing));
    }

    template<typename Value>
    struct OptionalField
    {
        bool m_present;
        Value m_value;
    };

    struct Point
    {
        double m_phase;
        double m_time;
    };

    struct SeedEntry
    {
        double m_phase;
        int64_t m_index;
    };

    static int64_t CapacityMinusOne()
    {
        return static_cast<int64_t>(MaxSamples - 1);
    }

    constexpr double GridResolution() const
    {
        if constexpr (FixedResolution != 0)
        {
            return static_cast<double>(FixedResolution);
        }
        else
        {
            return m_resolution;
        }
    }

    static bool SameSlot(int64_t a, int64_t b)
    {
        return WrapIndex(a) == WrapIndex(b);
    }

    static OptionalField<double> FiniteField(double value)
    {
        return {std::isfinite(value), value};
    }

    OptionalField<double> LoadBase(int64_t index) const
    {
        return FiniteField(m_writeHeadInverse[WrapIndex(index)].load());
    }

    OptionalField<double> LoadFallback(int64_t index) const
    {
        return FiniteField(m_fallbackRightTime[WrapIndex(index)].load());
    }

    OptionalField<double> LoadCut(int64_t index) const
    {
        return FiniteField(m_turnPhase[WrapIndex(index)].load());
    }

    OptionalField<float> LoadStartDelta(int64_t index) const
    {
        return {true, m_turnTime[WrapIndex(index)].load()};
    }

    OptionalField<float> LoadEndDelta(int64_t index) const
    {
        float value = m_currentRightTime[WrapIndex(index)].load();
        return {std::isfinite(value), value};
    }

    OptionalField<SeedEntry> LoadSeed() const
    {
        const double phase = m_seedPhase.load();
        if (std::isfinite(phase))
        {
            return {true, {phase, m_seedIndex.load()}};
        }

        return {false, {}};
    }

    OptionalField<Point> LoadFrontier() const
    {
        double phase = m_frontierPhase.load();
        double time = m_frontierTime.load();
        return {std::isfinite(time), {phase, time}};
    }

    OptionalField<Point> LoadPrevious() const
    {
        return {m_hasObservation, {m_observedWarpedTime, m_observedRealTime}};
    }

    OptionalField<int64_t> LoadHighest() const
    {
        return {m_hasHighest, m_highestWrittenIndex};
    }

    bool HasForward() const
    {
        return m_hasForward;
    }

    bool IsDescending() const
    {
        return m_descending;
    }

    void ClearSeed()
    {
        m_seedPhase.store(x_missing);
    }

    void StoreSeed(double phase, int64_t index)
    {
        assert(GridPhaseInRange(phase));
        assert(static_cast<double>(index) == std::floor(phase));
        m_seedIndex.store(index);
        m_seedPhase.store(phase);
    }

    void StoreFrontier(double phase, double time)
    {
        assert(std::abs(time) <= x_storedTimeLimit);
        m_frontierPhase.store(phase);
        m_frontierTime.store(time);
    }

    void StoreObservation(double phase, double time)
    {
        m_observedWarpedTime = phase;
        m_observedRealTime = time;
        m_hasObservation = true;
    }

    void StorePreviousObservation(double phase, double time)
    {
        m_previousObservedWarpedTime = phase;
        m_previousObservedRealTime = time;
    }

    void StoreHighest(int64_t index)
    {
        m_highestWrittenIndex = index;
        m_hasHighest = true;
    }

    void StoreOldest(double phase)
    {
        m_oldestWarpedTime = phase;
    }

    void StoreDirection(bool forward, bool descending)
    {
        m_hasForward = forward;
        m_descending = descending;
    }

    void MarkDescending()
    {
        m_descending = true;
    }

    void IncrementIslands()
    {
        ++m_islandReassignments;
    }

    void ClearIndex(int64_t index)
    {
        ClearCell(WrapIndex(index));
    }

    // The cut is the presence tag for the relative timestamps.
    //
    void PutCell(int64_t index, double time, bool closed, double stop)
    {
        assert(std::abs(time) <= x_storedTimeLimit);
        assert(!closed || std::abs(stop) <= x_storedTimeLimit);
        size_t slot = WrapIndex(index);
        m_writeHeadInverse[slot].store(time);
        m_fallbackRightTime[slot].store(closed ? stop : x_missing);
        m_turnPhase[slot].store(x_missing);
    }

    void PutFallback(int64_t index, double time)
    {
        assert(std::abs(time) <= x_storedTimeLimit);
        m_fallbackRightTime[WrapIndex(index)].store(time);
    }

    void PutReplacement(int64_t index, double phase, float start)
    {
        assert(std::abs(start) <= x_deltaLimit);
        size_t slot = WrapIndex(index);
        m_turnPhase[slot].store(phase);
        m_turnTime[slot].store(start);
        m_currentRightTime[slot].store(static_cast<float>(x_missing));
    }

    void PutEndDelta(int64_t index, bool closed, float stop)
    {
        assert(!closed || std::abs(stop) <= x_deltaLimit);
        m_currentRightTime[WrapIndex(index)].store(closed ? stop : static_cast<float>(x_missing));
    }

    void Write(double warpedTime, double realTime)
    {
        // The generated writer uses these bounds for its native index arithmetic.
        //
        assert(GridPhaseInRange(warpedTime * m_resolution));
        assert(std::abs(realTime) <= x_observationTimeLimit);
        assert(GridPhaseInRange(m_observedWarpedTime * m_resolution));
        assert(!(m_observedWarpedTime < warpedTime)
            || warpedTime * m_resolution - m_observedWarpedTime * m_resolution > 0.0);
        assert(m_highestWrittenIndex >= -x_gridLimit && m_highestWrittenIndex <= x_gridLimit);
        TimeWarpInverseWrite::Process(*this, warpedTime, realTime);
    }

    static bool GridPhaseInRange(double phase)
    {
        return -static_cast<double>(x_gridLimit) <= phase && phase <= static_cast<double>(x_gridLimit);
    }

    double ReadInverse(double warpedTime, double latestRealTime) const override
    {
        const double gridPhase = warpedTime * GridResolution();
        assert(GridPhaseInRange(gridPhase));
        struct Input
        {
            double m_phase;
            double m_cutoff;
            double m_missing;
        };

        for (;;)
        {
            uint64_t revision = m_revision.load();
            if (revision & 1)
            {
                continue;
            }

            double result = TimeWarpInverseRead::Process(*this,
                Input{gridPhase, latestRealTime, x_missing});
            if (m_revision.load() == revision)
            {
                return result;
            }
        }
    }

    double GetRealTime(double warpedTime) const
    {
        return ReadInverse(warpedTime, std::numeric_limits<double>::infinity());
    }

    template <size_t Size>
    MultiChannelNumber<double, Size> GetRealTime(const MultiChannelNumber<double, Size>& warpedTime)
    {
        MultiChannelNumber<double, Size> result;
        for (size_t i = 0; i < Size; ++i)
        {
            result[i] = GetRealTime(warpedTime[i]);
        }

        return result;
    }
};
