#pragma once

// Generated from DSP.AHD.Memory. Do not edit.
//

#include <array>
#include <cstdint>
#include <stddef.h>

template<typename Number>
struct AHDMemory
{
    size_t m_stage;
    Number m_position;
    Number m_previousSamples;
    Number m_startOutput;
    Number m_rawOutput;
    Number m_endSamples;
    bool m_armed;
    bool m_forwardOnly;
};
