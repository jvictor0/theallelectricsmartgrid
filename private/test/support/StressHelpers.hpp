#pragma once

// StressHelpers.hpp -- local conveniences for the WP-10 capstone stress / fuzz
// tests. NOT part of the frozen SynthRig API; these are test-side helpers built
// strictly on top of the public SynthRig front door.
//
// The two things every WP-10 test needs and that are non-obvious from the
// control-surface map:
//
//   1. GetNotesPlaying(rig): the recipe to make voices actually gate AND be
//      audible. Discovered empirically (see sys_startup_stability.cpp header):
//      starting the sequencer makes voices gate, but the per-track GAIN faders
//      default to 0 so the audio path is silent. Raising the three gain faders
//      (fader indices 5,6,7 -> SquiggleBoy m_faders[5..7], the GetGainFader
//      taps for the 9 voices / 3 tracks) opens the audio path. With gains up +
//      sequencer running the default LameJuis state already produces a steady
//      stream of notes (peak ~0.6-0.76 across the master chain).
//
//   2. The TheoryOfTime encoder-bank mapping (see TheoryOfTime control map in
//      the test headers): selecting the TheoryOfTime encoder bank requires
//      tapping the global-bank selector at BottomRight (route 3) x=3, y=8.
//      After that the route-4 encoder cells address ToT params:
//        Tempo        -> (0,0)   (drives TheoryOfTime::Input::m_freq)
//        TempoLFOSkew -> (0,2)
//        TempoLFOMult -> (1,2)
//        TempoLFOShape-> (2,2)
//        TempoLFOIndex-> (3,3)
//      (full table in ForEachSmartGridOneParam.hpp, Bank::TheoryOfTime rows.)

#include <cmath>
#include <cstddef>
#include <vector>

#include "SynthRig.hpp"

namespace stress
{

// Fader indices whose values feed SquiggleBoy::Input::GetGainFader for the three
// voice tracks. Raising these to 1.0 unmutes the audio path.
//
inline constexpr int kGainFaderLo = 5;
inline constexpr int kGainFaderHi = 7;

// TheoryOfTime encoder-bank selector cell (global bank #0) on the BottomRight
// grid (route 3).
//
inline constexpr int kToTBankSelX = 3;
inline constexpr int kToTBankSelY = 8;

// TheoryOfTime encoder cells (valid once the ToT bank is selected).
//
inline constexpr int kTempoEncX = 0, kTempoEncY = 0;        // Param::Tempo
inline constexpr int kLfoMultEncX = 1, kLfoMultEncY = 2;    // Param::TempoLFOMult
inline constexpr int kLfoIndexEncX = 3, kLfoIndexEncY = 3;  // Param::TempoLFOIndex
inline constexpr int kLfoSkewEncX = 0, kLfoSkewEncY = 2;    // Param::TempoLFOSkew

// Raise the three gain faders so voices are audible.
//
inline void OpenGain(synthrig::SynthRig& rig)
{
    for (int f = kGainFaderLo; f <= kGainFaderHi; ++f)
    {
        rig.SetFader(f, 1.0f);
    }
}

// Make voices actually gate and sound: warm up, open the gain, start the
// sequencer. After this the default pattern produces a steady note stream.
//
inline void GetNotesPlaying(synthrig::SynthRig& rig)
{
    rig.RunFrames(2);
    OpenGain(rig);
    rig.RunFrames(2);
    rig.StartSequencer();
}

// Select the TheoryOfTime encoder bank so the route-4 encoder cells address ToT
// tempo/LFO params.
//
inline void SelectToTBank(synthrig::SynthRig& rig)
{
    rig.TapPad(synthrig::SynthRig::RouteBottomRight, kToTBankSelX, kToTBankSelY);
    rig.RunFrames(1);
}

// Largest |x[i+1]-x[i]| across one channel of the captured output ring.
// channel 0..3 = quad, 4..5 = stereo, 6 = sub.
//
inline float ChannelMaxAbsDelta(const synthrig::SynthRig& rig, int channel)
{
    const auto& o = rig.Output();
    float maxd = 0.0f;
    for (std::size_t i = 0; i + 1 < o.size(); ++i)
    {
        const float* a = &o[i].m_quad[0];
        const float* b = &o[i + 1].m_quad[0];
        float d = std::fabs(b[channel] - a[channel]);
        if (d > maxd)
        {
            maxd = d;
        }
    }
    return maxd;
}

// Largest absolute first-difference over ALL 7 channels.
//
inline float OutputMaxAbsDelta(const synthrig::SynthRig& rig)
{
    float maxd = 0.0f;
    for (int c = 0; c < 7; ++c)
    {
        float d = ChannelMaxAbsDelta(rig, c);
        if (d > maxd)
        {
            maxd = d;
        }
    }
    return maxd;
}

} // namespace stress
