#pragma once

#include "MultiChannelNumber.hpp"
#include "QuadUtils.hpp"

using StereoFloat = MultiChannelNumber<float, 2>;

template <size_t Size>
using MultiChannelFloat = MultiChannelNumber<float, Size>;
