#pragma once

#include <JuceHeader.h>
#include "SmartGridOneScopeEnums.hpp"
#include <cmath>

struct ModulatorGlyphPaths
{
    using Glyph = SmartGridOne::ModulationGlyphs;
    static constexpr float x_size = 100.0f;
    static constexpr float x_stroke = 6.5f;

    static juce::Path MakePath(Glyph glyph)
    {
        juce::Path path;
        if (glyph == Glyph::SubSine || glyph == Glyph::HarmonicWave)
        {
            for (int sample = 0; sample <= 128; ++sample)
            {
                const float t = static_cast<float>(sample) / 128.0f;
                const float phase = juce::MathConstants<float>::twoPi * t;
                const float value = glyph == Glyph::SubSine
                    ? std::tanh(2.2f * std::sin(phase)) / std::tanh(2.2f)
                    : (std::cos(phase) + 0.30f * std::cos(2.0f * phase)
                        - 0.28f * std::cos(3.0f * phase)) / 1.2f;
                const float x = 9.0f + 82.0f * t;
                const float y = 50.0f - 25.0f * value;
                if (sample == 0)
                {
                    path.startNewSubPath(x, y);
                }
                else
                {
                    path.lineTo(x, y);
                }
            }
        }
        else if (glyph == Glyph::AudioOutput || glyph == Glyph::AudioSend)
        {
            path.startNewSubPath(48, 20);
            path.lineTo(28, 20);
            path.quadraticTo(20, 20, 20, 28);
            path.lineTo(20, 72);
            path.quadraticTo(20, 80, 28, 80);
            path.lineTo(72, 80);
            path.quadraticTo(80, 80, 80, 72);
            path.lineTo(80, 52);
            if (glyph == Glyph::AudioOutput)
            {
                path.startNewSubPath(50, 50);
                path.lineTo(84, 16);
                path.startNewSubPath(64, 16);
                path.lineTo(84, 16);
                path.lineTo(84, 36);
            }
            else
            {
                path.applyTransform(juce::AffineTransform(-1, 0, 100, 0, 1, 0));
                path.startNewSubPath(16, 16);
                path.lineTo(50, 50);
                path.startNewSubPath(30, 50);
                path.lineTo(50, 50);
                path.lineTo(50, 30);
            }
        }
        else if (glyph == Glyph::Wet)
        {
            for (int row = 0; row < 3; ++row)
            {
                for (int sample = 0; sample <= 96; ++sample)
                {
                    const float t = static_cast<float>(sample) / 96.0f;
                    const float x = 10.0f + 80.0f * t;
                    const float y = 30.0f + 20.0f * row
                        - 6.0f * std::sin(3.0f * juce::MathConstants<float>::pi * t);
                    if (sample == 0)
                    {
                        path.startNewSubPath(x, y);
                    }
                    else
                    {
                        path.lineTo(x, y);
                    }
                }
            }
        }
        else if (glyph == Glyph::VU)
        {
            path.addCentredArc(50, 73, 36, 36, 0, -1.35f, 1.35f, true);
            for (float angle : {-0.8f, 0.0f, 0.8f})
            {
                path.startNewSubPath(50.0f + 27.0f * std::sin(angle), 73.0f - 27.0f * std::cos(angle));
                path.lineTo(50.0f + 32.0f * std::sin(angle), 73.0f - 32.0f * std::cos(angle));
            }

            path.startNewSubPath(50, 73);
            path.lineTo(63, 52);
        }

        return path;
    }

    static juce::Image Render(Glyph glyph)
    {
        constexpr int x_pixels = 256;
        juce::Image image(juce::Image::ARGB, x_pixels, x_pixels, true);
        juce::Graphics graphics(image);
        graphics.addTransform(juce::AffineTransform::scale(static_cast<float>(x_pixels) / x_size));
        graphics.setColour(juce::Colours::white);
        graphics.strokePath(MakePath(glyph),
            juce::PathStrokeType(x_stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return image;
    }
};
