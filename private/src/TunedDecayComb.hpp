#pragma once

#include "MultiChannelNumber.hpp"
#include "SampledTransferFunction.hpp"
#include "CircularBufferGeneric.hpp"
#include "TunedDecayCombCoreGeneric.hpp"
#include "TunedDecayCombTuningGeneric.hpp"
#include "TunedDecayCombResponseGeneric.hpp"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cassert>

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
        using Tuning = TunedDecayCombTuningGeneric<double, 1>;
        using Vector = Tuning::Vector;
        Tuning::Input input{Vector::Splat(normalizedFrequency), Vector::Splat(feedbackSign),
            Vector::Splat(log2DecaySeconds), Vector::Splat(dampOctaves), Vector::Splat(sampleRate)};
        const auto prepared = Tuning::Prepare(input);
        assert(prepared.m_delay[0] >= 1 && prepared.m_delay[0] <= x_maxDelaySamples);
        Coefficients c;
        c.m_delay = static_cast<size_t>(prepared.m_delay[0]);
        c.m_a = prepared.m_a[0];
        c.m_b = prepared.m_b[0];
        c.m_g = prepared.m_g[0];
        c.m_k = prepared.m_k[0];
        c.m_gain = prepared.m_gain[0];
        c.m_h = prepared.m_h[0];
        c.m_r = prepared.m_r[0];
        c.m_delayGain = prepared.m_delayGain[0];
        c.m_bypass = prepared.m_bypass[0] != 0;
        c.m_sign = prepared.m_sign[0];
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

        template<typename Generated>
        void CopyTo(Generated& destination) const
        {
            destination.m_delay = m_delay.template Convert<double>();
            destination.m_a = m_a;
            destination.m_b = m_b;
            destination.m_r = m_r;
            destination.m_delayGain = m_delayGain;
            destination.m_g = m_g;
            destination.m_k = m_k;
            destination.m_h = m_h;
            destination.m_gain = m_gain;
            destination.m_sign = m_sign;
            destination.m_bypass = m_bypass;
        }
    };

    struct Snapshot : VectorResponseSnapshot<Size>
    {
        using Base = VectorResponseSnapshot<Size>;
        using typename Base::ComplexVector;
        using typename Base::Frequency;
        using typename Base::ResponseVector;
        ChannelCoefficients m_branch[2];
        double m_mix = 0;

        struct Fraction
        {
            ComplexVector m_numerator;
            ComplexVector m_denominator;
        };

        Fraction Evaluate(const Frequency& frequency) const
        {
            using Response = TunedDecayCombResponseGeneric<double, Size>;
            Response response{};
            m_branch[0].CopyTo(response.m_state.m_first);
            m_branch[1].CopyTo(response.m_state.m_second);
            response.m_state.m_mix = DoubleVector::Splat(m_mix);
            typename Response::Input input{frequency.m_cyclesPerSample, frequency.m_sinHalf, frequency.m_cosHalf,
                {frequency.m_zInverse.m_real, frequency.m_zInverse.m_imag}};
            response.Process(input);
            const auto& result = response.m_output;
            return {{result.m_numerator.m_real, result.m_numerator.m_imag},
                {result.m_denominator.m_real, result.m_denominator.m_imag}};
        }

        ComplexVector ComplexResponses(const Frequency& frequency) const override
        {
            auto response = Evaluate(frequency);
            return response.m_numerator / response.m_denominator;
        }

        ResponseVector MagnitudeSquaredResponses(const Frequency& frequency) const override
        {
            auto response = Evaluate(frequency);
            return (response.m_numerator.Norm() / response.m_denominator.Norm()).template Convert<float>();
        }
    };

    using UIState = TransferFunctionGeneric<Snapshot>;

    struct Branch
    {
        using Core = TunedDecayCombCoreGeneric<double, Size>;

        Core m_core{};
        MultiChannelNumber<size_t, Size> m_delay;

        Branch()
        {
            SetCoefficients(ChannelCoefficients{});
        }

        // Install arithmetic coefficients and native delay taps together,
        // preserving the core's live filter memories during a retune.
        //
        void SetCoefficients(const ChannelCoefficients& coefficients)
        {
            coefficients.CopyTo(m_core.m_state.m_coefficients);
            m_delay = coefficients.m_delay;
        }

        ChannelCoefficients GetCoefficients() const
        {
            const auto& source = m_core.m_state.m_coefficients;
            ChannelCoefficients c;
            c.m_delay = m_delay;
            c.m_a = source.m_a;
            c.m_b = source.m_b;
            c.m_r = source.m_r;
            c.m_delayGain = source.m_delayGain;
            c.m_g = source.m_g;
            c.m_k = source.m_k;
            c.m_h = source.m_h;
            c.m_gain = source.m_gain;
            c.m_sign = source.m_sign;
            c.m_bypass = source.m_bypass;
            return c;
        }

        DoubleVector Process(const DoubleVector& input, const DoubleVector& delayed)
        {
            typename Core::Input sample{input, delayed};
            m_core.Process(sample);
            return m_core.m_output.m_y;
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

    CircularBufferGeneric<double, Size, x_maxDelaySamples> m_buffer;
    Branch m_branch[2];
    ChannelCoefficients m_target;
    Params m_params;
    Params m_targetParams;
    double m_mix = 0.0;
    double m_step = 1.0;
    bool m_transition = false;
    bool m_pending = false;
    bool m_initialized = false;

    DoubleVector GetTargetResonanceFrequency() const
    {
        const auto active = m_targetParams.m_feedbackSign.Load() != 0.0;
        return DoubleVector::FromVector(active ? m_targetParams.m_frequency.Load() : typename DoubleVector::Vector{});
    }

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
            m_branch[0].SetCoefficients(m_target);
            m_branch[1].SetCoefficients(m_target);
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

    DoubleVector ReadDelay(const Branch& branch) const
    {
        return m_buffer.ReadDelays(branch.m_delay);
    }

    Vector Process(const Vector& input)
    {
        if (!m_transition && m_pending)
        {
            m_pending = false;
            if (UpdateTarget())
            {
                m_branch[1] = m_branch[0];
                m_branch[1].SetCoefficients(m_target);
                m_mix = 0.0;
                m_transition = true;
            }
        }

        DoubleVector in = input.template Convert<double>();
        DoubleVector output = m_branch[0].Process(in, ReadDelay(m_branch[0]));
        if (m_transition)
        {
            DoubleVector next = m_branch[1].Process(in, ReadDelay(m_branch[1]));
            output += (next - output) * m_mix;
            m_mix = std::min(1.0, m_mix + m_step);
            if (m_mix >= 1.0)
            {
                m_branch[0] = m_branch[1];
                m_mix = 0.0;
                m_transition = false;
            }
        }

        m_buffer.Push(output);
        return output.template Convert<float>();
    }

    void Reset()
    {
        m_buffer.Clear();
        UpdateTarget();
        for (auto& branch : m_branch)
        {
            branch = Branch{};
            branch.SetCoefficients(m_target);
        }

        m_mix = 0.0;
        m_transition = false;
        m_pending = false;
    }

    Snapshot MakeSnapshot() const
    {
        Snapshot snapshot;
        snapshot.m_branch[0] = m_branch[0].GetCoefficients();
        snapshot.m_branch[1] = m_branch[1].GetCoefficients();
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
using StereoTunedDecayComb = TunedDecayCombGeneric<2>;
using TrioTunedDecayComb = TunedDecayCombGeneric<3>;
using QuadTunedDecayComb = TunedDecayCombGeneric<4>;
