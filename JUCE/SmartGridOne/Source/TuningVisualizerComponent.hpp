#pragma once

#include <JuceHeader.h>
#include "SmartGridInclude.hpp"
#include "SmartGridOneMainVisualizerComponent.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <vector>

struct TuningVisualizerComponent : public SmartGridOneMainVisualizerComponent
{
    static constexpr double x_halfRangeCents = 50.0;
    static constexpr int x_fractionLimit = 16;

    struct Voice
    {
        size_t m_index;
        double m_frequencyHz;
    };

    struct Frame
    {
        double m_tempoHz = 0.0;
        double m_tuneHz = 0.0;
        std::array<Voice, SquiggleBoy::x_numVoices> m_voices{};
        size_t m_numVoices = 0;
    };

    struct Fraction
    {
        int m_numerator;
        int m_denominator;
        double m_octaves;
    };

    struct RuleMark
    {
        Fraction m_fraction;
        double m_cents;
    };

    TheNonagonSquiggleBoyInternal::UIState* m_uiState;

    explicit TuningVisualizerComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : m_uiState(uiState)
    {
    }

    static bool IsFrequency(double frequency)
    {
        return std::isfinite(frequency) && frequency > 0.0;
    }

    static double OctavePosition(double frequency)
    {
        double octaves = std::log2(frequency / Tuning::x_nyquistHz);
        return octaves - std::floor(octaves);
    }

    static double RelativeCents(double frequency, double center)
    {
        return 1200.0 * std::remainder(std::log2(frequency / center), 1.0);
    }

    static const std::vector<Fraction>& Fractions()
    {
        // Each reduced fraction occurs once in [1, 2); all octave copies share
        // its mark. Small numerators and denominators keep the ruler readable.
        //
        static const std::vector<Fraction> x_fractions = []
        {
            std::vector<Fraction> fractions;
            for (int numerator = 1; numerator <= x_fractionLimit; ++numerator)
            {
                for (int denominator = 1; denominator <= numerator; ++denominator)
                {
                    if (numerator >= 2 * denominator || std::gcd(numerator, denominator) != 1)
                    {
                        continue;
                    }

                    fractions.push_back({numerator, denominator,
                        std::log2(static_cast<double>(numerator) / denominator)});
                }
            }

            return fractions;
        }();
        return x_fractions;
    }

    static std::vector<RuleMark> MakeRuler(double center, double reference)
    {
        std::vector<RuleMark> marks;
        if (!IsFrequency(center) || !IsFrequency(reference))
        {
            return marks;
        }

        double offset = std::log2(reference / center);
        for (const auto& fraction : Fractions())
        {
            double cents = 1200.0 * std::remainder(offset + fraction.m_octaves, 1.0);
            if (std::abs(cents) <= x_halfRangeCents + 1.0e-7)
            {
                marks.push_back({fraction, cents});
            }
        }

        return marks;
    }

    bool PrepareFrame(Frame& frame) const
    {
        frame = Frame{};
        if (!m_uiState)
        {
            return false;
        }

        frame.m_tempoHz = m_uiState->m_tempoHz.load();
        frame.m_tuneHz = m_uiState->m_masterTuneHz.load();
        if (!IsFrequency(frame.m_tempoHz) || !IsFrequency(frame.m_tuneHz))
        {
            return false;
        }

        for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
        {
            double frequency = m_uiState->m_voiceFrequencyHz[voice].load();
            if (!m_uiState->m_nonagonUIState.m_muted[voice].load() && IsFrequency(frequency))
            {
                frame.m_voices[frame.m_numVoices++] = {voice, frequency};
            }
        }

        return true;
    }

    static juce::Point<float> CirclePoint(juce::Point<float> center, float radius, double position)
    {
        double angle = juce::MathConstants<double>::twoPi * position;
        return {center.x + radius * static_cast<float>(std::sin(angle)),
            center.y - radius * static_cast<float>(std::cos(angle))};
    }

    static juce::Point<float> DialPoint(juce::Point<float> center, float width, double cents)
    {
        float position = static_cast<float>(cents / x_halfRangeCents);
        return {center.x + 0.5f * width * position,
            center.y - width * 0.06f * position * position};
    }

    static void DrawSpoke(juce::Graphics& g, juce::Point<float> center, float radius,
        double frequency, juce::Colour color, float start, float end, float thickness)
    {
        double position = OctavePosition(frequency);
        auto from = CirclePoint(center, radius * start, position);
        auto to = CirclePoint(center, radius * end, position);
        g.setColour(color);
        g.drawLine(juce::Line<float>(from, to), thickness);
        g.fillEllipse(to.x - thickness, to.y - thickness, 2.0f * thickness, 2.0f * thickness);
    }

