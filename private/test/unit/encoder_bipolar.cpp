#include "doctest.h"

#include "EncoderBankBank.hpp"
#include "../support/GlobalEnv.hpp"

namespace
{

using Cell = SmartGrid::BankedEncoderCell;

struct EncoderRig
{
    SmartGrid::SceneManager m_scenes;
    EncoderBankBank m_banks;

    EncoderRig(bool bipolar, float defaultValue)
        : m_banks(1, 1, 1, &m_scenes)
    {
        GlobalEnv::Init();
        m_banks.InitMode(0, 2, 2);
        m_banks.InitBank(0, 0, SmartGrid::Color::Red);
        m_banks.CreateEncoder(0, 0, defaultValue, "Carrier", "CAR", SmartGrid::Color::Red, 0, bipolar);
        m_banks.PlaceEncoder(0, 0, 0, 0);
        for (size_t i = 0; i < Cell::x_numModulators; ++i)
        {
            Sources().SetModulatorColor(i, SmartGrid::Color::Red);
        }
    }

    Cell& Carrier()
    {
        return *m_banks.GetEncoder(0);
    }

    Cell::ModulatorValues& Sources()
    {
        return m_banks.m_bankModes[0].m_modulatorValues;
    }

    Cell& Depth(Cell& parent, size_t source)
    {
        parent.m_modulators.FillModulators(&parent, &m_scenes);
        return *parent.m_modulators.m_modulators[source];
    }

    void Compute()
    {
        Carrier().SetStateRecursive();
        Carrier().SetModulatorsAffecting();
        Carrier().Compute();
    }
};

JSON StoredValues(JsonArena& arena, float position)
{
    JSON root = arena.Object();
    JSON values = arena.Array();
    for (size_t scene = 0; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
    {
        JSON tracks = arena.Array();
        tracks.AppendNew(arena.Real(position));
        tracks.AppendNew(arena.Real(position));
        values.AppendNew(tracks);
    }

    root.SetNew("values", values);
    return root;
}

}

DOCTEST_TEST_CASE("encoder_bipolar: new depths are centered and collectible")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    DOCTEST_CHECK(depth.m_values[0][0] == 0.5f);
    DOCTEST_CHECK(depth.m_output[0] == 0.5f);
    DOCTEST_CHECK(depth.m_defaultValue == 0.5f);
    DOCTEST_CHECK(depth.CanBeGarbageCollected());
    depth.SetValueAllScenesAllTracks(0.0f);
    DOCTEST_CHECK_FALSE(depth.CanBeGarbageCollected());
}

DOCTEST_TEST_CASE("encoder_bipolar: depth crossfades toward normal and inverted sources")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    rig.Sources().m_value[0][0] = 0.8f;
    const float positions[] = {0.5f, 1.0f, 0.0f, 0.75f, 0.25f};
    const float outputs[] = {0.3f, 0.8f, 0.2f, 0.425f, 0.275f};
    for (size_t i = 0; i < 5; ++i)
    {
        depth.SetValueAllScenesAllTracks(positions[i]);
        rig.Compute();
        DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(outputs[i]));
    }

    DOCTEST_CHECK(rig.Carrier().m_minValue[0] == doctest::Approx(0.225f));
    DOCTEST_CHECK(rig.Carrier().m_maxValue[0] == doctest::Approx(0.475f));
}

DOCTEST_TEST_CASE("encoder_bipolar: mixed signs normalize absolute weights per voice")
{
    EncoderRig rig(false, 0.3f);
    rig.Depth(rig.Carrier(), 0).SetValueAllScenesAllTracks(1.0f);
    rig.Depth(rig.Carrier(), 1).SetValueAllScenesAllTracks(0.0f);
    rig.Sources().m_value[0][0] = 0.8f;
    rig.Sources().m_value[1][0] = 0.2f;
    rig.Sources().m_amplitude[0][1] = 0.0f;
    rig.Sources().m_amplitude[1][1] = 0.25f;
    rig.Sources().m_value[1][1] = 0.8f;
    rig.Compute();
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.8f));
    DOCTEST_CHECK(rig.Carrier().m_minValue[0] == 0.0f);
    DOCTEST_CHECK(rig.Carrier().m_maxValue[0] == 1.0f);
    DOCTEST_CHECK(rig.Carrier().m_output[1] == doctest::Approx(0.275f));
}

