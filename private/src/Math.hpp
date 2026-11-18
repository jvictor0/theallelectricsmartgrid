#pragma once

#include <complex>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Defined in MultiChannelNumber.hpp and BasicWaveTable.hpp, which include this
// file. The vector overloads below only need them complete where they are used.
//
template<class Number, size_t Size>
struct MultiChannelNumber;

template<typename RealType, size_t Size>
struct MultiChannelComplex;

template<typename Number, size_t Size, size_t Bits>
struct BasicWaveTableGeneric;

template<size_t Bits>
struct MathGeneric
{
    inline static MathGeneric s_instance;
    static constexpr size_t x_tableSize = 1 << Bits;
    static constexpr int x_hannKernelRadius = 8;
    static constexpr size_t x_hannKernelTableSize = x_tableSize;
    static constexpr float x_hannKernelMinOffset = -static_cast<float>(x_hannKernelRadius);
    static constexpr float x_hannKernelWidth = 2.0f * static_cast<float>(x_hannKernelRadius) + 1.0f;
    float m_cosTable[x_tableSize];
    std::complex<float> m_rootOfUnity[x_tableSize];
    std::complex<float> m_hannKernel[x_hannKernelTableSize + 1];

    MathGeneric()
    {
        // Initialize cosine wavetable
        //
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_cosTable[i] = static_cast<float>(std::cos(2.0 * M_PI * i / x_tableSize));
        }

        // Exact axes preserve relative accuracy when interpolating near zero.
        //
        if constexpr (Bits >= 2)
        {
            m_cosTable[x_tableSize / 4] = 0;
            m_cosTable[3 * x_tableSize / 4] = 0;
        }

