#include "doctest.h"
#include "StateInterchange.hpp"

DOCTEST_TEST_CASE("patch format: legacy voice trees migrate before load publication")
{
    StateInterchange interchange;
    JSON patch = interchange.ParseForLoad(R"({"other":{"keep":7},"squiggleBoy":{"Harmonics1":{"values":{"values":[[0.1,0.2,0.3],[0.4,0.5,0.6]]},"modulators":[null,{"values":{"values":[[-0.1,-0.2,-0.3],[0.7,0.8,0.9]]},"gestures":[{"values":{"values":[[0.11,0.12,0.13]]},"active":[false,true,false,false,false,false,false,false,false,false,false,false,false,false,false,false,true,false]}]}]},"HPCutoff":{"values":{"values":[[0.21,0.22,0.23],[0.31,0.32,0.33]]}},"PanRadius":{"values":{"values":[[0.41,0.42,0.43]]}},"LFO1Skew":{"values":{"values":[[0.51,0.52,0.53]]}},"DelayTime":{"values":{"values":[[0.61],[0.62]]}},"UnknownEncoder":{"custom":3}}})");
    DOCTEST_REQUIRE_FALSE(patch.IsNull());
    DOCTEST_CHECK(patch.Get("version").IntegerValue() == PatchFormat::x_currentVersion);
    DOCTEST_CHECK(patch.Get("other").Get("keep").IntegerValue() == 7);
    JSON encoders = patch.Get("squiggleBoy");
    DOCTEST_CHECK(encoders.Get("Harmonics1").IsNull());
    DOCTEST_CHECK(encoders.Get("Harmonics1Fire").Get("values").Get("values").GetAt(1).NumberValue() == doctest::Approx(0.5));
    DOCTEST_CHECK(encoders.Get("Harmonics1Earth").Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.3));
    JSON nested = encoders.Get("Harmonics1Fire").Get("modulators");
    DOCTEST_CHECK(nested.GetAt(0).IsNull());
    DOCTEST_CHECK(nested.GetAt(1).Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(-0.2));
    JSON gesture = nested.GetAt(1).Get("gestures").GetAt(0);
    DOCTEST_CHECK(gesture.Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.12));
    DOCTEST_CHECK(gesture.Get("active").GetAt(0).BooleanValue());
    DOCTEST_CHECK_FALSE(gesture.Get("active").GetAt(1).BooleanValue());
    DOCTEST_CHECK(encoders.Get("HPCutoffFire").Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.22));
    DOCTEST_CHECK(encoders.Get("PanRadiusEarth").Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.43));
    DOCTEST_CHECK(encoders.Get("LFO1SkewWater").Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.51));
    DOCTEST_CHECK(encoders.Get("DelayTime").Get("values").Get("values").GetAt(1).NumberValue() == doctest::Approx(0.62));
    DOCTEST_CHECK(encoders.Get("UnknownEncoder").Get("custom").IntegerValue() == 3);
}


DOCTEST_TEST_CASE("patch format: sparse first row and short voice activation use legacy indexes")
{
    StateInterchange interchange;
    JSON patch = interchange.ParseForLoad(R"({"squiggleBoy":{"Harmonics1":{"values":{"values":[null,[0.4,0.5,0.6]]},"gestures":[{"values":{"values":[null,[0.7,0.8,0.9]]},"active":[false,true,true,true,true,true,true,true]}]}}})");
    DOCTEST_REQUIRE_FALSE(patch.IsNull());
    JSON fire = patch.Get("squiggleBoy").Get("Harmonics1Fire");
    DOCTEST_CHECK(fire.Get("values").Get("values").GetAt(0).NumberValue() == 0);
    DOCTEST_CHECK(fire.Get("values").Get("values").GetAt(1).NumberValue() == doctest::Approx(0.5));
    JSON gesture = fire.Get("gestures").GetAt(0);
    DOCTEST_CHECK(gesture.Get("values").Get("values").GetAt(1).NumberValue() == doctest::Approx(0.8));
    DOCTEST_CHECK(gesture.Get("active").Size() == 8);
    DOCTEST_CHECK(gesture.Get("active").GetAt(0).BooleanValue());
    DOCTEST_CHECK_FALSE(gesture.Get("active").GetAt(1).BooleanValue());
}


