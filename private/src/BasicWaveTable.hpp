#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cmath>

#include "MultiChannelNumber.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-cxx-extension"

template<typename Number, size_t Size, size_t Bits>
struct WaveTableLookupGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr size_t x_tableSize = static_cast<size_t>(1) << Bits;
    size_t m_index[Size];
    Vector m_fraction;

    explicit WaveTableLookupGeneric(const Vector& phase)
    {
        Vector position = phase * static_cast<Number>(x_tableSize);
        Vector floored = position.Floor();
        m_fraction = position - floored;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            m_index[lane] = static_cast<size_t>(static_cast<int64_t>(floored[lane])) & (x_tableSize - 1);
        }
    }
};

template<typename Number, size_t Size, size_t Bits>
struct BasicWaveTableGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;

    using Lookup = WaveTableLookupGeneric<Number, Size, Bits>;
    static constexpr size_t x_tableSize = 1 << Bits;
    Vector m_table[x_tableSize];

    BasicWaveTableGeneric()
        : m_table{}
    {
        Init();
    }

    void Init()
    {
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_table[i] = Vector::Splat(0);
        }
    }

    Vector Evaluate(Number phase) const
    {
        return EvaluateLinear(phase);
    }

    Vector EvaluateLinear(Number phase) const
    {
        Number pos = phase * x_tableSize;
        Number floored = std::floor(pos);
        int index = static_cast<int>(floored) % x_tableSize;
        Number frac = pos - floored;
        return m_table[index] * (1 - frac) + m_table[(index + 1) % x_tableSize] * frac;
    }

    Vector EvaluateCubic(Number phase) const
    {
        Number pos = phase * x_tableSize;
        int index = static_cast<int>(std::floor(pos)) % x_tableSize;
        Number frac = pos - static_cast<Number>(index);

        // Get four sample points for cubic interpolation
        //
        int i0 = (index - 1 + x_tableSize) % x_tableSize;
        int i1 = index;
        int i2 = (index + 1) % x_tableSize;
        int i3 = (index + 2) % x_tableSize;

        const Vector& y0 = m_table[i0];
        const Vector& y1 = m_table[i1];
        const Vector& y2 = m_table[i2];
        const Vector& y3 = m_table[i3];

        // Cubic interpolation (Catmull-Rom)
        //
        Vector a0 = y0 * -0.5f + y1 * 1.5f - y2 * 1.5f + y3 * 0.5f;
        Vector a1 = y0 - y1 * 2.5f + y2 * 2.0f - y3 * 0.5f;
        Vector a2 = y0 * -0.5f + y2 * 0.5f;
        const Vector& a3 = y1;

        return ((a0 * frac + a1) * frac + a2) * frac + a3;
    }

    Vector Evaluate(const Vector& phase) const
    {
        return EvaluateLinear(phase);
    }

    Vector EvaluateLinear(const Vector& phase) const
    {
        Lookup lookup(phase);
        Vector left;
        Vector right;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            size_t index = lookup.m_index[lane];
            left[lane] = m_table[index][lane];
            right[lane] = m_table[(index + 1) % x_tableSize][lane];
        }

        return left * (Vector::Splat(1) - lookup.m_fraction) + right * lookup.m_fraction;
    }

    Vector EvaluateCubic(const Vector& phase) const
    {
        Lookup lookup(phase);
        Vector y0, y1, y2, y3;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            size_t index = lookup.m_index[lane];
            y0[lane] = m_table[(index + x_tableSize - 1) % x_tableSize][lane];
            y1[lane] = m_table[index][lane];
            y2[lane] = m_table[(index + 1) % x_tableSize][lane];
            y3[lane] = m_table[(index + 2) % x_tableSize][lane];
        }

        Vector a0 = y0 * -0.5 + y1 * 1.5 - y2 * 1.5 + y3 * 0.5;
        Vector a1 = y0 - y1 * 2.5 + y2 * 2.0 - y3 * 0.5;
        Vector a2 = y0 * -0.5 + y2 * 0.5;
        return ((a0 * lookup.m_fraction + a1) * lookup.m_fraction + a2) * lookup.m_fraction + y1;
    }

    void Write(size_t index, const Vector& value)
    {
        assert(index >= 0 && index < x_tableSize);
        m_table[index] = value;
    }

    Vector Amplitude() const
    {
        Vector max = Vector::Splat(0);
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            max = Vector::Max(max, m_table[i].Abs());
        }

        return max;
    }

    Vector RMSAmplitude() const
    {
        Vector sum = Vector::Splat(0);
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            sum += m_table[i] * m_table[i];
        }

        return (sum / x_tableSize).Sqrt();
    }

    // Normalizes each channel independently. Silent channels become zero.
    //
    void NormalizeAmplitude()
    {
        Vector rms = RMSAmplitude();
        Vector divisor = rms * static_cast<Number>(std::sqrt(2.0));
        Vector keep = Vector::Splat(1);
        for (size_t i = 0; i < Size; ++i)
        {
            if (rms[i] < 0.00000001)
            {
                divisor[i] = 1;
                keep[i] = 0;
            }
        }

        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_table[i] = m_table[i] / divisor * keep;
        }
    }

    void MakeSaw()
    {
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_table[i] = Vector::Splat(- 2 * (i / static_cast<Number>(x_tableSize) - 0.5f));
        }
    }

    void MakeSquare()
    {
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_table[i] = Vector::Splat((i / static_cast<Number>(x_tableSize) < 0.5f) ? 1.0f : -1.0f);
        }
    }

    void WriteCosHarmonic(size_t harmonic)
    {
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_table[i] += Vector::Splat(cosf(2.0f * M_PI * harmonic * i / x_tableSize));
        }
    }

    Vector StartValue() const
    {
        return m_table[0];
    }

    Vector CenterValue() const
    {
        return m_table[x_tableSize / 2];
    }
};

using BasicWaveTable = BasicWaveTableGeneric<float, 1, 10>;
using BasicWaveTable4096 = BasicWaveTableGeneric<float, 1, 12>;

#pragma clang diagnostic pop