        // Initialize root of unity table
        //
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_rootOfUnity[i] = std::complex<float>(cosf(2.0f * M_PI * i / x_tableSize), sinf(2.0f * M_PI * i / x_tableSize));
        }

        // Initialize Hann kernel table for windowed partial writes
        //
        for (size_t i = 0; i <= x_hannKernelTableSize; ++i)
        {
            float offset = x_hannKernelMinOffset + x_hannKernelWidth * static_cast<float>(i) / static_cast<float>(x_hannKernelTableSize);
            m_hannKernel[i] = ComputeHannKernel(offset);
        }
    }

    static std::complex<float> RootOfUnityByIndex(size_t index)
    {
        return s_instance.m_rootOfUnity[index];
    }

    // Linear interpolation of the cosine table at phase in [0, 1].
    //
    static float EvaluateCosTable(float phase)
    {
        return EvaluateCosTable(phase, 0);
    }

    static float EvaluateCosTable(float phase, size_t indexOffset)
    {
        float pos = phase * x_tableSize;
        float floored = std::floor(pos);
        size_t index = (static_cast<size_t>(floored) + indexOffset) % x_tableSize;
        float frac = pos - floored;
        return s_instance.m_cosTable[index] * (1.0f - frac) + s_instance.m_cosTable[(index + 1) % x_tableSize] * frac;
    }

    template<typename Number, size_t Size>
    static MultiChannelNumber<Number, Size> EvaluateCosTable(const MultiChannelNumber<Number, Size>& phase)
    {
        return EvaluateCosTable(phase, 0);
    }

    template<typename Number, size_t Size>
    static MultiChannelNumber<Number, Size> EvaluateCosTable(const MultiChannelNumber<Number, Size>& phase, size_t indexOffset)
    {
        using Vector = MultiChannelNumber<Number, Size>;
        using Index = MultiChannelNumber<uint32_t, Size>;
        Vector pos = phase * x_tableSize;
        Vector floored = pos.Floor();
        Vector frac = pos - floored;
        const auto indices = floored.template Convert<uint32_t>().Load() + static_cast<uint32_t>(indexOffset);
        const Vector y0 = Vector::Gather(s_instance.m_cosTable, Index::FromVector(indices & (x_tableSize - 1)));
        const Vector y1 = Vector::Gather(s_instance.m_cosTable, Index::FromVector((indices + 1) & (x_tableSize - 1)));

        return Vector::FusedMultiplyAdd(frac, y1 - y0, y0);
    }

    static float Sin2pi(float x)
    {
        // Shift the table index, not the floating phase: adding 0.75 loses
        // significant bits for small inputs. Odd symmetry also preserves them
        // for small negative inputs when reducing phase modulo one.
        //
        if constexpr (Bits >= 2)
        {
            const float magnitude = std::abs(x);
            const float phase = magnitude - std::floor(magnitude);
            return EvaluateCosTable(phase, 3 * x_tableSize / 4) * std::copysign(1.0f, x);
        }

        float phase = x - std::floor(x);
        phase += 0.75f;
        phase -= std::floor(phase);
        return EvaluateCosTable(phase);
    }

    template<typename Number, size_t Size>
    static MultiChannelNumber<Number, Size> Sin2pi(const MultiChannelNumber<Number, Size>& x)
    {
        if constexpr (Bits >= 2)
        {
            using Vector = MultiChannelNumber<Number, Size>;
            const Vector magnitude = x.Abs();
            const Vector phase = magnitude - magnitude.Floor();
            return EvaluateCosTable(phase, 3 * x_tableSize / 4) * Vector::Splat(1).CopySign(x);
        }

        MultiChannelNumber<Number, Size> phase = x - x.Floor();
        phase = phase + 0.75f;
        phase = phase - phase.Floor();
        return EvaluateCosTable(phase);
    }

    static float Sin(float x)
    {
        return Sin2pi(x / (2.0f * M_PI));
    }

    static float Cos2pi(float x)
    {
        float phase = x - std::floor(x);
        return EvaluateCosTable(phase);
    }

    template<typename Number, size_t Size>
    static MultiChannelNumber<Number, Size> Cos2pi(const MultiChannelNumber<Number, Size>& x)
    {
        return EvaluateCosTable(x - x.Floor());
    }

    static float Cos(float x)
    {
        return Cos2pi(x / (2.0f * M_PI));
    }

    static float Tan2pi(float x)
    {
        float s = Sin2pi(x);
        float c = Cos2pi(x);
        return s / c;
    }

    template<typename Number, size_t Size>
    static MultiChannelNumber<Number, Size> Tan2pi(const MultiChannelNumber<Number, Size>& x)
    {
        return Sin2pi(x) / Cos2pi(x);
    }

    static float TanPi(float x)
    {
        return Tan2pi(x / 2.0f);
    }

    template<typename Number, size_t Size>
    static MultiChannelNumber<Number, Size> TanPi(const MultiChannelNumber<Number, Size>& x)
    {
        return Tan2pi(x * static_cast<Number>(0.5));
    }

    static float Tan(float x)
    {
        return Tan2pi(x / (2.0f * static_cast<float>(M_PI)));
    }

    static std::complex<float> Polar(float mag, double phase)
    {
        return Polar2pi(mag, phase / (2.0 * M_PI));
    }

    static std::complex<float> Polar2pi(float mag, double phase)
    {
        // Reduce in double precision before narrowing to the float lookup phase.
        //
        const float wrappedPhase = static_cast<float>(phase - std::floor(phase));
        return std::complex<float>(mag * Cos2pi(wrappedPhase), mag * Sin2pi(wrappedPhase));
    }

    template<size_t Size>
    static MultiChannelComplex<float, Size> Polar2pi(const MultiChannelNumber<float, Size>& mag, const MultiChannelNumber<double, Size>& phase)
    {
        // Reduce in double precision before narrowing to the float lookup phase.
        //
        MultiChannelNumber<float, Size> wrappedPhase = (phase - phase.Floor()).template Convert<float>();
        return MultiChannelComplex<float, Size>(mag * Cos2pi(wrappedPhase), mag * Sin2pi(wrappedPhase));
    }

    static float Hann(size_t index)
    {
        return 0.5f * (1.0f - s_instance.m_cosTable[index]);
    }

    template<typename Number, size_t Size>
    static void Hann(BasicWaveTableGeneric<Number, Size, Bits>& table)
    {
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            Number window = Hann(i);
            table.m_table[i] *= window;
        }
    }

    static std::complex<float> ComputeRectKernel(float offset)
    {
        float denominator = sinf(static_cast<float>(M_PI) * offset / static_cast<float>(x_tableSize));
        if (std::abs(denominator) < 1e-6f)
        {
            if (std::abs(offset) < 1e-6f)
            {
                return std::complex<float>(1.0f, 0.0f);
            }

            return std::complex<float>(0.0f, 0.0f);
        }

        float magnitude = sinf(static_cast<float>(M_PI) * offset) / (static_cast<float>(x_tableSize) * denominator);
        float phase = static_cast<float>(M_PI) * offset * static_cast<float>(x_tableSize - 1) / static_cast<float>(x_tableSize);
        return std::complex<float>(magnitude * cosf(phase), magnitude * sinf(phase));
    }

    static std::complex<float> ComputeHannKernel(float offset)
    {
        return 0.5f * ComputeRectKernel(offset) - 0.25f * ComputeRectKernel(offset + 1.0f) - 0.25f * ComputeRectKernel(offset - 1.0f);
    }

    static std::complex<float> HannKernel(float offset)
    {
        float pos = (offset - x_hannKernelMinOffset) * static_cast<float>(x_hannKernelTableSize) / x_hannKernelWidth;
        if (pos <= 0.0f)
        {
            return s_instance.m_hannKernel[0];
        }

        if (static_cast<float>(x_hannKernelTableSize) <= pos)
        {
            return s_instance.m_hannKernel[x_hannKernelTableSize];
        }

        size_t index = static_cast<size_t>(std::floor(pos));
        float frac = pos - static_cast<float>(index);
        return s_instance.m_hannKernel[index] * (1.0f - frac) + s_instance.m_hannKernel[index + 1] * frac;
    }
};

typedef MathGeneric<10> Math;
typedef MathGeneric<12> Math4096;

