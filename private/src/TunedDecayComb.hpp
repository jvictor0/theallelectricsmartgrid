#pragma once

#include "MultiChannelNumber.hpp"
#include "SampledTransferFunction.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>

// H0(z) = L(z) / (1 - s z^-N A(z) L(z)).
// L is a two-pole low-pass with a unity peak at the played fundamental.
// Its variable Q tends to Butterworth as DAMP opens; excitation is filtered too.
// Integer delay and allpass A compensate L's phase at the played fundamental.
// Applying H0(z/r) to every memory sets an upper bound on all modal lifetimes.
//
struct TunedDecayCombBase
{
    static_assert(std::atomic<double>::is_always_lock_free, "Audio/UI snapshots require lock-free doubles");

    static constexpr size_t x_maxDelaySamples = 1 << 14;
    static constexpr double x_pi = 3.14159265358979323846;
    static constexpr double x_sqrt2 = 1.41421356237309504880;

    struct Coefficients
    {
        size_t m_delay = 100;
        double m_a = 0.0;
        double m_b = 1.0;
        double m_g = 0.01;
        double m_k = x_sqrt2;
        double m_gain = 1.0;
        double m_h = 1.0;
        double m_r = 0.9;
        double m_delayGain = 0.0;
        bool m_bypass = true;
        double m_sign = 0.0;

        std::complex<double> Numerator(const std::complex<double>& z) const
        {
            if (m_bypass)
            {
                return 1.0;
            }

            auto q = m_r * z;
            double g = m_g;
            double k = m_k;
            double gain = m_gain;
            return gain * g * g * (1.0 + q) * (1.0 + q) /
                ((1.0 + k * g + g * g) + 2.0 * (g * g - 1.0) * q +
                 (1.0 - k * g + g * g) * q * q);
        }

        std::complex<double> Loop(const ResponseFrequency& frequency, const std::complex<double>& numerator) const
        {
            auto z = frequency.m_zInverse;
            auto q = m_r * z;
            double a = m_a;
            auto allpass = (a + q) / (1.0 + a * q);
            return m_sign * m_delayGain *
                static_cast<std::complex<double>>(Math::Polar2pi(1, -frequency.m_cyclesPerSample * m_delay))
                * allpass * numerator;
        }
    };

    // DECAY maps u = |2 * knob - 1| to T60 = 0.001 * 5000^u seconds, kept as
    // log2(T60), negative feedback below the center and dry at it. DAMP sets the
    // low-pass cutoff 1 to 7 octaves above the fundamental.
    //
    static constexpr double x_log2MinDecaySeconds = -9.965784284662087;
    static constexpr double x_log2DecayRange = 12.287712379549449;
    static constexpr double x_minDampOctaves = 1.0;
    static constexpr double x_dampOctaveRange = 6.0;

    // Inputs are already clamped. Reserve one sample in the allpass after
    // compensating low-pass phase. At the highest negative pitches delta may
    // fall below 1; N stays >= 1.
    //
    static Coefficients ComputeCoefficients(double normalizedFrequency, double feedbackSign, double log2DecaySeconds, double dampOctaves, double sampleRate)
    {
        double cycles = feedbackSign < 0.0 ? 0.5 : 1.0;
        double duration = std::exp2(log2DecaySeconds);
        double omega = 2.0 * x_pi * normalizedFrequency;
        Coefficients c;
        c.m_bypass = feedbackSign == 0.0;
        double cutoff = std::min(0.49, normalizedFrequency * std::exp2(dampOctaves));
        double t = std::tan(x_pi * normalizedFrequency);
        double v = std::tan(x_pi * cutoff) / t;
        double shape = std::hypot(1.0, v * v - 1.0);
        double u2 = 1.0 / shape;
        c.m_g = t * std::sqrt(shape);
        c.m_k = std::sqrt(2.0 * (1.0 - u2));
        c.m_gain = std::sqrt((1.0 - u2) * (1.0 + u2));
        c.m_h = 1.0 / (1.0 + c.m_k * c.m_g + c.m_g * c.m_g);
        double filterPhase = c.m_bypass ? 0.0 : -std::atan2(c.m_k * std::sqrt(u2), 1.0 - u2);

        double delay = (2.0 * x_pi * cycles + filterPhase) / omega;
        c.m_delay = static_cast<size_t>(std::max(1.0, std::floor(delay) - 1.0));
        double delta = delay - c.m_delay;
        double td = std::tan(omega * delta * 0.5);
        c.m_a = (t - td) / (t + td);
        c.m_b = std::sqrt(std::max(0.0, 1.0 - c.m_a * c.m_a));
        c.m_r = std::exp(-std::log(1000.0) / (sampleRate * duration));
        c.m_delayGain = std::pow(c.m_r, static_cast<double>(c.m_delay));
        c.m_sign = feedbackSign;
        return c;
    }
};

