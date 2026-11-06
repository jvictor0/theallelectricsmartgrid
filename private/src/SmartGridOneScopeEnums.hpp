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
        GangedRandom1 = 7,
        GangedRandom2 = 8,
        GangedRandom3 = 9,
        GangedRandom4 = 10,
        ModulationEnvelope = 11,
        AmpEnvelope = 12,
        NumScopes = 13
    };

    inline size_t ControlScopeToModulatorIndex(size_t scopeIx)
    {
        static constexpr size_t x_scopeToModulator[] = { 6, 7, 0, 1, 2, 3, 4, 5 };
        constexpr size_t x_firstControl = static_cast<size_t>(VoiceScopes::LFO1);
        return scopeIx >= x_firstControl && scopeIx < static_cast<size_t>(VoiceScopes::NumScopes)
            ? x_scopeToModulator[scopeIx - x_firstControl] : 0;
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
        NumScopes = 13
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
    };
}

//

