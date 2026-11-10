#pragma once

#include "MeterComponent.hpp"

struct EnvelopeFollowerComponent : SmartGridOneMainVisualizerComponent
{
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;
    QuadModulator m_modulator;

    EnvelopeFollowerComponent(TheNonagonSquiggleBoyInternal::UIState* uiState, QuadModulator modulator)
        : m_uiState(uiState)
        , m_modulator(modulator)
    {
        assert(SmartGridOneModulators::GetMixerScope(modulator) != SmartGridOne::QuadScopes::NumScopes);
        assert(!SmartGridOneModulators::IsMixerAudio(modulator));
    }

    float GetEnvelope(size_t lane) const
    {
        auto scope = SmartGridOneModulatorHistory::GetControlScope(*m_uiState, ModulatorRef(m_modulator, lane));
        float value = 0.0f;
        if (scope.m_writer && scope.m_writer->m_acceptedIndex > 0)
        {
            scope.m_writer->ReadHistory(scope.m_lane, lane, scope.m_writer->m_acceptedIndex - 1, 0, value);
        }

        return value;
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> bounds) override
    {
        g.fillAll(juce::Colours::black);
        MeterDrawer drawer{static_cast<float>(bounds.getHeight()), static_cast<float>(bounds.getWidth()),
            static_cast<float>(bounds.getX()), static_cast<float>(bounds.getY())};
        drawer.DrawRadialScale(g);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            drawer.DrawRadialEnvelopeNeedle(g, GetEnvelope(lane), J(TheNonagonSmartGrid::VoiceColor(lane)));
        }
    }
};
