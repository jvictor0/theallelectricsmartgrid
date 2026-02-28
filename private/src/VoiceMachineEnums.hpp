#pragma once

namespace VoiceMachine
{
    enum class SourceMachine : int
    {
        DualWaveShapingVCO = 0,
        Thru = 1,
    };

    enum class FilterMachine : int
    {
        Ladder4Pole = 0,
        SVF2Pole = 1,
    };
}
