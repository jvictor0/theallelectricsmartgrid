#pragma once

#include <JuceHeader.h>

namespace EncoderGeometry
{
inline float ValueToArcAngle(float value)
{
    return juce::MathConstants<float>::pi * 1.25f + value * juce::MathConstants<float>::pi * 1.5f;
}

struct MotionIndicator
{
    float m_start;
    float m_end;
    float m_outlineWidth;
    float m_outlineAlpha;
    float m_outerWidth;
    float m_outerAlpha;
    float m_midWidth;
    float m_midAlpha;
    float m_coreWidth;
    float m_coreAlpha;
};

inline MotionIndicator GetMotionIndicator(float radius, float value, float spread)
{
    constexpr float x_fullBlurSpread = 0.20f;
    constexpr float x_probableSigma = 2.0f;
    constexpr float x_collapsedHalfWidth = 0.35f;
    float motion = std::clamp(spread / x_fullBlurSpread, 0.0f, 1.0f);
    float radialMotion = motion * motion;
    float dotRadius = radius * 0.15f;
    float halfValue = x_collapsedHalfWidth / (radius * juce::MathConstants<float>::pi * 1.5f)
        + std::max(0.0f, spread) * x_probableSigma;
    float center = std::clamp(value, 0.0f, 1.0f);
    float outerWidth = dotRadius * 2.0f + radialMotion * radius * 0.30f;
    float coreWidth = std::max(dotRadius * 0.95f,
        dotRadius * (2.0f - motion * 0.85f) + radialMotion * radius * 0.05f);
    return {
        std::clamp(center - halfValue, 0.0f, 1.0f),
        std::clamp(center + halfValue, 0.0f, 1.0f),
        coreWidth + 1.0f + motion * std::max(0.0f, outerWidth - coreWidth),
        0.55f * (1.0f - 0.95f * radialMotion),
        outerWidth,
        motion * (0.10f + motion * 0.08f),
        dotRadius * 1.35f + radialMotion * radius * 0.18f,
        motion * (0.24f + motion * 0.16f),
        coreWidth,
        0.96f - motion * 0.08f
    };
}

}
