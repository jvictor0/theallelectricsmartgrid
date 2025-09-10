#pragma once

#include <cstdint>
#include "HSV.hpp"
#include <vector>

namespace SmartGrid
{
    struct Color
    {
        constexpr Color()
            : m_red(0)
            , m_green(0)
            , m_blue(0)
            , m_unused(0)
        {
        }
        
        constexpr Color(uint8_t r, uint8_t g, uint8_t b)
            : m_red(r)
            , m_green(g)
            , m_blue(b)
            , m_unused(0)
        {
        }

        constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t u)
            : m_red(r)
            , m_green(g)
            , m_blue(b)
            , m_unused(u)
        {
        }

        uint32_t To32Bit() const
        {
            return (m_red << 24) | (m_green << 16) | (m_blue << 8) | m_unused;
        }

        bool operator == (const Color& c) const
        {
            return To32Bit() == c.To32Bit();
        }

        bool operator != (const Color& c) const
        {
            return !(*this == c);
        }

        size_t ZEncode()
        {
            size_t result = 0;
            for (size_t i = 0; i < 8; ++i)
            {
                result |= static_cast<size_t>((m_red >> i) & 1) << (3 * i);
                result |= static_cast<size_t>((m_green >> i) & 1) << (3 * i + 1);
                result |= static_cast<size_t>((m_blue >> i) & 1) << (3 * i + 2);
            }
            
            return result;        
        }

        static Color ZDecode(size_t x)
        {
            Color result;
            for (size_t i = 0; i < 8; ++i)
            {
                result.m_red |= ((x >> (3 * i)) & 1) << i;
                result.m_green |= ((x >> (3 * i + 1)) & 1) << i;
                result.m_blue |= ((x >> (3 * i + 2)) & 1) << i;
            }

            return result;
        }

        float ZEncodeFloat()
        {
            return ZToFloat(ZEncode());
        }

        static Color ZDecodeFloat(float x)
        {
            return ZDecode(FloatToZ(x));
        }

        static size_t FloatToZ(float x)
        {
            return std::min<size_t>((1 << 24) - 1, static_cast<size_t>(x * (1 << 24)));
        }

        static float ZToFloat(size_t z)
        {
            return static_cast<float>(z) / static_cast<float>(1 << 24);
        }

        constexpr Color AdjustBrightness(float x) const
        {
            return Color(std::max<int>(0, std::min<int>(x * m_red, 255)),
                        std::max<int>(0, std::min<int>(x * m_green, 255)),
                        std::max<int>(0, std::min<int>(x * m_blue, 255)));
        }

        constexpr Color Dim() const
        {
            return AdjustBrightness(1.0 / 8.0);
        }

        Color Saturate() const
        {
            uint8_t maxPrime = std::max(m_red, std::max(m_green, m_blue));
            Color c = *this;
            c.m_red *= 255 / maxPrime;
            c.m_green *= 255 / maxPrime;
            c.m_blue *= 255 / maxPrime;
            return c;
        }

        Color Interpolate(Color other, float position) const
        {
            if (position <= 0)
            {
                return *this;
            }
            else if (position >= 1)
            {
                return other;
            }
            else
            {
                return Color(
                    static_cast<float>(m_red) + (static_cast<float>(other.m_red) - static_cast<float>(m_red)) * position,
                    static_cast<float>(m_green) + (static_cast<float>(other.m_green) - static_cast<float>(m_green)) * position,
                    static_cast<float>(m_blue) + (static_cast<float>(other.m_blue) - static_cast<float>(m_blue)) * position);
            }
        }

        uint8_t& operator[] (size_t i)
        {
            switch (i)
            {
                case 0: return m_red;
                case 1: return m_green;
                case 2: return m_blue;
                default: return m_unused;
            }
        }

        uint8_t ToTwister()
        {
            return RGB2MFTHue(m_red, m_green, m_blue);
        }

        static Color FromTwister(uint8_t twister)
        {
            Color c;
            MFTHue2RGB(twister, &c.m_red, &c.m_green, &c.m_blue);
            return c;
        }

