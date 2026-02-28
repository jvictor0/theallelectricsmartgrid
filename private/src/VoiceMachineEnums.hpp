#pragma once

namespace VoiceMachine
{
    enum class SourceMachine : int
    {
        DualWaveShapingVCO = 0,
        Thru = 1,
        NumSourceMachines = 2,
    };

    enum class FilterMachine : int
    {
        Ladder4Pole = 0,
        SVF2Pole = 1,
        NumFilterMachines = 2,
    };

    inline const char* ToString(SourceMachine machine)
    {
        switch (machine)
        {
            case SourceMachine::DualWaveShapingVCO:
                return "Dual VCO";
            case SourceMachine::Thru:
                return "Thru";
        }

        return "Unknown";
    }

    inline const char* ToString(FilterMachine machine)
    {
        switch (machine)
        {
            case FilterMachine::Ladder4Pole:
                return "Ladder";
            case FilterMachine::SVF2Pole:
                return "SVF";
        }

        return "Unknown";
    }
}
