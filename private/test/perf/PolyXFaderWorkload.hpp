#pragma once

#include "Bench.hpp"
#include "PolyXFader.hpp"

inline float Motion(const Signals& signals, size_t n, size_t c, int mode)
{
    return (mode == 3 || (mode == 2 && c == 1)) ? signals.m_controls[n % signals.m_period][c] : 0;
}

template <size_t Size, int Mode>
struct PolyXFaderWorkload
{
    static constexpr size_t x_rate = SampleTimer::x_sampleRate;
    using Core = PolyXFaderGeneric<Size>;
    Signals m_signals{x_rate};
    TheoryOfTimeBase m_clock;
    static constexpr size_t x_groups = Size == 4 ? 1 : 3 / Size;
    Core m_core[x_groups];
    typename Core::Input m_input[x_groups];
    PolyXFaderWorkload()
    {
        m_clock.m_globalPeriodSamples = x_rate;
        auto& sample = m_clock.m_sample;
        sample.m_running = true;
        for (size_t j = 0; j < 6; ++j)
        {
            sample.m_loops[j].m_cycleRatio = 1ll << (5 - j);
        }

        for (auto& p : m_input)
        {
            p = Core::MakeInput();
            p.m_size = 6;
        }
    }

    std::array<float, 4> Process(size_t n)
    {
        auto& sample = m_clock.m_sample;
        sample.m_modulatedPhase = static_cast<double>(n) / x_rate;
        sample.m_modulatedPhaseIncrement = 1.0 / x_rate;
        m_clock.m_phaseClock.m_phase = sample.m_modulatedPhase;
        m_clock.m_phaseClock.m_increment = sample.m_modulatedPhaseIncrement;
        std::array<float, 4> out{};
        for (size_t g = 0; g < x_groups; ++g)
        {
            auto& p = m_input[g];
            for (size_t c = 0; c < Size; ++c)
            {
                size_t ch = g * Size + c;
                float move = Motion(m_signals, n, ch, Mode);
                p.m_attackFrac[c] = 0.3f + 0.1f * ch + 0.1f * move;
                p.m_shape[c] = 0.25f + 0.1f * move;
                p.m_mult[c] = 1.3f + 0.3f * ch + 0.1f * move;
                p.m_amplitude[c] = 0.8f;
                p.m_phaseShift[c] = 0.1f * ch;
                p.m_slope[c] = 0.65f + 0.1f * move;
                p.m_center[c] = 0.5f + 0.1f * move;
                p.m_shFade[c] = 0.1f;
                p.m_trig[c] = (n + 211 * ch) % 4096 == 0;
            }

            m_core[g].Process(p, m_clock, PhaseDomain::Modulated);
            for (size_t c = 0; c < Size; ++c)
            {
                out[g * Size + c] = m_core[g].m_output[c];
            }
        }

        return out;
    }
};
