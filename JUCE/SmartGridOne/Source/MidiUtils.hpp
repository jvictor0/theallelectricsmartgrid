#pragma once

#include <JuceHeader.h>

inline juce::String MidiInputDeviceIdentifierFromName(const juce::String& name)
{
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        if (device.name == name)
        {
            return device.identifier;
        }
    }

    return "";
}

inline juce::String MidiOutputDeviceIdentifierFromName(const juce::String& name)
{
    for (const auto& device : juce::MidiOutput::getAvailableDevices())
    {
        if (device.name == name)
        {
            return device.identifier;
        }
    }

    return "";
}