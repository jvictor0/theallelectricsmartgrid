#pragma once

#include <cstdint>

#include "BitSet.hpp"
#include "VoiceMachineEnums.hpp"

// Tracks which source and filter machines a parameter applies to.
// Constructed from two bit vectors for compact storage in the parameter list.
//
struct MachineFlags
{
    static constexpr uint8_t x_all = 0b111;
    static constexpr uint8_t x_dualWaveShapingVCOOnly = 0b001;
    static constexpr uint8_t x_physicalModelingOnly = 0b100;
    static constexpr size_t x_numSourceMachines = 3;
    static constexpr size_t x_numFilterMachines = 2;

    bool m_sourceMachines[x_numSourceMachines];
    bool m_filterMachines[x_numFilterMachines];

    MachineFlags(BitSet8 sourceBits, BitSet8 filterBits)
    {
        for (size_t i = 0; i < x_numSourceMachines; ++i)
        {
            m_sourceMachines[i] = sourceBits.Get(static_cast<int>(i));
        }

        for (size_t i = 0; i < x_numFilterMachines; ++i)
        {
            m_filterMachines[i] = filterBits.Get(static_cast<int>(i));
        }
    }

    bool AppliesToSource(VoiceMachine::SourceMachine machine) const
    {
        return m_sourceMachines[static_cast<int>(machine)];
    }

    bool AppliesToFilter(VoiceMachine::FilterMachine machine) const
    {
        return m_filterMachines[static_cast<int>(machine)];
    }
};