// Channels share one crossfade schedule: any channel's parameter change starts
// a transition for all of them. An unchanged channel fades between two
// identical branches, so its output is unaffected.
//
template <size_t Size>
struct TunedDecayCombGeneric : TunedDecayCombBase
{
    using Vector = MultiChannelNumber<float, Size>;
    using DoubleVector = MultiChannelNumber<double, Size>;

    // m_bypass holds 1 for bypassed channels and 0 otherwise.
    //
    struct ChannelCoefficients
    {
        MultiChannelNumber<size_t, Size> m_delay;
        DoubleVector m_a;
        DoubleVector m_b;
        DoubleVector m_g;
        DoubleVector m_k;
        DoubleVector m_gain;
        DoubleVector m_h;
        DoubleVector m_r;
        DoubleVector m_delayGain;
        DoubleVector m_bypass;
        DoubleVector m_sign;

        ChannelCoefficients()
        {
            Coefficients c;
            for (size_t i = 0; i < Size; ++i)
            {
                Set(i, c);
            }
        }

        void Set(size_t channel, const Coefficients& c)
        {
            m_delay[channel] = c.m_delay;
            m_a[channel] = c.m_a;
            m_b[channel] = c.m_b;
            m_g[channel] = c.m_g;
            m_k[channel] = c.m_k;
            m_gain[channel] = c.m_gain;
            m_h[channel] = c.m_h;
            m_r[channel] = c.m_r;
            m_delayGain[channel] = c.m_delayGain;
            m_bypass[channel] = c.m_bypass ? 1.0 : 0.0;
            m_sign[channel] = c.m_sign;
        }

        Coefficients Get(size_t channel) const
        {
            Coefficients c;
            c.m_delay = m_delay[channel];
            c.m_a = m_a[channel];
            c.m_b = m_b[channel];
            c.m_g = m_g[channel];
            c.m_k = m_k[channel];
            c.m_gain = m_gain[channel];
            c.m_h = m_h[channel];
            c.m_r = m_r[channel];
            c.m_delayGain = m_delayGain[channel];
            c.m_bypass = m_bypass[channel] != 0.0;
            c.m_sign = m_sign[channel];
            return c;
        }
    };

    struct Snapshot : VectorResponseSnapshot<Size>
    {
        using Base = VectorResponseSnapshot<Size>;
        using typename Base::ComplexVector;
        using typename Base::ResponseVector;
        ChannelCoefficients m_branch[2];
        double m_mix = 0;

        struct Fraction
        {
            ComplexVector m_numerator;
            ComplexVector m_denominator;
        };

        static ComplexVector Numerator(const ChannelCoefficients& c, const ResponseFrequency& frequency)
        {
            if (c.m_bypass == DoubleVector::Splat(1))
            {
                return 1.0;
            }

            auto q = ComplexVector(frequency.m_zInverse) * c.m_r;
            auto onePlus = ComplexVector(1.0) + q;
            auto g2 = c.m_g * c.m_g;
            auto kg = c.m_k * c.m_g;
            auto numerator = onePlus * onePlus * (c.m_gain * g2);
            auto denominator = ComplexVector(DoubleVector::Splat(1) + kg + g2, DoubleVector()) + q * ((g2 - 1) * 2)
                + q * q * (DoubleVector::Splat(1) - kg + g2);
            auto response = numerator / denominator;
            return {DoubleVector::IfLess(c.m_bypass, DoubleVector::Splat(0.5), response.m_real, DoubleVector::Splat(1)),
                DoubleVector::IfLess(c.m_bypass, DoubleVector::Splat(0.5), response.m_imag, DoubleVector())};
        }

        static ComplexVector Loop(const ChannelCoefficients& c, const ResponseFrequency& frequency,
            const ComplexVector& numerator)
        {
            if (c.m_sign == DoubleVector())
            {
                return {};
            }

            auto q = ComplexVector(frequency.m_zInverse) * c.m_r;
            auto allpass = (ComplexVector(c.m_a, DoubleVector()) + q) / (ComplexVector(1.0) + q * c.m_a);
            auto delay = Math::Polar2pi(ResponseVector::Splat(1),
                c.m_delay.template Convert<double>() * -frequency.m_cyclesPerSample).template Convert<double>();
            return delay * allpass * numerator * (c.m_sign * c.m_delayGain);
        }

