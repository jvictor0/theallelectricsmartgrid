#pragma once

#include "Math.hpp"
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

// Fixed-width bundle of arithmetic per-channel values. Element-wise operations
// run on a compiler vector type.
// Width-specific functions static_assert on Size; class template member bodies
// are only compiled when called, so each width only sees the functions it uses.
//
template<class Number, size_t Size>
struct MultiChannelNumber
{
    static_assert(std::is_arithmetic<Number>::value, "MultiChannelNumber requires arithmetic elements; use MultiChannelComplex for complex values");
    static_assert(Size == 3 || (Size > 0 && (Size & (Size - 1)) == 0), "MultiChannelNumber size must be a power of two or three");

    static constexpr size_t x_numChannels = Size;

    // Three channels are stored in four lanes so Load and Store move one whole
    // vector. The dummy lane is never read as a channel.
    //
    static constexpr size_t x_numLanes = Size == 3 ? 4 : Size;
    using Vector = Number __attribute__((vector_size(sizeof(Number) * x_numLanes)));

    Number m_values[x_numLanes];

    MultiChannelNumber()
        : m_values{}
    {
    }

    explicit MultiChannelNumber(const Number* values)
        : m_values{}
    {
        for (size_t i = 0; i < x_numChannels; ++i)
        {
            m_values[i] = values[i];
        }
    }

    MultiChannelNumber(Number left, Number right)
        : m_values{left, right}
    {
        static_assert(Size == 2, "two-value constructor requires two channels");
    }

    MultiChannelNumber(Number x, Number y, Number z)
        : m_values{x, y, z}
    {
        static_assert(Size == 3, "three-value constructor requires three channels");
    }

    MultiChannelNumber(Number x, Number y, Number z, Number w)
        : m_values{x, y, z, w}
    {
        static_assert(Size == 4, "four-value constructor requires four channels");
    }

    // A one-channel value converts implicitly to and from Number, so it can stand
    // in for a scalar. Wider values get an unusable NotScalar in Number's place.
    //
    struct NotScalar
    {
    };

    using Scalar = typename std::conditional<Size == 1, Number, NotScalar>::type;

    MultiChannelNumber(Scalar value)
        : m_values{value}
    {
    }

    operator Scalar() const
    {
        return m_values[0];
    }

    static MultiChannelNumber FromVector(Vector values)
    {
        MultiChannelNumber result;
        result.Store(values);
        return result;
    }

    static MultiChannelNumber Splat(Number value)
    {
        MultiChannelNumber result;
        for (size_t i = 0; i < x_numLanes; ++i)
        {
            result.m_values[i] = value;
        }

        return result;
    }

    Vector Load() const
    {
        Vector result;
        memcpy(&result, m_values, sizeof(m_values));
        return result;
    }

    void Store(Vector values)
    {
        memcpy(m_values, &values, sizeof(m_values));
    }

    Number& operator[](int index)
    {
        return m_values[index];
    }

    const Number& operator[](int index) const
    {
        return m_values[index];
    }

    MultiChannelNumber operator+(const MultiChannelNumber& other) const
    {
        return FromVector(Load() + other.Load());
    }

    MultiChannelNumber operator-(const MultiChannelNumber& other) const
    {
        return FromVector(Load() - other.Load());
    }

    MultiChannelNumber operator*(const MultiChannelNumber& other) const
    {
        return FromVector(Load() * other.Load());
    }

    MultiChannelNumber operator/(const MultiChannelNumber& other) const
    {
        return FromVector(Load() / other.Load());
    }

    // Scalar operators are templates so that any operand convertible to Number
    // (including a one-channel value) is an exact match, beating the built-in
    // operators a one-channel value converts into.
    //
    template <typename T, typename std::enable_if<std::is_convertible<T, Number>::value, int>::type = 0>
    MultiChannelNumber operator+(T scalar) const
    {
        return FromVector(Load() + static_cast<Number>(scalar));
    }

    template <typename T, typename std::enable_if<std::is_convertible<T, Number>::value, int>::type = 0>
    MultiChannelNumber operator-(T scalar) const
    {
        return FromVector(Load() - static_cast<Number>(scalar));
    }

