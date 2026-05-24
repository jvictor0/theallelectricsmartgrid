#pragma once

#include <cstddef>

struct RecordingConfig
{
    enum class Source
    {
        Water,
        Earth,
        Fire,
        None,
    };

    Source m_source = Source::Water;

    bool GetTrioIndex(size_t& trioIndexOut) const
    {
        switch (m_source)
        {
        case Source::None:
            return false;

        case Source::Water:
            trioIndexOut = 0;
            return true;

        case Source::Earth:
            trioIndexOut = 1;
            return true;

        case Source::Fire:
            trioIndexOut = 2;
            return true;

        default:
            return false;
        }
    }
};
