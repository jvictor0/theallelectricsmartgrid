#pragma once

#include <cmath>
#include <cstdint>

namespace SmartGrid
{
    struct ExponentialEncoderDelta
    {
        static constexpr double x_minimumDelta = 1.0 / 65536.0;
        static constexpr double x_codesPerDoubling = 8.0;

        static double Decode(uint8_t value)
        {
            const int offset = static_cast<int>(value) - 64;
            const double exponent = std::log(2.0) / x_codesPerDoubling;
            const double magnitude = x_minimumDelta * std::expm1(exponent * std::abs(offset))
                / std::expm1(exponent);
            return offset < 0 ? -magnitude : magnitude;
        }
    };
}
