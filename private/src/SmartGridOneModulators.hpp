#pragma once

#include "Color.hpp"
#include "SmartGridOneLayout.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include <array>

enum class VoiceModulator : size_t
{
    Random1 = 0,
    Random2 = 1,
    Random3 = 2,
    Random4 = 3,
    ModulationEnvelope = 4,
    AmpEnvelope = 5,
    LFO1 = 6,
    LFO2 = 7,
    Sheaf1 = 8,
    Sheaf2 = 9,
    Sheaf3 = 10,
    Spread = 11,
    Noise = 14
};

enum class QuadModulator : size_t
{
    Random1 = 0,
    Random2 = 1,
    Random3 = 2,
    Random4 = 3,
    DelayLFO = 4,
    ReverbLFO = 5,
    LFO1 = 6,
    LFO2 = 7,
    Spread = 11,
    Noise = 14
};

enum class GlobalModulator : size_t
{
    Random1 = 2,
    Random2 = 3,
    Noise = 14
};

namespace SmartGridOneModulators
{
using Mode = SmartGridOneLayout::ParamMode;

struct Skin
{
    SmartGridOne::ModulationGlyphs m_glyph = SmartGridOne::ModulationGlyphs::None;
    SmartGrid::Color m_color;
    const char* m_shortName = nullptr;
};

constexpr Mode GetMode(VoiceModulator)
{
    return Mode::Voice;
}

constexpr Mode GetMode(QuadModulator)
{
    return Mode::Quad;
}

constexpr Mode GetMode(GlobalModulator)
{
    return Mode::Global;
}

constexpr auto BuildSkins()
{
    std::array<std::array<Skin, 15>, 3> skins{};
    bool occupied[3][15]{};
    auto add = [&](Mode mode, size_t index, SmartGrid::Color color, SmartGridOne::ModulationGlyphs glyph, const char* shortName)
    {
        size_t row = static_cast<size_t>(mode);
        if (row >= skins.size() || index >= skins[0].size() || occupied[row][index])
        {
            throw "Invalid or duplicate modulator catalogue row";
        }

        occupied[row][index] = true;
        skins[row][index] = {glyph, color, shortName};
    };

#define F(enumType, enumValue, mode, color, glyph, shortName) \
    static_assert(GetMode(enumType::enumValue) == Mode::mode, "Modulator enum/mode mismatch"); \
    add(Mode::mode, static_cast<size_t>(enumType::enumValue), color, SmartGridOne::ModulationGlyphs::glyph, #shortName);
#include "ForEachSmartGridOneModulator.hpp"
#undef F
    return skins;
}

inline constexpr auto x_skins = BuildSkins();
}

template<typename Modulator>
constexpr SmartGrid::Color GetModulatorColor(Modulator modulator)
{
    return SmartGridOneModulators::x_skins[static_cast<size_t>(SmartGridOneModulators::GetMode(modulator))]
        [static_cast<size_t>(modulator)].m_color;
}
