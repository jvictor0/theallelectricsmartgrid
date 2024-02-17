#pragma once

struct TrioOctaveSwitches
{
    static constexpr size_t x_numTrios = 3;
    static constexpr size_t x_voicesPerTrio = 3;
    
    enum class Spread : int
    {
        Tight = 0,
        OneUp = 1,
        TwoUp = 2,
        Loose = 3,
        NumSpreads = 4
    };
    
    static int SpreadOffset(Spread spread, size_t voice)
    {
        if (spread == Spread::Loose)
        {
            return voice;
        }
        else
        {
            return (static_cast<int>(spread) + voice) / 3;
        }
    }

    struct Input
    {
        int m_octave[x_numTrios];
        Spread m_spread[x_numTrios];

        float Octavize(float in, size_t voiceIx)
        {
            size_t trioIx = voiceIx / x_voicesPerTrio;
            return in + m_octave[trioIx] + SpreadOffset(m_spread[trioIx], voiceIx % x_voicesPerTrio);
        }

        Input()
        {
            for (size_t i = 0; i < x_numTrios; ++i)
            {
                m_octave[i] = 0;
                m_spread[i] = Spread::Tight;
            }
        }
    };
};
