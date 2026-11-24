#pragma once

// Generated from PhaseTime.InverseRuntime. Do not edit.
//

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>

struct TimeWarpInverseKernels
{
    static constexpr unsigned x_batchCapacity = 8;

    static bool Less(double a, double b)
    {
        return a < b;
    }

    static bool AtMost(double a, double b)
    {
        return a <= b;
    }

    static bool Both(bool a, bool b)
    {
        return a && b;
    }

    static bool Either(bool a, bool b)
    {
        return a || b;
    }

    static bool Negate(bool value)
    {
        return !value;
    }

    static double Interpolate(double lowPhase, double highPhase, double lowTime, double highTime, double query)
    {
        const double v0 = (query - lowPhase);
        const double v1 = (highPhase - lowPhase);
        const double v2 = (v0 / v1);
        const double v3 = (highTime - lowTime);
        const double v4 = (v2 * v3);
        const double v5 = (lowTime + v4);
        return v5;
    }

    static double ReadInterval(double lowPhase, double highPhase, double lowTime, double highTime, double query, double cutoff, double missing)
    {
        const bool v0 = std::isfinite(lowTime);
        const bool v1 = Negate(v0);
        const bool v2 = Less(query, lowPhase);
        const bool v3 = Negate(v2);
        const bool v4 = Less(cutoff, lowTime);
        const bool v5 = Negate(v4);
        const bool v6 = Both(v3, v5);
        const bool v7 = Negate(v6);
        const bool v8 = Either(v1, v7);
        const bool v9 = (v8 == true);
        double v32;
        if (v9)
        {
            v32 = missing;
        }
        else
        {
            const bool v10 = (query == lowPhase);
            const bool v11 = (v10 == true);
            double v31;
            if (v11)
            {
                v31 = lowTime;
            }
            else
            {
                const bool v12 = std::isfinite(highTime);
                const bool v13 = Negate(v12);
                const bool v14 = Less(highPhase, query);
                const bool v15 = Negate(v14);
                const bool v16 = Less(lowPhase, highPhase);
                const bool v17 = Both(v15, v16);
                const bool v18 = Negate(v17);
                const bool v19 = Either(v13, v18);
                const bool v20 = (v19 == true);
                double v30;
                if (v20)
                {
                    v30 = missing;
                }
                else
                {
                    const double v21 = (query - lowPhase);
                    const double v22 = (highPhase - lowPhase);
                    const double v23 = (v21 / v22);
                    const double v24 = (highTime - lowTime);
                    const double v25 = (v23 * v24);
                    const double v26 = (lowTime + v25);
                    const bool v27 = AtMost(v26, cutoff);
                    const bool v28 = (v27 == true);
                    double v29;
                    if (v28)
                    {
                        v29 = v26;
                    }
                    else
                    {
                        v29 = missing;
                    }

                    v30 = v29;
                }

                v31 = v30;
            }

            v32 = v31;
        }

        return v32;
    }

    static bool FrontierCloses(double lowPhase, double highPhase, double frontier)
    {
        const bool v0 = AtMost(lowPhase, frontier);
        const bool v1 = Less(frontier, highPhase);
        const bool v2 = Both(v0, v1);
        return v2;
    }

    static bool KeepCut(double frontier, double highPhase, double newCut, double oldCut)
    {
        const bool v0 = AtMost(frontier, highPhase);
        const bool v1 = Less(newCut, oldCut);
        const bool v2 = Negate(v1);
        const bool v3 = Both(v0, v2);
        return v3;
    }

};

template<typename Storage>
struct TimeWarpInverseCore
{
    // Keep the earliest minimum during a replacement visit confined to this
    // cell, including touches of its right edge. A visit after leaving the cell
    // replaces the cut and can reassign older islands to the fixed fallback.
    // An unfinished first traversal uses its entry and forward frontier.
    //
    static void BeginBranch(Storage& storage, double phase, double time)
    {
        double floorPhase = std::floor(phase);
        if (phase == floorPhase)
        {
            return;
        }

        size_t slot = storage.WrapIndex(static_cast<int64_t>(floorPhase));
        double seed = storage.m_seedPhase.load();
        bool retained = storage.m_highestWrittenIndex - static_cast<double>(Storage::x_maxSamples - 1) <= floorPhase
            && floorPhase <= storage.m_highestWrittenIndex;
        storage.BeginWrite();
        if (!retained || !std::isfinite(storage.m_writeHeadInverse[slot].load())
            || (std::floor(seed) == floorPhase && phase < seed))
        {
            storage.SeedCell(phase, time);
        }
        else if (std::isfinite(storage.m_fallbackRightTime[slot].load()))
        {
            double oldTurn = storage.m_turnPhase[slot].load();
            if (!std::isfinite(oldTurn) || !TimeWarpInverseKernels::KeepCut(
                    storage.m_frontierPhase.load(), floorPhase + 1.0, phase, oldTurn))
            {
                storage.m_islandReassignments += std::isfinite(oldTurn) && oldTurn < phase;
                double base = storage.m_writeHeadInverse[slot].load();
                storage.m_turnPhase[slot].store(phase);
                storage.m_turnTime[slot].store(static_cast<float>(time - base));
            }

            // A boundary touch may have stored an endpoint. Reopen against the
            // live frontier when this confined visit resumes below that edge.
            //
            storage.m_currentRightTime[slot].store(static_cast<float>(Storage::x_missing));
        }

        storage.m_frontierPhase.store(phase);
        storage.m_frontierTime.store(time);
        storage.EndWrite();
    }