DOCTEST_TEST_CASE("patch format: explicit zero replaces version once and shared activation selects track zero")
{
    StateInterchange interchange;
    JSON patch = interchange.ParseForLoad(R"({"version":0,"squiggleBoy":{"DelayTime":{"values":{"values":[[0.31,0.32,0.33]]},"gestures":[{"values":{"values":[[0.41,0.42,0.43]]},"active":[true,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,true]}]}}})");
    DOCTEST_REQUIRE_FALSE(patch.IsNull());
    DOCTEST_CHECK(patch.Get("version").IntegerValue() == PatchFormat::x_currentVersion);
    int versionCount = 0;
    const JsonMember* members = static_cast<const JsonMember*>(patch.m_node->m_container.m_entries);
    for (uint32_t i = 0; i < patch.m_node->m_container.m_size; ++i)
    {
        if (strcmp(members[i].m_key, "version") == 0)
        {
            ++versionCount;
        }
    }

    DOCTEST_CHECK(versionCount == 1);
    JSON shared = patch.Get("squiggleBoy").Get("DelayTime");
    DOCTEST_CHECK(shared.Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.31));
    JSON gesture = shared.Get("gestures").GetAt(0);
    DOCTEST_CHECK(gesture.Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.41));
    DOCTEST_CHECK(gesture.Get("active").GetAt(0).BooleanValue());
    DOCTEST_CHECK_FALSE(gesture.Get("active").GetAt(1).BooleanValue());
}

DOCTEST_TEST_CASE("patch format: strict version dispatch and collision rejection")
{
    StateInterchange interchange;
    for (const char* text : {"[]", "null", R"({"version":null})", R"({"version":4.0})", R"({"version":-1})", R"({"version":5})", R"({"version":"4"})", R"({"version":4294967299})", R"({"squiggleBoy":{"Harmonics1":{},"Harmonics1Fire":{}}})"})
    {
        DOCTEST_CHECK(interchange.ParseForLoad(text).IsNull());
        DOCTEST_CHECK_FALSE(interchange.IsLoadRequested());
    }

    JSON current = interchange.ParseForLoad(R"({"version":4,"other":9})");
    DOCTEST_REQUIRE_FALSE(current.IsNull());
    DOCTEST_CHECK(current.Get("other").IntegerValue() == 9);
}

DOCTEST_TEST_CASE("patch format: SRR polarity migrations compose with legacy trio conversion")
{
    StateInterchange interchange;
    JSON patch = interchange.ParseForLoad(R"({
        "squiggleBoy":{
            "SampleRateReduction":{"values":{"values":[[0,0.25,1],[0.625,0.5,0.125]]}},
            "PMSampleRateReduction":{"values":{"values":[[0.1,0.2,0.3]]}}
        }
    })");
    DOCTEST_REQUIRE_FALSE(patch.IsNull());
    JSON encoders = patch.Get("squiggleBoy");
    DOCTEST_CHECK(encoders.Get("SampleRateReduction").IsNull());
    DOCTEST_CHECK(encoders.Get("SampleRateReductionWater").Get("values").Get("values").GetAt(0).NumberValue() == 1);
    DOCTEST_CHECK(encoders.Get("SampleRateReductionFire").Get("values").Get("values").GetAt(0).NumberValue() == 0.75);
    DOCTEST_CHECK(encoders.Get("SampleRateReductionEarth").Get("values").Get("values").GetAt(0).NumberValue() == 0);
    DOCTEST_CHECK(encoders.Get("SampleRateReductionEarth").Get("values").Get("values").GetAt(1).NumberValue() == 0.875);
    DOCTEST_CHECK(encoders.Get("PMSampleRateReductionFire").Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.8));
}

DOCTEST_TEST_CASE("patch format: FACT switch positions migrate to their original loop fractions")
{
    StateInterchange interchange;
    JSON patch = interchange.ParseForLoad(R"({
        "version":3,
        "squiggleBoy":{"DelayTimeFactor":{
            "values":{"values":[0,0.25,0.5,0.75,1]},
            "gestures":[null,{"values":{"values":[0.24]},"active":[true]}],
            "modulators":[{"values":{"values":[-0.25]}}]
        }}
    })");
    DOCTEST_REQUIRE_FALSE(patch.IsNull());
    DOCTEST_CHECK(patch.Get("version").IntegerValue() == PatchFormat::x_currentVersion);
    JSON factor = patch.Get("squiggleBoy").Get("DelayTimeFactor");
    JSON values = factor.Get("values").Get("values");
    const double x_expected[] = {0.8, 2.0 / 3.0, 1.0, 0.75, 0.625};
    for (size_t scene = 0; scene < std::size(x_expected); ++scene)
    {
        DOCTEST_CHECK(values.GetAt(scene).NumberValue() == doctest::Approx(x_expected[scene]));
    }

    JSON gesture = factor.Get("gestures").GetAt(1);
    DOCTEST_CHECK(gesture.Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(2.0 / 3.0));
    DOCTEST_CHECK(gesture.Get("active").GetAt(0).BooleanValue());
    DOCTEST_CHECK(factor.Get("modulators").GetAt(0).Get("values").Get("values").GetAt(0).NumberValue() == -0.25);
    DOCTEST_CHECK(PatchFormat::Upgrade(interchange.m_loadArena, patch).m_node == patch.m_node);
}

