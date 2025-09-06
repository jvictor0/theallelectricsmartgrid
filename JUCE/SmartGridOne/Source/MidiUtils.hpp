#pragma once

#include <JuceHeader.h>

inline juce::String MidiInputDeviceIdentifierFromName(const juce::String& name)
{
    for (int i = 0; i < juce::MidiInput::getAvailableDevices().size(); ++i)
    {
        if (juce::MidiInput::getAvailableDevices()[i].name == name)
        {
            return juce::MidiInput::getAvailableDevices()[i].identifier;
        }
    }

    return "";
}

inline juce::String MidiOutputDeviceIdentifierFromName(const juce::String& name)
{
    for (int i = 0; i < juce::MidiOutput::getAvailableDevices().size(); ++i)
    {
        if (juce::MidiOutput::getAvailableDevices()[i].name == name)
        {
            return juce::MidiOutput::getAvailableDevices()[i].identifier;
        }
    }

    return "";
}