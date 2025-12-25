#pragma once

#include "QuadUtils.hpp"
#include "StereoUtils.hpp"
#include "Math.hpp"
#include <cmath>

struct QuadToStereoMixdown
{
    static constexpr float x_theta = M_PI / 6;
    static constexpr float x_sinTheta = 0.5f;  // sin(30°)
    static constexpr float x_cosTheta = 0.8660254037844386f;  // cos(30°)

    StereoFloat m_output;
    QuadFloat m_rightMatrix;
    QuadFloat m_leftMatrix;

    QuadToStereoMixdown()
    {
        m_rightMatrix = QuadFloat(
            Math::Cos2pi(GetPosition(0, 1) / 4), 
            Math::Cos2pi(GetPosition(1, 1) / 4), 
            Math::Cos2pi(GetPosition(1, 0) / 4), 
            Math::Cos2pi(GetPosition(0, 0) / 4));
        m_leftMatrix = QuadFloat(
            Math::Sin2pi(GetPosition(0, 1) / 4), 
            Math::Sin2pi(GetPosition(1, 1) / 4), 
            Math::Sin2pi(GetPosition(1, 0) / 4), 
            Math::Sin2pi(GetPosition(0, 0) / 4));
    }

    static float GetPosition(float x, float y)
    {
        x = 2 * x - 1;
        y = 2 * y - 1;
        float position = (x * x_cosTheta - y * x_sinTheta) / (2 * (x_sinTheta + x_cosTheta)) + 0.5f;
        return position;
    }

    void Clear()
    {
        m_output = StereoFloat();
    }

    void MixSample(float x, float y, float sample)
    {
        float position = GetPosition(x, y);
        m_output += StereoFloat(
            Math::Sin2pi((1 - position) / 4) * sample,
            Math::Sin2pi(position / 4) * sample);
    }

    void MixQuadSample(QuadFloat x)
    {
        m_output[0] += (m_leftMatrix * x).Sum();
        m_output[1] += (m_rightMatrix * x).Sum();
    }
};