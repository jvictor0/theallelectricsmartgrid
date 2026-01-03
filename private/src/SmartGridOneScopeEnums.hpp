#pragma once

namespace SmartGridOne
{
    enum class AudioScopes : size_t
    {
        VCO1 = 0,
        VCO2 = 1,
        PostFilter = 2,
        PostAmp = 3,
        NumScopes = 4
    };

    enum class ControlScopes : size_t
    {
        LFO1 = 0,
        LFO2 = 1,
        GangedRandom1 = 2,
        GangedRandom2 = 3,
        NumScopes = 4
    };

    enum class MonoScopes : size_t
    {
        TheoryOfTime = 0,
        NumScopes = 1
    };

    enum class SourceScopes : size_t
    {
        PreFilter = 0,
        PostFilter = 1,
        NumScopes = 2
    };

    enum class QuadScopes : size_t
    {
        Delay = 0,
        Reverb = 1,
        Master = 2,
        Dry = 3,
        Stereo = 4,
        NumScopes = 5
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

