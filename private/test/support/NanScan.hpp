#pragma once

// NanScan.hpp -- NaN / Inf / denormal detection for DSP output buffers.
//
// FROZEN API (WP-2).

#include <cstddef>
#include <cmath>

#include "doctest.h"

namespace TestNan
{

// "Bad" = NaN or +/-Inf. Denormals are intentionally NOT bad here (they are
// finite, valid floats); use IsDenormal to flag them separately.
inline bool IsBad(float x)
{
    return std::isnan(x) || std::isinf(x);
}

// True for a non-zero subnormal (denormal) float. Zero is normal, not denormal.
inline bool IsDenormal(float x)
{
    return x != 0.0f && std::fpclassify(x) == FP_SUBNORMAL;
}

// Index of the first bad (NaN/Inf) sample, or -1 if the buffer is clean.
inline long ScanBuffer(const float* buf, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
    {
        if (IsBad(buf[i]))
        {
            return static_cast<long>(i);
        }
    }
    return -1;
}

// Count of non-zero denormal samples in the buffer.
inline std::size_t CountDenormals(const float* buf, std::size_t n)
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        if (IsDenormal(buf[i]))
        {
            ++count;
        }
    }
    return count;
}

// doctest assertion: CHECK that the buffer contains no NaN/Inf, reporting the
// offending index if it does.
inline void AssertClean(const float* buf, std::size_t n)
{
    long bad = ScanBuffer(buf, n);
    DOCTEST_INFO("first bad (NaN/Inf) sample index: " << bad);
    DOCTEST_CHECK(bad == -1);
}

} // namespace TestNan
