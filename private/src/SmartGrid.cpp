#include "SmartGrid.hpp"

namespace SmartGrid
{

Color Color::Off(0);
Color Color::Dim(1);
Color Color::Grey(2);
Color Color::White(3);
Color Color::Red(5);
Color Color::Orange(9);
Color Color::Yellow(13);
Color Color::Green(21);
Color Color::SeaGreen(25);
Color Color::Ocean(33);
Color Color::Blue(45);
Color Color::Fuscia(53);
Color Color::Indigo(80);
Color Color::Purple(81);
Color Color::DimPurple(82);

ColorScheme ColorScheme::Whites(std::vector<Color>({Color::Dim, Color::Grey, Color::White}));
ColorScheme ColorScheme::Rainbow(std::vector<Color>({
            Color::Red,
            Color::Orange,
            Color::Yellow,
            Color::Green,
            Color::Blue,
            Color::Indigo,
            Color::Purple}));
ColorScheme ColorScheme::Reds(std::vector<Color>({4, 94, 57}));
ColorScheme ColorScheme::Greens(std::vector<Color>({24, 123, 65}));
ColorScheme ColorScheme::Blues(std::vector<Color>({44, 78, 40}));
ColorScheme ColorScheme::RedHues = ColorScheme::Hues(Color::Red);
ColorScheme ColorScheme::GreenHues = ColorScheme::Hues(Color::Green);
ColorScheme ColorScheme::BlueHues = ColorScheme::Hues(Color::Blue);

GridIdAllocator g_gridIds;
SmartBusHolder g_smartBus;

}
