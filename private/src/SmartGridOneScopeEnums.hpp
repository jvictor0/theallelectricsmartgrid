#pragma once

namespace SmartGridOne
{
    constexpr size_t VoiceScopeIndex(size_t logicalScope, size_t trio)
    {
        return logicalScope * 3 + trio;
    }

    enum class VoiceScopes : size_t
    {
        VCO1 = 0,
        VCO2 = 1,
        PostFilter = 2,
        PostAmp = 3,
        Sub = 4,
        LFO1 = 5,
        LFO2 = 6,
        PhysicalModelingEnvelope = 7,
        ModulationEnvelope = 8,
        AmpEnvelope = 9,
        NumScopes = 10
    };

    inline size_t ControlScopeToModulatorIndex(size_t scopeIx)
    {
        static constexpr size_t x_scopeToModulator[] = { 21, 22, 0, 13, 12, 6, 7, 22, 4, 5 };
        return scopeIx < static_cast<size_t>(VoiceScopes::NumScopes) ? x_scopeToModulator[scopeIx] : 0;
    }

    enum class MonoScopes : size_t
    {
        TheoryOfTime = 0,
        PartialMachine = 1,
        GlobalGangedRandom1 = 2,
        GlobalGangedRandom2 = 3,
        NumScopes = 4
    };

    enum class QuadScopes : size_t
    {
        Delay = 0,
        Reverb = 1,
        Master = 2,
        Dry = 3,
        Stereo = 4,
        PartialMachine = 5,
        Sub = 6,
        QuadGangedRandom1 = 7,
        QuadGangedRandom2 = 8,
        QuadLFO1 = 9,
        QuadLFO2 = 10,
        SourcePreFilter = 11,
        SourcePostFilter = 12,
#define F(tap, envelope, audio) envelope, audio,
#include "ForEachMixerModulationTap.hpp"
#undef F
        NumScopes
    };

    enum class ModulationGlyphs : size_t
    {
        LFO = 0,
        ADSR = 1,
        Sheaf = 2,
        SmoothRandom = 3,
        Noise = 4,
        Quadrature = 5,
        Spread = 6,
        None = 7,
        SubSine = 8,
        HarmonicWave = 9,
        AudioOutput = 10,
        AudioSend = 11,
        Wet = 12,
        VU = 13,
        NumGlyphs = 14,
    };
}

//

