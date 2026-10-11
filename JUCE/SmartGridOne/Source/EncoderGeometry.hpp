#pragma once

#include <JuceHeader.h>

namespace EncoderGeometry
{
inline float ValueToArcAngle(float value)
{
    return juce::MathConstants<float>::pi * 1.25f + value * juce::MathConstants<float>::pi * 1.5f;
}
}
