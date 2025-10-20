#pragma once

#include "AdaptiveWaveTable.hpp"

struct SemiQuantizedWavetable
{
    static constexpr float x_detuneCents = 20.0;
    static constexpr float x_detuneRatio = powf(2.0, x_detuneCents / 1200.0);

    static void WriteRegion(BasicWaveTable& waveTable, int numSamples, int centerSample, float centerValue)
    {
        waveTable[centerSample] = centerValue;
        for (size_t i = 0; i < numSamples; ++i)
        {
            float detuneFactor = powf(x_detuneRatio, static_cast<float>(i + 1) / numSamples);
            if (centerSample + i < BasicWaveTable::x_tableSize)
            {
                waveTable[centerSample + i] = centerValue * detuneFactor;
            }

            if (centerSample - i >= 0)
            {
                waveTable[centerSample - i] = centerValue * detuneFactor;
            }
        }
    }

    static void LinearFill(BasicWaveTable& waveTable, int startSample, int endSample)
    {
        for (int i = startSample + 1; i < endSample; ++i)
        {
            float startValue = waveTable[startSample];
            float endValue = waveTable[endSample];
            float t = static_cast<float>(i - startSample) / (endSample - startSample);
            waveTable[i] = startValue * (1 - t) + endValue * t;
        }
    }

    static void GenerateDetuneWaveTable(BasicWaveTable& waveTable)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            size_t octaveSample = BasicWaveTable::x_tableSize * i / 4;
            size_t fifthSample = BasicWaveTable::x_tableSize * (2 * i + 1) / 8;
            size_t nextOctaveSample = BasicWaveTable::x_tableSize * (i + 1) / 4;
            WriteRegion(waveTable, BasicWaveTable::x_tableSize / 8, octaveSample, std::pow(2.0, i - 2));
            WriteRegion(waveTable, BasicWaveTable::x_tableSize / 16, fifthSample, std::pow(2.0, i - 3) * 3);
            LinearFill(waveTable, octaveSample, fifthSample);
            LinearFill(waveTable, fifthSample, nextOctaveSample);
        }

        WriteRegion(waveTable, BasicWaveTable::x_tableSize / 8 - 1, BasicWaveTable::x_tableSize - 1, std::pow(2.0, 2));
    }
};