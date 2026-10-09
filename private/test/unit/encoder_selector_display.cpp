#include "doctest.h"

#include "../support/SynthRig.hpp"
#include "TheNonagonSquiggleBoyWrldBldr.hpp"

#include <array>
#include <memory>

namespace
{

using Encoders = SmartGridOneEncoders;
using Bank = Encoders::Bank;

struct SelectorCase
{
    int m_x;
    Bank m_bank;
};

struct VoiceSelectorCase
{
    int m_x;
    std::array<Bank, 3> m_banks;
};

constexpr std::array<VoiceSelectorCase, 4> x_voiceSelectors =
{{
    {0, {{Bank::SourceWater, Bank::SourceFire, Bank::SourceEarth}}},
    {1, {{Bank::FilterAndAmpWater, Bank::FilterAndAmpFire, Bank::FilterAndAmpEarth}}},
    {2, {{Bank::PanningAndSequencingWater, Bank::PanningAndSequencingFire, Bank::PanningAndSequencingEarth}}},
    {3, {{Bank::VoiceLFOsWater, Bank::VoiceLFOsFire, Bank::VoiceLFOsEarth}}}
}};

constexpr std::array<SelectorCase, 4> x_quadSelectors =
{{
    {0, Bank::Delay},
    {1, Bank::Reverb},
    {2, Bank::PartialMachine},
    {3, Bank::QuadLFOs}
}};

constexpr std::array<SelectorCase, 4> x_globalSelectors =
{{
    {4, Bank::TheoryOfTime},
    {5, Bank::Mastering},
    {6, Bank::Inputs},
    {7, Bank::DeepVocoder}
}};

void CheckSelectors(SmartGrid::Grid& grid, int voiceRow, int sharedRow,
    TheNonagonSquiggleBoyInternal& internal)
{
    for (size_t trio = 0; trio < 3; ++trio)
    {
        internal.SetActiveTrio(static_cast<TheNonagonSmartGrid::Trio>(trio));
        for (const auto& selector : x_voiceSelectors)
        {
            auto* cell = grid.Get(selector.m_x, voiceRow);
            DOCTEST_REQUIRE(cell != nullptr);
            cell->OnPress(100);
            DOCTEST_CHECK(internal.m_squiggleBoy.m_encoders.m_selectedBank
                == selector.m_banks[trio]);
        }
    }

    DOCTEST_CHECK(grid.Get(4, voiceRow) == nullptr);
    for (const auto& selector : x_quadSelectors)
    {
        auto* cell = grid.Get(selector.m_x, sharedRow);
        DOCTEST_REQUIRE(cell != nullptr);
        cell->OnPress(100);
        DOCTEST_CHECK(internal.m_squiggleBoy.m_encoders.m_selectedBank == selector.m_bank);
    }

    for (const auto& selector : x_globalSelectors)
    {
        auto* cell = grid.Get(selector.m_x, sharedRow);
        DOCTEST_REQUIRE(cell != nullptr);
        cell->OnPress(100);
        DOCTEST_CHECK(internal.m_squiggleBoy.m_encoders.m_selectedBank == selector.m_bank);
    }
}

}

DOCTEST_TEST_CASE("encoder selectors: Quad Launchpad exposes four families and shared banks")
{
    synthrig::SynthRig rig;
    CheckSelectors(*rig.Quad().m_bottomRightGrid, 9, 8, rig.Internal());
}

DOCTEST_TEST_CASE("encoder selectors: WorldBuilder exposes four families and shared banks")
{
    synthrig::SynthRig rig;
    TheNonagonSquiggleBoyWrldBldr world(&rig.Internal());
    CheckSelectors(world.m_auxGrid, 3, 2, rig.Internal());
}

DOCTEST_TEST_CASE("encoder display: every selected trio bank maps to its family display")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& ui = rig.Internal().m_uiState.m_squiggleBoyUIState;
    using Display = SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode;
    const std::array<std::pair<Bank, Display>, 4> cases =
    {{
        {Bank::Source, Display::Voice},
        {Bank::FilterAndAmp, Display::Filter},
        {Bank::PanningAndSequencing, Display::PanAndMelody},
        {Bank::VoiceLFOs, Display::Control}
    }};

    for (size_t trio = 0; trio < 3; ++trio)
    {
        synth.SetTrack(trio);
        for (const auto& item : cases)
        {
            synth.SelectEncoderBank(item.first);
            synth.PopulateUIState(&ui);
            DOCTEST_CHECK(ui.m_visualDisplayMode.load() == item.second);
        }
    }
}

DOCTEST_TEST_CASE("encoder display: shared banks keep their display modes")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& ui = rig.Internal().m_uiState.m_squiggleBoyUIState;
    using Display = SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode;
    const std::array<std::pair<Bank, Display>, 8> cases =
    {{
        {Bank::Delay, Display::Delay},
        {Bank::Reverb, Display::Reverb},
        {Bank::PartialMachine, Display::QuadMaster},
        {Bank::QuadLFOs, Display::Control},
        {Bank::TheoryOfTime, Display::QuadMaster},
        {Bank::Mastering, Display::StereoMaster},
        {Bank::Inputs, Display::StereoMaster},
        {Bank::DeepVocoder, Display::StereoMaster}
    }};

    for (const auto& item : cases)
    {
        synth.SelectEncoderBank(item.first);
        synth.PopulateUIState(&ui);
        DOCTEST_CHECK(ui.m_visualDisplayMode.load() == item.second);
    }
}
