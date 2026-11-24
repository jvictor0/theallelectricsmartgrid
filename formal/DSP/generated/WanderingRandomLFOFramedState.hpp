#pragma once

// Generated from DSP.WanderingRandomLFO.FramedStream. Do not edit.
//

#include <array>
#include <cstdint>
#include <stddef.h>

template<typename Number>
struct WanderingRandomLFOFramedState
{
    struct
    {
        struct
        {
            double m_p0;
            double m_p1;
            double m_p2;
            double m_p3;
            double m_beforePrevious;
            double m_previous;
            double m_width;
            double m_next;
            double m_afterNext;
        } m_segment;

        struct
        {
            Number m_a;
            Number m_b;
            Number m_c;
            Number m_d;
        } m_cubic;

        double m_phase;
        double m_phaseIncrement;
        double m_lambdaSamples;
        size_t m_frameRemaining;
        std::uint64_t m_seed;
        std::uint64_t m_speedSeed;
    } m_endpoint;

    Number m_left;
    Number m_right;
    Number m_slope;
    size_t m_position;
};
