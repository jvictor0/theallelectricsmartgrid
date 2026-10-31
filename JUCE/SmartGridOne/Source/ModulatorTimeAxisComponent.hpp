#pragma once

#include <JuceHeader.h>
#include "SmartGridInclude.hpp"
#include "SmartGridOneMainVisualizerComponent.hpp"

struct ModulatorTimeAxisComponent : SmartGridOneMainVisualizerComponent
{
    using Mode = SmartGridOneLayout::ParamMode;
    static constexpr size_t x_numPoints = 256;
    TheNonagonSquiggleBoyInternal::UIState* m_historyUIState = nullptr;
    Mode m_historyMode = Mode::Global;
    size_t m_historySource = 0;
    int m_trackOverride = -1;
    int* m_historyVoiceOffset = nullptr;
    bool m_fallback = false;
    bool m_hasWeights = false;
    std::array<WeightedModulators, 4> m_weightedModulators;

    void Configure(TheNonagonSquiggleBoyInternal::UIState* uiState, Mode mode,
        size_t source, int* voiceOffset)
    {
        m_historyUIState = uiState;
        m_historyMode = mode;
        m_historySource = source;
        m_historyVoiceOffset = voiceOffset;
    }

    void SetVoiceTrack(size_t track) override
    {
        m_trackOverride = static_cast<int>(track);
    }

    size_t GetFirstVoice() const
    {
        if (m_historyMode != Mode::Voice)
        {
            return 0;
        }

        return 3 * (m_trackOverride >= 0 ? static_cast<size_t>(m_trackOverride)
            : m_historyUIState->m_squiggleBoyUIState.m_activeTrack.load());
    }

    size_t GetVoiceCount() const
    {
        return m_historyMode == Mode::Voice ? 3 : m_historyMode == Mode::Quad ? 4 : 1;
    }

    ModulatorRef GetSource(size_t voice) const
    {
        ModulatorRef source(GlobalModulator::Noise);
        source.m_mode = m_historyMode;
        source.m_index = m_historySource;
        source.m_voice = GetFirstVoice() + voice;
        return source;
    }

    ModulatorTimeAxis GetTimeAxis(size_t voice) const
    {
        if (!m_historyUIState)
        {
            return {};
        }

        if (m_fallback)
        {
            double now = SmartGridOneModulatorHistory::GetLatestSample(*m_historyUIState);
            return now < 0.0 ? ModulatorTimeAxis{} : ModulatorTimeAxis{now - SampleTimer::x_sampleRate, now};
        }

        return SmartGridOneModulatorHistory::GetTimeAxis(*m_historyUIState, GetSource(voice));
    }

    double GetTimeExtent() const override
    {
        double extent = 0.0;
        for (size_t voice = 0; voice < GetVoiceCount(); ++voice)
        {
            extent = std::max(extent, GetTimeAxis(voice).GetExtent());
        }

        return extent;
    }

    void SetWeightedModulators(size_t voice, const WeightedModulators& modulators) override
    {
        assert(voice < m_weightedModulators.size());
        m_hasWeights = true;
        m_weightedModulators[voice] = modulators;
    }

    void ClearWeightedModulators() override
    {
        m_hasWeights = false;
        m_weightedModulators = {};
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        DrawTimeAxis(g, boundsRect);
    }

    bool DrawTimeAxis(juce::Graphics& g, juce::Rectangle<int> boundsRect)
    {
        if (!m_historyUIState)
        {
            return false;
        }

        g.fillAll(juce::Colours::black);
        double now = SmartGridOneModulatorHistory::GetLatestSample(*m_historyUIState);
        if (now < 0.0 || boundsRect.isEmpty())
        {
            return true;
        }

        bool random = !m_fallback && (m_historyMode == Mode::Global || m_historySource < 4);
        bool bipolar = !m_fallback && m_historyMode == Mode::Quad
            && (m_historySource == 4 || m_historySource == 5);
        auto bounds = boundsRect.toFloat();
        auto plot = random ? bounds.reduced(std::min(4.0f, bounds.getWidth() * 0.5f),
            std::min(4.0f, bounds.getHeight() * 0.5f)) : bounds;
        float yScale = random || bipolar ? 1.0f : 0.9f;
        float yOffset = random || bipolar ? 0.0f : 0.05f;
        g.setColour(juce::Colours::darkgrey);
        g.drawHorizontalLine(static_cast<int>(plot.getCentreY()), plot.getX(), plot.getRight());
        SmartGrid::Color sourceColor = m_fallback ? SmartGrid::Color::White
            : SmartGridOneModulators::x_skins[static_cast<size_t>(m_historyMode)][m_historySource].m_color;

        for (size_t voice = 0; voice < GetVoiceCount(); ++voice)
        {
            if (m_historyMode == Mode::Voice && !random && m_historyVoiceOffset)
            {
                int selected = *m_historyVoiceOffset;
                if ((selected >= 0 && voice != static_cast<size_t>(selected))
                    || (selected < 0 && m_historyUIState->m_nonagonUIState.m_muted[GetFirstVoice() + voice].load()))
                {
                    continue;
                }
            }

            auto axis = GetTimeAxis(voice);
            if (axis.GetExtent() <= 0.0)
            {
                continue;
            }

            WeightedModulators weights = m_weightedModulators[voice];
            if (!m_hasWeights)
            {
                weights.Add(GetSource(voice), 1.0f);
            }

            if (weights.m_count == 0)
            {
                continue;
            }

            auto pointAt = [&](double position)
            {
                float value = SmartGridOneModulatorHistory::GetWeightedModulatorValue(
                    *m_historyUIState, weights, axis.GetSample(position), now);
                return juce::Point<float>{plot.getX() + plot.getWidth() * static_cast<float>(position),
                    plot.getBottom() - plot.getHeight() * (value * yScale + yOffset)};
            };
            juce::Path path;
            for (size_t point = 0; point < x_numPoints; ++point)
            {
                double position = static_cast<double>(point) / x_numPoints;
                double previous = static_cast<double>(point == 0 ? 0 : point - 1) / x_numPoints;
                bool gap = (previous < axis.m_transfer && axis.m_transfer <= position)
                    || (axis.m_predictive && position > axis.m_present && (point / 4) % 2 == 0);
                if (point == 0 || gap)
                {
                    path.startNewSubPath(pointAt(position));
                }
                else
                {
                    path.lineTo(pointAt(position));
                }
            }

            SmartGrid::Color curveColor = m_historyMode == Mode::Voice
                ? TheNonagonSmartGrid::VoiceColor(GetFirstVoice() + voice)
                : m_historyMode == Mode::Quad && random ? TheNonagonSmartGrid::VoiceColor(voice) : sourceColor;
            g.setColour(J(curveColor));
            g.strokePath(path, juce::PathStrokeType(1.4f));
            auto marker = pointAt(axis.m_present);
            float radius = std::min({3.0f, bounds.getWidth() * 0.5f, bounds.getHeight() * 0.5f});
            marker.x = std::clamp(marker.x, bounds.getX() + radius, bounds.getRight() - radius);
            marker.y = std::clamp(marker.y, bounds.getY() + radius, bounds.getBottom() - radius);
            g.setColour(J(sourceColor));
            g.fillEllipse(marker.x - radius, marker.y - radius, radius * 2.0f, radius * 2.0f);
        }

        return true;
    }
};
