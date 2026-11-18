#pragma once

#include "Filter.hpp"
#include "Math.hpp"
#include "MultiChannelNumber.hpp"
#include "Saturator.hpp"
#include "TPTSVFFilter.hpp"
#include <cmath>
#include <complex>

template <typename Number, size_t Size>
struct BandPassedEnvelopeFollowerGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr Number x_rmsCycles = static_cast<Number>(2);

    TPTSVFBandPassFilterGeneric<Number, Size> m_stage1;
    TPTSVFBandPassFilterGeneric<Number, Size> m_stage2;
    Vector m_power;
    Vector m_powerAlpha;

    BandPassedEnvelopeFollowerGeneric()
        : m_power()
        , m_powerAlpha()
    {
        // Q = 1 gives each bandpass unity gain at its center frequency.
        //
        m_stage1.SetCutoffAndQ(Vector::Splat(0.1), Vector::Splat(1));
        m_stage2.SetCutoffAndQ(Vector::Splat(0.1), Vector::Splat(1));
    }

    void SetCutoff(const Vector& cutoff, const Vector& envelopeCutoff)
    {
        m_stage1.SetCutoffAndQ(cutoff, Vector::Splat(1));
        m_stage2.SetCutoffAndQ(cutoff, Vector::Splat(1));
        for (size_t i = 0; i < Size; ++i)
        {
            m_powerAlpha[i] = -std::expm1(-envelopeCutoff[i] / x_rmsCycles);
        }
    }

    Vector Process(const Vector& input)
    {
        m_stage1.Process(input);
        m_stage2.Process(m_stage1.GetBandPass());
        Vector bandPass = m_stage2.GetBandPass();
        m_power += m_powerAlpha * (bandPass * bandPass - m_power);
        return m_power;
    }

    Vector GetPower() const
    {
        return m_power;
    }

    void Reset()
    {
        m_stage1.Reset();
        m_stage2.Reset();
        m_power = Vector();
    }
};

using BandPassedEnvelopeFollower = BandPassedEnvelopeFollowerGeneric<float, 1>;

template <typename Number, size_t Size>
struct LadderFilterLPGeneric
{
    static constexpr float x_maxCutoff = 0.499f;
    static constexpr float x_dampingEpsilon = 0.0001f;

    static std::complex<float> TransferFunction(float alpha, float feedback, float freq)
    {
        Snapshot snapshot;
        snapshot.m_alpha = Vector::Splat(alpha);
        snapshot.m_feedback = Vector::Splat(feedback);
        return snapshot.ComplexResponse(ResponseFrequency::CyclesPerSample(freq), 0);
    }

    static float FrequencyResponse(float alpha, float feedback, float freq)
    {
        Snapshot snapshot;
        snapshot.m_alpha = Vector::Splat(alpha);
        snapshot.m_feedback = Vector::Splat(feedback);
        return snapshot.MagnitudeResponse(ResponseFrequency::CyclesPerSample(freq), 0);
    }

    using Vector = MultiChannelNumber<Number, Size>;
    using Saturator = TanhSaturatorGeneric<Number, Size, true>;

    struct Snapshot : VectorResponseSnapshot<Size>
    {
        using Base = VectorResponseSnapshot<Size>;
        using typename Base::ComplexVector;
        using typename Base::Frequency;
        using typename Base::DoubleVector;
        using typename Base::ResponseVector;
        Vector m_alpha;
        Vector m_feedback;

        ComplexVector Denominator(const Frequency& frequency) const
        {
            auto a = m_alpha.template Convert<double>();
            auto b = DoubleVector::Splat(1) - a;
            ComplexVector stage(a + b * (frequency.m_sinHalf * 2 * frequency.m_sinHalf),
                b * (frequency.m_sinHalf * 2 * frequency.m_cosHalf));
            auto square = stage * stage;
            auto denominator = square * square + frequency.m_zInverse
                * (m_feedback.template Convert<double>() * a * a * a * a);
            denominator.m_real = DoubleVector::IfLess(DoubleVector(), a, denominator.m_real, DoubleVector::Splat(1));
            return denominator;
        }

        DoubleVector Numerator() const
        {
            auto a = m_alpha.template Convert<double>();
            return (DoubleVector::Splat(1) + m_feedback.template Convert<double>()) * a * a * a * a;
        }

        ComplexVector ComplexResponses(const Frequency& frequency) const override
        {
            return ComplexVector(Numerator(), DoubleVector()) / Denominator(frequency);
        }

        ResponseVector MagnitudeSquaredResponses(const Frequency& frequency) const override
        {
            return (Numerator() * Numerator() / Denominator(frequency).Norm()).template Convert<float>();
        }
    };

