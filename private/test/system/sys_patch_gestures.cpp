#include "doctest.h"
#include "../support/SynthRig.hpp"
#include "TheNonagonSquiggleBoyWrldBldr.hpp"

#include <cmath>

namespace
{

using Encoders = SmartGridOneEncoders;
using Color = SmartGrid::Color;

struct PatchGestureRig
{
    synthrig::SynthRig m_rig;
    TheNonagonSquiggleBoyWrldBldr m_controller;

    PatchGestureRig()
        : m_controller(&m_rig.Internal())
    {
        m_controller.m_auxFocus = true;
    }

    void AddGesture(int gesture, Encoders::Bank bank, size_t trio, int x, int y)
    {
        m_rig.Internal().SetActiveTrio(static_cast<TheNonagonSmartGrid::Trio>(trio));
        m_rig.Internal().m_squiggleBoy.m_encoders.SelectBank(bank);
        m_rig.SetEncoder(x, y, 0.2f);
        m_rig.SetFader(gesture, 1.0f);
        m_rig.RunFrames(2);
        auto* pad = m_controller.m_auxGrid.Get(gesture % 8, gesture / 8);
        pad->OnPress(127);
        m_rig.IncEncoder(x, y, 50);
        m_rig.RunFrames(1);
        pad->OnRelease();
        m_rig.RunFrames(2);
    }

    Color PublishedColor(int gesture)
    {
        m_controller.ProcessFrame();
        return m_controller.m_uiState.m_colorBus[2].Get(gesture % 8, gesture / 8);
    }
};

}

DOCTEST_TEST_CASE("Patch gestures: fresh load publishes assigned colors without interaction")
{
    std::string patch;
    {
        PatchGestureRig source;
        source.AddGesture(0, Encoders::Bank::Source, 0, 0, 0);
        source.AddGesture(9, Encoders::Bank::FilterAndAmp, 2, 2, 1);
        source.m_rig.Internal().SetActiveTrio(TheNonagonSmartGrid::Trio::Water);
        DOCTEST_REQUIRE(source.PublishedColor(0) == Color::Red);
        DOCTEST_REQUIRE(source.PublishedColor(9) == Color::Green);
        patch = source.m_rig.SavePatch();
        DOCTEST_REQUIRE_FALSE(patch.empty());
    }

    PatchGestureRig loaded;
    DOCTEST_SUBCASE("startup before the first control frame")
    {
    }

    DOCTEST_SUBCASE("startup after control frames have settled")
    {
        loaded.m_rig.RunFrames(2);
    }

    DOCTEST_REQUIRE(loaded.m_rig.LoadPatch(patch));
    loaded.m_rig.RunFrames(2);
    DOCTEST_CHECK(loaded.PublishedColor(0) == Color::Red);
    DOCTEST_CHECK(loaded.PublishedColor(9) == Color::Green);
    DOCTEST_CHECK(loaded.PublishedColor(1) == Color::Grey.Dim());
}

DOCTEST_TEST_CASE("Patch gestures: same-scene loads replace assignment colors")
{
    PatchGestureRig fixture;
    fixture.m_rig.RunFrames(2);
    const std::string emptyPatch = fixture.m_rig.SavePatch();
    fixture.AddGesture(0, Encoders::Bank::Source, 0, 0, 0);
    const std::string redPatch = fixture.m_rig.SavePatch();
    fixture.AddGesture(0, Encoders::Bank::FilterAndAmp, 0, 2, 1);
    const std::string mixedPatch = fixture.m_rig.SavePatch();
    DOCTEST_REQUIRE_FALSE(emptyPatch.empty());
    DOCTEST_REQUIRE_FALSE(redPatch.empty());
    DOCTEST_REQUIRE_FALSE(mixedPatch.empty());
    DOCTEST_REQUIRE(fixture.PublishedColor(0) == Color::White);

    DOCTEST_REQUIRE(fixture.m_rig.LoadPatch(redPatch));
    fixture.m_rig.RunFrames(2);
    DOCTEST_CHECK(fixture.PublishedColor(0) == Color::Red);

    DOCTEST_REQUIRE(fixture.m_rig.ReloadPatch(mixedPatch));
    fixture.m_rig.RunFrames(2);
    DOCTEST_CHECK(fixture.PublishedColor(0) == Color::White);

    DOCTEST_REQUIRE(fixture.m_rig.LoadPatch(emptyPatch));
    fixture.m_rig.RunFrames(2);
    DOCTEST_CHECK(fixture.PublishedColor(0) == Color::Grey.Dim());

    DOCTEST_REQUIRE(fixture.m_rig.ReloadPatch(redPatch));
    fixture.m_rig.RunFrames(2);
    DOCTEST_CHECK(fixture.PublishedColor(0) == Color::Red);
}

DOCTEST_TEST_CASE("Patch gestures: loaded targets respond to faders without scene changes")
{
    std::string patch;
    float base = 0.0f;
    float target = 0.0f;
    {
        PatchGestureRig source;
        source.AddGesture(0, Encoders::Bank::Source, 0, 0, 0);
        target = source.m_rig.Internal().m_squiggleBoy.m_encoders.GetValueNoSlew(Encoders::Param::Harmonics1);
        source.m_rig.SetFader(0, 0.0f);
        source.m_rig.RunFrames(4);
        base = source.m_rig.Internal().m_squiggleBoy.m_encoders.GetValueNoSlew(Encoders::Param::Harmonics1);
        DOCTEST_REQUIRE(std::fabs(target - base) > 0.01f);
        source.m_rig.SetFader(0, 1.0f);
        source.m_rig.RunFrames(4);
        patch = source.m_rig.SavePatch();
        DOCTEST_REQUIRE_FALSE(patch.empty());
    }

    PatchGestureRig loaded;
    loaded.m_rig.RunFrames(2);
    const std::string emptyPatch = loaded.m_rig.SavePatch();
    DOCTEST_REQUIRE_FALSE(emptyPatch.empty());
    DOCTEST_REQUIRE(loaded.m_rig.LoadPatch(patch));
    loaded.m_rig.RunFrames(2);
    auto& encoders = loaded.m_rig.Internal().m_squiggleBoy.m_encoders;
    DOCTEST_CHECK(encoders.GetValueNoSlew(Encoders::Param::Harmonics1) == doctest::Approx(target));

    const float x_weights[] = {0.0f, 0.5f, 1.0f};
    for (float weight : x_weights)
    {
        DOCTEST_CAPTURE(weight);
        loaded.m_rig.SetFader(0, weight);
        loaded.m_rig.RunFrames(4);
        DOCTEST_CHECK(encoders.GetValueNoSlew(Encoders::Param::Harmonics1)
            == doctest::Approx(base * (1.0f - weight) + target * weight).epsilon(0.001));
    }

    DOCTEST_REQUIRE(loaded.m_rig.ReloadPatch(emptyPatch));
    loaded.m_rig.RunFrames(2);
    DOCTEST_CHECK(encoders.GetValueNoSlew(Encoders::Param::Harmonics1) == doctest::Approx(0.0f));
    DOCTEST_CHECK_FALSE(loaded.m_rig.SawNaN());
}
