#pragma once

#include "SmoothRandomLFOCubicGeneric.hpp"
#include "SmoothRandomLFOPrepareGeneric.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <random>

template<typename Number, size_t Size>
struct SmoothRandomLFOGeneric
{
    using Cubic = SmoothRandomLFOCubicGeneric<Number, Size>;
    using Vector = typename Cubic::Vector;
    using Clock = MultiChannelNumber<double, Size>;
    using Preparer = SmoothRandomLFOPrepareGeneric<double, 1>;

    static constexpr double x_minIncrement = 0x1p-48;

    typename Cubic::Snapshot m_coefficients{};
    Clock m_elapsed{};
    Clock m_width{};
    Clock m_phaseIncrement{};
    std::array<typename Preparer::Input, Size> m_segments{};
    std::mt19937 m_rng;
    std::uniform_real_distribution<double> m_uniform{0.0, 1.0};
    std::exponential_distribution<double> m_increment;

    SmoothRandomLFOGeneric(double sampleRateHz, double lambda, std::mt19937::result_type seed)
        : m_rng(seed)
    {
        SetExponentialRate(sampleRateHz, lambda);
        for (size_t lane = 0; lane < Size; ++lane)
        {
            m_segments[lane] = {m_uniform(m_rng), m_uniform(m_rng), m_uniform(m_rng), m_uniform(m_rng),
                DrawWidth(), DrawWidth(), DrawWidth(), DrawWidth(), DrawWidth()};
            Prepare(lane);
        }
    }

    // Lambda is the exponential rate for phase cycles per second. The mean
    // velocity of a draw is 1 / lambda Hz, so the per-sample rate is lambda * sampleRateHz.
    // Rate changes apply to newly drawn spans entering the two-span lookahead.
    //
    void SetExponentialRate(double sampleRateHz, double lambda)
    {
        const double rate = lambda * sampleRateHz;
        assert(sampleRateHz > 0 && lambda > 0 && rate > 0 && std::isfinite(rate));
        m_increment.param(std::exponential_distribution<double>::param_type(rate));
    }

    double DrawWidth()
    {
        // A span lasts at least one sample. The lower increment bound keeps
        // even the longest span within the double clock's unit-sample precision.
        //
        return 1.0 / std::clamp(m_increment(m_rng), x_minIncrement, 1.0);
    }

    void Prepare(size_t lane)
    {
        const auto& segment = m_segments[lane];
        const auto coefficients = Preparer::Prepare(segment);
        m_width[lane] = segment.m_width[0];
        m_phaseIncrement[lane] = 1.0 / m_width[lane];
        m_coefficients.m_a[lane] = static_cast<Number>(coefficients.m_a[0]);
        m_coefficients.m_b[lane] = static_cast<Number>(coefficients.m_b[0]);
        m_coefficients.m_c[lane] = static_cast<Number>(coefficients.m_c[0]);
        m_coefficients.m_d[lane] = static_cast<Number>(coefficients.m_d[0]);
    }

    // Keep RNG and coefficient preparation out of the per-sample stack frame.
    //
    [[gnu::noinline, gnu::cold]]
    void AdvanceWrapped()
    {
        for (size_t lane = 0; lane < Size; ++lane)
        {
            if (m_elapsed[lane] >= m_width[lane])
            {
                // Carry elapsed samples across the knot so that the new
                // velocity also scales the fractional-sample overshoot.
                //
                m_elapsed[lane] -= m_width[lane];
                auto& s = m_segments[lane];
                s = {s.m_p1, s.m_p2, s.m_p3, m_uniform(m_rng),
                    s.m_previous, s.m_width, s.m_next, s.m_afterNext, DrawWidth()};
                Prepare(lane);
            }
        }
    }

    Vector Process()
    {
        const Vector phase = (m_elapsed * m_phaseIncrement).template Convert<Number>();
        const Vector output = Cubic::Evaluate(m_coefficients, phase);

        m_elapsed += Clock::Splat(1);
        const Clock wrapped = Clock::IfLess(m_elapsed, m_width, Clock::Splat(0), Clock::Splat(1));
        if (wrapped != Clock::Splat(0))
        {
            AdvanceWrapped();
        }

        return output;
    }
};