        Fraction Evaluate(const ResponseFrequency& frequency) const
        {
            auto first = Numerator(m_branch[0], frequency);
            auto loop = Loop(m_branch[0], frequency, first);
            if (m_mix == 0)
            {
                return {first, ComplexVector(1.0) - loop};
            }

            auto second = Numerator(m_branch[1], frequency);
            loop = loop * (1 - m_mix) + Loop(m_branch[1], frequency, second) * m_mix;
            return {first * (1 - m_mix) + second * m_mix, ComplexVector(1.0) - loop};
        }

        ComplexVector ComplexResponses(const ResponseFrequency& frequency) const override
        {
            auto response = Evaluate(frequency);
            return response.m_numerator / response.m_denominator;
        }

        ResponseVector MagnitudeResponses(const ResponseFrequency& frequency) const override
        {
            auto response = Evaluate(frequency);
            return (response.m_numerator.Norm() / response.m_denominator.Norm()).Sqrt().template Convert<float>();
        }
    };

    using UIState = TransferFunctionGeneric<Snapshot>;

    struct Branch
    {
        ChannelCoefficients m_coeff;
        DoubleVector m_allpass;
        DoubleVector m_ic1;
        DoubleVector m_ic2;

        DoubleVector Process(const DoubleVector& input, const DoubleVector& delayed)
        {
            const ChannelCoefficients& c = m_coeff;
            DoubleVector x = delayed * c.m_delayGain;
            DoubleVector memory = c.m_r * m_allpass;
            DoubleVector allpass = c.m_a * x + c.m_b * memory;
            m_allpass = c.m_b * x - c.m_a * memory;
            DoubleVector excitation = input + c.m_sign * allpass;

            DoubleVector ic1 = c.m_r * m_ic1;
            DoubleVector ic2 = c.m_r * m_ic2;
            DoubleVector v1 = c.m_h * (ic1 + c.m_g * (excitation - ic2));
            DoubleVector v2 = ic2 + c.m_g * v1;

            // Bypassed channels clear dormant integrators so bypass cannot
            // accumulate energy that resurfaces when a later branch re-enables
            // the low-pass.
            //
            DoubleVector half = DoubleVector::Splat(0.5);
            DoubleVector zero = DoubleVector::Splat(0);
            m_ic1 = DoubleVector::IfLess(c.m_bypass, half, v1 * 2 - ic1, zero);
            m_ic2 = DoubleVector::IfLess(c.m_bypass, half, v2 * 2 - ic2, zero);
            return DoubleVector::IfLess(c.m_bypass, half, c.m_gain * v2, excitation);
        }
    };

    // Clamped physical parameters: pitch in cycles per sample, feedback sign
    // (-1, 0 for dry, +1), log2 of T60 in seconds, and cutoff octaves above
    // the fundamental.
    //
    struct Params
    {
        DoubleVector m_frequency = DoubleVector::Splat(-1.0);
        DoubleVector m_feedbackSign;
        DoubleVector m_log2DecaySeconds;
        DoubleVector m_dampOctaves;
        double m_sampleRate = -1.0;

        bool ChannelDiffers(const Params& other, size_t channel) const
        {
            return m_sampleRate != other.m_sampleRate ||
                m_frequency[channel] != other.m_frequency[channel] ||
                m_feedbackSign[channel] != other.m_feedbackSign[channel] ||
                m_log2DecaySeconds[channel] != other.m_log2DecaySeconds[channel] ||
                m_dampOctaves[channel] != other.m_dampOctaves[channel];
        }

        bool operator!=(const Params& other) const
        {
            return m_sampleRate != other.m_sampleRate ||
                m_frequency != other.m_frequency ||
                m_feedbackSign != other.m_feedbackSign ||
                m_log2DecaySeconds != other.m_log2DecaySeconds ||
                m_dampOctaves != other.m_dampOctaves;
        }
    };

    std::array<DoubleVector, x_maxDelaySamples> m_delay{};
    size_t m_index = 0;
    Branch m_branch[2];
    ChannelCoefficients m_target;
    Params m_params;
    Params m_targetParams;
    double m_mix = 0.0;
    double m_step = 1.0;
    bool m_transition = false;
    bool m_pending = false;
    bool m_initialized = false;

