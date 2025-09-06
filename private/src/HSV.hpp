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

    return static_cast<uint8_t>(code);
}

// Map MIDI Fighter Twister 7-bit hue code back to RGB (0..255).
// Inverse of RGB2MFTHue - converts hue code back to RGB values.
//
static inline void MFTHue2RGB(uint8_t hue, uint8_t* r8, uint8_t* g8, uint8_t* b8)
{
    // Convert from doubled code back to 1..126 range
    //
    int code = hue;
    if (code < 1)   code = 1;
    if (code > 126) code = 126;

    float h0_deg = 240.0f;
    const float step = 360.0f / 126.0f;

    // Convert code back to hue degrees
    // Code increases as hue decreases, so we reverse the calculation
    //
    float t = (code - 1) * step;
    float hue_deg = std::fmod(h0_deg - t + 360.0f, 360.0f);

    // Convert HSV to RGB
    // For simplicity, we'll use S=1.0, V=1.0 (full saturation and value)
    //
    float h = hue_deg / 60.0f;
    float c = 1.0f; // Saturation = 1.0
    float x = c * (1.0f - std::abs(std::fmod(h, 2.0f) - 1.0f));
    float m = 0.0f; // Value - Saturation = 1.0 - 1.0 = 0.0

    float r, g, b;
    if (h < 1.0f)
    {
        r = c; g = x; b = 0.0f;
    }
    else if (h < 2.0f)
    {
        r = x; g = c; b = 0.0f;
    }
    else if (h < 3.0f)
    {
        r = 0.0f; g = c; b = x;
    }
    else if (h < 4.0f)
    {
        r = 0.0f; g = x; b = c;
    }
    else if (h < 5.0f)
    {
        r = x; g = 0.0f; b = c;
    }
    else
    {
        r = c; g = 0.0f; b = x;
    }

    // Convert back to 0-255 range
    //
    *r8 = static_cast<uint8_t>(std::lround((r + m) * 255.0f));
    *g8 = static_cast<uint8_t>(std::lround((g + m) * 255.0f));
    *b8 = static_cast<uint8_t>(std::lround((b + m) * 255.0f));
}
