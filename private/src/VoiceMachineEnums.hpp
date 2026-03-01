#pragma once

namespace VoiceMachine
{
    enum class SourceMachine : int
    {
        DualWaveShapingVCO = 0,
        Thru = 1,
        PhysicalModeling = 2,
        NumSourceMachines = 3,
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
            case SourceMachine::PhysicalModeling:
                return "PhysMod";
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
