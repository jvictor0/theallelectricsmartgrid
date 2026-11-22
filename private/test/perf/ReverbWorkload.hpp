#pragma once

#include "Bench.hpp"
#include "LegacyQuadReverb.hpp"
#include "QuadReverb.hpp"
#include <type_traits>

enum class ReverbImplementation
{
    Legacy,
    FixedTaps,
    WanderingEverySample,
    Wandering
};

template <int Mode, ReverbImplementation Implementation>
struct ReverbWorkload
{
    static constexpr bool x_generated = Implementation != ReverbImplementation::Legacy;
    static constexpr bool x_framed = Implementation == ReverbImplementation::Wandering;
    static constexpr bool x_wandering = x_framed || Implementation == ReverbImplementation::WanderingEverySample;
    using Core = std::conditional_t<x_generated, SquiggleBoyQuadReverb<float, x_wandering, x_framed>, LegacyQuadReverb>;

    static constexpr size_t x_rate = SampleTimer::x_sampleRate;
    static constexpr size_t x_ratio = x_rate / 48000;

    Signals m_signals{x_rate};
    Core m_core;
    QuadReverbInputSetter m_setter;
    QuadReverb::LogicalSpec m_spec;
    QuadReverbInputSetter::Input m_knobs;
    static Core InitCore()
    {
        if constexpr (x_generated)
        {
            return Core({QuadFloat::Splat(1), QuadFloat(), QuadFloat::Splat(0.1f), QuadFloat::Splat(2),
                QuadFloat::Splat(QuadReverb::x_defaultSaturatorGain)});
        }
        else
        {
            return Core();
        }
    }

    ReverbWorkload() : m_core(InitCore())
    {
        Set(0);
        m_spec = m_setter.Process(m_knobs);
        if constexpr (Mode == 0)
        {
            for (size_t c = 0; c < 4; ++c)
            {
                m_spec.m_delaySamples[c] = (2400 + 400 * c) * x_ratio * (1.0f - 0.003f * c)
                    + (m_knobs.m_delayOffsetKnob[c] * 2.0f - 1.0f) * QuadReverb::x_modulationSamples;
                m_spec.m_feedback[c] = 0.7f;
                m_spec.m_dampingBase[c] = 90.0f / x_rate;
                m_spec.m_dampingWidth[c] = 100;
            }
        }
    }

    void Set(size_t n)
    {
        for (size_t c = 0; c < 4; ++c)
        {
            float move = Mode == 3 || (Mode == 2 && c == 1)
                ? m_signals.m_controls[n % m_signals.m_period][c] : 0;
            m_knobs.m_delayOffsetKnob[c] = 0.5f + 0.03f * c + 0.2f * move;
            m_knobs.m_reverbTimeKnob[c] = 0.55f + 0.04f * c + 0.06f * move;
            m_knobs.m_feedbackKnob[c] = 0.65f + 0.02f * c + 0.025f * move;
            m_knobs.m_dampingBaseKnob[c] = 0.18f + 0.01f * c + 0.03f * move;
            m_knobs.m_dampingWidthKnob[c] = 0.61f - 0.02f * c + 0.04f * move;
        }
    }

    std::array<float, 4> Process(size_t n)
    {
        if constexpr (Mode != 0)
        {
            Set(n);
            m_spec = m_setter.Process(m_knobs);
        }

        QuadFloat input;
        for (size_t c = 0; c < 4; ++c)
        {
            input[c] = m_signals.m_audio[n % m_signals.m_period][c];
        }

        QuadFloat value;
        if constexpr (x_generated)
        {
            m_core.ComputeCoefficients({m_spec.m_delaySamples, m_spec.m_feedback,
                m_spec.m_dampingBase, m_spec.m_dampingWidth, m_spec.m_saturatorGain});
            value = m_core.Process(input);
        }
        else
        {
            value = m_core.Process(input, m_spec);
        }

        return {value[0], value[1], value[2], value[3]};
    }
};

template <int Mode, ReverbImplementation Implementation>
void MeasureReverb(double seconds, const char* path, std::ostream& report)
{
    using Work = ReverbWorkload<Mode, Implementation>;
    static_assert(Work::x_rate == 192000, "The reverb comparison uses 192 kHz tuning");
    constexpr const char* x_name = Work::x_framed ? "reverb_generated"
        : Work::x_wandering ? "reverb_every_sample" : Work::x_generated ? "reverb_fixed" : "reverb";
    auto workload = std::make_unique<Work>();
    Measure(*workload, x_name, Mode, Work::x_rate, seconds, 2, path, report);
}
