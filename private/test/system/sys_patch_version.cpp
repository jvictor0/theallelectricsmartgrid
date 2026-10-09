#include "doctest.h"
#include "../support/SynthRig.hpp"

DOCTEST_TEST_CASE("sys_patch_version: legacy trio values survive live load save reload")
{
    synthrig::SynthRig rig;
    rig.RunFrames(2);
    const std::string legacy = R"({"squiggleBoy":{"Harmonics1":{"values":{"values":[[0.11,0.22,0.33],[0.41,0.52,0.63]]},"gestures":[{"values":{"values":[[0.15,0.26,0.37]]},"active":[true,true,true]}]},"HPCutoff":{"values":{"values":[[0.14,0.25,0.36]]}},"PanRadius":{"values":{"values":[[0.17,0.28,0.39]]}},"LFO1Skew":{"values":{"values":[[0.19,0.29,0.49]]}},"DelayTime":{"values":{"values":[[0.71,0.82,0.93]]}}}})";
    DOCTEST_REQUIRE(rig.LoadPatch(legacy));
    rig.RunFrames(2);
    auto& encoders = rig.Internal().m_squiggleBoy.m_encoders;
    DOCTEST_CHECK(encoders.GetValueNoSlew(SmartGridOneEncoders::Param::Harmonics1, 0) == doctest::Approx(0.11));
    DOCTEST_CHECK(encoders.GetValueNoSlew(SmartGridOneEncoders::Param::Harmonics1, 3) == doctest::Approx(0.22));
    DOCTEST_CHECK(encoders.GetValueNoSlew(SmartGridOneEncoders::Param::Harmonics1, 6) == doctest::Approx(0.33));
    DOCTEST_CHECK(encoders.GetValueNoSlew(SmartGridOneEncoders::Param::HPCutoff, 3) == doctest::Approx(0.25));
    DOCTEST_CHECK(encoders.GetValueNoSlew(SmartGridOneEncoders::Param::PanRadius, 6) == doctest::Approx(0.39));
    DOCTEST_CHECK(encoders.GetValueNoSlew(SmartGridOneEncoders::Param::LFO1Skew, 0) == doctest::Approx(0.19));
    DOCTEST_CHECK(encoders.GetValueNoSlew(SmartGridOneEncoders::Param::DelayTime, 0) == doctest::Approx(0.71));
    std::string saved = rig.SavePatch();
    JsonArena arena;
    arena.Init(JsonArena::kDefaultCapacity);
    JSON patch = arena.Loads(saved.c_str());
    DOCTEST_REQUIRE_FALSE(patch.IsNull());
    DOCTEST_CHECK(patch.Get("version").IntegerValue() == 1);
    DOCTEST_CHECK(patch.Get("squiggleBoy").Get("Harmonics1").IsNull());
    DOCTEST_CHECK(patch.Get("squiggleBoy").Get("Harmonics1Fire").Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.22));
    JSON waterGesture = patch.Get("squiggleBoy").Get("Harmonics1Water").Get("gestures").GetAt(0);
    JSON fireGesture = patch.Get("squiggleBoy").Get("Harmonics1Fire").Get("gestures").GetAt(0);
    JSON earthGesture = patch.Get("squiggleBoy").Get("Harmonics1Earth").Get("gestures").GetAt(0);
    DOCTEST_CHECK(waterGesture.Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.15));
    DOCTEST_CHECK(fireGesture.Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.26));
    DOCTEST_CHECK(earthGesture.Get("values").Get("values").GetAt(0).NumberValue() == doctest::Approx(0.37));
    DOCTEST_CHECK(waterGesture.Get("active").GetAt(0).BooleanValue());
    DOCTEST_CHECK(fireGesture.Get("active").GetAt(0).BooleanValue());
    DOCTEST_CHECK(earthGesture.Get("active").GetAt(0).BooleanValue());
    DOCTEST_REQUIRE(rig.LoadPatch(saved));
    rig.RunFrames(2);
    DOCTEST_CHECK(encoders.GetValueNoSlew(SmartGridOneEncoders::Param::Harmonics1, 3) == doctest::Approx(0.22));
}
