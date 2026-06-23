#pragma once

#include <cstddef>

namespace SmartGrid
{
    struct MessageInLatency
    {
        static constexpr size_t x_latencyUs = 20000;

        static size_t WithLatency(size_t timestamp)
        {
            return timestamp + x_latencyUs;
        }
    };
}
