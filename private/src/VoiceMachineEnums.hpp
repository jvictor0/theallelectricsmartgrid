#pragma once

#include <cstdint>

namespace VoiceMachine
{
    enum class SourceMachine : int
    {
        DualWaveShapingVCO = 0,
        Thru = 1,
        PhysicalModeling = 2,
        DualSample = 3,
        NumSourceMachines = 4,
    };

    // Flags for specifying which source machines a component applies to
    //
    struct SourceMachineFlags
    {
        uint8_t m_flags;

        static constexpr uint8_t x_dualWaveShapingVCO = 1 << static_cast<int>(SourceMachine::DualWaveShapingVCO);
        static constexpr uint8_t x_thru = 1 << static_cast<int>(SourceMachine::Thru);
        static constexpr uint8_t x_physicalModeling = 1 << static_cast<int>(SourceMachine::PhysicalModeling);
        static constexpr uint8_t x_dualSample = 1 << static_cast<int>(SourceMachine::DualSample);
        static constexpr uint8_t x_all = 0xFF;

        SourceMachineFlags()
            : m_flags(x_all)
        {
        }

        SourceMachineFlags(uint8_t flags)
            : m_flags(flags)
        {
        }

        static SourceMachineFlags DualVCOOnly()
        {
            return SourceMachineFlags(x_dualWaveShapingVCO);
        }

        static SourceMachineFlags PhysicalModelingOnly()
        {
            return SourceMachineFlags(x_physicalModeling);
        }

        static SourceMachineFlags DualSampleOnly()
        {
            return SourceMachineFlags(x_dualSample);
        }

        static SourceMachineFlags All()
        {
            return SourceMachineFlags(x_all);
        }

        bool Matches(SourceMachine machine) const
        {
            return (m_flags & (1 << static_cast<int>(machine))) != 0;
        }
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
            case SourceMachine::DualSample:
                return "Dual Sample";
            default:
                return "Unknown";
        }
    }

    inline const char* ToString(FilterMachine machine)
    {
        switch (machine)
        {
            case FilterMachine::Ladder4Pole:
                return "Ladder";
            case FilterMachine::SVF2Pole:
                return "SVF";
            default:
                return "Unknown";
        }
    }
}
