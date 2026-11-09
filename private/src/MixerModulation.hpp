#pragma once

#include <cstddef>

enum class MixerModulationTap : size_t
{
#define F(tap, envelope, audio) tap,
#include "ForEachMixerModulationTap.hpp"
#undef F
    NumTaps
};
