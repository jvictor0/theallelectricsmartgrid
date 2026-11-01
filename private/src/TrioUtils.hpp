#pragma once

#include "MultiChannelNumber.hpp"

// One lane per voice of a three-voice track. The compiler pads the vector to
// four lanes.
//
using TrioFloat = MultiChannelNumber<float, 3>;
using TrioDouble = MultiChannelNumber<double, 3>;
