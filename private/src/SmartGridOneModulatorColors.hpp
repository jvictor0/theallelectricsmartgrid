#pragma once

#include "Color.hpp"

// The slot palette is an authored convention documented in docs/encoder-system.md.
// Share the Sheafy colors with Nonagon without depending on encoder routing.
//
namespace SmartGridOneModulatorColors
{
inline constexpr SmartGrid::Color x_coralRed{255, 0, 71};
inline constexpr SmartGrid::Color x_cyan{0, 219, 255};
inline constexpr SmartGrid::Color x_berry{194, 37, 127};
inline constexpr SmartGrid::Color x_lime{118, 199, 14};
inline constexpr SmartGrid::Color x_gold{255, 198, 40};
inline constexpr SmartGrid::Color x_violet{141, 95, 255};
inline constexpr SmartGrid::Color x_fuchsia{255, 18, 210};
inline constexpr SmartGrid::Color x_olive{85, 106, 0};
inline constexpr SmartGrid::Color x_ocean{25, 248, 186};
inline constexpr SmartGrid::Color x_indigo = SmartGrid::Color::Indigo;
inline constexpr SmartGrid::Color x_seaGreen{0, 253, 28};
inline constexpr SmartGrid::Color x_deepRed{183, 0, 0};
inline constexpr SmartGrid::Color x_paleLilac{252, 196, 255};
}
