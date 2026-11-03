#pragma once

#include "MultiChannelNumber.hpp"
#include <algorithm>
#include <cmath>

// Both fixed curves share one interpolated segment lookup per lane. The
// non-template table is shared by every instance and channel count.
//
struct PolyXFaderTables
{
    static constexpr size_t x_intervals = 1024;

    struct Entry
    {
        float m_sine;
        float m_hard;
        float m_sineDelta;
        float m_hardDelta;
    };

    Entry m_entries[x_intervals];
    static const PolyXFaderTables s_instance;

    static double Sine(double x)
    {
        return (1.0 - std::cos(std::acos(-1.0) * x)) * 0.5;
    }

    static double Hard(double x)
    {
        double t = 8.0 * (2.0 * Sine(x) - 1.0);
        double saturated = std::clamp(t * (27.0 + t * t) / (27.0 + 9.0 * t * t), -1.0, 1.0);
        return (saturated + 1.0) * 0.5;
    }

    PolyXFaderTables()
    {
        for (size_t i = 0; i < x_intervals; ++i)
        {
            double x0 = static_cast<double>(i) / x_intervals;
            double x1 = static_cast<double>(i + 1) / x_intervals;
            float sine = static_cast<float>(Sine(x0));
            float hard = static_cast<float>(Hard(x0));
            m_entries[i] = {sine, hard, static_cast<float>(Sine(x1)) - sine,
                static_cast<float>(Hard(x1)) - hard};
        }
    }

    template<size_t Size>
    static void Evaluate(const MultiChannelNumber<float, Size>& input,
        MultiChannelNumber<float, Size>& sine, MultiChannelNumber<float, Size>& hard)
    {
        using Vector = MultiChannelNumber<float, Size>;
        Vector x = Vector::Min(Vector::Splat(1), Vector::Max(Vector(), input));
        Vector pos = x * x_intervals;
        Vector floored = Vector::Min(pos.Floor(), Vector::Splat(x_intervals - 1));
        Vector frac = pos - floored;
        Vector s0;
        Vector h0;
        Vector ds;
        Vector dh;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            const Entry& entry = s_instance.m_entries[static_cast<size_t>(floored[lane])];
            s0[lane] = entry.m_sine;
            h0[lane] = entry.m_hard;
            ds[lane] = entry.m_sineDelta;
            dh[lane] = entry.m_hardDelta;
        }

        sine = s0 + ds * frac;
        hard = h0 + dh * frac;
    }
};

inline const PolyXFaderTables PolyXFaderTables::s_instance;

