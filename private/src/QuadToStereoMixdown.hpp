#pragma once

#include "QuadUtils.hpp"
#include "WaveTable.hpp"
#include "StereoUtils.hpp"
#include <cmath>

struct QuadToStereoMixdown
{
    static constexpr float x_theta = M_PI / 6;
    static constexpr float x_sinTheta = 0.5f;  // sin(30°)
    static constexpr float x_cosTheta = 0.8660254037844386f;  // cos(30°)

    StereoFloat m_output;
    QuadFloat m_rightMatrix;
    QuadFloat m_leftMatrix;

    const WaveTable* m_sin;
    const WaveTable* m_cos;

    QuadToStereoMixdown()
    {
        m_sin = &WaveTable::GetSine();
        m_cos = &WaveTable::GetCosine();
        
        m_rightMatrix = QuadFloat(
            m_cos->Evaluate(GetPosition(0, 1) / 4), 
            m_cos->Evaluate(GetPosition(1, 1) / 4), 
            m_cos->Evaluate(GetPosition(1, 0) / 4), 
            m_cos->Evaluate(GetPosition(0, 0) / 4));
        m_leftMatrix = QuadFloat(
            m_sin->Evaluate(GetPosition(0, 1) / 4), 
            m_sin->Evaluate(GetPosition(1, 1) / 4), 
            m_sin->Evaluate(GetPosition(1, 0) / 4), 
            m_sin->Evaluate(GetPosition(0, 0) / 4));
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
        m_output += StereoFloat::Pan(position, sample, m_sin);
    }

    void MixQuadSample(QuadFloat x)
    {
        m_output[0] += (m_leftMatrix * x).Sum();
        m_output[1] += (m_rightMatrix * x).Sum();
    }
};