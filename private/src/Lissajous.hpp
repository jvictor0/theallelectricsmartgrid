#pragma once
#include "Slew.hpp"
#include "Math.hpp"

struct LissajousLFOInternal
{    
    struct Input
    {
        float m_multX;
        float m_multY;

        float m_centerX;
        float m_centerY;

        float m_phaseShift;

        float m_radius;

        Input()
            : m_multX(1.0f)
            , m_multY(1.0f)
            , m_centerX(0)
            , m_centerY(0)
            , m_phaseShift(0.0f)
            , m_radius(1.0f)
        {
        }

        std::pair<float, float> Compute(float t)
        {
            float ampX = 1;
            float ampY = 1;
            t += m_phaseShift;
            float tx = (t + 0.25 - std::floorf(t + 0.25)) * m_multX;
            float ty = (t - std::floorf(t)) * m_multY;
            float floorMultX = std::floorf(m_multX);
            float floorMultY = std::floorf(m_multY);

            if (floorMultX < tx)
            {
                ampX = m_multX - floorMultX;
                tx = (tx - floorMultX) / ampX;
            }
            else
            {
                tx = tx - std::floorf(tx);
            }

            if (floorMultY < ty)
            {
                ampY = m_multY - floorMultY;
                ty = (ty - floorMultY) / ampY;
            }
            else
            {
                ty = ty - std::floorf(ty);
            }

            return std::make_pair(
                m_radius * ampX * Math::Sin2pi(tx) + (1 - m_radius) * m_centerX,
                m_radius * ampY * Math::Sin2pi(ty) + (1 - m_radius) * m_centerY);
        }
    };

    LissajousLFOInternal()
        : m_outputX(0.0f)
        , m_outputY(0.0f)
    {
    }
    
    float m_outputX;
    float m_outputY;

    void Process(float in, Input& input)
    {
        std::pair<float, float> out = input.Compute(in);
        m_outputX = out.first;
        m_outputY = out.second;
    }
};

