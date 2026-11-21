#include "SmoothRandomLFO.hpp"
#include <cassert>
#include <cmath>
#include <limits>

using Segment = SmoothRandomLFOPrepareGeneric<double, 1>::Input;

// Independent de Boor evaluation in sample time, with each interpolation
// using its own knot interval. The runtime instead uses a prepared cubic.
//
double Reference(const Segment& s, double t)
{
    const auto lerp = [](double a, double b, double u)
    {
        return a + (b - a) * u;
    };

    const double previous = s.m_previous[0];
    const double before = previous + s.m_beforePrevious[0];
    const double width = s.m_width[0];
    const double next = s.m_next[0];
    const double left = lerp(s.m_p0[0], s.m_p1[0], (t + before) / (before + width));
    const double middle = lerp(s.m_p1[0], s.m_p2[0], (t + previous) / (previous + width + next));
    const double right = lerp(s.m_p2[0], s.m_p3[0], t / (width + next + s.m_afterNext[0]));
    const double lower = lerp(left, middle, (t + previous) / (previous + width));
    const double upper = lerp(middle, right, t / (width + next));
    return lerp(lower, upper, t / width);
}

template<typename Number, size_t Size>
void CheckProcess()
{
    SmoothRandomLFOGeneric<Number, Size> lfo(48, 0.25, 247);
    for (size_t lane = 0; lane < Size; ++lane)
    {
        lfo.m_segments[lane] = {0.07, 0.87, 0.35, 0.95, 9.5, 3.125, 5.25 + lane, 2.5, 7.25};
        lfo.Prepare(lane);
    }

    lfo.m_elapsed[0] = 5;
    auto copy = lfo;
    size_t wraps = 0;
    for (size_t sample = 0; sample < 1024; ++sample)
    {
        if (sample == 100)
        {
            lfo.SetExponentialRate(48, 0.125);
            copy.SetExponentialRate(48, 0.125);
            assert(lfo.m_increment.lambda() == 6);
        }

        const auto oldSegments = lfo.m_segments;
        const auto oldElapsed = lfo.m_elapsed;
        const auto rng = lfo.m_rng;
        const auto y = lfo.Process();
        assert(copy.Process() == y);
        bool wrapped = false;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            const auto& old = oldSegments[lane];
            const auto& now = lfo.m_segments[lane];
            assert(std::abs(y[lane] - Reference(old, oldElapsed[lane])) <
                32 * std::numeric_limits<Number>::epsilon());
            const double elapsed = oldElapsed[lane] + 1;
            const double width = old.m_width[0];
            if (elapsed >= width)
            {
                wrapped = true;
                ++wraps;
                assert(lfo.m_elapsed[lane] == elapsed - width);
                assert(now.m_beforePrevious[0] == old.m_previous[0]);
                assert(now.m_previous[0] == width);
                assert(now.m_width[0] == old.m_next[0]);
                assert(now.m_next[0] == old.m_afterNext[0]);
                assert(now.m_afterNext[0] >= 1 && now.m_afterNext[0] <= 0x1p48);
                assert(now.m_p0[0] == old.m_p1[0]);
                assert(now.m_p1[0] == old.m_p2[0]);
                assert(now.m_p2[0] == old.m_p3[0]);
                assert(now.m_p3[0] >= 0 && now.m_p3[0] < 1);
                assert(lfo.m_elapsed[lane] < now.m_width[0]);
            }
            else
            {
                assert(lfo.m_elapsed[lane] == elapsed);
                assert(now.m_width[0] == width);
                assert(now.m_p3[0] == old.m_p3[0]);
            }
        }

        assert((lfo.m_rng != rng) == wrapped);
    }

    assert(wraps > Size);
}

int main()
{
    CheckProcess<float, 1>();
    CheckProcess<float, 3>();
    CheckProcess<float, 4>();
    CheckProcess<double, 2>();
}