    template <typename T, typename std::enable_if<std::is_convertible<T, Number>::value, int>::type = 0>
    MultiChannelNumber operator*(T scalar) const
    {
        return FromVector(Load() * static_cast<Number>(scalar));
    }

    template <typename T, typename std::enable_if<std::is_convertible<T, Number>::value, int>::type = 0>
    MultiChannelNumber operator/(T scalar) const
    {
        return FromVector(Load() / static_cast<Number>(scalar));
    }

    MultiChannelNumber operator-() const
    {
        return FromVector(-Load());
    }

    MultiChannelNumber& operator+=(const MultiChannelNumber& other)
    {
        Store(Load() + other.Load());
        return *this;
    }

    MultiChannelNumber& operator-=(const MultiChannelNumber& other)
    {
        Store(Load() - other.Load());
        return *this;
    }

    MultiChannelNumber& operator*=(const MultiChannelNumber& other)
    {
        Store(Load() * other.Load());
        return *this;
    }

    MultiChannelNumber& operator/=(const MultiChannelNumber& other)
    {
        Store(Load() / other.Load());
        return *this;
    }

    MultiChannelNumber& operator*=(Number scalar)
    {
        Store(Load() * scalar);
        return *this;
    }

    MultiChannelNumber& operator/=(Number scalar)
    {
        Store(Load() / scalar);
        return *this;
    }

    MultiChannelNumber Abs() const
    {
        return FromVector(__builtin_elementwise_abs(Load()));
    }

    MultiChannelNumber Floor() const
    {
        return FromVector(__builtin_elementwise_floor(Load()));
    }

    // Per-channel std::round: halves round away from zero.
    //
    MultiChannelNumber Round() const
    {
        return FromVector(__builtin_elementwise_round(Load()));
    }

    // Per-channel static_cast to another number type.
    //
    template<class Other>
    MultiChannelNumber<Other, Size> Convert() const
    {
        using OtherVector = typename MultiChannelNumber<Other, Size>::Vector;
        return MultiChannelNumber<Other, Size>::FromVector(__builtin_convertvector(Load(), OtherVector));
    }

    // Per-channel a < b ? ifLess : otherwise.
    //
    static MultiChannelNumber IfLess(const MultiChannelNumber& a, const MultiChannelNumber& b, const MultiChannelNumber& ifLess, const MultiChannelNumber& otherwise)
    {
        return FromVector(a.Load() < b.Load() ? ifLess.Load() : otherwise.Load());
    }

    MultiChannelNumber Sqrt() const
    {
        return FromVector(__builtin_elementwise_sqrt(Load()));
    }

    // Per-channel std::max(a, b): returns a unless a < b.
    //
    static MultiChannelNumber Max(const MultiChannelNumber& a, const MultiChannelNumber& b)
    {
        Vector x = a.Load();
        Vector y = b.Load();
        return FromVector(x < y ? y : x);
    }

    // Per-channel std::min(a, b): returns a unless b < a.
    //
    static MultiChannelNumber Min(const MultiChannelNumber& a, const MultiChannelNumber& b)
    {
        Vector x = a.Load();
        Vector y = b.Load();
        return FromVector(y < x ? y : x);
    }

    // Per-channel std::copysign(*this, sign).
    //
    MultiChannelNumber CopySign(const MultiChannelNumber& sign) const
    {
        return FromVector(__builtin_elementwise_copysign(Load(), sign.Load()));
    }

