#pragma once

// Signal.hpp -- deterministic, header-only signal generators for DSP unit tests.
//
// FROZEN API (WP-2): other agents build on these. Keep small and stable.
//
// None of these touch a global RNG: WhiteNoise owns its own PRNG state seeded
// at construction, so two generators with the same seed produce identical
// sequences regardless of test order or other randomness in the process.

#include <cstddef>
#include <cstdint>
#include <cmath>

namespace TestSignal
{

// ---------------------------------------------------------------------------
// WhiteNoise -- self-contained uniform white noise in [-1, 1].
//
// Uses a 64-bit xorshift* PRNG (no std::random global state, no shared engine).
// Deterministic: same seed => identical Next() sequence.
// ---------------------------------------------------------------------------
//
struct WhiteNoise
{
    explicit WhiteNoise(std::uint64_t seed)
        : m_state(seed ? seed : 0x9E3779B97F4A7C15ull)  // avoid the all-zero fixed point
    {
    }

    // Next uniform sample in [-1, 1].
    //
    float Next()
    {
        // xorshift64* -- good enough for test signals, fully deterministic.
        //
        std::uint64_t x = m_state;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        m_state = x;
        std::uint64_t r = x * 0x2545F4914F6CDD1Dull;
        // Map the top 53 bits to [0, 1), then to [-1, 1].
        //
        double u = static_cast<double>(r >> 11) * (1.0 / 9007199254740992.0);  // 2^53
        return static_cast<float>(2.0 * u - 1.0);
    }

private:
    std::uint64_t m_state;
};

// ---------------------------------------------------------------------------
// Sine -- phase-accumulating sine oscillator.
// ---------------------------------------------------------------------------
//
struct Sine
{
    Sine(double freqHz, double sampleRate, float amp = 1.0f)
        : m_phase(0.0)
        , m_phaseInc(freqHz / sampleRate)
        , m_amp(amp)
    {
    }

    float Next()
    {
        float v = m_amp * static_cast<float>(std::sin(2.0 * M_PI * m_phase));
        m_phase += m_phaseInc;
        if (m_phase >= 1.0)
        {
            m_phase -= std::floor(m_phase);
        }
        return v;
    }

private:
    double m_phase;
    double m_phaseInc;  // cycles per sample
    float m_amp;
};

// ---------------------------------------------------------------------------
// Sweep -- logarithmic (exponential) frequency sweep from f0 to f1 over
// numSamples samples. Amplitude 1.0. Phase is integrated analytically so the
// instantaneous frequency is exact at every sample.
// ---------------------------------------------------------------------------
//
struct Sweep
{
    Sweep(double f0, double f1, std::size_t numSamples, double sampleRate)
        : m_f0(f0)
        , m_sampleRate(sampleRate)
        , m_n(0)
        , m_numSamples(numSamples ? numSamples : 1)
        // k such that f(t) = f0 * k^(t / numSamples), with f(numSamples) = f1.
        //
        , m_k(f1 / (f0 != 0.0 ? f0 : 1.0))
        , m_phase(0.0)
    {
    }

    float Next()
    {
        float v = static_cast<float>(std::sin(2.0 * M_PI * m_phase));
        // Instantaneous frequency at sample index m_n (log sweep).
        //
        double frac = static_cast<double>(m_n) / static_cast<double>(m_numSamples);
        double freq = m_f0 * std::pow(m_k, frac);
        m_phase += freq / m_sampleRate;
        if (m_phase >= 1.0)
        {
            m_phase -= std::floor(m_phase);
        }
        ++m_n;
        return v;
    }

private:
    double m_f0;
    double m_sampleRate;
    std::size_t m_n;
    std::size_t m_numSamples;
    double m_k;
    double m_phase;
};

// ---------------------------------------------------------------------------
// Fill helpers
// ---------------------------------------------------------------------------

// Fill buf[0..n) by repeatedly calling gen.Next(). gen is any object with a
// `float Next()` method (WhiteNoise, Sine, Sweep, or a lambda wrapper).
//
template <typename Gen>
inline void Fill(Gen& gen, float* buf, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
    {
        buf[i] = gen.Next();
    }
}

// Unit impulse: buf[0] = 1, rest 0.
//
inline void Impulse(float* buf, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
    {
        buf[i] = 0.0f;
    }
    if (n > 0)
    {
        buf[0] = 1.0f;
    }
}

// Constant DC level.
//
inline void DC(float* buf, std::size_t n, float level)
{
    for (std::size_t i = 0; i < n; ++i)
    {
        buf[i] = level;
    }
}

} // namespace TestSignal
