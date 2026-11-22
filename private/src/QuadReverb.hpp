#pragma once

#include "Filter.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "SquiggleBoyQuadReverb.hpp"
#include "TPTBaseWidthFilter.hpp"

struct QuadReverb : SquiggleBoyQuadReverb<float>
{
    using Core = SquiggleBoyQuadReverb<float>;

    static constexpr float x_delayScale = static_cast<float>(SampleTimer::x_sampleRate) / 192000.0f;
    static constexpr float x_minDelaySamples = 240.0f * x_delayScale;
    static constexpr float x_maxDelaySamples = 245760.0f * x_delayScale;
    static constexpr float x_defaultDelaySamples = 256.0f * x_delayScale;
    static constexpr float x_modulationSamples = 4000.0f * x_delayScale;
    static_assert(x_maxDelaySamples + x_modulationSamples < x_delayCapacity);

    QuadReverb()
        : Core({QuadFloat::Splat(x_defaultDelaySamples), QuadFloat::Splat(0.5f),
            QuadFloat::Splat(static_cast<float>(QuadTPTBaseWidthFilter::x_minBaseHz / SampleTimer::x_sampleRate)),
            QuadFloat::Splat(1.0f)})
    {
    }

    struct UIState
    {
        DampingUIState m_dampingFilter;
    };

    SampledSnapshotWriter<DampingUIState> m_responseSampler;

    void SetupResponseUIState(UIState* uiState)
    {
        m_responseSampler.Bind(&uiState->m_dampingFilter);
    }

    void PopulateUIStateSample()
    {
        m_responseSampler.PopulateUIStateSample([&]
        {
            return MakeDampingSnapshot();
        });
    }
};

struct QuadReverbInputSetter
{
    using QuadExpParam = PhaseUtils::ExpParamGeneric<float, 4>;
    using QuadZeroedExpParam = PhaseUtils::ZeroedExpParamGeneric<float, 4>;

    QuadExpParam m_delayTime;
    QuadExpParam m_dampingBase;
    QuadExpParam m_dampingWidth;

    // Each channel widens over its own range, so the wideners stay per channel.
    //
    PhaseUtils::ExpParam m_wideners[4];

    QuadZeroedExpParam m_feedback;

    // Double state: 1 Hz one-poles at the engine rate stall short of their
    // targets in float.
    //
    OPLowPassFilterGeneric<double, 4> m_delayTimeFilter;

    struct Input
    {
        QuadFloat m_reverbTimeKnob;
        QuadFloat m_delayOffsetKnob;
        QuadFloat m_feedbackKnob;
        QuadFloat m_dampingBaseKnob;
        QuadFloat m_dampingWidthKnob;

        Input()
            : m_reverbTimeKnob()
            , m_delayOffsetKnob(QuadFloat::Splat(0.5f))
            , m_feedbackKnob()
            , m_dampingBaseKnob()
            , m_dampingWidthKnob()
        {
        }
    };

    QuadReverbInputSetter()
      : m_delayTime(QuadReverb::x_minDelaySamples, QuadReverb::x_maxDelaySamples)
      , m_dampingBase(SampleTimer::HzToCycPerSamp(QuadTPTBaseWidthFilter::x_minBaseHz), SampleTimer::HzToCycPerSamp(QuadTPTBaseWidthFilter::x_maxBaseHz))
      , m_dampingWidth(1.0, 2048.0)
      , m_wideners
      {
        PhaseUtils::ExpParam(1.0, 0.978615137775),
        PhaseUtils::ExpParam(1.0, 1.0),
        PhaseUtils::ExpParam(1.0, 0.986803398875),
        PhaseUtils::ExpParam(1.0, 0.989701964951)
      }
    {
        m_delayTimeFilter.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(1.0));
        m_feedback.SetBaseByCenter(0.25);
    }

    QuadReverb::LogicalSpec Process(const Input& input)
    {
        const auto delayTime = m_delayTime.Update(m_delayTimeFilter.Process(
            input.m_reverbTimeKnob.Convert<double>()).Convert<float>());
        const auto dampingBase = m_dampingBase.Update(input.m_dampingBaseKnob);
        const auto dampingWidth = m_dampingWidth.Update(input.m_dampingWidthKnob);
        const auto delayOffset = (input.m_delayOffsetKnob * 2.0f - 1.0f)
            * QuadReverb::x_modulationSamples;
        const auto feedback = m_feedback.Update(input.m_feedbackKnob) * 1.25f;
        QuadFloat widen;
        for (int i = 0; i < 4; ++i)
        {
            widen[i] = m_wideners[i].Update(1.0);
        }

        return {delayTime * widen + delayOffset, feedback, dampingBase, dampingWidth};
    }
};
