#pragma once

#include "PartialMachine.hpp"
#include "PathDrawer.hpp"
#include "SmartGridInclude.hpp"
#include "SmartGridOneMainVisualizerComponent.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include <JuceHeader.h>

#include <algorithm>
#include <cmath>

struct PartialMachineComponentUtils
{
    static float HueFromIndex(int index, float offset)
    {
        float hue = (static_cast<float>(index) + offset) / 4.0f;
        return hue - std::floor(hue);
    }

    static juce::Colour ColourFromIndex(const PartialMachine::Index& index)
    {
        float hue = HueFromIndex(index.m_index, index.m_interp);
        return juce::Colour::fromHSV(hue, 0.85f, 1.0f, 1.0f);
    }

    static float ScaledMagnitude(float magnitude)
    {
        return std::min(1.0f, std::max(0.0f, PathDrawer::AmpToDbNormalized(magnitude)));
    }

    static float ScreenXForOmega(float omega, float width)
    {
        float clampedOmega = std::min(0.5f, std::max(1.0f / static_cast<float>(PathDrawer::x_numPoints * 2), omega));
        return width * PathDrawer::LinearToLog(clampedOmega);
    }
};

struct PartialMachineInputSpectrumComponent : public SmartGridOneMainVisualizerComponent
{
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;
    WindowedFFT m_windowedFFT;

    PartialMachineInputSpectrumComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : SmartGridOneMainVisualizerComponent()
        , m_uiState(uiState)
        , m_windowedFFT(
            &uiState->m_squiggleBoyUIState.m_monoAudioScopeWriter,
            static_cast<size_t>(SmartGridOne::MonoAudioScopes::PartialMachine))
    {
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        g.fillAll(juce::Colours::black);

        auto bounds = boundsRect.toFloat();
        auto width = bounds.getWidth();
        auto height = bounds.getHeight();

        m_windowedFFT.Compute(0);
        PathDrawer pathDrawer(height, width, bounds.getX(), bounds.getY());
        pathDrawer.DrawWindowedDFT(g, juce::Colours::darkgrey, &m_windowedFFT);

        PartialMachine::UIState& partialMachineUIState = m_uiState->m_squiggleBoyUIState.m_partialMachineUIState;
        PartialMachine::Input dspInput = partialMachineUIState.ToInput();
        const PartialMachine::Snapshot& snapshot = partialMachineUIState.GetCurrentSnapshot();

        for (size_t i = 0; i < snapshot.m_numAtoms; ++i)
        {
            const PartialMachine::SpectralModel::Atom& atom = snapshot.m_atoms[i];
            float reduction = PartialMachine::SynthesisContext::GetReduction(atom, dspInput.m_synthesisContextInput);
            float magnitude = PartialMachineComponentUtils::ScaledMagnitude(atom.m_synthesisMagnitude * reduction);
            float x = bounds.getX() + PartialMachineComponentUtils::ScreenXForOmega(atom.m_synthesisOmega, width);
            float y = bounds.getY() + height * (1.0f - magnitude);

            g.setColour(PartialMachineComponentUtils::ColourFromIndex(atom.m_index));
            g.drawLine(x, bounds.getBottom(), x, y, 1.0f);
        }
    }
};

struct PartialMachineSpatialComponent : public SmartGridOneMainVisualizerComponent
{
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;

    PartialMachineSpatialComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : SmartGridOneMainVisualizerComponent()
        , m_uiState(uiState)
    {
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        g.fillAll(juce::Colours::black);

        auto bounds = boundsRect.toFloat();
        float width = bounds.getWidth();
        float height = bounds.getHeight();
        float centerX = bounds.getCentreX();
        float centerY = bounds.getCentreY();
        float maxPositionRadius = std::min(width, height) * 0.42f;

        g.setColour(juce::Colours::darkgrey);
        g.drawEllipse(
            centerX - maxPositionRadius,
            centerY - maxPositionRadius,
            maxPositionRadius * 2.0f,
            maxPositionRadius * 2.0f,
            1.0f);

        PartialMachine::UIState& partialMachineUIState = m_uiState->m_squiggleBoyUIState.m_partialMachineUIState;
        PartialMachine::Input dspInput = partialMachineUIState.ToInput();
        const PartialMachine::Snapshot& snapshot = partialMachineUIState.GetCurrentSnapshot();

        for (size_t i = 0; i < snapshot.m_numAtoms; ++i)
        {
            const PartialMachine::SpectralModel::Atom& atom = snapshot.m_atoms[i];
            float reduction = PartialMachine::SynthesisContext::GetReduction(atom, dspInput.m_synthesisContextInput);
            float radius = PartialMachine::SynthesisContext::GetRadius(atom, dspInput.m_synthesisContextInput);
            float azimuth = PartialMachine::SynthesisContext::GetAzimuth(atom, dspInput.m_synthesisContextInput);
            float scaledMagnitude = PartialMachineComponentUtils::ScaledMagnitude(atom.m_synthesisMagnitude * reduction);
            float displayRadius = 1.0f + scaledMagnitude * std::min(width, height) * 0.04f;
            float x = centerX + maxPositionRadius * radius * std::cos(juce::MathConstants<float>::twoPi * azimuth);
            float y = centerY - maxPositionRadius * radius * std::sin(juce::MathConstants<float>::twoPi * azimuth);

            g.setColour(PartialMachineComponentUtils::ColourFromIndex(atom.m_index));
            g.fillEllipse(
                x - displayRadius,
                y - displayRadius,
                displayRadius * 2.0f,
                displayRadius * 2.0f);
        }
    }
};
