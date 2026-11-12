#pragma once

#include "Filter.hpp"
#include "Math.hpp"
#include "MultiChannelNumber.hpp"
#include "Saturator.hpp"
#include "DebugMeter.hpp"
#include "StateVariableFilter.hpp"
#include <atomic>
#include <cmath>
#include <complex>

template <typename Number, size_t Size>
struct BandPassedEnvelopeFollowerGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr Number x_rmsCycles = static_cast<Number>(2);

    LinearStateVariableFilterGeneric<Number, Size> m_stage1;
    LinearStateVariableFilterGeneric<Number, Size> m_stage2;
    Vector m_power;
    Vector m_powerAlpha;

    BandPassedEnvelopeFollowerGeneric()
        : m_power()
        , m_powerAlpha()
    {
        // Q = 1 gives each bandpass unity gain at its center frequency.
        //
        m_stage1.SetCutoffAndQ(m_stage1.m_cutoff, Vector::Splat(1));
        m_stage2.SetCutoffAndQ(m_stage2.m_cutoff, Vector::Splat(1));
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
        snapshot.m_alpha = alpha;
        snapshot.m_feedback = feedback;
        return snapshot.ComplexResponse(ResponseFrequency::CyclesPerSample(freq), 0);
    }

    static float FrequencyResponse(float alpha, float feedback, float freq)
    {
        Snapshot snapshot;
        snapshot.m_alpha = alpha;
        snapshot.m_feedback = feedback;
        return snapshot.MagnitudeResponse(ResponseFrequency::CyclesPerSample(freq), 0);
    }

    using Vector = MultiChannelNumber<Number, Size>;
    using Saturator = TanhSaturatorGeneric<Number, Size, true>;

    struct Snapshot : FilterResponseSnapshot
    {
        float m_alpha = 0;
        float m_feedback = 0;

        // Four one-poles with the existing delayed feedback and DC compensation.
        // Multiply through by the stage denominator to avoid complex divisions.
        //
        std::complex<double> Denominator(const ResponseFrequency& frequency) const
        {
            double b = 1 - static_cast<double>(m_alpha);
            auto stage = std::complex<double>(m_alpha + 2 * b * frequency.m_sinHalf * frequency.m_sinHalf,
                2 * b * frequency.m_sinHalf * frequency.m_cosHalf);
            auto square = stage * stage;
            double a = m_alpha;
            return square * square + static_cast<double>(m_feedback) * a * a * a * a * frequency.m_zInverse;
        }

        double Numerator() const
        {
            double a = m_alpha;
            return (1.0 + m_feedback) * a * a * a * a;
        }

        std::complex<float> ComplexResponse(const ResponseFrequency& frequency, size_t) const override
        {
            return m_alpha == 0 ? std::complex<float>{}
                : static_cast<std::complex<float>>(Numerator() / Denominator(frequency));
        }

        float MagnitudeResponse(const ResponseFrequency& frequency, size_t) const override
        {
            return m_alpha == 0 ? 0 : static_cast<float>(Numerator() / std::abs(Denominator(frequency)));
        }

    };

    using UIState = TransferFunctionGeneric<Snapshot>;

    Snapshot MakeSnapshot(size_t channel) const
    {
        Snapshot snapshot;
        snapshot.m_alpha = m_stage4.m_alpha[channel];
        snapshot.m_feedback = m_kEff[channel];
        return snapshot;
    }

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer, size_t channel) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot(channel);
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

    // Debug meters on the first channel: input, feedback input, output
    //
    static constexpr bool x_debugMetersOn = false;
    DebugMeter<x_debugMetersOn> m_meterInput;
    DebugMeter<x_debugMetersOn> m_meterFbInput;
    DebugMeter<x_debugMetersOn> m_meterOutput;

    // Debug meters: before and after saturation for each stage
    //
    DebugMeter<x_debugMetersOn> m_meterStage1Pre;
    DebugMeter<x_debugMetersOn> m_meterStage1Post;
    DebugMeter<x_debugMetersOn> m_meterStage2Pre;
    DebugMeter<x_debugMetersOn> m_meterStage2Post;
    DebugMeter<x_debugMetersOn> m_meterStage3Pre;
    DebugMeter<x_debugMetersOn> m_meterStage3Post;
    DebugMeter<x_debugMetersOn> m_meterStage4Pre;
    DebugMeter<x_debugMetersOn> m_meterStage4Post;

    LadderFilterLPGeneric()
        : m_saturationGain()
        , m_saturationTanhGain()
        , m_cutoff(Vector::Splat(static_cast<Number>(0.1)))
        , m_feedback()
        , m_input()
        , m_output()
        , m_kEff()
        , m_criticalFeedback(Vector::Splat(4))
    {
        SetSaturationGain(Vector::Splat(static_cast<Number>(0.5)));
    }

    void PopulateUIState(UIState* uiState, size_t channel) const
    {
        uiState->m_history.Push(MakeSnapshot(channel));
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
        m_meterInput.Process(input[0]);

        // Calculate effective Nyquist gain including saturation small-signal gain
        //
        Vector smallSignalGain = SaturationDerivativeZero();
        Vector perStageGain = (m_stage4.m_alpha / (Vector::Splat(2) - m_stage4.m_alpha)) * smallSignalGain;
        Vector perStageGainSquared = perStageGain * perStageGain;
        Vector Cpi = perStageGainSquared * perStageGainSquared;
        Vector kSafe = Vector::Splat(static_cast<Number>(0.9)) / (Cpi + static_cast<Number>(1e-8));
        Vector baseFeedback = Vector::Min(m_feedback, kSafe);

        // At the ideal loop's 180-degree crossover, the raw LP gain is
        // 1 / (critical feedback - feedback). No saturation or output gain.
        //
        Vector referenceGain = Vector::Splat(1) / (m_criticalFeedback - baseFeedback);

        // Compare the previous sample's mean-square estimates in power units.
        // Epsilon squared is the input power floor; epsilon scales excess power.
        //
        Vector inputPower = m_inputEnvelope.GetPower();
        Vector feedbackPower = m_feedbackEnvelope.GetPower();
        Vector excessPower = Vector::Max(Vector(),
            feedbackPower - referenceGain * referenceGain * inputPower)
            / (inputPower + x_dampingEpsilon * x_dampingEpsilon);

        // Scale damping by the three saturators inside the feedback loop.
        //
        Vector dampingStrength = smallSignalGain * smallSignalGain * smallSignalGain;
        m_kEff = baseFeedback / (dampingStrength * x_dampingEpsilon * excessPower + 1);

        // Apply resonance feedback from LP output
        //
        Vector feedbackInput = input - m_kEff * m_stage4.m_output;
        m_input = feedbackInput;
        m_meterFbInput.Process(feedbackInput[0]);

        // Process through the 4-pole ladder with saturation after each stage
        //
        Vector stage1Pre = m_stage1.Process(feedbackInput);
        m_meterStage1Pre.Process(stage1Pre[0]);
        Vector stage1Out = Saturate(stage1Pre);
        m_meterStage1Post.Process(stage1Out[0]);

        Vector stage2Pre = m_stage2.Process(stage1Out);
        m_meterStage2Pre.Process(stage2Pre[0]);
        Vector stage2Out = Saturate(stage2Pre);
        m_meterStage2Post.Process(stage2Out[0]);

        Vector stage3Pre = m_stage3.Process(stage2Out);
        m_meterStage3Pre.Process(stage3Pre[0]);
        Vector stage3Out = Saturate(stage3Pre);
        m_meterStage3Post.Process(stage3Out[0]);

        Vector stage4Pre = m_stage4.Process(stage3Out);
        m_meterStage4Pre.Process(stage4Pre[0]);
        Vector stage4Out = Saturate(stage4Pre);
        m_meterStage4Post.Process(stage4Out[0]);

        m_inputEnvelope.Process(input);
        m_feedbackEnvelope.Process(stage4Pre);

        // Output gain compensation to counter resonance-induced passband drop
        // Maintains approximately unity DC gain as resonance increases
        //
        m_output = stage4Out * (m_kEff + 1);
        m_meterOutput.Process(m_output[0]);
        return m_output;
    }

    void SetSaturationGain(const Vector& gain)
    {
        if (gain != m_saturationGain)
        {
            m_saturationGain = gain;
            m_saturationTanhGain = Saturator::Tanh(gain);
        }
    }

    void SetCutoff(const Vector& cutoffParam)
    {
        Vector cutoff = Vector::Min(Vector::Splat(x_maxCutoff), cutoffParam);
        bool initialized = true;
        for (size_t i = 0; i < Size; ++i)
        {
            initialized = initialized && m_stage4.m_alpha[i] > 0;
        }

        if (cutoff == m_cutoff && initialized)
        {
            return;
        }

        m_cutoff = cutoff;
        Vector c = Math::Cos2pi(m_cutoff);

        // Solve for the -3dB point: |H|² = 2^(-1/4) per stage
        // Equation: (1-k)α² + 2k(1-c)α - 2k(1-c) = 0
        //
        static constexpr Number k = static_cast<Number>(0.84089641525);
        Vector oneMinusC = Vector::Splat(1) - c;
        Vector kOneMinusC = oneMinusC * k;
        Vector discriminant = kOneMinusC * (kOneMinusC + 2 * (1 - k));

        Vector alpha = (discriminant.Sqrt() - kOneMinusC) / (1 - k);
        SetAlphaDirect(alpha);
    }

    void SetAlphaDirect(const Vector& alpha)
    {
        m_stage1.m_alpha = alpha;
        m_stage2.m_alpha = alpha;
        m_stage3.m_alpha = alpha;
        m_stage4.m_alpha = alpha;

        // Unit-gain one-poles with one sample of feedback delay. The first
        // unit-circle crossing moves to Nyquist when alpha reaches 2/3.
        //
        Vector bandPassCutoff = Vector::Splat(static_cast<Number>(0.5));
        for (size_t i = 0; i < Size; ++i)
        {
            Number a = alpha[i];
            if (a < static_cast<Number>(2) / 3)
            {
                Number ratio = (2 - a) / (1 - a);
                m_criticalFeedback[i] = ratio * ratio;
                bandPassCutoff[i] = std::asin(a / (2 * (1 - a))) / static_cast<Number>(M_PI);
            }
            else
            {
                Number ratio = (2 - a) / a;
                Number ratioSquared = ratio * ratio;
                m_criticalFeedback[i] = ratioSquared * ratioSquared;
            }
        }

        // Track the loop resonance while retaining the ladder cutoff's envelope timing.
        //
        m_inputEnvelope.SetCutoff(bandPassCutoff, m_cutoff);
        m_feedbackEnvelope.SetCutoff(bandPassCutoff, m_cutoff);
    }

    void SetResonance(const Vector& resonance)
    {
        // Calculate feedback amount based on resonance
        // Higher resonance = more feedback = more pronounced filter character
        //
        m_feedback = resonance * static_cast<Number>(3.9);
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

        m_meterInput.Reset();
        m_meterFbInput.Reset();
        m_meterOutput.Reset();
        m_meterStage1Pre.Reset();
        m_meterStage1Post.Reset();
        m_meterStage2Pre.Reset();
        m_meterStage2Post.Reset();
        m_meterStage3Pre.Reset();
        m_meterStage3Post.Reset();
        m_meterStage4Pre.Reset();
        m_meterStage4Post.Reset();
    }

    void DebugPrint()
    {
        if (!x_debugMetersOn)
        {
            return;
        }

        float smallSignalGain = SaturationDerivativeZero()[0];
        float satInputGain = m_saturationGain[0];
        float alpha = m_stage4.m_alpha[0];
        float perStageGain = (alpha / (2.0f - alpha)) * smallSignalGain;
        float Cpi = std::pow(perStageGain, 4.0f);

        printf("=== LadderFilterLP Debug ===\n");
        printf("Cutoff: %.6f  Alpha: %.6f\n", static_cast<float>(m_cutoff[0]), alpha);
        printf("Feedback: %.6f  kEff: %.6f\n", static_cast<float>(m_feedback[0]), static_cast<float>(m_kEff[0]));
        printf("Sat InputGain: %.6f  SmallSigGain: %.6f\n", satInputGain, smallSignalGain);
        printf("PerStageGain: %.6f  Cpi: %.6f\n", perStageGain, Cpi);
        printf("\n");

        m_meterInput.Print("Input          ");
        m_meterFbInput.Print("FB Input       ");
        m_meterOutput.Print("Output         ");
        printf("\n");

        m_meterStage1Pre.Print("Stage1 Pre-Sat ");
        m_meterStage1Post.Print("Stage1 Post-Sat");
        m_meterStage2Pre.Print("Stage2 Pre-Sat ");
        m_meterStage2Post.Print("Stage2 Post-Sat");
        m_meterStage3Pre.Print("Stage3 Pre-Sat ");
        m_meterStage3Post.Print("Stage3 Post-Sat");
        m_meterStage4Pre.Print("Stage4 Pre-Sat ");
        m_meterStage4Post.Print("Stage4 Post-Sat");
        printf("============================\n");
    }
};

using LadderFilterLP = LadderFilterLPGeneric<float, 1>;
using TrioLadderFilterLP = LadderFilterLPGeneric<float, 3>;
