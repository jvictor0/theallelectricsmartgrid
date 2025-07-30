#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>

// Map RGB (0..255) to MIDI Fighter Twister 7-bit hue code (1..126).
// Code increases as HSV hue decreases (blue -> cyan -> green -> yellow -> red -> magenta -> blue).
// h0_deg lets you rotate the mapping; 240° ≈ blue. Use ~233° if you want to match your picture exactly.
//
static inline uint8_t RGB2MFTHue(uint8_t r8, uint8_t g8, uint8_t b8)
{
    float h0_deg = 240.0f;
    float r = r8 / 255.0f;
    float g = g8 / 255.0f;
    float b = b8 / 255.0f;

    float cmax = std::max({r, g, b});
    float cmin = std::min({r, g, b});
    float delta = cmax - cmin;

    // HSV hue in degrees [0,360). If delta == 0 (gray), hue is undefined; we leave it at 0.
    //
    float hue_deg = 0.0f;
    if (delta > 0.0f)
    {
        if (cmax == r)
        {
            hue_deg = 60.0f * std::fmod((g - b) / delta, 6.0f);
        }
        else if (cmax == g)
        {
            hue_deg = 60.0f * (((b - r) / delta) + 2.0f);
        }
        else // cmax == b
        {
            hue_deg = 60.0f * (((r - g) / delta) + 4.0f);
        }
        if (hue_deg < 0.0f)
        {
            hue_deg += 360.0f;
        }
    }

    // One LED hue step (degrees per code)
    //
    const float step = 360.0f / 126.0f;

    // Code increases as hue decreases (clockwise), starting near blue.
    //
    float t = std::fmod(h0_deg - hue_deg + 360.0f, 360.0f);
    int code = 1 + static_cast<int>(std::lround(t / step));

    if (code < 1)   code = 1;
    if (code > 126) code = 126;

    return static_cast<uint8_t>(2 * code);
}
