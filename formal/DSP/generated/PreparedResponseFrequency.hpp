#pragma once

// Generated from DSP.ResponseFrequency. Do not edit.
//

#include <array>

template<typename Number>
struct PreparedResponseFrequency
{
    Number m_cyclesPerSample;
    Number m_sinHalf;
    Number m_cosHalf;
    Number m_zInverseReal;
    Number m_zInverseImag;
};