    // Fast per-channel 2^x for float. Splits x into the nearest integer n and
    // f in [-0.5, 0.5], evaluates the degree-6 Taylor polynomial of 2^f (relative
    // error below 2e-7, exactly 1 at f = 0), then adds n to the exponent bits.
    // Valid while n stays a normal exponent, roughly x in [-126, 127].
    // Other channel types fall back to std::exp2 per channel.
    //
    MultiChannelNumber Exp2() const
    {
        if constexpr (std::is_same<Number, float>::value)
        {
            using IntVector = int32_t __attribute__((vector_size(sizeof(int32_t) * x_numLanes)));

            Vector x = Load();
            Vector n = __builtin_elementwise_floor(x + 0.5f);
            Vector f = x - n;

            Vector p = f * 1.5403530393381606e-4f + 1.3333558146428443e-3f;
            p = p * f + 9.6181291076284770e-3f;
            p = p * f + 5.5504108664821580e-2f;
            p = p * f + 2.4022650695910071e-1f;
            p = p * f + 6.9314718055994531e-1f;
            p = p * f + 1.0f;

            IntVector exponent = __builtin_convertvector(n, IntVector) << 23;
            IntVector bits = __builtin_bit_cast(IntVector, p) + exponent;
            return FromVector(__builtin_bit_cast(Vector, bits));
        }
        else
        {
            MultiChannelNumber result;
            for (size_t i = 0; i < Size; ++i)
            {
                result.m_values[i] = std::exp2(m_values[i]);
            }

            return result;
        }
    }

    // Fast per-channel log2(x) for positive normal float x. Writes x as 2^e * m
    // with m in [sqrt(1/2), sqrt(2)) and sums the odd series of
    // log2(m) = 2 * atanh(t) / ln(2) in t = (m - 1) / (m + 1); |t| < 0.172, so
    // five terms reach float precision. Other channel types fall back to
    // std::log2 per channel.
    //
    MultiChannelNumber Log2() const
    {
        if constexpr (std::is_same<Number, float>::value)
        {
            using IntVector = int32_t __attribute__((vector_size(sizeof(int32_t) * x_numLanes)));
            static constexpr int32_t x_sqrtHalfBits = 0x3F3504F3;

            IntVector bits = __builtin_bit_cast(IntVector, Load());
            IntVector exponent = (bits - x_sqrtHalfBits) >> 23;
            Vector m = __builtin_bit_cast(Vector, bits - (exponent << 23));
            Vector t = (m - 1.0f) / (m + 1.0f);
            Vector t2 = t * t;

            Vector p = t2 * 0.32059889797532520f + 0.41219858311113240f;
            p = p * t2 + 0.57707801635558536f;
            p = p * t2 + 0.96179669392597560f;
            p = p * t2 + 2.88539008177792681f;
            return FromVector(p * t + __builtin_convertvector(exponent, Vector));
        }
        else
        {
            MultiChannelNumber result;
            for (size_t i = 0; i < Size; ++i)
            {
                result.m_values[i] = std::log2(m_values[i]);
            }

            return result;
        }
    }

    // Per-channel e^x, computed as 2^(x * log2(e)).
    //
    MultiChannelNumber Exp() const
    {
        static constexpr Number x_log2e = static_cast<Number>(1.4426950408889634);
        return (*this * x_log2e).Exp2();
    }

    MultiChannelNumber ModOne() const
    {
        Vector values = Load();
        Vector result = values - __builtin_elementwise_floor(values);

        // Tiny negative inputs can round up to exactly 1.  A second pass maps
        // those to 0 and leaves values already in [0, 1) unchanged.
        //
        result = result - __builtin_elementwise_floor(result);

        for (size_t i = 0; i < Size; ++i)
        {
            assert(result[i] >= 0);
            assert(result[i] < 1);
        }

        return FromVector(result);
    }

    bool operator==(const MultiChannelNumber& other) const
    {
        return !(*this != other);
    }

    bool operator!=(const MultiChannelNumber& other) const
    {
        auto differs = Load() != other.Load();
        if constexpr (Size == 3)
        {
            differs[3] = 0;
#if defined(__aarch64__)
            // Masks are zero or -1. Four 32-bit lanes let NEON reduce float
            // and double comparisons with one horizontal minimum.
            //
            using Mask = int32_t __attribute__((vector_size(sizeof(int32_t) * x_numLanes)));
            return __builtin_reduce_min(__builtin_convertvector(differs, Mask)) != 0;
#else
            return __builtin_reduce_or(differs) != 0;
#endif
        }

        auto anyDiffers = differs[0];
        for (size_t i = 1; i < Size; ++i)
        {
            anyDiffers |= differs[i];
        }

        return anyDiffers != 0;
    }

    bool operator==(Scalar other) const
    {
        return m_values[0] == other;
    }

    bool operator!=(Scalar other) const
    {
        return m_values[0] != other;
    }

