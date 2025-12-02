#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

struct HSV
{
    float h;    // 0..1
    float s;    // 0..1
    float v;    // 0..1
};

inline HSV RGBToHSV(uint8_t r8, uint8_t g8, uint8_t b8)
{
    float r = r8 / 255.0f;
    float g = g8 / 255.0f;
    float b = b8 / 255.0f;

    float maxc = std::fmax(r, std::fmax(g, b));
    float minc = std::fmin(r, std::fmin(g, b));
    float delta = maxc - minc;

    HSV out {};

    out.v = maxc;

    if (delta < 1e-6f)
    {
        // grey (no hue)
        out.h = 0.0f;
        out.s = 0.0f;
        return out;
    }

    out.s = delta / maxc;

    float h;

    if (maxc == r)
    {
        h = (g - b) / delta;
    }
    else if (maxc == g)
    {
        h = 2.0f + (b - r) / delta;
    }
    else
    {
        h = 4.0f + (r - g) / delta;
    }

    h /= 6.0f;

    if (h < 0.0f)
    {
        h += 1.0f;
    }

    out.h = h;

    return out;
}

// Map 24-bit RGB to 7-bit palette code (0..127)
inline uint8_t RGBToYaeltexCode(uint8_t r, uint8_t g, uint8_t b)
{
    HSV hsv = RGBToHSV(r, g, b);

    float s = hsv.s;
    float v = hsv.v;

    // 1) "white" short-circuit:
    //    low-ish saturation, high value.
    if (s < 0.10f && v > 0.85f)
    {
        return 127;    // greyish-white slot
    }

    // 2) black / very dark
    //    either almost no value, or below the 1/4 saturation bin.
    if (v < 0.10f || s < 0.25f)
    {
        return 0;      // black
    }

    // 3) quantise saturation into four bands:
    //    <1/4 -> black (already handled)
    //    1/4-1/2 -> 1/3
    //    1/2-3/4 -> 2/3
    //    >3/4    -> 1
    int satIndex;

    if (s < 0.50f)
    {
        satIndex = 0;      // 1/3
    }
    else if (s < 0.75f)
    {
        satIndex = 1;      // 2/3
    }
    else
    {
        satIndex = 2;      // 1
    }

    // 4) quantise hue into 42 slices around the circle
    constexpr float hueStep = 360.0f / 42.0f;

    float hueDeg = hsv.h * 360.0f;
    float rawIndex = hueDeg / hueStep;

    int hueIndex = static_cast<int>(std::round(rawIndex));

    // wrap just in case rounding hits 42
    hueIndex %= 42;
    if (hueIndex < 0)
    {
        hueIndex += 42;
    }

    // 5) reconstruct palette index:
    // codes 1..126 = 42 hues * 3 saturations
    //   code = 1 + 3*hueIndex + satIndex
    uint8_t code = static_cast<uint8_t>(1 + 3 * hueIndex + satIndex);

    // safety clamp (should already be 1..126)
    if (code > 126)
    {
        code = 126;
    }

    return code;
}