DOCTEST_TEST_CASE("encoder_bipolar: recursive depth stays normalized until its parent reads it")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    rig.Depth(depth, 1).SetValueAllScenesAllTracks(1.0f);
    rig.Sources().m_value[0][0] = 0.8f;
    rig.Sources().m_value[1][0] = 0.25f;
    rig.Compute();
    DOCTEST_CHECK(depth.m_output[0] == doctest::Approx(0.25f));
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.275f));
    DOCTEST_CHECK_FALSE(depth.CanBeGarbageCollected());
}

DOCTEST_TEST_CASE("encoder_bipolar: amplitude alone invalidates the modulation output")
{
    EncoderRig rig(false, 0.3f);
    rig.Depth(rig.Carrier(), 0).SetValueAllScenesAllTracks(1.0f);
    rig.Sources().m_value[0][0] = 0.8f;
    rig.Sources().ComputeChanged();
    rig.Compute();
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.8f));
    rig.Sources().m_amplitude[0][0] = 0.0f;
    rig.Sources().ComputeChanged();
    rig.Carrier().Compute();
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.3f));
}

DOCTEST_TEST_CASE("encoder_bipolar: unversioned JSON stores signed knob positions")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    JsonArena arena(1024 * 1024);
    const float stored[] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
    for (float value : stored)
    {
        depth.StateEncoderCell::FromJSON(StoredValues(arena, value));
        DOCTEST_CHECK(depth.m_bankedValue[0] == doctest::Approx((value + 1.0f) * 0.5f));
        JSON saved = depth.ToJSON(arena);
        DOCTEST_CHECK(saved.Get("values").Get("values").GetAt(0).GetAt(0).NumberValue() == doctest::Approx(value));
        DOCTEST_CHECK(saved.Get("version").IsNull());
        DOCTEST_CHECK(saved.Get("bipolar").IsNull());
    }

    rig.Carrier().StateEncoderCell::FromJSON(StoredValues(arena, 0.3f));
    DOCTEST_CHECK(rig.Carrier().m_bankedValue[0] == doctest::Approx(0.3f));
}

DOCTEST_TEST_CASE("encoder_bipolar: DSP getters convert once while UI and slew stay normalized")
{
    EncoderRig rig(true, 0.0f);
    DOCTEST_CHECK(rig.Carrier().m_values[0][0] == 0.5f);
    rig.Carrier().SetValueAllScenesAllTracks(0.25f);
    rig.Carrier().InitSlewState(0.25f);
    rig.Compute();
    DOCTEST_CHECK(rig.Carrier().GetValue(0) == -0.5f);
    DOCTEST_CHECK(rig.Carrier().m_bankedValue[0] == 0.25f);
    DOCTEST_CHECK(rig.m_banks.GetValueByEncoderIndex(0, 0) == -0.5f);
    DOCTEST_CHECK(rig.m_banks.GetValueNoSlewByEncoderIndex(0, 0) == -0.5f);
    DOCTEST_CHECK(rig.m_banks.GetValue(0, 0, 0, 0) == -0.5f);
    DOCTEST_CHECK(rig.m_banks.GetValueNoSlew(0, 0, 0, 0) == -0.5f);
    DOCTEST_CHECK(rig.Carrier().m_slew[0].m_output == 0.25f);

    EncoderBankUIState ui;
    rig.m_banks.m_banks[0].PopulateUIState(&ui);
    DOCTEST_CHECK(ui.GetValue(0, 0, 0) == 0.25f);
    DOCTEST_CHECK(ui.GetBipolar(0, 0));
    rig.m_banks.m_banks[0].PlaceEncoder(0, 0, nullptr);
    rig.m_banks.m_banks[0].PopulateUIState(&ui);
    DOCTEST_CHECK_FALSE(ui.GetBipolar(0, 0));
}