    // Caller holds the writer revision or validates it after reading. Fallback
    // and replacement never borrow an endpoint from the neighboring cell.
    //
    static double ReadPayload(const Storage& storage, double gridPhase, double cutoff)
    {
        double floorPhase = std::floor(gridPhase);
        size_t slot = storage.WrapIndex(static_cast<int64_t>(floorPhase));
        double base = storage.m_writeHeadInverse[slot].load();
        double seed = storage.m_seedPhase.load();
        double lowPhase = std::floor(seed) == floorPhase ? seed : floorPhase;
        double lowTime = base;
        double highPhase = floorPhase + 1.0;
        double highTime = storage.m_fallbackRightTime[slot].load();
        double frontierPhase = storage.m_frontierPhase.load();
        double frontierTime = storage.m_frontierTime.load();
        if (gridPhase == frontierPhase && std::isfinite(frontierTime))
        {
            return frontierTime <= cutoff ? frontierTime : Storage::x_missing;
        }

        double turn = storage.m_turnPhase[slot].load();
        if (std::isfinite(turn) && gridPhase >= turn)
        {
            lowPhase = turn;
            lowTime = base + static_cast<double>(storage.m_turnTime[slot].load());
            highTime = base + static_cast<double>(storage.m_currentRightTime[slot].load());
        }

        if (!std::isfinite(highTime) && TimeWarpInverseKernels::FrontierCloses(lowPhase, highPhase, frontierPhase))
        {
            highPhase = frontierPhase;
            highTime = frontierTime;
        }

        return TimeWarpInverseKernels::ReadInterval(lowPhase, highPhase, lowTime, highTime,
            gridPhase, cutoff, Storage::x_missing);
    }

    static void ScatterBufferToInverse(Storage& storage, double low, double high, double lowTime, double highTime)
    {
        int64_t first = static_cast<int64_t>(std::ceil(low));
        int64_t last = static_cast<int64_t>(std::floor(high));
        for (int64_t index = first; index <= last;)
        {
            size_t count = static_cast<size_t>(std::min<int64_t>(TimeWarpInverseKernels::x_batchCapacity, last - index + 1));
            std::array<double, TimeWarpInverseKernels::x_batchCapacity> times;
            for (size_t i = 0; i < count; ++i)
            {
                const double phase = static_cast<double>(index + static_cast<int64_t>(i));
                times[i] = TimeWarpInverseKernels::Interpolate(low, high, lowTime, highTime, phase);
            }

            storage.BeginWrite();
            if (static_cast<double>(index) > low)
            {
                storage.FinishCell(index - 1, times[0]);
            }

            for (size_t i = 0; i + 1 < count; ++i)
            {
                storage.CompleteCell(index + static_cast<int64_t>(i), times[i], times[i + 1]);
            }

            int64_t finalIndex = index + static_cast<int64_t>(count) - 1;
            storage.EnterCell(finalIndex, times[count - 1]);
            storage.m_frontierPhase.store(static_cast<double>(finalIndex));
            storage.m_frontierTime.store(times[count - 1]);
            storage.EndWrite();
            index += static_cast<int64_t>(count);
        }

        storage.m_highestWrittenIndex = std::max(storage.m_highestWrittenIndex, std::floor(high));
        storage.m_oldestWarpedTime = (storage.m_highestWrittenIndex - static_cast<double>(Storage::x_maxSamples - 1)) / storage.m_resolution;
    }

    static void Write(Storage& storage, double warpedTime, double realTime)
    {
        assert(std::isfinite(warpedTime) && std::isfinite(realTime));
        bool previousObservation = storage.m_hasObservation;
        storage.m_previousObservedRealTime = previousObservation ? storage.m_observedRealTime : realTime;
        storage.m_previousObservedWarpedTime = previousObservation ? storage.m_observedWarpedTime : warpedTime;
        storage.m_observedWarpedTime = warpedTime;
        storage.m_observedRealTime = realTime;
        storage.m_hasObservation = true;
        if (previousObservation && warpedTime > storage.m_lastWarpedTime)
        {
            double low = storage.m_lastWarpedTime * storage.m_resolution;
            double high = warpedTime * storage.m_resolution;
            if (!storage.m_hasForward || storage.m_descending)
            {
                BeginBranch(storage, low, storage.m_previousRealTime);
            }

            ScatterBufferToInverse(storage, low, high, storage.m_previousRealTime, realTime);
            storage.BeginWrite();
            storage.m_frontierPhase.store(high);
            storage.m_frontierTime.store(realTime);
            storage.EndWrite();
            storage.m_hasForward = true;
            storage.m_descending = false;
        }
        else if (previousObservation && warpedTime < storage.m_lastWarpedTime)
        {
            storage.m_descending = true;
        }

        storage.m_lastWarpedTime = warpedTime;
        storage.m_previousRealTime = realTime;
    }
};
