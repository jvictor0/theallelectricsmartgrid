#include "doctest.h"

#include "IndexArp.hpp"

#include <array>

namespace
{
IndexArp::Input SparseArpInput()
{
    IndexArp::Input input;
    input.m_clock = true;
    input.m_read = true;
    input.m_rhythmLength = 4;
    for (size_t slot = 0; slot < IndexArp::x_rhythmLength; ++slot)
    {
        input.m_rhythm[slot] = slot == 1 || slot == 3;
    }

    input.m_offset = 0.0625f;
    input.m_interval = 0.125f;
    input.m_pageInterval = 0.25f;
    input.m_min = -1.0f;
    input.m_max = 1.0f;
    return input;
}
}

DOCTEST_TEST_CASE("SequencerUI: enabled arp slots share signed and wide coordinate mapping")
{
    struct Example
    {
        int64_t m_position;
        int64_t m_note;
        int64_t m_motive;
        float m_choice;
    };

    constexpr std::array<Example, 6> x_examples =
    {{
        {1, 0, 0, -0.875f},
        {3, 1, 0, -0.625f},
        {5, 0, 1, -0.375f},
        {7, 1, 1, -0.125f},
        {-1, 1, -1, 0.875f},
        {4294967297LL, 0, 1073741824LL, -0.875f}
    }};

    IndexArp arp;
    auto input = SparseArpInput();
    IndexArp::UIState ui;
    arp.PopulateUIState(&ui, input);
    ui.Snapshot();
    for (const auto& example : x_examples)
    {
        DOCTEST_CAPTURE(example.m_position);
        input.m_clockPosition = example.m_position;
        arp.Process(input);
        DOCTEST_CHECK(arp.m_triggered);
        DOCTEST_CHECK(arp.m_noteIndex == example.m_note);
        DOCTEST_CHECK(arp.m_motivePosition == example.m_motive);
        DOCTEST_CHECK(arp.m_choiceValue == doctest::Approx(example.m_choice));
        DOCTEST_CHECK(ui.GetChoiceValue(example.m_position) == doctest::Approx(example.m_choice));
    }
}

DOCTEST_TEST_CASE("SequencerUI: arp publication preserves the current snapshot until refresh")
{
    IndexArp arp;
    auto input = SparseArpInput();
    IndexArp::UIState ui;
    arp.PopulateUIState(&ui, input);
    ui.Snapshot();
    DOCTEST_CHECK_FALSE(ui.Changed());
    DOCTEST_CHECK(ui.GetChoiceValue(5) == doctest::Approx(-0.375f));

    input.m_rhythmLength = 3;
    input.m_rhythm[1] = false;
    input.m_rhythm[2] = true;
    input.m_rhythm[3] = false;
    input.m_offset = 0.875f;
    input.m_interval = 0.5f;
    input.m_pageInterval = 0.25f;
    input.m_min = -2.0f;
    input.m_max = 2.0f;
    input.m_retro = true;
    input.m_cycle = true;
    input.m_invert = true;
    arp.PopulateUIState(&ui, input);
    DOCTEST_CHECK(ui.Changed());
    DOCTEST_CHECK(ui.GetChoiceValue(5) == doctest::Approx(-0.375f));

    ui.Snapshot();
    DOCTEST_CHECK_FALSE(ui.Changed());
    DOCTEST_CHECK(ui.GetChoiceValue(5) == doctest::Approx(0.5f));
    input.m_clockPosition = 5;
    arp.Process(input);
    DOCTEST_CHECK(arp.m_choiceValue == doctest::Approx(0.5f));
}