DOCTEST_TEST_CASE("patch format: Shape SRR reflects gesture targets and negates recursive depths")
{
    StateInterchange interchange;
    JSON patch = interchange.ParseForLoad(R"({
        "version":1,
        "squiggleBoy":{
            "SampleRateReductionWater":{
                "values":{"values":[0.25,1,null],"extra":9},
                "modulators":[null,{"values":{"values":[-0.5,0.25]},
                    "gestures":[{"values":{"values":[-0.75]},"active":[true]}],
                    "modulators":[{"values":{"values":[0.125]}}]}],
                "gestures":[{"values":{"values":[0.875]},"active":[true]}]
            },
            "SampleRateReductionFire":{"gestures":[null,{}]},
            "SampleRateReductionEarth":{"values":{"values":[]}},
            "PMSampleRateReductionWater":{"values":{"values":[0.25]}}
        }
    })");
    DOCTEST_REQUIRE_FALSE(patch.IsNull());
    DOCTEST_CHECK(patch.Get("version").IntegerValue() == PatchFormat::x_currentVersion);
    JSON encoders = patch.Get("squiggleBoy");
    auto scene = [](JSON cell, size_t i)
    {
        return cell.Get("values").Get("values").GetAt(i).NumberValue();
    };
    JSON water = encoders.Get("SampleRateReductionWater");
    DOCTEST_CHECK(scene(water, 0) == 0.75);
    DOCTEST_CHECK(scene(water, 1) == 0);
    DOCTEST_CHECK(scene(water, 2) == 1);
    DOCTEST_CHECK(scene(water, 7) == 1);
    DOCTEST_CHECK(water.Get("values").Get("extra").IntegerValue() == 9);
    JSON mods = water.Get("modulators");
    DOCTEST_CHECK(mods.GetAt(0).IsNull());
    DOCTEST_CHECK(scene(mods.GetAt(1), 0) == 0.5);
    DOCTEST_CHECK(scene(mods.GetAt(1), 1) == -0.25);
    DOCTEST_CHECK(scene(mods.GetAt(1).Get("modulators").GetAt(0), 0) == -0.125);
    JSON depthGesture = mods.GetAt(1).Get("gestures").GetAt(0);
    DOCTEST_CHECK(scene(depthGesture, 0) == 0.75);
    DOCTEST_CHECK(depthGesture.Get("active").GetAt(0).BooleanValue());
    JSON gesture = water.Get("gestures").GetAt(0);
    DOCTEST_CHECK(scene(gesture, 0) == 0.125);
    DOCTEST_CHECK(gesture.Get("active").GetAt(0).BooleanValue());
    DOCTEST_CHECK(encoders.Get("SampleRateReductionFire").Get("values").IsNull());
    DOCTEST_CHECK(scene(encoders.Get("SampleRateReductionEarth"), 0) == 1);
    DOCTEST_CHECK(scene(encoders.Get("PMSampleRateReductionWater"), 0) == 0.75);
    DOCTEST_CHECK(PatchFormat::Upgrade(interchange.m_loadArena, patch).m_node == patch.m_node);
}

DOCTEST_TEST_CASE("patch format: publication, busy retry, and conversion growth")
{
    StateInterchange interchange;
    interchange.m_loadArena.Init(1024);
    const std::string legacy = R"({"squiggleBoy":{"Harmonics1":{"values":{"values":[[0.1,0.2,0.3]]}}}})";
    JsonArena parseOnly(1024);
    DOCTEST_REQUIRE_FALSE(parseOnly.Loads(legacy.c_str()).IsNull());
    DOCTEST_CHECK_FALSE(parseOnly.Failed());
    DOCTEST_REQUIRE(interchange.RequestLoadText(legacy, true));
    DOCTEST_CHECK(interchange.m_loadArena.Capacity() > 1024);
    DOCTEST_REQUIRE(interchange.IsLoadRequested());
    DOCTEST_CHECK(interchange.GetToLoad().Get("version").IntegerValue() == PatchFormat::x_currentVersion);
    DOCTEST_CHECK(interchange.GetToLoad().Get("squiggleBoy").Get("Harmonics1Fire").Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.2));
    DOCTEST_REQUIRE(interchange.RequestLoadText(R"({"version":5})", false));
    DOCTEST_CHECK(interchange.m_pendingLoad == R"({"version":5})");
    interchange.AckLoadCompleted();
    interchange.RetryPendingLoad();
    DOCTEST_CHECK_FALSE(interchange.IsLoadRequested());
    DOCTEST_CHECK(interchange.m_pendingLoad.empty());
}

