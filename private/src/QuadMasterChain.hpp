#pragma once

#include "QuadUtils.hpp"
#include "LinkwitzRileyCrossover.hpp"
#include "Filter.hpp"
#include "StereoUtils.hpp"

struct QuadFloatWithStereoAndSub
{
    QuadFloat m_output;
    StereoFloat m_stereoOutput;
    float m_sub;

    QuadFloatWithStereoAndSub()
    {
        m_output = QuadFloat();
        m_stereoOutput = StereoFloat();
        m_sub = 0.0f;
    }
};

struct QuadMasterChain
{
    struct Input
    {
        Input()
         : m_saturationGain(1.0)
        {
        }

        float m_saturationGain;
    };

    TanhSaturator<false> m_saturator;
    LinkwitzRileyCrossover m_linkwitzRileyCrossover[4];

    QuadFloatWithStereoAndSub m_output;

    QuadMasterChain()
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_linkwitzRileyCrossover[i].SetCyclesPerSample(100.0 / 48000.0);
        }
    }

    QuadFloatWithStereoAndSub Process(const Input& input, QuadFloat in)
    {
        m_saturator.SetInputGain(input.m_saturationGain);
        m_output.m_sub = 0;

        for (size_t i = 0; i < 4; ++i)
        {
            LinkwitzRileyCrossover::CrossoverOutput crossover = m_linkwitzRileyCrossover[i].Process(in[i]);
            m_output.m_output[i] = crossover.m_highPass;
            m_output.m_sub += crossover.m_lowPass / 4.0;
        }

        m_output.m_output = m_output.m_output + QuadFloat(m_output.m_sub, m_output.m_sub, m_output.m_sub, m_output.m_sub);
 
        m_output.m_output = m_saturator.Process(m_output.m_output);
        return m_output;
    }
};