DOCTEST_TEST_CASE("SequencerUI: published trio zones retain overlapping ranges and clock routing")
{
    NonagonIndexArp arp;
    NonagonIndexArp::Input input;
    for (size_t voice = 0; voice < NonagonIndexArp::x_numVoices; ++voice)
    {
        input.m_zoneHeight[voice] = 1.0f;
        input.m_zoneOverlap[voice] = 0.5f;
        input.m_offset[voice] = 0.25f;
    }

    input.m_clockSelect[1] = 3;
    input.m_resetSelect[1] = 5;
    input.m_clockSelect[2] = -1;
    input.SetTrioInputs();
    NonagonIndexArp::UIState ui;
    arp.PopulateUIState(&ui, input);
    ui.Snapshot();
    DOCTEST_CHECK_FALSE(ui.Changed());
    for (size_t voice = 0; voice < NonagonIndexArp::x_numVoices; ++voice)
    {
        DOCTEST_CAPTURE(voice);
        constexpr std::array<float, 3> x_choices = {0.25f, 0.75f, 1.25f};
        constexpr std::array<int, 3> x_clocks = {0, 3, -1};
        constexpr std::array<int, 3> x_resets = {-1, 5, -1};
        DOCTEST_CHECK(ui.GetChoiceValue(voice, 0, 0) == doctest::Approx(x_choices[voice % 3]));
        DOCTEST_CHECK(ui.GetClockSelect(voice) == x_clocks[voice / 3]);
        DOCTEST_CHECK(ui.GetResetSelect(voice) == x_resets[voice / 3]);
    }

    input.m_clockSelect[1] = 4;
    input.m_resetSelect[1] = -1;
    arp.PopulateUIState(&ui, input);
    DOCTEST_CHECK(ui.Changed());
    DOCTEST_CHECK(ui.GetClockSelect(4) == 3);
    ui.Snapshot();
    DOCTEST_CHECK_FALSE(ui.Changed());
    DOCTEST_CHECK(ui.GetClockSelect(4) == 4);
    DOCTEST_CHECK(ui.GetResetSelect(4) == -1);
}

DOCTEST_TEST_CASE("SequencerUI: forward arp preview holds the previous motive through a leading rest")
{
    IndexArp arp;
    auto input = SparseArpInput();
    input.m_rhythmLength = 2;
    input.m_rhythm[3] = false;
    input.m_offset = 0.0f;
    input.m_interval = 0.25f;
    input.m_pageInterval = 0.125f;
    input.m_min = 0.0f;
    input.m_max = 1.0f;
    IndexArp::UIState ui;
    arp.PopulateUIState(&ui, input);
    ui.Snapshot();

    input.m_clockPosition = 1;
    arp.Process(input);
    DOCTEST_REQUIRE(arp.m_choiceValue == doctest::Approx(0.0f));
    input.m_clockPosition = 2;
    arp.Process(input);
    DOCTEST_CHECK_FALSE(arp.m_triggered);
    DOCTEST_CHECK(arp.m_choiceValue == doctest::Approx(0.0f));
    DOCTEST_CHECK(ui.GetChoiceValue(2) == doctest::Approx(0.0f));
}

DOCTEST_TEST_CASE("SequencerUI: forward arp rests hold across signed motive boundaries")
{
    IndexArp arp;
    auto input = SparseArpInput();
    input.m_rhythmLength = 5;
    input.m_rhythm[3] = false;
    input.m_rhythm[4] = true;
    input.m_offset = 0.0f;
    input.m_interval = 0.25f;
    input.m_pageInterval = 0.125f;
    input.m_min = 0.0f;
    input.m_max = 1.0f;
    IndexArp::UIState ui;
    arp.PopulateUIState(&ui, input);
    ui.Snapshot();
    constexpr std::array<float, 12> x_choices =
    {0.0f, 0.0f, 0.875f, 0.875f, 0.875f, 0.125f, 0.125f, 0.0f, 0.0f, 0.0f, 0.25f, 0.25f};
    for (size_t index = 0; index < x_choices.size(); ++index)
    {
        input.m_clockPosition = static_cast<int64_t>(index) - 6;
        DOCTEST_CAPTURE(input.m_clockPosition);
        arp.Process(input);
        DOCTEST_CHECK(arp.m_choiceValue == doctest::Approx(x_choices[index]));
        DOCTEST_CHECK(ui.GetChoiceValue(input.m_clockPosition) == doctest::Approx(x_choices[index]));
    }
}

DOCTEST_TEST_CASE("SequencerUI: a wholly inactive arp rhythm previews the reset choice")
{
    IndexArp arp;
    auto input = SparseArpInput();
    input.m_rhythmLength = 2;
    input.m_rhythm[1] = false;
    input.m_offset = 0.25f;
    input.m_interval = 0.125f;
    input.m_pageInterval = 0.25f;
    input.m_min = 0.0f;
    input.m_max = 1.0f;
    IndexArp::UIState ui;
    arp.PopulateUIState(&ui, input);
    ui.Snapshot();
    for (int64_t position : {-3, 0, 5})
    {
        input.m_clockPosition = position;
        arp.Process(input);
        DOCTEST_CHECK_FALSE(arp.m_triggered);
        DOCTEST_CHECK(arp.m_choiceValue == doctest::Approx(0.25f));
        DOCTEST_CHECK(ui.GetChoiceValue(position) == doctest::Approx(0.25f));
    }
}
