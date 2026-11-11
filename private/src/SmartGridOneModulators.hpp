#pragma once

#include "Color.hpp"
#include "SmartGridOneModulatorColors.hpp"
#include "SmartGridOneLayout.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "VoiceMachineEnums.hpp"
#include <array>
#include <algorithm>

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
    Sub = 12,
    Voice = 13,
    Noise = 14,
    Random5 = 15,
    Random6 = 16,
    Random7 = 17,
    Random8 = 18,
    Random9 = 19,
    Random10 = 20,
    SourceMod1 = 21,
    SourceMod2 = 22
};

enum class QuadModulator : size_t
{
    Random1 = 0,
    Random2 = 1,
    Random3 = 2,
    Random4 = 3,
    LFO1 = 6,
    LFO2 = 7,
    Spread = 11,
    Noise = 14,
    Random5 = 15,
    Random6 = 16,
    Random7 = 17,
    Random8 = 18,
#define F(tap, envelope, audio) envelope, audio,
#include "ForEachMixerModulationTap.hpp"
#undef F
};

enum class GlobalModulator : size_t
{
    Random1 = 2,
    Random2 = 3,
    Noise = 14
};

// Voice indices span all tracks (0..8); quad channels are 0..3.
//
struct ModulatorRef
{
    SmartGridOneLayout::ParamMode m_mode;
    size_t m_index;
    size_t m_voice;

    ModulatorRef(VoiceModulator modulator, size_t voice)
        : m_mode(SmartGridOneLayout::ParamMode::Voice)
        , m_index(static_cast<size_t>(modulator))
        , m_voice(voice)
    {
    }

    ModulatorRef(QuadModulator modulator, size_t channel)
        : m_mode(SmartGridOneLayout::ParamMode::Quad)
        , m_index(static_cast<size_t>(modulator))
        , m_voice(channel)
    {
    }

