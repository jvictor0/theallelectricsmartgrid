#pragma once

#include <JuceHeader.h>
#include "SmartGridInclude.hpp"
#include "SmartGridOneMainVisualizerComponent.hpp"
#include "EncoderGeometry.hpp"

struct SheafyModulatorDialComponent : SmartGridOneMainVisualizerComponent
{
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;
    VoiceModulator m_modulator;

    SheafyModulatorDialComponent(TheNonagonSquiggleBoyInternal::UIState* uiState, VoiceModulator modulator)
        : m_uiState(uiState)
        , m_modulator(modulator)
    {
        jassert(modulator >= VoiceModulator::Sheaf1 && modulator <= VoiceModulator::Sheaf3);
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        size_t trio = m_uiState->m_squiggleBoyUIState.m_activeTrack.load();
        if (trio >= TheNonagonInternal::x_numTrios || boundsRect.isEmpty())
        {
            return;
        }

        size_t axis = static_cast<size_t>(m_modulator) - static_cast<size_t>(VoiceModulator::Sheaf1);
        auto bounds = boundsRect.toFloat();
        auto center = bounds.getCentre();
        float size = std::min(bounds.getWidth(), bounds.getHeight());
        float outerRadius = size * 0.28f;
        float bandWidth = size * 0.045f;
        for (size_t voice = 0; voice < TheNonagonInternal::x_voicesPerTrio; ++voice)
        {
            size_t voiceIndex = trio * TheNonagonInternal::x_voicesPerTrio + voice;
            float value = m_uiState->m_nonagonUIState.m_extraTimbre[voiceIndex][axis].load();
            if (value <= 0.0f)
            {
                continue;
            }

            float radius = outerRadius - voice * size * 0.0575f;
            juce::Path arc;
            arc.addPieSegment(center.x - radius, center.y - radius, radius * 2, radius * 2,
                EncoderGeometry::ValueToArcAngle(0), EncoderGeometry::ValueToArcAngle(value),
                (radius - bandWidth) / radius);
            g.setColour(J(TheNonagonSmartGrid::VoiceColor(voiceIndex)));
            g.fillPath(arc);
        }

        auto& state = m_uiState->m_nonagonUIState.m_lameJuisUIState;
        auto lens = state.GetLensForLane(trio);
        HarmonicSheaf::TimeSliceClassIterator iterator(lens, HarmonicSheaf::BitVector(state.m_currentTimeSlice.load()));
        juce::Path spokes;
        while (!iterator.Done())
        {
            auto section = state.m_harmonicSheafState.GetSection(iterator.Get());
            float angle = EncoderGeometry::ValueToArcAngle(section.Timbre(axis));
            spokes.startNewSubPath(center);
            spokes.lineTo(center.x + outerRadius * std::sin(angle), center.y - outerRadius * std::cos(angle));
            iterator.Next();
        }

        g.setColour(J(GetModulatorColor(m_modulator)));
        g.strokePath(spokes, juce::PathStrokeType(1.5f));
    }
};

struct NoiseModulatorComponent : SmartGridOneMainVisualizerComponent
{
    SmartGrid::Color m_color;
    juce::Random m_random;

    explicit NoiseModulatorComponent(SmartGrid::Color color)
        : m_color(color)
    {
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        if (boundsRect.isEmpty())
        {
            return;
        }

        auto bounds = boundsRect.toFloat();
        juce::Path path;
        path.startNewSubPath(bounds.getX(), bounds.getY() + m_random.nextFloat() * bounds.getHeight());
        for (int x = boundsRect.getX() + 1; x <= boundsRect.getRight(); ++x)
        {
            path.lineTo(static_cast<float>(x), bounds.getY() + m_random.nextFloat() * bounds.getHeight());
        }

        g.setColour(J(m_color));
        g.strokePath(path, juce::PathStrokeType(1.4f));
    }
};

struct SpreadModulatorComponent : SmartGridOneMainVisualizerComponent
{
    size_t m_numVoices;
    std::atomic<size_t>* m_activeTrio;

    SpreadModulatorComponent(size_t numVoices, std::atomic<size_t>* activeTrio)
        : m_numVoices(numVoices)
        , m_activeTrio(activeTrio)
    {
        jassert(numVoices > 1);
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        float size = std::min(boundsRect.getWidth(), boundsRect.getHeight()) * 0.4f;
        auto bounds = boundsRect.toFloat().withSizeKeepingCentre(size, size);
        float slotWidth = bounds.getWidth() / m_numVoices;
        float gap = std::min(2.0f, slotWidth * 0.2f);
        float barWidth = (slotWidth - gap) * 0.5f;
        size_t firstVoice = m_activeTrio ? m_activeTrio->load() * TheNonagonInternal::x_voicesPerTrio : 0;
        for (size_t voice = 0; voice < m_numVoices; ++voice)
        {
            float value = static_cast<float>(voice) / (m_numVoices - 1);
            float top = bounds.getY() + (1.1f - value) / 1.2f * bounds.getHeight();
            g.setColour(J(TheNonagonSmartGrid::VoiceColor(firstVoice + voice)));
            g.fillRect(bounds.getX() + voice * slotWidth + (slotWidth - barWidth) * 0.5f,
                top, barWidth, bounds.getBottom() - top);
        }
    }
};