    static void DrawDial(juce::Graphics& g, juce::Point<float> center, float width,
        double frequency, double reference, juce::Colour color, juce::Colour referenceColor,
        const Frame& frame, bool showVoices)
    {
        float scale = width / 180.0f;
        juce::Path arc;
        for (int step = 0; step <= 64; ++step)
        {
            double cents = -x_halfRangeCents + 2.0 * x_halfRangeCents * step / 64.0;
            auto point = DialPoint(center, width, cents);
            if (step == 0)
            {
                arc.startNewSubPath(point);
            }
            else
            {
                arc.lineTo(point);
            }
        }

        g.setColour(juce::Colours::white.withAlpha(0.32f));
        g.strokePath(arc, juce::PathStrokeType(scale));
        for (int cents = -50; cents <= 50; cents += 10)
        {
            auto point = DialPoint(center, width, cents);
            g.drawLine(point.x, point.y - 2.0f * scale, point.x, point.y + 3.0f * scale, scale);
        }

        if (showVoices)
        {
            for (size_t i = frame.m_numVoices; i > 0; --i)
            {
                const auto& voice = frame.m_voices[i - 1];
                double cents = RelativeCents(voice.m_frequencyHz, frequency);
                if (std::abs(cents) > x_halfRangeCents)
                {
                    continue;
                }

                auto point = DialPoint(center, width, cents);
                float top = point.y - (12.0f + 3.0f * static_cast<float>(voice.m_index)) * scale;
                g.setColour(J(TheNonagonSmartGrid::VoiceColor(voice.m_index)));
                g.drawLine(point.x, point.y - 3.0f * scale, point.x, top, 1.5f * scale);
                g.fillEllipse(point.x - 2.0f * scale, top - 2.0f * scale, 4.0f * scale, 4.0f * scale);
            }
        }

        g.setFont(10.0f * scale);
        std::vector<juce::Rectangle<float>> labels;
        for (const auto& mark : MakeRuler(frequency, reference))
        {
            auto point = DialPoint(center, width, mark.m_cents);
            bool unison = mark.m_fraction.m_numerator == 1 && mark.m_fraction.m_denominator == 1;
            float halfHeight = (unison ? 18.0f : 6.0f) * scale;
            float thickness = (unison ? 3.0f : 1.0f) * scale;
            g.setColour(unison ? referenceColor : juce::Colours::white.withAlpha(0.8f));
            g.drawLine(point.x, point.y - halfHeight, point.x, point.y + halfHeight, thickness);
            juce::Rectangle<float> labelBounds(point.x - 16.0f * scale,
                point.y + halfHeight + 4.0f * scale, 32.0f * scale, 14.0f * scale);
            bool overlaps = std::any_of(labels.begin(), labels.end(), [&labelBounds](const auto& label)
            {
                return label.intersects(labelBounds);
            });

            if (!overlaps)
            {
                auto label = juce::String(mark.m_fraction.m_numerator) + "/"
                    + juce::String(mark.m_fraction.m_denominator);
                g.drawText(label, labelBounds, juce::Justification::centred);
                labels.push_back(labelBounds);
            }
        }

        g.setColour(color);
        g.drawLine(center.x, center.y - 7.0f * scale, center.x, center.y + 7.0f * scale, 2.0f * scale);
        juce::Path pointer;
        pointer.addTriangle(center.x, center.y - 6.0f * scale,
            center.x - 3.5f * scale, center.y - 12.0f * scale,
            center.x + 3.5f * scale, center.y - 12.0f * scale);
        g.fillPath(pointer);
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(boundsRect);
        g.setColour(juce::Colours::black);
        g.fillRect(boundsRect);
        Frame frame;
        if (boundsRect.isEmpty() || !PrepareFrame(frame))
        {
            return;
        }

        auto bounds = boundsRect.toFloat();
        float size = std::min(bounds.getWidth(), bounds.getHeight());
        float scale = size / 336.0f;
        auto center = bounds.getCentre();
        float radius = size * 0.37f;
        g.setColour(juce::Colours::white.withAlpha(0.28f));
        g.drawEllipse(center.x - radius, center.y - radius, 2.0f * radius, 2.0f * radius, scale);
        for (int semitone = 0; semitone < 12; ++semitone)
        {
            auto from = CirclePoint(center, radius * 0.96f, semitone / 12.0);
            auto to = CirclePoint(center, radius, semitone / 12.0);
            g.drawLine(juce::Line<float>(from, to), scale);
        }

        DrawSpoke(g, center, radius, frame.m_tempoHz, juce::Colours::yellow, 0.85f, 1.22f, 2.0f * scale);
        DrawSpoke(g, center, radius, frame.m_tuneHz, juce::Colours::white, 0.9f, 1.18f, 1.5f * scale);

        // Draw longer spokes first so octave unisons retain every voice color.
        //
        for (size_t i = frame.m_numVoices; i > 0; --i)
        {
            const auto& voice = frame.m_voices[i - 1];
            float end = 1.06f + 0.014f * static_cast<float>(voice.m_index);
            DrawSpoke(g, center, radius, voice.m_frequencyHz,
                J(TheNonagonSmartGrid::VoiceColor(voice.m_index)), 1.0f, end, 1.5f * scale);
        }

        float width = radius * 1.45f;
        DrawDial(g, {center.x, center.y - radius * 0.24f}, width,
            frame.m_tempoHz, frame.m_tuneHz, juce::Colours::yellow, juce::Colours::white, frame, true);
        DrawDial(g, {center.x, center.y + radius * 0.47f}, width,
            frame.m_tuneHz, frame.m_tempoHz, juce::Colours::white, juce::Colours::yellow, frame, false);
    }
};
