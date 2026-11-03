#pragma once

#include "MultiChannelNumber.hpp"
#include <cmath>
#include <cstdint>

// Approximately Gaussian white noise per channel, with standard deviation 1/3.
// Each channel runs its own xorshift32 generator; two draws split into four
// 16-bit uniforms whose Irwin-Hall sum is rescaled to the target deviation.
// Output is bounded to +-2 / sqrt(3).
//
template <size_t Size>
struct WhiteNoiseGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    using StateVector = uint32_t __attribute__((vector_size(sizeof(uint32_t) * Vector::x_numLanes)));

    StateVector m_state;

    explicit WhiteNoiseGeneric(uint32_t seed)
    {
        SetSeed(seed);
    }

    void SetSeed(uint32_t seed)
    {
        for (size_t i = 0; i < Vector::x_numLanes; ++i)
        {
            uint32_t channelSeed = (seed + static_cast<uint32_t>(i) + 1) * 2654435761u;
            m_state[i] = channelSeed == 0 ? 1 : channelSeed;
        }
    }

    StateVector Next()
    {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return m_state;
    }

    // Uniform on [0, 1) from the top 24 bits of one draw.
    //
    Vector Uniform()
    {
        using FloatVector = typename Vector::Vector;
        static constexpr float x_scale = 1.0f / 16777216.0f;

        return Vector::FromVector(__builtin_convertvector(Next() >> 8, FloatVector) * x_scale);
    }

    Vector Generate()
    {
        using FloatVector = typename Vector::Vector;
        static constexpr float x_uniformScale = 1.0f / 65536.0f;

        StateVector first = Next();
        StateVector second = Next();
        FloatVector sum = __builtin_convertvector(first & 0xFFFF, FloatVector);
        sum += __builtin_convertvector(first >> 16, FloatVector);
        sum += __builtin_convertvector(second & 0xFFFF, FloatVector);
        sum += __builtin_convertvector(second >> 16, FloatVector);

        // Four 16-bit uniforms sum to mean 4 * 32767.5; scaled to [0, 1) they
        // have variance 1/3.
        //
        static constexpr float x_mean = 4.0f * 32767.5f;
        static constexpr float x_scale = x_uniformScale / 3.0f * 1.7320508075688772f;
        return Vector::FromVector((sum - x_mean) * x_scale);
    }
};

using TrioWhiteNoise = WhiteNoiseGeneric<3>;

// Voss-McCartney pink noise in [-1, 1] per channel. Seven rows of uniform
// values each redraw at half the rate of the previous one; all channels redraw
// the same rows on the same sample, drawing from WhiteNoiseGeneric.
//
template <size_t Size>
struct PinkNoiseGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    static constexpr int x_numRows = 7;
    static constexpr int x_maxKey = (1 << x_numRows) - 1;

    WhiteNoiseGeneric<Size> m_white;
    Vector m_rows[x_numRows];
    int m_key;

    explicit PinkNoiseGeneric(uint32_t seed)
        : m_white(seed)
        , m_key(0)
    {
        SetSeed(seed);
    }

    void SetSeed(uint32_t seed)
    {
        m_white.SetSeed(seed);
        m_key = 0;
        for (int i = 0; i < x_numRows; ++i)
        {
            m_rows[i] = m_white.Uniform();
        }
    }

    // The sum of seven uniforms has mean 3.5 and range [0, 7].
    //
    Vector Generate()
    {
        int lastKey = m_key;
        m_key++;
        if (m_key > x_maxKey)
        {
            m_key = 1;
        }

        int diff = lastKey ^ m_key;
        Vector sum = Vector::Splat(0);
        for (int i = 0; i < x_numRows; ++i)
        {
            if (diff & (1 << i))
            {
                m_rows[i] = m_white.Uniform();
            }

            sum += m_rows[i];
        }

        return (sum - 3.5f) / 3.5f;
    }
};

using TrioPinkNoise = PinkNoiseGeneric<3>;