        Color VeryDifferent() const
        {
            if (m_red < m_green)
            {
                if (m_green < m_blue)
                {
                    // red < green < blue
                    //
                    return Color(m_blue, m_green, m_red);
                }
                else if (m_red < m_blue)
                {
                    // red < blue < green
                    //
                    return Color(m_green, m_red, m_blue);
                }
                else
                {
                    // blue < red < green
                    //
                    return Color(m_red, m_blue, m_green);
                }
            }
            else if (m_green < m_blue)
            {
                // green < red < blue
                //
                return Color(m_red, m_blue, m_green);
            }
            else if (m_green < m_blue)
            {
                // blue < green < red
                //
                return Color(m_blue, m_green, m_red);
            }
            else
            {
                // green < blue < red
                //
                return Color(m_green, m_red, m_blue);
            }
        }

        Color Similar() const
        {
            return Interpolate(VeryDifferent(), 0.25);
        }

        std::string ToString() const
        {
            return "(" + std::to_string(m_red) + ", " + std::to_string(m_green) + ", " + std::to_string(m_blue) + ")";
        }

        uint8_t m_red;
        uint8_t m_green;
        uint8_t m_blue;
        uint8_t m_unused;

        static const Color InvalidColor;
        static const Color Off;
        static const Color Grey;
        static const Color White;
        static const Color Red;
        static const Color Orange;
        static const Color Yellow;
        static const Color Green;
        static const Color SeaGreen;
        static const Color Ocean;
        static const Color Blue;
        static const Color Fuscia;
        static const Color Indigo;
        static const Color Purple;
        static const Color Pink;
    };

    struct ColorScheme
    {
        Color back()
        {
            return m_colors.back();
        }

        Color operator[] (size_t ix)
        {
            return m_colors[ix];
        }

        size_t size()
        {
            return m_colors.size();
        }

        ColorScheme(const std::vector<Color>& colors)
            : m_colors(colors)
        {
        }

        ColorScheme()
        {
        }

        static ColorScheme Hues(Color c)
        {
            c = c.Saturate();

            std::vector<Color> result;
            while (c.m_red > 48 || c.m_green > 48 || c.m_blue > 48)
            {
                result.push_back(c);
                c.m_red /= 2;
                c.m_green /= 2;
                c.m_blue /= 2;
            }

            std::reverse(result.begin(), result.end());
            return ColorScheme(result);
        }
        
        std::vector<Color> m_colors;

        static ColorScheme Whites;
        static ColorScheme Rainbow;
        static ColorScheme Reds;
        static ColorScheme Greens;
        static ColorScheme Blues;
        static ColorScheme RedHues;
        static ColorScheme OrangeHues;
        static ColorScheme YellowHues;
        static ColorScheme GreenHues;
        static ColorScheme BlueHues;
    };

    // Color inline const definitions
    inline const Color Color::InvalidColor{0, 0, 0, 1};
    inline const Color Color::Off{0, 0, 0};
    inline const Color Color::Grey{143, 143, 143};
    inline const Color Color::White{253, 253, 253};
    inline const Color Color::Red{255, 30, 18};
    inline const Color Color::Orange{255, 108, 29};
    inline const Color Color::Yellow{255, 248, 63};
    inline const Color Color::Green{9, 255, 29};
    inline const Color Color::SeaGreen{9, 246, 59};
    inline const Color Color::Ocean{0, 247, 167};
    inline const Color Color::Blue{0, 60, 249};
    inline const Color Color::Fuscia{255, 71, 250};
    inline const Color Color::Indigo{56, 61, 249};
    inline const Color Color::Purple{134, 63, 249};
    inline const Color Color::Pink{255, 50, 120};

    // ColorScheme inline definitions (not constexpr due to std::vector)
    inline ColorScheme ColorScheme::Whites{std::vector<Color>({Color::White.Dim(), Color::Grey, Color::White})};
    inline ColorScheme ColorScheme::Rainbow{std::vector<Color>({
            Color::Red,
            Color::Orange,
            Color::Yellow,
            Color::Green,
            Color::Blue,
            Color::Indigo,
            Color::Purple})};
    inline ColorScheme ColorScheme::Reds{std::vector<Color>({Color(255,101,92), Color(218,70,250), Color(255,44,101)})};
    inline ColorScheme ColorScheme::Greens{std::vector<Color>({Color(67,247,104), Color(1,79,12), Color(0,101,67)})};
    inline ColorScheme ColorScheme::Blues{std::vector<Color>({Color(79,105,250), Color(0,184,252), Color(73,158,251)})};
    inline ColorScheme ColorScheme::RedHues{std::vector<Color>({
        Color::Red.Saturate().AdjustBrightness(1.0f/128.0f),
        Color::Red.Saturate().AdjustBrightness(1.0f/64.0f),
        Color::Red.Saturate().AdjustBrightness(1.0f/32.0f),
        Color::Red.Saturate().AdjustBrightness(1.0f/16.0f),
        Color::Red.Saturate().AdjustBrightness(1.0f/8.0f),
        Color::Red.Saturate().AdjustBrightness(1.0f/4.0f),
        Color::Red.Saturate().AdjustBrightness(1.0f/2.0f),
        Color::Red.Saturate().AdjustBrightness(1.0f)
    })};
    inline ColorScheme ColorScheme::OrangeHues{std::vector<Color>({
        Color::Orange.Saturate().AdjustBrightness(1.0f/128.0f),
        Color::Orange.Saturate().AdjustBrightness(1.0f/64.0f),
        Color::Orange.Saturate().AdjustBrightness(1.0f/32.0f),
        Color::Orange.Saturate().AdjustBrightness(1.0f/16.0f),
        Color::Orange.Saturate().AdjustBrightness(1.0f/8.0f),
        Color::Orange.Saturate().AdjustBrightness(1.0f/4.0f),
        Color::Orange.Saturate().AdjustBrightness(1.0f/2.0f),
        Color::Orange.Saturate().AdjustBrightness(1.0f)
    })};
    inline ColorScheme ColorScheme::YellowHues{std::vector<Color>({
        Color::Yellow.Saturate().AdjustBrightness(1.0f/128.0f),
        Color::Yellow.Saturate().AdjustBrightness(1.0f/64.0f),
        Color::Yellow.Saturate().AdjustBrightness(1.0f/32.0f),
        Color::Yellow.Saturate().AdjustBrightness(1.0f/16.0f),
        Color::Yellow.Saturate().AdjustBrightness(1.0f/8.0f),
        Color::Yellow.Saturate().AdjustBrightness(1.0f/4.0f),
        Color::Yellow.Saturate().AdjustBrightness(1.0f/2.0f),
        Color::Yellow.Saturate().AdjustBrightness(1.0f)
    })};
    inline ColorScheme ColorScheme::GreenHues{std::vector<Color>({
        Color::Green.Saturate().AdjustBrightness(1.0f/128.0f),
        Color::Green.Saturate().AdjustBrightness(1.0f/64.0f),
        Color::Green.Saturate().AdjustBrightness(1.0f/32.0f),
        Color::Green.Saturate().AdjustBrightness(1.0f/16.0f),
        Color::Green.Saturate().AdjustBrightness(1.0f/8.0f),
        Color::Green.Saturate().AdjustBrightness(1.0f/4.0f),
        Color::Green.Saturate().AdjustBrightness(1.0f/2.0f),
        Color::Green.Saturate().AdjustBrightness(1.0f)
    })};
    inline ColorScheme ColorScheme::BlueHues{std::vector<Color>({
        Color::Blue.Saturate().AdjustBrightness(1.0f/128.0f),
        Color::Blue.Saturate().AdjustBrightness(1.0f/64.0f),
        Color::Blue.Saturate().AdjustBrightness(1.0f/32.0f),
        Color::Blue.Saturate().AdjustBrightness(1.0f/16.0f),
        Color::Blue.Saturate().AdjustBrightness(1.0f/8.0f),
        Color::Blue.Saturate().AdjustBrightness(1.0f/4.0f),
        Color::Blue.Saturate().AdjustBrightness(1.0f/2.0f),
        Color::Blue.Saturate().AdjustBrightness(1.0f)
    })};
}