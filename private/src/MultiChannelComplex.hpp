#pragma once

#include <cassert>
#include <complex>
#include <cstddef>
#include <type_traits>

#include "MultiChannelNumber.hpp"

// Stores real and imaginary parts in separate vectors so complex arithmetic
// operates across channels. Multiplication uses the component-wise algebraic
// formula, without std::complex's recovery for non-finite intermediate values.
//
template<typename RealType, size_t Size>
struct MultiChannelComplex
{
    static_assert(std::is_floating_point<RealType>::value, "complex channels require a floating-point real type");

    using Vector = MultiChannelNumber<RealType, Size>;
    using Complex = std::complex<RealType>;
    static constexpr size_t x_numChannels = Size;

    Vector m_real;
    Vector m_imag;

    MultiChannelComplex()
        : m_real()
        , m_imag()
    {
    }

    MultiChannelComplex(const Vector& real, const Vector& imag)
        : m_real(real)
        , m_imag(imag)
    {
    }

    MultiChannelComplex(RealType value)
        : m_real(Vector::Splat(value))
        , m_imag()
    {
    }

    MultiChannelComplex(const Complex& value)
        : m_real(Vector::Splat(value.real()))
        , m_imag(Vector::Splat(value.imag()))
    {
    }

    static MultiChannelComplex Splat(const Complex& value)
    {
        return MultiChannelComplex(value);
    }

    // A lane is assembled by value. Use SetChannel or the real/imag vectors to
    // write it; const prevents accidentally assigning to a temporary lane.
    //
    const Complex operator[](size_t index) const
    {
        assert(index < Size);
        return Complex(m_real[index], m_imag[index]);
    }

    void SetChannel(size_t index, const Complex& value)
    {
        assert(index < Size);
        m_real[index] = value.real();
        m_imag[index] = value.imag();
    }

    Vector& real()
    {
        return m_real;
    }

    const Vector& real() const
    {
        return m_real;
    }

    Vector& imag()
    {
        return m_imag;
    }

    const Vector& imag() const
    {
        return m_imag;
    }

    MultiChannelComplex Conj() const
    {
        return MultiChannelComplex(m_real, -m_imag);
    }

    // Per-channel magnitude as sqrt(re^2 + im^2), without std::abs's hypot
    // overflow protection.
    //
    Vector Abs() const
    {
        return Norm().Sqrt();
    }

    Vector Norm() const
    {
        return m_real * m_real + m_imag * m_imag;
    }

    template<typename Other>
    MultiChannelComplex<Other, Size> Convert() const
    {
        return {m_real.template Convert<Other>(), m_imag.template Convert<Other>()};
    }

    // Per-channel std::arg.
    //
    Vector Arg() const
    {
        Vector result;
        for (size_t i = 0; i < Size; ++i)
        {
            result[i] = std::atan2(m_imag[i], m_real[i]);
        }

        return result;
    }

    MultiChannelComplex operator+(const MultiChannelComplex& other) const
    {
        return MultiChannelComplex(m_real + other.m_real, m_imag + other.m_imag);
    }

    MultiChannelComplex operator-(const MultiChannelComplex& other) const
    {
        return MultiChannelComplex(m_real - other.m_real, m_imag - other.m_imag);
    }

    MultiChannelComplex operator-() const
    {
        return MultiChannelComplex(-m_real, -m_imag);
    }

    MultiChannelComplex operator*(const MultiChannelComplex& other) const
    {
        return MultiChannelComplex(m_real * other.m_real - m_imag * other.m_imag,
                                   m_real * other.m_imag + m_imag * other.m_real);
    }

    MultiChannelComplex operator*(const Vector& scale) const
    {
        return MultiChannelComplex(m_real * scale, m_imag * scale);
    }

    MultiChannelComplex operator/(const Vector& scale) const
    {
        return MultiChannelComplex(m_real / scale, m_imag / scale);
    }

    // Finite response coefficients use the direct quotient in double precision.
    //
    MultiChannelComplex operator/(const MultiChannelComplex& other) const
    {
        return (*this * other.Conj()) / other.Norm();
    }

    MultiChannelComplex operator*(RealType scale) const
    {
        return MultiChannelComplex(m_real * scale, m_imag * scale);
    }

    MultiChannelComplex operator/(RealType scale) const
    {
        return MultiChannelComplex(m_real / scale, m_imag / scale);
    }

    MultiChannelComplex& operator+=(const MultiChannelComplex& other)
    {
        m_real += other.m_real;
        m_imag += other.m_imag;
        return *this;
    }

    MultiChannelComplex& operator-=(const MultiChannelComplex& other)
    {
        m_real -= other.m_real;
        m_imag -= other.m_imag;
        return *this;
    }

    MultiChannelComplex& operator*=(const MultiChannelComplex& other)
    {
        *this = *this * other;
        return *this;
    }

    MultiChannelComplex& operator*=(const Vector& scale)
    {
        *this = *this * scale;
        return *this;
    }

    MultiChannelComplex& operator/=(const Vector& scale)
    {
        *this = *this / scale;
        return *this;
    }

    MultiChannelComplex& operator*=(RealType scale)
    {
        *this = *this * scale;
        return *this;
    }

    MultiChannelComplex& operator/=(RealType scale)
    {
        *this = *this / scale;
        return *this;
    }
};
