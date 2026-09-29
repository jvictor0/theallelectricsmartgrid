#pragma once

#include "SmartGridInclude.hpp"
#include "SmartGridOneMainVisualizerComponent.hpp"

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>

template<size_t x_voiceCount>
struct GangedRandomLFOComponent : public SmartGridOneMainVisualizerComponent
{
    enum class ColorMode
    {
        TrackVoices,
        Voices,
        Fixed,
    };

    struct VoiceTiming
    {
        double m_waitingSamples{0.0};
        double m_movingSamples{0.0};
        double m_totalSamples{0.0};
    };

    static constexpr size_t x_pathSegments = 64;
    static constexpr float x_inset = 4.0f;
    static constexpr float x_dotRadius = 3.0f;

    GangedRandomLFOUIState<x_voiceCount>* m_uiStates;
    size_t m_numGangs;
    std::atomic<size_t>* m_selectedGang;
    ColorMode m_colorMode;
    SmartGrid::Color m_fixedColor;

    GangedRandomLFOComponent(
        GangedRandomLFOUIState<x_voiceCount>* uiStates,
        size_t numGangs,
        std::atomic<size_t>* selectedGang,
        ColorMode colorMode,
        SmartGrid::Color fixedColor)
        : SmartGridOneMainVisualizerComponent()
        , m_uiStates(uiStates)
        , m_numGangs(numGangs)
        , m_selectedGang(selectedGang)
        , m_colorMode(colorMode)
        , m_fixedColor(fixedColor)
    {
    }

    size_t GetGangIndex() const
    {
        if (!m_selectedGang || m_numGangs == 0)
        {
            return 0;
        }

        return std::min(m_selectedGang->load(), m_numGangs - 1);
    }

    juce::Colour GetVoiceColor(size_t voice, size_t gang) const
    {
        SmartGrid::Color color = m_fixedColor;
        if (m_colorMode == ColorMode::TrackVoices)
        {
            color = TheNonagonSmartGrid::VoiceColor(gang * x_voiceCount + voice);
        }
        else if (m_colorMode == ColorMode::Voices)
        {
            color = TheNonagonSmartGrid::VoiceColor(voice);
        }

        return juce::Colour(color.m_red, color.m_green, color.m_blue);
    }

    bool ComputeTiming(
        const GangedRandomLFOVoiceSnapshot& voice,
        VoiceTiming& timing) const
    {
        if (!std::isfinite(voice.m_waitingIncrement) || voice.m_waitingIncrement <= 0.0
            || !std::isfinite(voice.m_movingIncrement) || voice.m_movingIncrement <= 0.0
            || !std::isfinite(voice.m_source) || voice.m_source < 0.0f || voice.m_source > 1.0f
            || !std::isfinite(voice.m_target) || voice.m_target < 0.0f || voice.m_target > 1.0f
            || !std::isfinite(voice.m_output) || voice.m_output < 0.0f || voice.m_output > 1.0f
            || !std::isfinite(voice.m_shape) || voice.m_shape < 0.0f || voice.m_shape > 1.0f
            || !std::isfinite(voice.m_currentStateProgress))
        {
            return false;
        }

        timing.m_waitingSamples = std::ceil(1.0 / voice.m_waitingIncrement);
        timing.m_movingSamples = std::ceil(1.0 / voice.m_movingIncrement);
        timing.m_totalSamples = timing.m_waitingSamples + timing.m_movingSamples;
        return std::isfinite(timing.m_waitingSamples) && timing.m_waitingSamples > 0.0
            && std::isfinite(timing.m_movingSamples) && timing.m_movingSamples > 0.0
            && std::isfinite(timing.m_totalSamples) && timing.m_totalSamples > 0.0;
    }

    float ValueAtSample(
        const GangedRandomLFOVoiceSnapshot& voice,
        const VoiceTiming& timing,
        double sample) const
    {
        if (sample <= timing.m_waitingSamples)
        {
            return voice.m_source;
        }

        if (sample >= timing.m_totalSamples)
        {
            return voice.m_target;
        }

        double movingProgress =
            (sample - timing.m_waitingSamples) * voice.m_movingIncrement;
        return GangedRandomLFOShapedInterpolate(
            voice.m_source,
            voice.m_target,
            voice.m_shape,
            movingProgress);
    }

    juce::Point<float> PointAtSample(
        const GangedRandomLFOVoiceSnapshot& voice,
        const VoiceTiming& timing,
        double sample,
        double sharedDurationSamples,
        juce::Rectangle<float> plot) const
    {
        double normalizedX = std::clamp(sample / sharedDurationSamples, 0.0, 1.0);
        float value = std::clamp(ValueAtSample(voice, timing, sample), 0.0f, 1.0f);
        return {
            plot.getX() + plot.getWidth() * static_cast<float>(normalizedX),
            plot.getY() + plot.getHeight() * (1.0f - value),
        };
    }

