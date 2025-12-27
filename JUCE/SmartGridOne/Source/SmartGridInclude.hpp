#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshorten-64-to-32"
#include "TheNonagonSquiggleBoyQuadLaunchpadTwister.hpp"
#include "LaunchPadMidi.hpp"
#include "EncoderMidi.hpp"
#include "SmartBus.hpp"
#include "TheNonagonSquiggleBoyWrldBldr.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "WrldBLDRMidi.hpp"
#include "AudioInputBuffer.hpp"
#include "KMixMidi.hpp"
#include "RollingBuffer.hpp"
#include "Mutex.hpp"
#include "SampleTimer.hpp"
#pragma GCC diagnostic pop

inline juce::Colour J(SmartGrid::Color color)
{
    return juce::Colour(color.m_red, color.m_green, color.m_blue);
}