// Suppress integer precision warnings from private source files
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshorten-64-to-32"

#include "SmartGrid.hpp"

#pragma GCC diagnostic pop

namespace SmartGrid
{

Color Color::InvalidColor(0, 0, 0, 1);
Color Color::Off(0, 0, 0);
Color Color::Grey(143,143,143);
Color Color::White(253,253,253);
Color Color::Red(255,30,18);
Color Color::Orange(255,108,29);
Color Color::Yellow(255,248,63);
Color Color::Green(9,255,29);
Color Color::SeaGreen(9,246,59);
Color Color::Ocean(0,247,167);
Color Color::Blue(0,60,249);
Color Color::Fuscia(255,71,250);
Color Color::Indigo(56,61,249);
Color Color::Purple(134,63,249);

ColorScheme ColorScheme::Whites(std::vector<Color>({Color::White.Dim(), Color::Grey, Color::White}));
ColorScheme ColorScheme::Rainbow(std::vector<Color>({
            Color::Red,
            Color::Orange,
            Color::Yellow,
            Color::Green,
            Color::Blue,
            Color::Indigo,
            Color::Purple}));
ColorScheme ColorScheme::Reds(std::vector<Color>({Color(255,101,92), Color(218,70,250), Color(255,44,101)}));
ColorScheme ColorScheme::Greens(std::vector<Color>({Color(67,247,104), Color(1,79,12), Color(0,101,67)}));
ColorScheme ColorScheme::Blues(std::vector<Color>({Color(79,105,250), Color(0,184,252), Color(73,158,251)}));
ColorScheme ColorScheme::RedHues = ColorScheme::Hues(Color::Red);
ColorScheme ColorScheme::OrangeHues = ColorScheme::Hues(Color::Orange);
ColorScheme ColorScheme::YellowHues = ColorScheme::Hues(Color::Yellow);
ColorScheme ColorScheme::GreenHues = ColorScheme::Hues(Color::Green);
ColorScheme ColorScheme::BlueHues = ColorScheme::Hues(Color::Blue);

GridIdAllocator g_gridIds;
SmartBusHolder g_smartBus;

}