DOCTEST_TEST_CASE("encoder_bipolar: centered scenes preserve negative routes and reset to neutral")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    depth.SetValueAllScenesAllTracks(0.5f);
    depth.m_values[0][0] = 0.0f;
    depth.m_values[0][1] = 1.0f;
    rig.m_scenes.m_blendFactor = 0.5f;
    rig.Sources().m_value[0][0] = 0.8f;
    rig.Compute();
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.3f));
    DOCTEST_CHECK_FALSE(depth.CanBeGarbageCollected());
    DOCTEST_CHECK(rig.Carrier().m_modulatorsAffectingPerTrack[0].Get(0));
    DOCTEST_CHECK_FALSE(rig.Carrier().m_modulatorsAffectingPerTrack[1].Get(0));
    rig.m_scenes.m_blendFactor = 0.75f;
    rig.Compute();
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.425f));
    rig.Carrier().HandleShiftPress();
    rig.Carrier().Compute();
    DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(0.3f));
    DOCTEST_CHECK(rig.Carrier().m_modulatorsAffecting.IsZero());
    DOCTEST_CHECK(rig.Depth(rig.Carrier(), 0).m_values[0][0] == 0.5f);
}

DOCTEST_TEST_CASE("encoder_bipolar: recursive patches preserve signed gesture targets without reapplying the curve")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    depth.SetValueAllScenesAllTracks(0.25f);
    depth.m_modulators.AddGesture(&depth, 0);
    Cell& gesture = *depth.m_modulators.m_gestures[0];
    gesture.SetActive(true);
    gesture.SetValueAllScenesAllTracks(0.75f);
    rig.Depth(depth, 1).SetValueAllScenesAllTracks(0.75f);
    rig.Sources().m_value[0][0] = 0.8f;
    rig.Sources().m_value[1][0] = 0.25f;
    rig.Sources().m_gestureWeights[0] = 1.0f;
    rig.Compute();
    const float expected = rig.Carrier().m_output[0];

    for (size_t iteration = 0; iteration < 5; ++iteration)
    {
        JsonArena arena(1024 * 1024);
        JSON saved = rig.Carrier().ToJSON(arena);
        JSON savedDepth = saved.Get("modulators").GetAt(0);
        DOCTEST_CHECK(savedDepth.Get("values").Get("values").GetAt(0).GetAt(0).NumberValue() == -0.5);
        JSON savedGesture = savedDepth.Get("gestures").GetAt(0);
        DOCTEST_CHECK(savedGesture.Get("values").Get("values").GetAt(0).GetAt(0).NumberValue() == 0.5);
        rig.Carrier().FromJSON(saved);
        rig.Compute();
        DOCTEST_CHECK(rig.Carrier().m_output[0] == doctest::Approx(expected));
    }
}

DOCTEST_TEST_CASE("encoder_bipolar: small signed depths meet continuously at zero")
{
    EncoderRig rig(false, 0.3f);
    Cell& depth = rig.Depth(rig.Carrier(), 0);
    rig.Sources().m_value[0][0] = 1.0f;
    depth.SetValueAllScenesAllTracks(0.50001f);
    rig.Compute();
    const float positive = rig.Carrier().m_output[0];
    depth.SetValueAllScenesAllTracks(0.49999f);
    rig.Compute();
    const float negative = rig.Carrier().m_output[0];
    DOCTEST_CHECK(positive > 0.3f);
    DOCTEST_CHECK(negative < 0.3f);
    DOCTEST_CHECK(positive - negative < 0.00001f);
}

