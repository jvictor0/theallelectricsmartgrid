#pragma once

#include "VoiceMachineEnums.hpp"
#include <JuceHeader.h>

struct SmartGridOneMainVisualizerComponent
{
    using SourceMachineFlags = VoiceMachine::SourceMachineFlags;

    // Machine flags determining which source machines this visualizer applies to
    //
    SourceMachineFlags m_sourceMachineFlags;

    SmartGridOneMainVisualizerComponent()
        : m_sourceMachineFlags(SourceMachineFlags::All())
    {
    }

    SmartGridOneMainVisualizerComponent(SourceMachineFlags flags)
        : m_sourceMachineFlags(flags)
    {
    }

    // Check if this visualizer should be shown for the given source machine
    //
    bool ShouldShowForMachine(VoiceMachine::SourceMachine machine) const
    {
        return m_sourceMachineFlags.Matches(machine);
    }

    // Main drawing method - receives bounds for the block area
    //
    virtual void Draw(juce::Graphics& g, juce::Rectangle<int> bounds) = 0;

    // Optional click handler - event position is in block-local coordinates
    //
    virtual void OnClick(const juce::MouseEvent& event)
    {
    }

    virtual ~SmartGridOneMainVisualizerComponent() = default;
};
