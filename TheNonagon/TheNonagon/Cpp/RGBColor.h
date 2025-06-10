#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct RGBColor 
{
    uint8_t m_red;
    uint8_t m_green;
    uint8_t m_blue;

#ifdef __cplusplus
    RGBColor();
    RGBColor(uint8_t red, uint8_t green, uint8_t blue);
#endif
};

#ifdef __cplusplus
}

// C++ only additions
#ifdef __cplusplus
inline RGBColor::RGBColor() 
    : m_red(0)
    , m_green(0)
    , m_blue(0)
{
}

inline RGBColor::RGBColor(uint8_t red, uint8_t green, uint8_t blue) 
    : m_red(red)
    , m_green(green)
    , m_blue(blue)
{
}
#endif

#endif 