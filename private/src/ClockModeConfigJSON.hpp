#pragma once

#include "Json.hpp"

namespace ClockModeConfigJSON
{
    static constexpr char x_externalClockKey[] = "external_clock";

    inline void WriteExternalClock(JSON nonagonConfig, JsonArena& arena, bool externalClock)
    {
        nonagonConfig.SetNew(x_externalClockKey, arena.Boolean(externalClock));
    }

    inline bool ReadExternalClock(JSON nonagonConfig, bool defaultValue)
    {
        JSON externalClockJ = nonagonConfig.Get(x_externalClockKey);
        if (externalClockJ.IsNull())
        {
            return defaultValue;
        }

        return externalClockJ.BooleanValue();
    }
}