    // Scalar left-to-right sum, so results match the original per-channel code
    // bit for bit.
    //
    Number Sum() const
    {
        Number result = m_values[0];
        for (size_t i = 1; i < Size; ++i)
        {
            result += m_values[i];
        }

        return result;
    }

    Number Average() const
    {
        return Sum() / static_cast<Number>(Size);
    }

    // Four-channel functions
    //
    MultiChannelNumber Rotate(Number angle)
    {
        static_assert(Size == 4, "Rotate requires four channels");
        MultiChannelNumber values = *this;

        angle *= 4;
        size_t index = static_cast<size_t>(angle) % 4;
        size_t nextIndex = (index + 1) % 4;
        Number lerp = angle - static_cast<Number>(index);

        Number s = std::sin(lerp * M_PI / 2);
        Number c = std::cos(lerp * M_PI / 2);

        MultiChannelNumber result;
        for (size_t i = 0; i < 4; i++)
        {
            result[i] = (values[(i + index) % 4] * c + values[(i + nextIndex) % 4] * s) / (c + s);
        }

        return result;
    }

    MultiChannelNumber RotateLinear(MultiChannelNumber angle)
    {
        static_assert(Size == 4, "RotateLinear requires four channels");
        MultiChannelNumber result;
        for (int i = 0; i < 4; ++i)
        {
            Number angleI = angle[i] * 4;
            size_t index = static_cast<size_t>(angleI);
            Number lerp = angleI - static_cast<Number>(index);

            result[i] = m_values[(i + index) % 4] * (1 - lerp) + m_values[(i + index + 1) % 4] * lerp;
        }

        return result;
    }

    MultiChannelNumber Rotate90()
    {
        static_assert(Size == 4, "Rotate90 requires four channels");
        return MultiChannelNumber(m_values[3], m_values[0], m_values[1], m_values[2]);
    }

    MultiChannelNumber Hadamard()
    {
        static_assert(Size == 4, "Hadamard requires four channels");
        return MultiChannelNumber(
            (m_values[0] + m_values[1] + m_values[2] + m_values[3]) / 2,
            (m_values[0] - m_values[1] + m_values[2] - m_values[3]) / 2,
            (m_values[0] + m_values[1] - m_values[2] - m_values[3]) / 2,
            (m_values[0] - m_values[1] - m_values[2] + m_values[3]) / 2);
    }

    MultiChannelNumber Widen(Number amount) const
    {
        static_assert(Size == 4, "Widen requires four channels");
        Number sum = Sum() / 4;
        return MultiChannelNumber(m_values[0] + (sum - m_values[0]) * amount, m_values[1] + (sum - m_values[1]) * amount, m_values[2] + (sum - m_values[2]) * amount, m_values[3] + (sum - m_values[3]) * amount);
    }

    static MultiChannelNumber Pan(Number x, Number y, Number sample)
    {
        static_assert(Size == 4, "x/y Pan requires four channels");
        return MultiChannelNumber(
            Math::Sin2pi((1 - x) * y / 4),
            Math::Sin2pi(x * y / 4),
            Math::Sin2pi(x * (1 - y) / 4),
            Math::Sin2pi((1 - x) * (1 - y) / 4)) * sample;
    }

    // Two-channel functions
    //
    static MultiChannelNumber Pan(Number x, Number sample)
    {
        static_assert(Size == 2, "x Pan requires two channels");
        return MultiChannelNumber(
            Math::Sin2pi((1 - x) / 4),
            Math::Sin2pi(x / 4)) * sample;
    }

    MultiChannelNumber<Number, 4> EmbedQuadEmpty() const
    {
        static_assert(Size == 2, "EmbedQuadEmpty requires two channels");
        return MultiChannelNumber<Number, 4>(m_values[0], m_values[1], static_cast<Number>(0), static_cast<Number>(0));
    }

    MultiChannelNumber<Number, 4> CombineToQuad(const MultiChannelNumber& other) const
    {
        static_assert(Size == 2, "CombineToQuad requires two channels");
        return MultiChannelNumber<Number, 4>(m_values[0], m_values[1], other.m_values[0], other.m_values[1]);
    }
};