    void DrawPath(
        juce::Graphics& graphics,
        const GangedRandomLFOVoiceSnapshot& voice,
        const VoiceTiming& timing,
        double startSample,
        double endSample,
        double sharedDurationSamples,
        juce::Rectangle<float> plot,
        juce::Colour color) const
    {
        if (endSample <= startSample)
        {
            return;
        }

        juce::Path path;
        path.startNewSubPath(PointAtSample(
            voice,
            timing,
            startSample,
            sharedDurationSamples,
            plot));
        for (size_t segment = 1; segment <= x_pathSegments; ++segment)
        {
            double sample = startSample
                + (endSample - startSample)
                    * static_cast<double>(segment)
                    / static_cast<double>(x_pathSegments);
            path.lineTo(PointAtSample(
                voice,
                timing,
                sample,
                sharedDurationSamples,
                plot));
        }

        graphics.setColour(color);
        graphics.strokePath(path, juce::PathStrokeType(1.4f));
    }

    void Draw(juce::Graphics& graphics, juce::Rectangle<int> boundsRect) override
    {
        graphics.fillAll(juce::Colours::black);

        juce::Rectangle<float> bounds = boundsRect.toFloat();
        float insetX = std::min(x_inset, bounds.getWidth() * 0.5f);
        float insetY = std::min(x_inset, bounds.getHeight() * 0.5f);
        juce::Rectangle<float> plot = bounds.reduced(insetX, insetY);
        graphics.setColour(juce::Colours::darkgrey);
        graphics.drawLine(
            plot.getX(),
            plot.getCentreY(),
            plot.getRight(),
            plot.getCentreY(),
            1.0f);

        if (!m_uiStates || m_numGangs == 0 || plot.isEmpty())
        {
            return;
        }

        size_t gang = GetGangIndex();
        GangedRandomLFOSnapshot<x_voiceCount> snapshot;
        if (!m_uiStates[gang].ReadSnapshot(snapshot)
            || !std::isfinite(snapshot.m_sampleRate)
            || snapshot.m_sampleRate <= 0.0
            || !std::isfinite(snapshot.m_roundElapsedSamples)
            || snapshot.m_roundElapsedSamples < 0.0)
        {
            return;
        }

        std::array<VoiceTiming, x_voiceCount> timings{};
        double sharedDurationSamples = 0.0;
        for (size_t voice = 0; voice < x_voiceCount; ++voice)
        {
            if (!ComputeTiming(snapshot.m_voices[voice], timings[voice]))
            {
                return;
            }

            sharedDurationSamples =
                std::max(sharedDurationSamples, timings[voice].m_totalSamples);
        }

        if (!std::isfinite(sharedDurationSamples) || sharedDurationSamples <= 0.0)
        {
            return;
        }

        double presentSample = std::clamp(
            snapshot.m_roundElapsedSamples,
            0.0,
            sharedDurationSamples);
        for (size_t voice = 0; voice < x_voiceCount; ++voice)
        {
            const GangedRandomLFOVoiceSnapshot& voiceSnapshot = snapshot.m_voices[voice];
            juce::Colour color = GetVoiceColor(voice, gang);
            DrawPath(
                graphics,
                voiceSnapshot,
                timings[voice],
                0.0,
                presentSample,
                sharedDurationSamples,
                plot,
                color);

            if (presentSample < sharedDurationSamples)
            {
                for (size_t segment = 0; segment < x_pathSegments; segment += 2)
                {
                    double dashStart = presentSample
                        + (sharedDurationSamples - presentSample)
                            * static_cast<double>(segment)
                            / static_cast<double>(x_pathSegments);
                    double dashEnd = presentSample
                        + (sharedDurationSamples - presentSample)
                            * static_cast<double>(segment + 1)
                            / static_cast<double>(x_pathSegments);
                    juce::Point<float> start = PointAtSample(
                        voiceSnapshot,
                        timings[voice],
                        dashStart,
                        sharedDurationSamples,
                        plot);
                    juce::Point<float> end = PointAtSample(
                        voiceSnapshot,
                        timings[voice],
                        dashEnd,
                        sharedDurationSamples,
                        plot);
                    graphics.setColour(color);
                    graphics.drawLine(start.x, start.y, end.x, end.y, 1.4f);
                }
            }

            juce::Point<float> dot = PointAtSample(
                voiceSnapshot,
                timings[voice],
                presentSample,
                sharedDurationSamples,
                plot);
            float radius = std::min({
                x_dotRadius,
                plot.getWidth() * 0.5f,
                plot.getHeight() * 0.5f,
            });
            if (radius > 0.0f)
            {
                float centerX = std::clamp(dot.x, bounds.getX() + radius, bounds.getRight() - radius);
                float centerY = std::clamp(dot.y, bounds.getY() + radius, bounds.getBottom() - radius);
                graphics.setColour(color);
                graphics.fillEllipse(
                    centerX - radius,
                    centerY - radius,
                    radius * 2.0f,
                    radius * 2.0f);
            }
        }
    }
};