DOCTEST_TEST_CASE("patch format: held recorder reader defers and then upgrades pending legacy load")
{
    StateInterchange interchange;
    DOCTEST_REQUIRE(interchange.RequestLoadText(R"({"version":1,"marker":7})", true));
    JSON held = interchange.GetToLoad();
    interchange.AckLoadCompleted();
    DOCTEST_REQUIRE(interchange.m_loadArena.TryRead());
    DOCTEST_CHECK(interchange.RequestLoadText(R"({"squiggleBoy":{"Harmonics1":{"values":{"values":[[0.1,0.2,0.3]]}}}})", true));
    DOCTEST_CHECK(interchange.m_pendingLoad.size() > 0);
    DOCTEST_CHECK(held.Get("marker").IntegerValue() == 7);
    interchange.RetryPendingLoad();
    DOCTEST_CHECK_FALSE(interchange.IsLoadRequested());
    DOCTEST_CHECK(held.Get("marker").IntegerValue() == 7);
    interchange.m_loadArena.Release();
    interchange.RetryPendingLoad();
    DOCTEST_REQUIRE(interchange.IsLoadRequested());
    JSON upgraded = interchange.GetToLoad();
    DOCTEST_CHECK(upgraded.Get("version").IntegerValue() == PatchFormat::x_currentVersion);
    DOCTEST_CHECK(upgraded.Get("squiggleBoy").Get("Harmonics1Fire").Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.2));
    interchange.AckLoadCompleted();
}

DOCTEST_TEST_CASE("patch format: duplicate version or encoder root is rejected")
{
    StateInterchange interchange;
    for (const char* text : {R"({"version":0,"version":2})", R"({"version":1,"version":1})", R"({"squiggleBoy":{},"squiggleBoy":{}})"})
    {
        DOCTEST_CHECK(interchange.ParseForLoad(text).IsNull());
    }
}

DOCTEST_TEST_CASE("patch format: empty and null-only legacy Voice rows become eight zeros")
{
    StateInterchange interchange;
    JSON patch = interchange.ParseForLoad(R"({"squiggleBoy":{"Harmonics1":{"values":{"values":[]}},"HPCutoff":{"values":{"values":[null,null]}}}})");
    DOCTEST_REQUIRE_FALSE(patch.IsNull());
    JSON encoders = patch.Get("squiggleBoy");
    for (const char* name : {"Harmonics1Water", "Harmonics1Fire", "Harmonics1Earth", "HPCutoffWater", "HPCutoffFire", "HPCutoffEarth"})
    {
        JSON values = encoders.Get(name).Get("values").Get("values");
        DOCTEST_CHECK(values.Size() == 8);
        DOCTEST_CHECK(values.GetAt(0).NumberValue() == 0);
        DOCTEST_CHECK(values.GetAt(7).NumberValue() == 0);
    }
}

DOCTEST_TEST_CASE("patch format: gesture to modulator recursion retains null indexes")
{
    StateInterchange interchange;
    JSON patch = interchange.ParseForLoad(R"({"squiggleBoy":{"Harmonics1":{"gestures":[{"values":{"values":[[0.11,0.22,0.33]]},"modulators":[null,{"values":{"values":[[0.41,0.52,0.63],[0.71,0.82,0.93]]}}]}]}}})");
    DOCTEST_REQUIRE_FALSE(patch.IsNull());
    JSON gesture = patch.Get("squiggleBoy").Get("Harmonics1Fire").Get("gestures").GetAt(0);
    DOCTEST_CHECK(gesture.Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.22));
    JSON modulators = gesture.Get("modulators");
    DOCTEST_CHECK(modulators.GetAt(0).IsNull());
    DOCTEST_CHECK(modulators.GetAt(1).Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.52));
    DOCTEST_CHECK(modulators.GetAt(1).Get("values").Get("values").GetAt(1).NumberValue() == doctest::Approx(0.82));
}
