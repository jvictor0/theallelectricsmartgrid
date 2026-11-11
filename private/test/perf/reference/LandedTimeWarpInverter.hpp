// Frozen landed inverse at de51539eb16f929321c7a3dec275be5b051424ac.
// Only the type name and this provenance comment differ from production.
//
#pragma once

#include "MultiChannelNumber.hpp"
#include "PhaseUtils.hpp"
#include "TimeWarpView.hpp"
#include <algorithm>
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
struct LandedTimeWarpInverter : TimeWarpSource
{
    static constexpr size_t x_maxSamples = MaxSamples;
    static constexpr size_t x_maxCellPieces = 2;
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
    // they see one unchanged even revision. Each write section covers one
    // cell transition and its frontier, never the scatter loop. The single
    // audio writer does not wait for readers. See PhaseTime/Concurrency.lean.
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

    LandedTimeWarpInverter() : LandedTimeWarpInverter(1.0)
    {
    }

    explicit LandedTimeWarpInverter(double resolution) : TimeWarpSource(MaxSamples, resolution)
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

    void EnterCell(int64_t index, double time)
    {
        size_t slot = WrapIndex(index);
        double seed = m_seedPhase.load();
        if (std::isfinite(seed) && WrapIndex(static_cast<int64_t>(std::floor(seed))) == slot)
        {
            m_seedPhase.store(x_missing);
        }

        ClearCell(slot);
        m_writeHeadInverse[slot].store(time);
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

    // Keep the earliest minimum during a replacement visit confined to this
    // cell, including touches of its right edge. A visit after leaving the cell
    // replaces the cut and can reassign older islands to the fixed fallback.
    // An unfinished first traversal uses its entry and forward frontier.
    //
    void BeginBranch(double phase, double time)
    {
        double floorPhase = std::floor(phase);
        if (phase == floorPhase)
        {
            return;
        }

        size_t slot = WrapIndex(static_cast<int64_t>(floorPhase));
        double seed = m_seedPhase.load();
        bool retained = m_highestWrittenIndex - static_cast<double>(MaxSamples - 1) <= floorPhase
            && floorPhase <= m_highestWrittenIndex;
        BeginWrite();
        if (!retained || !std::isfinite(m_writeHeadInverse[slot].load())
            || (std::floor(seed) == floorPhase && phase < seed))
        {
            SeedCell(phase, time);
        }
        else if (std::isfinite(m_fallbackRightTime[slot].load()))
        {
            double oldTurn = m_turnPhase[slot].load();
            bool sameVisit = std::isfinite(oldTurn) && m_frontierPhase.load() <= floorPhase + 1.0;
            if (!sameVisit || phase < oldTurn)
            {
                m_islandReassignments += std::isfinite(oldTurn) && oldTurn < phase;
                double base = m_writeHeadInverse[slot].load();
                m_turnPhase[slot].store(phase);
                m_turnTime[slot].store(static_cast<float>(time - base));
            }

            // A boundary touch may have stored an endpoint. Reopen against the
            // live frontier when this confined visit resumes below that edge.
            //
            m_currentRightTime[slot].store(static_cast<float>(x_missing));
        }

        m_frontierPhase.store(phase);
        m_frontierTime.store(time);
        EndWrite();
    }

    // Caller holds the writer revision or validates it after reading. Fallback
    // and replacement never borrow an endpoint from the neighboring cell.
    //
    double ReadPayload(double gridPhase, double cutoff) const
    {
        double floorPhase = std::floor(gridPhase);
        size_t slot = WrapIndex(static_cast<int64_t>(floorPhase));
        double base = m_writeHeadInverse[slot].load();
        double seed = m_seedPhase.load();
        double lowPhase = std::floor(seed) == floorPhase ? seed : floorPhase;
        double lowTime = base;
        double highPhase = floorPhase + 1.0;
        double highTime = m_fallbackRightTime[slot].load();
        double frontierPhase = m_frontierPhase.load();
        double frontierTime = m_frontierTime.load();
        if (gridPhase == frontierPhase && std::isfinite(frontierTime))
        {
            return frontierTime <= cutoff ? frontierTime : x_missing;
        }

        double turn = m_turnPhase[slot].load();
        if (std::isfinite(turn) && gridPhase >= turn)
        {
            lowPhase = turn;
            lowTime = base + static_cast<double>(m_turnTime[slot].load());
            highTime = base + static_cast<double>(m_currentRightTime[slot].load());
        }

        if (!std::isfinite(highTime) && lowPhase <= frontierPhase && frontierPhase < highPhase)
        {
            highPhase = frontierPhase;
            highTime = frontierTime;
        }

        if (!std::isfinite(lowTime) || lowTime > cutoff || gridPhase < lowPhase)
        {
            return x_missing;
        }

        if (gridPhase == lowPhase)
        {
            return lowTime;
        }

        if (!std::isfinite(highTime) || gridPhase > highPhase || lowPhase >= highPhase)
        {
            return x_missing;
        }

        double alpha = (gridPhase - lowPhase) / (highPhase - lowPhase);
        double time = lowTime + alpha * (highTime - lowTime);
        return time <= cutoff ? time : x_missing;
    }

    void ScatterBufferToInverse(double low, double high, double lowTime, double highTime)
    {
        int64_t first = static_cast<int64_t>(std::ceil(low));
        int64_t last = static_cast<int64_t>(std::floor(high));
        for (int64_t index = first; index <= last; ++index)
        {
            double alpha = (static_cast<double>(index) - low) / (high - low);
            double time = lowTime + alpha * (highTime - lowTime);
            BeginWrite();
            if (static_cast<double>(index) > low)
            {
                FinishCell(index - 1, time);
            }

            EnterCell(index, time);
            m_frontierPhase.store(static_cast<double>(index));
            m_frontierTime.store(time);
            EndWrite();
        }

        m_highestWrittenIndex = std::max(m_highestWrittenIndex, std::floor(high));
        m_oldestWarpedTime = (m_highestWrittenIndex - static_cast<double>(MaxSamples - 1)) / m_resolution;
    }

    void Write(double warpedTime, double realTime)
    {
        assert(std::isfinite(warpedTime) && std::isfinite(realTime));
        bool previousObservation = m_hasObservation;
        m_previousObservedRealTime = previousObservation ? m_observedRealTime : realTime;
        m_previousObservedWarpedTime = previousObservation ? m_observedWarpedTime : warpedTime;
        m_observedWarpedTime = warpedTime;
        m_observedRealTime = realTime;
        m_hasObservation = true;
        if (previousObservation && warpedTime > m_lastWarpedTime)
        {
            double low = m_lastWarpedTime * m_resolution;
            double high = warpedTime * m_resolution;
            if (!m_hasForward || m_descending)
            {
                BeginBranch(low, m_previousRealTime);
            }

            ScatterBufferToInverse(low, high, m_previousRealTime, realTime);
            BeginWrite();
            m_frontierPhase.store(high);
            m_frontierTime.store(realTime);
            EndWrite();
            m_hasForward = true;
            m_descending = false;
        }
        else if (previousObservation && warpedTime < m_lastWarpedTime)
        {
            m_descending = true;
        }

        m_lastWarpedTime = warpedTime;
        m_previousRealTime = realTime;
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

            double result = ReadPayload(warpedTime * m_resolution, latestRealTime);
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