    using UIState = TransferFunctionGeneric<Snapshot>;

    Snapshot MakeSnapshot() const
    {
        Snapshot snapshot;
        snapshot.m_alpha = m_stage4.m_alpha;
        snapshot.m_feedback = m_kEff;
        return snapshot;
    }

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

    OPLowPassFilterGeneric<Number, Size> m_stage1;
    OPLowPassFilterGeneric<Number, Size> m_stage2;
    OPLowPassFilterGeneric<Number, Size> m_stage3;
    OPLowPassFilterGeneric<Number, Size> m_stage4;

    // Normalized tanh saturation after each stage, Tanh(gain * x) / Tanh(gain),
    // with a per-channel gain.
    //
    Vector m_saturationGain;
    Vector m_saturationTanhGain;

    Vector m_cutoff;
    Vector m_feedback;

    Vector m_input;
    Vector m_output;
    Vector m_kEff;

    BandPassedEnvelopeFollowerGeneric<Number, Size> m_inputEnvelope;
    BandPassedEnvelopeFollowerGeneric<Number, Size> m_feedbackEnvelope;
    Vector m_criticalFeedback;

    // Parameter-only feedback terms. The envelope-dependent reduction remains
    // in Process. The Nyquist guard is the existing heuristic, not a proof of
    // stability of the complete nonlinear, time-varying loop.
    //
    Vector m_kSafe;
    Vector m_baseFeedback;
    Vector m_referencePowerGain;
    Vector m_dampingScale;
    bool m_coefficientsInitialized;

    LadderFilterLPGeneric()
        : m_saturationGain()
        , m_saturationTanhGain()
        , m_cutoff(Vector::Splat(static_cast<Number>(0.1)))
        , m_feedback()
        , m_input()
        , m_output()
        , m_kEff()
        , m_criticalFeedback(Vector::Splat(4))
        , m_kSafe()
        , m_baseFeedback()
        , m_referencePowerGain()
        , m_dampingScale()
        , m_coefficientsInitialized(false)
    {
        SetSaturationGain(Vector::Splat(static_cast<Number>(0.5)));
    }

    void PopulateUIState(UIState* uiState, size_t channel) const
    {
        uiState->m_history.Push(MakeSnapshot());
    }

    void PopulateUIState(UIState* uiState) const
    {
        static_assert(Size == 1, "multi-channel filters must name the channel to publish");
        PopulateUIState(uiState, 0);
    }

    Vector Saturate(const Vector& input) const
    {
        return Saturator::Tanh(input * m_saturationGain) / m_saturationTanhGain;
    }

    // Small-signal gain of Saturate at zero.
    //
    Vector SaturationDerivativeZero() const
    {
        return m_saturationGain / m_saturationTanhGain;
    }

    Vector Process(const Vector& input)
    {
        // Compare the previous sample's mean-square estimates in power units.
        // Epsilon squared is the input power floor; epsilon scales excess power.
        //
        Vector inputPower = m_inputEnvelope.GetPower();
        Vector feedbackPower = m_feedbackEnvelope.GetPower();
        Vector excessPower = Vector::Max(Vector(),
            feedbackPower - m_referencePowerGain * inputPower)
            / (inputPower + x_dampingEpsilon * x_dampingEpsilon);

        m_kEff = m_baseFeedback / (m_dampingScale * excessPower + 1);

        // Feed back the previous sample's unsaturated fourth-stage state.
        //
        Vector feedbackInput = input - m_kEff * m_stage4.m_output;
        m_input = feedbackInput;

        // Process through the 4-pole ladder with saturation after each stage
        //
        Vector stage1Pre = m_stage1.Process(feedbackInput);
        Vector stage1Out = Saturate(stage1Pre);

        Vector stage2Pre = m_stage2.Process(stage1Out);
        Vector stage2Out = Saturate(stage2Pre);

        Vector stage3Pre = m_stage3.Process(stage2Out);
        Vector stage3Out = Saturate(stage3Pre);

        Vector stage4Pre = m_stage4.Process(stage3Out);
        Vector stage4Out = Saturate(stage4Pre);

        m_inputEnvelope.Process(input);
        m_feedbackEnvelope.Process(stage4Pre);

        // Output gain compensation to counter resonance-induced passband drop
        // Maintains approximately unity DC gain as resonance increases
        //
        m_output = stage4Out * (m_kEff + 1);
        return m_output;
    }

    void SetSaturationGain(const Vector& gain)
    {
        if (gain != m_saturationGain)
        {
            m_saturationGain = gain;
            m_saturationTanhGain = Saturator::Tanh(gain);
            UpdateFeedbackCoefficients();
        }
    }

