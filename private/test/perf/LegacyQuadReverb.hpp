#pragma once

#include "AllPassFilter.hpp"
#include "DelayLine.hpp"
#include "QuadReverb.hpp"
#include "Saturator.hpp"

// Handwritten baseline for the 192 kHz comparison.
//
struct LegacyQuadReverb
{
    QuadDelayLine<262144> m_delay;
    QuadParallelAllPassFilter<8> m_intro;
    QuadAllPassFilter m_preFeedback;
    QuadAllPassFilter m_firstAllPass;
    QuadTPTBaseWidthFilter m_damping;
    QuadAllPassFilter m_secondAllPass;
    QuadTanhSaturator<true> m_saturator{0.5f};
    QuadFloat m_previous;

    LegacyQuadReverb()
    {
        m_intro.SetGain(0.6f);
        m_preFeedback.SetGain(0.6f);
        m_firstAllPass.SetGain(0.6f);
        m_secondAllPass.SetGain(0.6f);

        constexpr int x_intro[8] = {148, 244, 452, 716, 1124, 1724, 2452, 3508};
        constexpr int x_pre[4] = {12, 20, 28, 44};
        constexpr int x_first[4] = {68, 124, 212, 92};
        constexpr int x_second[4] = {596, 844, 452, 1172};
        for (int i = 0; i < 8; ++i)
        {
            m_intro.SetDelaySamples(i, x_intro[i]);
        }

        for (int lane = 0; lane < 4; ++lane)
        {
            m_preFeedback.SetDelaySamples(lane, x_pre[lane]);
            m_firstAllPass.SetDelaySamples(lane, x_first[lane]);
            m_secondAllPass.SetDelaySamples(lane, x_second[lane]);
        }
    }

    QuadFloat Process(const QuadFloat& input, const QuadReverb::LogicalSpec& spec)
    {
        m_damping.SetBaseWidth(spec.m_dampingBase, spec.m_dampingWidth);
        const auto diffused = m_intro.Process(input);
        const auto feedback = m_preFeedback.Process(m_previous) * spec.m_feedback;
        m_delay.Write(m_saturator.Process(diffused + feedback));
        const auto mixed = m_delay.Read(spec.m_delaySamples).Hadamard();
        m_previous = m_secondAllPass.Process(m_damping.Process(m_firstAllPass.Process(mixed)));
        return m_previous;
    }
};
