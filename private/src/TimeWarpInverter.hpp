#pragma once

#include "MultiChannelNumber.hpp"
#include "PhaseUtils.hpp"
#include "TimeWarpView.hpp"
#include "../../formal/PhaseTime/generated/TimeWarpInverseCore.hpp"
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

// Each cell owns a fallback traversal and at most one replacement. Starting a
// new replacement reassigns older islands to the fallback. Keeping both fallback
// endpoints prevents a later write in the next cell from changing this traversal.
// See formal/PhaseTime/FixedCell.lean for selection and confinement proofs.
//
template <size_t MaxSamples>
struct TimeWarpInverter : TimeWarpSource
{
    static constexpr size_t x_maxSamples = MaxSamples;
    static constexpr size_t x_maxCellPieces = 2;
    using Core = TimeWarpInverseCore<TimeWarpInverter>;
    static constexpr double x_missing = std::numeric_limits<double>::quiet_NaN();
    static_assert(std::atomic<double>::is_always_lock_free);
    static_assert(std::atomic<float>::is_always_lock_free);
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
    //
    std::atomic<double> m_seedPhase{x_missing};
    size_t m_islandReassignments = 0;
    double m_highestWrittenIndex = -std::numeric_limits<double>::infinity();
    double m_lastWarpedTime = 0.0;
    double m_previousRealTime = 0.0;
    bool m_descending = false;
    bool m_hasForward = false;

    TimeWarpInverter() : TimeWarpInverter(1.0)
    {
    }

    explicit TimeWarpInverter(double resolution) : TimeWarpSource(MaxSamples, resolution)
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
        m_turnTime[slot].store(static_cast<float>(x_missing));
        m_currentRightTime[slot].store(static_cast<float>(x_missing));
    }

    void RetireSeed(size_t slot)
    {
        double seed = m_seedPhase.load();
        if (std::isfinite(seed) && WrapIndex(static_cast<int64_t>(std::floor(seed))) == slot)
        {
            m_seedPhase.store(x_missing);
        }
    }

    // A missing cut makes both relative timestamps unreadable. A later branch
    // initializes them before publishing its cut, so stale values need no clear.
    //
    void EnterCell(int64_t index, double time)
    {
        size_t slot = WrapIndex(index);
        RetireSeed(slot);
        m_writeHeadInverse[slot].store(time);
        m_fallbackRightTime[slot].store(x_missing);
        m_turnPhase[slot].store(x_missing);
    }

    // Equivalent to EnterCell followed by FinishCell, including seed retirement.
    // The endpoints are prepared with the original interpolation expression.
    //
    void CompleteCell(int64_t index, double leftTime, double rightTime)
    {
        size_t slot = WrapIndex(index);
        RetireSeed(slot);
        m_writeHeadInverse[slot].store(leftTime);
        m_fallbackRightTime[slot].store(rightTime);
        m_turnPhase[slot].store(x_missing);
    }

    void SeedCell(double phase, double time)
    {
        double seed = m_seedPhase.load();
        if (std::isfinite(seed))
        {
            ClearCell(WrapIndex(static_cast<int64_t>(std::floor(seed))));
        }

        EnterCell(static_cast<int64_t>(std::floor(phase)), time);
        m_seedPhase.store(phase);
    }

    // The first complete traversal becomes the fallback. Tiny reversals before
    // that exit may change the selected time, but every intervening sample is
    // confined to this cell. Later exits complete only the replacement.
    //
    void FinishCell(int64_t index, double time)
    {
        size_t slot = WrapIndex(index);
        double base = m_writeHeadInverse[slot].load();
        assert(std::isfinite(base));
        if (!std::isfinite(m_fallbackRightTime[slot].load()))
        {
            m_fallbackRightTime[slot].store(time);
        }
        else
        {
            assert(std::isfinite(m_turnPhase[slot].load()));
            m_currentRightTime[slot].store(static_cast<float>(time - base));
        }
    }

    void Write(double warpedTime, double realTime)
    {
        Core::Write(*this, warpedTime, realTime);
    }

    double ReadInverse(double warpedTime, double latestRealTime) const override
    {
        assert(std::isfinite(warpedTime));
        for (;;)
        {
            uint64_t revision = m_revision.load();
            if (revision & 1)
            {
                continue;
            }

            double result = Core::ReadPayload(*this, warpedTime * m_resolution, latestRealTime);
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