    explicit ModulatorRef(GlobalModulator modulator)
        : m_mode(SmartGridOneLayout::ParamMode::Global)
        , m_index(static_cast<size_t>(modulator))
        , m_voice(0)
    {
    }
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

static constexpr size_t x_numSourceIndices = std::max(
    static_cast<size_t>(VoiceModulator::SourceMod2), static_cast<size_t>(QuadModulator::OutputAudio)) + 1;
static constexpr size_t x_numVoiceRandoms = 10;
static constexpr size_t x_numQuadRandoms = 8;
static constexpr size_t x_noRandom = static_cast<size_t>(-1);

constexpr VoiceModulator GetVoiceRandom(size_t index)
{
    assert(index < x_numVoiceRandoms);
    return static_cast<VoiceModulator>(index < 4 ? index : index + 11);
}

constexpr QuadModulator GetQuadRandom(size_t index)
{
    assert(index < x_numQuadRandoms);
    return static_cast<QuadModulator>(index < 4 ? index : index + 11);
}

inline size_t GetRandomIndex(ModulatorRef source)
{
    if (source.m_mode == Mode::Global)
    {
        return source.m_index >= 2 && source.m_index <= 3 ? source.m_index - 2 : x_noRandom;
    }

    size_t count = source.m_mode == Mode::Voice ? x_numVoiceRandoms : x_numQuadRandoms;
    return source.m_index < 4 ? source.m_index
        : source.m_index >= 15 && source.m_index < count + 11 ? source.m_index - 11 : x_noRandom;
}

constexpr SmartGridOne::QuadScopes GetMixerScope(QuadModulator modulator)
{
    switch (modulator)
    {
#define F(tap, envelope, audio) \
        case QuadModulator::envelope: return SmartGridOne::QuadScopes::envelope; \
        case QuadModulator::audio: return SmartGridOne::QuadScopes::audio;
#include "ForEachMixerModulationTap.hpp"
#undef F
        default: return SmartGridOne::QuadScopes::NumScopes;
    }
}

constexpr bool IsMixerAudio(QuadModulator modulator)
{
    switch (modulator)
    {
#define F(tap, envelope, audio) case QuadModulator::audio: return true;
#include "ForEachMixerModulationTap.hpp"
#undef F
        default: return false;
    }
}

constexpr auto BuildSkins()
{
    std::array<std::array<Skin, x_numSourceIndices>, 3> skins{};
    bool occupied[3][x_numSourceIndices]{};
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

// Global source identity is independent of its positions in modulation groups.
//
enum class Source : size_t
{
#define F(name, mode, type, value, downsample) name,
#include "ForEachSmartGridOneModulatorSource.hpp"
#undef F
    NumSources
};

enum class Group : size_t
{
#define F(name, lanes) name,
#include "ForEachSmartGridOneModulatorGroup.hpp"
#undef F
    NumGroups
};

static constexpr size_t x_numSources = static_cast<size_t>(Source::NumSources);
static constexpr size_t x_numGroups = static_cast<size_t>(Group::NumGroups);
static constexpr size_t x_numSlots = 15;
inline constexpr std::array<size_t, x_numGroups> x_groupLanes =
{{
#define F(name, lanes) lanes,
#include "ForEachSmartGridOneModulatorGroup.hpp"
#undef F
}};

using BankMode = SmartGridOne::EncoderBankMode;

struct SourceDefinition
{
    BankMode m_mode;
    size_t m_index;
    Skin m_skin;
    bool m_requiresDownsampling;
};

inline constexpr std::array<SourceDefinition, x_numSources> x_sources =
{{
#define F(name, mode, type, value, downsample) \
    {BankMode::mode, static_cast<size_t>(type::value), \
        x_skins[static_cast<size_t>(GetMode(type::value))][static_cast<size_t>(type::value)], downsample},
#include "ForEachSmartGridOneModulatorSource.hpp"
#undef F
}};

constexpr auto BuildSourceIds()
{
    std::array<std::array<Source, x_numSourceIndices>, static_cast<size_t>(BankMode::NumModes)> ids{};
    for (auto& mode : ids)
    {
        for (auto& source : mode)
        {
            source = Source::NumSources;
        }
    }

    for (size_t i = 0; i < x_sources.size(); ++i)
    {
        const auto& source = x_sources[i];
        auto& id = ids[static_cast<size_t>(source.m_mode)][source.m_index];
        if (id != Source::NumSources)
        {
            throw "Duplicate global source identity";
        }

        id = static_cast<Source>(i);
    }

    return ids;
}

inline constexpr auto x_sourceIds = BuildSourceIds();

constexpr Source GetSource(VoiceModulator source, size_t trio)
{
    assert(trio < SmartGridOneLayout::x_numTrios);
    return x_sourceIds[trio][static_cast<size_t>(source)];
}

constexpr Source GetSource(QuadModulator source)
{
    return x_sourceIds[static_cast<size_t>(BankMode::Quad)][static_cast<size_t>(source)];
}

constexpr Source GetSource(GlobalModulator source)
{
    return x_sourceIds[static_cast<size_t>(BankMode::Global)][static_cast<size_t>(source)];
}

constexpr auto BuildGroups()
{
    std::array<std::array<Source, x_numSlots>, x_numGroups> groups{};
    for (auto& group : groups)
    {
        for (auto& source : group)
        {
            source = Source::NumSources;
        }
    }

    auto add = [&](Group group, size_t slot, Source source)
    {
        if (slot >= x_numSlots || groups[static_cast<size_t>(group)][slot] != Source::NumSources)
        {
            throw "Invalid or duplicate group slot";
        }

        groups[static_cast<size_t>(group)][slot] = source;
    };

#define F(group, slot, source) add(Group::group, static_cast<size_t>(slot), Source::source);
#include "ForEachSmartGridOneModulatorGroupSlot.hpp"
#undef F
    return groups;
}

inline constexpr auto x_groups = BuildGroups();

constexpr auto BuildParamGroups()
{
    std::array<std::array<Group, SmartGridOneLayout::x_numTrios>, SmartGridOneLayout::x_numParams> groups{};
    std::array<bool, SmartGridOneLayout::x_numParams> assigned{};
    auto add = [&](SmartGridOneLayout::Param param, Group water, Group fire, Group earth)
    {
        size_t index = static_cast<size_t>(param);
        if (assigned[index])
        {
            throw "Duplicate parameter group assignment";
        }

        assigned[index] = true;
        groups[index] = {water, fire, earth};
    };

#define F(param, water, fire, earth) add(SmartGridOneLayout::Param::param, Group::water, Group::fire, Group::earth);
#include "ForEachSmartGridOneParamModulatorGroup.hpp"
#undef F
    for (bool present : assigned)
    {
        if (!present)
        {
            throw "Missing parameter group assignment";
        }
    }

    return groups;
}

inline constexpr auto x_paramGroups = BuildParamGroups();

constexpr Group GetGroup(SmartGridOneLayout::Param param, size_t trio)
{
    return x_paramGroups[static_cast<size_t>(param)][trio];
}

constexpr Skin GetSkin(Group group, size_t slot)
{
    if (static_cast<size_t>(group) >= x_numGroups || slot >= x_numSlots)
    {
        return {};
    }

    Source source = x_groups[static_cast<size_t>(group)][slot];
    return source == Source::NumSources ? Skin{} : x_sources[static_cast<size_t>(source)].m_skin;
}

using SourceMachines = std::array<VoiceMachine::SourceMachine, SmartGridOneLayout::x_numTrios>;

constexpr Skin GetSkin(Group group, size_t slot, const SourceMachines& machines)
{
    Skin skin = GetSkin(group, slot);
    if (static_cast<size_t>(group) >= x_numGroups || slot >= x_numSlots)
    {
        return skin;
    }

    Source source = x_groups[static_cast<size_t>(group)][slot];
    if (source == Source::NumSources)
    {
        return skin;
    }

    const auto& definition = x_sources[static_cast<size_t>(source)];
    if (definition.m_mode != BankMode::VoiceWater && definition.m_mode != BankMode::VoiceFire
        && definition.m_mode != BankMode::VoiceEarth)
    {
        return skin;
    }

    if (definition.m_index != static_cast<size_t>(VoiceModulator::SourceMod1)
        && definition.m_index != static_cast<size_t>(VoiceModulator::SourceMod2))
    {
        return skin;
    }

    using Glyph = SmartGridOne::ModulationGlyphs;
    const size_t trio = static_cast<size_t>(definition.m_mode) - static_cast<size_t>(BankMode::VoiceWater);
    switch (machines[trio])
    {
        case VoiceMachine::SourceMachine::DualWaveShapingVCO:
            skin.m_glyph = Glyph::HarmonicWave;
            break;
        case VoiceMachine::SourceMachine::PhysicalModeling:
            skin.m_glyph = definition.m_index == static_cast<size_t>(VoiceModulator::SourceMod1)
                ? Glyph::Noise : Glyph::ADSR;
            break;
        default:
            skin.m_glyph = Glyph::None;
            break;
    }

    return skin;
}

inline ModulatorRef GetRef(Source source, size_t lane)
{
    assert(static_cast<size_t>(source) < x_numSources);
    assert(lane < 4);
    const auto& definition = x_sources[static_cast<size_t>(source)];
    if (definition.m_mode == BankMode::Quad)
    {
        return ModulatorRef(static_cast<QuadModulator>(definition.m_index), lane);
    }

    if (definition.m_mode == BankMode::Global)
    {
        ModulatorRef ref(static_cast<GlobalModulator>(definition.m_index));
        ref.m_voice = lane;
        return ref;
    }

    return ModulatorRef(static_cast<VoiceModulator>(definition.m_index),
        lane < 3 ? static_cast<size_t>(definition.m_mode) * 3 + lane : 9);
}

}

template<typename Modulator>
constexpr SmartGrid::Color GetModulatorColor(Modulator modulator)
{
    return SmartGridOneModulators::x_skins[static_cast<size_t>(SmartGridOneModulators::GetMode(modulator))]
        [static_cast<size_t>(modulator)].m_color;
}
