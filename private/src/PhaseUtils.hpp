#pragma once
#include <cmath>
#include "WaveTable.hpp"

namespace PhaseUtils
{
float HzToNatural(float hz, float deltaT)
{
    return hz * deltaT;
}

float VOctToHz(float vOct)
{
    static const float x_middleC = 261.6256f;
    return powf(2.0f, vOct) * x_middleC;
}

float VOctToNatural(float vOct, float deltaT)
{
    return HzToNatural(VOctToHz(vOct), deltaT);
}

float SyncedEval(
    const WaveTable* waveTable,
    float phase,
    float mult,
    float offset)
{
    float thisPhase = fmod(phase * mult + offset, mult);
    float floorMult = floorf(mult);

    if (floorMult < thisPhase)
    {
        float amp = mult - floorMult;
        return amp * waveTable->Evaluate((thisPhase - floorMult) / amp);
    }
    else
    {
        return waveTable->Evaluate(thisPhase - floorf(thisPhase));
    }
}

} // namespace PhaseUtils
