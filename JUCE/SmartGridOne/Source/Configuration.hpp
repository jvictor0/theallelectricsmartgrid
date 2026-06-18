#pragma once

#include <JuceHeader.h>

struct Configuration
{
    bool m_stereo = false;
    bool m_forceStereo = false;
    juce::String m_audioInputDeviceName;
    juce::String m_audioOutputDeviceName;
};
