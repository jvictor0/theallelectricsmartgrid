#pragma once

// Generated from DSP.SquiggleBoyQuadReverb. Do not edit.
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include "CircularBufferGeneric.hpp"
#include "SchroederAllPassCoreGeneric.hpp"
#include "SmoothRandomLFOStreamGeneric.hpp"
#include "TPTBaseWidthGeneric.hpp"
#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

template<typename Number, bool Wandering = true, bool Framed = true>
struct SquiggleBoyQuadReverb
{
    using Vector = MultiChannelNumber<Number, 4>;
    using DampingUIState = typename TPTBaseWidthGeneric<Number, 4>::UIState;
    static constexpr size_t x_delayCapacity = 262144;

    struct LogicalSpec
    {
        Vector m_delaySamples;
        Vector m_feedback;
        Vector m_dampingBase;
        Vector m_dampingWidth;
        Vector m_saturatorGain;
    };

    explicit SquiggleBoyQuadReverb(const LogicalSpec& spec)
    {
        ComputeCoefficients(spec);
    }

    auto MakeDampingSnapshot() const
    {
        return m_damping.m_filter.MakeSnapshot();
    }

    void ComputeCoefficients(const LogicalSpec& spec)
    {
#ifndef NDEBUG
        for (int ch = 0; ch < 4; ++ch)
        {
            assert(spec.m_delaySamples[ch] > 0);
        }
#endif

        m_damping.Tune(spec.m_dampingBase, spec.m_dampingWidth);
        m_delaySamples = spec.m_delaySamples;
        m_feedback = spec.m_feedback;
        m_saturatorGain = spec.m_saturatorGain;
    }

    Vector Process(const Vector& input)
    {
        m_diffusion.Process({input});
        const Vector v0 = m_diffusion.Y();
        const Vector v1 = m_secondAllPass.Y();
        const Vector v2 = SumFeedback(v0, v1, m_feedback);
        m_delay.Push(v2);
        const Vector v3 = m_delay.ReadCubic(m_delaySamples);
        const Vector v4 = Mix(v3);
        m_firstAllPass.Process({v4});
        const Vector v5 = m_firstAllPass.Y();
        const Vector v6 = Saturate(m_saturatorGain, v5);
        m_damping.Process({v6});
        const Vector v7 = m_damping.Y();
        m_secondAllPass.Process({v7});
        const Vector v8 = m_secondAllPass.Y();
        return v8;
    }

private:
    static constexpr size_t x_depth = Wandering ? 128 : 0;
    static constexpr double x_lambdaSamples = 0x1d4c0000000000p-32;