    // Cheap enough to call every sample: only stores the mapped parameters.
    // Coefficients follow when the next crossfade starts. Frequencies are
    // normalized to the processing rate.
    //
    void SetParams(const DoubleVector& normalizedFrequency, const DoubleVector& decayKnob, const DoubleVector& dampKnob, double sampleRate)
    {
        DoubleVector zero = DoubleVector::Splat(0);
        DoubleVector half = DoubleVector::Splat(0.5);
        DoubleVector one = DoubleVector::Splat(1);
        DoubleVector decay = DoubleVector::Min(DoubleVector::Max(decayKnob, zero), one);
        DoubleVector damp = DoubleVector::Min(DoubleVector::Max(dampKnob, zero), one);
        DoubleVector sign = DoubleVector::IfLess(decay, half, -one, DoubleVector::IfLess(half, decay, one, zero));

        // Negative feedback resonates at half a cycle per period.
        //
        DoubleVector cycles = DoubleVector::IfLess(sign, zero, half, one);
        DoubleVector minFrequency = cycles / static_cast<double>(x_maxDelaySamples - 1);

        m_params.m_frequency = DoubleVector::Min(DoubleVector::Max(normalizedFrequency, minFrequency), DoubleVector::Splat(0.25));
        m_params.m_feedbackSign = sign;
        m_params.m_log2DecaySeconds = (decay * 2 - 1).Abs() * x_log2DecayRange + x_log2MinDecaySeconds;
        m_params.m_dampOctaves = damp * x_dampOctaveRange + x_minDampOctaves;
        m_params.m_sampleRate = std::max(1.0, sampleRate);

        if (!m_initialized)
        {
            UpdateTarget();
            m_branch[0].m_coeff = m_target;
            m_branch[1].m_coeff = m_target;
            m_initialized = true;
        }
        else
        {
            m_pending = m_params != m_targetParams;
        }
    }

    // Recomputes coefficients for the channels whose parameters changed.
    //
    bool UpdateTarget()
    {
        bool changed = false;
        for (size_t i = 0; i < Size; ++i)
        {
            if (m_params.ChannelDiffers(m_targetParams, i))
            {
                m_target.Set(i, ComputeCoefficients(
                    m_params.m_frequency[i],
                    m_params.m_feedbackSign[i],
                    m_params.m_log2DecaySeconds[i],
                    m_params.m_dampOctaves[i],
                    m_params.m_sampleRate));
                changed = true;
            }
        }

        m_targetParams = m_params;
        m_step = 1.0 / std::max(1.0, 0.005 * m_params.m_sampleRate);
        return changed;
    }

    DoubleVector ReadDelay(const ChannelCoefficients& c) const
    {
        DoubleVector result;
        for (size_t i = 0; i < Size; ++i)
        {
            result[i] = m_delay[(m_index + x_maxDelaySamples - c.m_delay[i]) % x_maxDelaySamples][i];
        }

        return result;
    }

    Vector Process(const Vector& input)
    {
        if (!m_transition && m_pending)
        {
            m_pending = false;
            if (UpdateTarget())
            {
                m_branch[1] = m_branch[0];
                m_branch[1].m_coeff = m_target;
                m_mix = 0.0;
                m_transition = true;
            }
        }

        DoubleVector in = input.template Convert<double>();
        DoubleVector output = m_branch[0].Process(in, ReadDelay(m_branch[0].m_coeff));
        if (m_transition)
        {
            DoubleVector next = m_branch[1].Process(in, ReadDelay(m_branch[1].m_coeff));
            output += (next - output) * m_mix;
            m_mix = std::min(1.0, m_mix + m_step);
            if (m_mix >= 1.0)
            {
                m_branch[0] = m_branch[1];
                m_mix = 0.0;
                m_transition = false;
            }
        }

        m_delay[m_index] = output;
        m_index = (m_index + 1) % x_maxDelaySamples;
        return output.template Convert<float>();
    }

    void Reset()
    {
        m_delay.fill(DoubleVector());
        m_index = 0;
        UpdateTarget();
        for (auto& branch : m_branch)
        {
            branch = Branch{};
            branch.m_coeff = m_target;
        }

        m_mix = 0.0;
        m_transition = false;
        m_pending = false;
    }

    Snapshot MakeSnapshot() const
    {
        Snapshot snapshot;
        snapshot.m_branch[0] = m_branch[0].m_coeff;
        snapshot.m_branch[1] = m_branch[1].m_coeff;
        snapshot.m_mix = m_mix;
        return snapshot;
    }

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

    void PopulateUIState(UIState* state, size_t channel) const
    {
        state->m_history.Push(MakeSnapshot());
    }

    void PopulateUIState(UIState* state) const
    {
        static_assert(Size == 1, "PopulateUIState without a channel requires one channel");
        PopulateUIState(state, 0);
    }
};

using TunedDecayComb = TunedDecayCombGeneric<1>;
using TrioTunedDecayComb = TunedDecayCombGeneric<3>;
