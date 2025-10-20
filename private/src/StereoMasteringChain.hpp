#pragma once

#include "StereoUtils.hpp"
#include "MultibandSaturator.hpp"

struct StereoMasteringChain
{
    MultibandSaturator<4, 2> m_saturator;
    StereoFloat m_output;

    struct Input
    {
        MultibandSaturator<4, 2>::Input m_saturatorInput;
    };

    StereoFloat Process(const Input& input, StereoFloat in)
    {
        m_output = m_saturator.Process(input.m_saturatorInput, in, true /* monoTheBass */);
        return m_output;
    }
};