    static Vector AllPassGain()
    {
        return Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53));
    }

    static std::pair<Vector, Vector> DampingCutoffs(const Vector& base, const Vector& width)
    {
        const Vector v0 = Vector::IfLess(base, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)), base, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)));
        const Vector v1 = (base * width);
        const Vector v2 = (base * width);
        const Vector v3 = Vector::IfLess(v1, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)), v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)));
        return {v0, v3};
    }

    static Vector SumFeedback(const Vector& input, const Vector& previous, const Vector& feedback)
    {
        const Vector v0 = (previous * feedback);
        const Vector v1 = (input + v0);
        return v1;
    }

    static Vector Saturate(const Vector& gain, const Vector& sample)
    {
        const Vector v0 = (sample * gain);
        const Vector v1 = (v0 * v0);
        const Vector v2 = (v0 * (v1 + Vector::Splat(27)));
        const Vector v3 = Vector::FusedMultiplyAdd(v0 * Vector::Splat(9), v0, Vector::Splat(27));
        const Vector v4 = (v2 / v3);
        const Vector v5 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v4));
        return v5;
    }

    static std::pair<Vector, Vector> FractionalPosition(const Vector& samples)
    {
        const Vector v0 = (-samples);
        const Vector v1 = v0.Floor();
        const Vector v2 = (-v1);
        const Vector v3 = (v2 - samples);
        return {v2, v3};
    }

    static Vector WanderOffset(const Vector& depth, const Vector& spline)
    {
        const Vector v1 = Vector::IfLess(spline, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), spline, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v2 = Vector::IfLess(spline, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), spline, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v4 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v1, v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v5 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v4);
        const Vector v6 = (depth * v5);
        return v6;
    }

    static Vector Interpolate(const Vector& older, const Vector& center, const Vector& newer, const Vector& newest, const Vector& fraction)
    {
        const Vector v0 = (newer - older);
        const Vector v2 = (newer * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
        const Vector v3 = Vector::FusedMultiplyAdd(center, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v2);
        const Vector v4 = Vector::FusedMultiplyAdd(older, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v3);
        const Vector v5 = (v4 - newest);
        const Vector v6 = (center - newer);
        const Vector v7 = (newest - older);
        const Vector v8 = Vector::FusedMultiplyAdd(v6, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v7);
        const Vector v9 = (fraction * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v10 = Vector::FusedMultiplyAdd(fraction, v8, v5);
        const Vector v11 = Vector::FusedMultiplyAdd(fraction, v10, v0);
        const Vector v12 = Vector::FusedMultiplyAdd(v9, v11, center);
        return v12;
    }

    static Vector Mix(const Vector& input)
    {
        const Vector v1 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xb9611ap-24f : 0x172c234f72c235p-53)) * Vector::Splat(input[0]));
        const Vector v3 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xb08d3ep-24f : 0x1611a7b9611a7cp-53)) * Vector::Splat(input[1]));
        const Vector v4 = (v1 - v3);
        const Vector v6 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xb08d3ep-24f : 0x1611a7b9611a7cp-53)) * Vector::Splat(input[0]));
        const Vector v8 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xb9611ap-24f : 0x172c234f72c235p-53)) * Vector::Splat(input[1]));
        const Vector v9 = (v6 + v8);
        const Vector v11 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xd95bc6p-24f : 0x1b2b78c13521d0p-53)) * Vector::Splat(input[2]));
        const Vector v13 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x873ecbp-24f : 0x10e7d95bc609a9p-53)) * Vector::Splat(input[3]));
        const Vector v14 = (v11 - v13);
        const Vector v16 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x873ecbp-24f : 0x10e7d95bc609a9p-53)) * Vector::Splat(input[2]));
        const Vector v18 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xd95bc6p-24f : 0x1b2b78c13521d0p-53)) * Vector::Splat(input[3]));
        const Vector v19 = (v16 + v18);
        const Vector v21 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc0e070p-24f : 0x181c0e070381c1p-53)) * v4);
        const Vector v24 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa8542ap-24f : -0x150a8542a150a8p-53)) * v14);
        const Vector v25 = (v21 - v24);
        const Vector v28 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa8542ap-24f : -0x150a8542a150a8p-53)) * v4);
        const Vector v30 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc0e070p-24f : 0x181c0e070381c1p-53)) * v14);
        const Vector v31 = (v28 + v30);
        const Vector v33 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xab8be0p-24f : 0x15717c0a8e83f5p-53)) * v9);
        const Vector v35 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xbe0547p-24f : 0x17c0a8e83f5718p-53)) * v19);
        const Vector v36 = (v33 - v35);
        const Vector v38 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xbe0547p-24f : 0x17c0a8e83f5718p-53)) * v9);
        const Vector v40 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xab8be0p-24f : 0x15717c0a8e83f5p-53)) * v19);
        const Vector v41 = (v38 + v40);
        return {v25[0], v36[0], v31[0], v41[0]};
    }

    struct FixedTaps {};

    template<size_t Capacity, std::uint64_t Seed, size_t Tap0, size_t Tap1 = Tap0, size_t Tap2 = Tap0, size_t Tap3 = Tap0>
    struct AllPass
    {
        static_assert(x_depth + 3 <= Tap0 && Tap0 + x_depth + 1 <= Capacity);
        static_assert(x_depth + 3 <= Tap1 && Tap1 + x_depth + 1 <= Capacity);
        static_assert(x_depth + 3 <= Tap2 && Tap2 + x_depth + 1 <= Capacity);
        static_assert(x_depth + 3 <= Tap3 && Tap3 + x_depth + 1 <= Capacity);
        using Stream = std::conditional_t<Framed, SmoothRandomLFOFramedGeneric<Number>, SmoothRandomLFOStreamGeneric<Number>>;
        using Modulator = std::conditional_t<Wandering, Stream, FixedTaps>;

        CircularBufferGeneric<Number, 4, Capacity> m_history;
        Vector m_gain = AllPassGain();
        Vector m_output;
        Modulator m_lfo = InitModulator();

        static Modulator InitModulator()
        {
            if constexpr (Wandering)
            {
                return Modulator(x_lambdaSamples, Seed);
            }
            else
            {
                return {};
            }
        }

        Vector ReadTaps(size_t ageOffset) const
        {
            if constexpr (Tap0 == Tap1 && Tap0 == Tap2 && Tap0 == Tap3)
            {
                return m_history.ReadAge((Tap0 + ageOffset) % Capacity);
            }
            else
            {
                return {m_history.ReadAge((Tap0 + ageOffset) % Capacity)[0],
                    m_history.ReadAge((Tap1 + ageOffset) % Capacity)[1],
                    m_history.ReadAge((Tap2 + ageOffset) % Capacity)[2],
                    m_history.ReadAge((Tap3 + ageOffset) % Capacity)[3]};
            }
        }

        Vector ReadDelayed()
        {
            if constexpr (Wandering)
            {
                const auto offset = WanderOffset(Vector::Splat(x_depth), Vector::Splat(m_lfo.Process()));
                const auto [rounded, fraction] = FractionalPosition(offset);
                const auto center = static_cast<size_t>(rounded[0]) + Capacity - x_depth - 1;
                return Interpolate(ReadTaps(center + 1), ReadTaps(center),
                    ReadTaps(center - 1), ReadTaps(center - 2), fraction);
            }
            else
            {
                return ReadTaps(Capacity - 1);
            }
        }

        Vector Process(const Vector& audio)
        {
            SchroederAllPassCoreGeneric<Number, 4> core{{m_gain}, {}};
            typename SchroederAllPassCoreGeneric<Number, 4>::Input input{audio, ReadDelayed()};
            core.Process(input);
            m_output = core.m_output.m_y;
            m_history.Push(core.m_output.m_written);
            return m_output;
        }

        Vector Y() const
        {
            return m_output;
        }

    };

    struct Damping
    {
        TPTBaseWidthGeneric<Number, 4> m_filter{};

        void Tune(const Vector& base, const Vector& width)
        {
            const auto [high, low] = DampingCutoffs(base, width);
            m_filter.ComputeCoefficients({{high}, {low}});
        }

        void Process(const Vector& audio)
        {
            typename TPTBaseWidthGeneric<Number, 4>::Input input{audio};
            m_filter.Process(input);
        }

        Vector Y() const
        {
            return m_filter.m_output.m_x;
        }

    };

    template<size_t Capacity>
    struct CubicDelay
    {
        CircularBufferGeneric<Number, 4, Capacity> m_history;

        void Push(const Vector& audio)
        {
            m_history.Push(audio);
        }

        Vector ReadCubic(const Vector& samples) const
        {
            const auto [rounded, fraction] = FractionalPosition(samples);
            Vector older, center, newer, newest;
            for (int ch = 0; ch < 4; ++ch)
            {
                const auto whole = static_cast<size_t>(rounded[ch]);
                older[ch] = m_history.ReadAge(whole % Capacity)[ch];
                center[ch] = m_history.ReadAge((whole + Capacity - 1) % Capacity)[ch];
                newer[ch] = m_history.ReadAge((whole + Capacity - 2) % Capacity)[ch];
                newest[ch] = m_history.ReadAge((whole + 2 * Capacity - 3) % Capacity)[ch];
            }

            return Interpolate(older, center, newer, newest, fraction);
        }

    };

    struct Diffusion
    {
        AllPass<2048, 1, 293> m_stage0;
        AllPass<2048, 2, 487> m_stage1;
        AllPass<2048, 3, 907> m_stage2;
        AllPass<2048, 4, 1433> m_stage3;
        AllPass<8192, 5, 4493> m_stage4;
        AllPass<8192, 6, 6899> m_stage5;
        AllPass<16384, 7, 9811> m_stage6;
        AllPass<16384, 8, 14033> m_stage7;
        Vector m_output;

        void Process(const Vector& input)
        {
            Vector audio = input;
            audio = m_stage0.Process(audio);
            audio = m_stage1.Process(audio);
            audio = m_stage2.Process(audio);
            audio = m_stage3.Process(audio);
            audio = m_stage4.Process(audio);
            audio = m_stage5.Process(audio);
            audio = m_stage6.Process(audio);
            audio = m_stage7.Process(audio);
            m_output = audio;
        }

        Vector Y() const
        {
            return m_output;
        }

    };

    Diffusion m_diffusion;
    CubicDelay<x_delayCapacity> m_delay;
    Vector m_delaySamples;
    Vector m_feedback;
    Vector m_saturatorGain;
    AllPass<8192, 9, 2039, 3719, 6359, 2753> m_firstAllPass;
    Damping m_damping;
    AllPass<32768, 10, 11923, 16879, 9041, 23447> m_secondAllPass;

};