    // Cutoff is the unit-gain, zero-feedback four-stage cascade's -3.0103 dB
    // frequency. The normalized saturators and adaptive feedback are separate.
    // Preserve the existing resonance mapping, 0..1 -> feedback 0..3.9.
    //
    void SetCutoffAndResonance(const Vector& cutoffParam, const Vector& resonance)
    {
        const Vector cutoff = Vector::Min(Vector::Splat(x_maxCutoff), cutoffParam);
        const Vector feedback = resonance * static_cast<Number>(3.9);
        const bool cutoffChanged = !m_coefficientsInitialized || cutoff != m_cutoff;
        const bool feedbackChanged = feedback != m_feedback;
        m_feedback = feedback;
        if (cutoffChanged)
        {
            m_cutoff = cutoff;
            const Vector c = Math::Cos2pi(cutoff);

            // Shared quadratic solution: alpha = (root - d)/(1-k), while
            // poleRatio = (root + d)/(1-k) is mathematically alpha/(1-alpha).
            // Derive both from cutoff; do not reconstruct detector tuning by
            // inverting the rounded alpha. Keep the existing alpha arithmetic.
            //
            static constexpr Number x_stagePower = static_cast<Number>(0.84089641525);
            const Vector d = (Vector::Splat(1) - c) * x_stagePower;
            const Vector root = (d * (d + 2 * (1 - x_stagePower))).Sqrt();
            const Vector alpha = (root - d) / (1 - x_stagePower);
            const Vector poleRatio = (root + d) / (1 - x_stagePower);
            m_stage1.m_alpha = alpha;
            m_stage2.m_alpha = alpha;
            m_stage3.m_alpha = alpha;
            m_stage4.m_alpha = alpha;

            // For the unit-gain loop z^-1 * H(z)^4, the first -180-degree
            // crossing is asin(poleRatio/2)/pi below poleRatio=2, else Nyquist.
            // Its critical feedback is (poleRatio+2)^2 below that boundary,
            // and ((poleRatio+2)/poleRatio)^4 above it.
            //
            Vector detectorCutoff = Vector::Splat(static_cast<Number>(0.5));
            for (size_t lane = 0; lane < Size; ++lane)
            {
                const Number ratio = poleRatio[lane];
                if (ratio < 2)
                {
                    const Number crossingGain = ratio + 2;
                    m_criticalFeedback[lane] = crossingGain * crossingGain;
                    detectorCutoff[lane] = std::asin(ratio / 2) / static_cast<Number>(M_PI);
                }
                else
                {
                    const Number crossingGain = (ratio + 2) / ratio;
                    const Number squared = crossingGain * crossingGain;
                    m_criticalFeedback[lane] = squared * squared;
                }
            }

            // Detector centers follow the loop crossing. Envelope timing retains
            // the original -3 dB cutoff, in engine cycles/sample.
            //
            m_inputEnvelope.SetCutoff(detectorCutoff, cutoff);
            m_feedbackEnvelope.SetCutoff(detectorCutoff, cutoff);
            m_coefficientsInitialized = true;
        }

        if (cutoffChanged || feedbackChanged)
        {
            UpdateFeedbackCoefficients();
        }
    }

    void UpdateFeedbackCoefficients()
    {
        const Vector smallSignalGain = SaturationDerivativeZero();
        const Vector perStageGain = (m_stage4.m_alpha / (Vector::Splat(2) - m_stage4.m_alpha)) * smallSignalGain;
        const Vector squared = perStageGain * perStageGain;
        const Vector nyquistGain = squared * squared;
        m_kSafe = Vector::Splat(static_cast<Number>(0.9)) / (nyquistGain + static_cast<Number>(1e-8));
        m_baseFeedback = Vector::Min(m_feedback, m_kSafe);

        // Unit-gain reference at the loop crossing, before output compensation.
        // Three normalized saturators contribute to the adaptive damping strength.
        //
        const Vector referenceGain = Vector::Splat(1) / (m_criticalFeedback - m_baseFeedback);
        m_referencePowerGain = referenceGain * referenceGain;
        m_dampingScale = smallSignalGain * smallSignalGain * smallSignalGain * x_dampingEpsilon;
    }

    void Reset()
    {
        m_input = Vector();
        m_output = Vector();
        m_kEff = Vector();
        m_stage1.m_output = Vector();
        m_stage2.m_output = Vector();
        m_stage3.m_output = Vector();
        m_stage4.m_output = Vector();
        m_inputEnvelope.Reset();
        m_feedbackEnvelope.Reset();
    }
};

using LadderFilterLP = LadderFilterLPGeneric<float, 1>;
using TrioLadderFilterLP = LadderFilterLPGeneric<float, 3>;