DOCTEST_TEST_CASE("encoder_bipolar: gesture sweeps a bipolar parameter across zero")
{
    EncoderRig rig(true, -0.5f);
    Cell& carrier = rig.Carrier();
    carrier.m_modulators.AddGesture(&carrier, 0);
    Cell& gesture = *carrier.m_modulators.m_gestures[0];
    gesture.SetActive(true);
    gesture.SetValueAllScenesAllTracks(0.75f);
    rig.Compute();
    DOCTEST_REQUIRE(gesture.m_bipolar);

    const float weights[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.0f};
    const float expected[] = {-0.5f, -0.25f, 0.0f, 0.25f, 0.5f, -0.5f};
    for (size_t i = 0; i < 6; ++i)
    {
        rig.Sources().m_gestureWeights[0] = weights[i];
        rig.Sources().ComputeChanged();
        carrier.Compute();
        for (size_t voice = 0; voice < 2; ++voice)
        {
            DOCTEST_CHECK(carrier.GetValueNoSlew(voice) == doctest::Approx(expected[i]));
        }

        DOCTEST_CHECK(carrier.GetValueNoSlew(2) == -0.5f);
    }
}

DOCTEST_TEST_CASE("encoder_bipolar: reset without gestures restores the signed default and slew")
{
    EncoderRig rig(true, -0.5f);
    Cell& carrier = rig.Carrier();
    carrier.SetValueAllScenesAllTracks(0.875f);
    carrier.InitSlewState(0.875f);
    rig.Compute();
    DOCTEST_REQUIRE(carrier.GetValueNoSlew(0) == 0.75f);
    DOCTEST_REQUIRE(carrier.GetSlewedValue(0) == 0.75f);

    carrier.RevertToDefault(false, false);
    carrier.Compute();
    DOCTEST_CHECK(carrier.GetValue(0) == -0.5f);
    for (size_t voice = 0; voice < 2; ++voice)
    {
        DOCTEST_CHECK(carrier.GetValueNoSlew(voice) == -0.5f);
        DOCTEST_CHECK(carrier.GetSlewedValue(voice) == -0.5f);
        DOCTEST_CHECK(carrier.m_slew[voice].m_output == 0.25f);
    }
}

DOCTEST_TEST_CASE("encoder_bipolar: reset with an active gesture restores the signed default")
{
    EncoderRig rig(true, -0.5f);
    Cell& carrier = rig.Carrier();
    carrier.SetValueAllScenesAllTracks(0.625f);
    carrier.m_modulators.AddGesture(&carrier, 0);
    Cell& gesture = *carrier.m_modulators.m_gestures[0];
    gesture.SetActive(true);
    gesture.SetValueAllScenesAllTracks(1.0f);
    rig.Sources().m_gestureWeights[0] = 1.0f;
    rig.Compute();
    carrier.InitSlewState(1.0f);
    DOCTEST_REQUIRE(carrier.GetValueNoSlew(0) == 1.0f);
    DOCTEST_REQUIRE(carrier.m_gesturesAffectingPerTrack[0].Get(0));

    carrier.RevertToDefault(false, false);
    carrier.Compute();
    DOCTEST_CHECK(carrier.GetValue(0) == -0.5f);
    DOCTEST_CHECK(carrier.GetValueNoSlew(0) == -0.5f);
    DOCTEST_CHECK(carrier.GetSlewedValue(0) == -0.5f);
    DOCTEST_CHECK(carrier.m_gesturesAffectingPerTrack[0].IsZero());
    DOCTEST_CHECK(carrier.m_modulators.m_gestures[0].get() == nullptr);

    const float weights[] = {0.0f, 0.5f, 1.0f};
    for (float weight : weights)
    {
        rig.Sources().m_gestureWeights[0] = weight;
        rig.Sources().ComputeChanged();
        carrier.Compute();
        DOCTEST_CHECK(carrier.GetValueNoSlew(0) == -0.5f);
    }
}
