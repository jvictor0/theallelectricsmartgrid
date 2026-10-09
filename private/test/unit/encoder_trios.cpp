#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "SmartGridOneEncoders.hpp"

namespace
{

using Encoders = SmartGridOneEncoders;
using Cell = SmartGrid::BankedEncoderCell;

struct TrioEncoderRig
{
    SmartGridOneContext m_context;
    Encoders m_encoders;

    TrioEncoderRig()
        : m_encoders(&m_context)
    {
        for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
        {
            m_encoders.SetTrack(trio);
            m_encoders.UpdateEncodersForMachine(
                VoiceMachine::SourceMachine::DualWaveShapingVCO,
                VoiceMachine::FilterMachine::Ladder4Pole);
        }

        m_encoders.SetTrack(0);
    }

    Cell& Carrier(size_t trio)
    {
        return *m_encoders.m_encoderBankBank.GetEncoder(
            m_encoders.EncoderIndex(Encoders::Param::Harmonics1, trio));
    }

    void SetCarrier(size_t trio, float value)
    {
        Carrier(trio).SetValue(value, true);
        Carrier(trio).SetForceUpdateRecursive();
    }
};

}

DOCTEST_TEST_CASE("encoder_trios: defaults and assignments reach the computed base")
{
    GlobalEnv::ResetPerTest();
    TrioEncoderRig rig;
    Cell& cell = rig.Carrier(0);
    cell.Compute();
    DOCTEST_CHECK(cell.m_bankedValue == doctest::Approx(cell.m_defaultValue));
    DOCTEST_CHECK(cell.GetValueNoSlew(0) == doctest::Approx(cell.m_defaultValue));

    rig.SetCarrier(0, 0.7f);
    cell.Compute();
    DOCTEST_CHECK(cell.m_values[0] == doctest::Approx(0.7f));
    DOCTEST_CHECK(cell.m_bankedValue == doctest::Approx(0.7f));
    DOCTEST_CHECK(cell.GetValueNoSlew(0) == doctest::Approx(0.7f));
}

DOCTEST_TEST_CASE("encoder_trios: scene refresh and copy use stored base values")
{
    GlobalEnv::ResetPerTest();
    TrioEncoderRig rig;
    Cell& cell = rig.Carrier(0);
    cell.SetAndRecordValue(0.2f, 0);
    cell.SetAndRecordValue(0.8f, 1);
    rig.m_context.m_sceneManager.m_blendFactor = 0.25f;
    cell.SetStateRecursive();
    cell.Compute();
    DOCTEST_CHECK(cell.m_bankedValue == doctest::Approx(0.35f));

    cell.FillModulators(&rig.m_context);
    auto& source = rig.m_encoders.GetModulatorValues(Encoders::BankMode::VoiceWater);
    source.m_value[0][0] = 0.9f;
    cell.GetModulator(0)->SetValue(1.0f, true);
    cell.SetStateRecursive();
    cell.SetModulatorsAffecting();
    cell.Compute();
    DOCTEST_CHECK(cell.GetValueNoSlew(0) == doctest::Approx(0.9f));

    rig.m_encoders.CopyToScene(2);
    DOCTEST_CHECK(cell.m_values[2] == doctest::Approx(0.35f));
    DOCTEST_CHECK(cell.GetModulator(0)->m_values[2] == 1.0f);
}

DOCTEST_TEST_CASE("encoder_trios: global voice reads and UI use the selected trio's local channels")
{
    GlobalEnv::ResetPerTest();
    TrioEncoderRig rig;
    const float x_bases[] = {0.2f, 0.6f, 0.8f};
    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        rig.SetCarrier(trio, x_bases[trio]);
        rig.Carrier(trio).InitSlewState(x_bases[trio]);
    }

    rig.m_encoders.Process();
    const auto address = rig.m_encoders.GetParamAddress(Encoders::Param::Harmonics1);
    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        rig.m_encoders.SetTrack(trio);
        rig.m_encoders.SelectBank(Encoders::Bank::Source);
        EncoderBankUIState ui;
        rig.m_encoders.PopulateUIState(&ui);
        DOCTEST_CHECK(ui.GetNumVoices() == 3);
        for (size_t local = 0; local < Encoders::x_voicesPerTrio; ++local)
        {
            const int voice = static_cast<int>(trio * Encoders::x_voicesPerTrio + local);
            DOCTEST_CHECK(rig.m_encoders.GetValueNoSlew(Encoders::Param::Harmonics1, voice) == doctest::Approx(x_bases[trio]));
            DOCTEST_CHECK(rig.m_encoders.GetValue(Encoders::Param::Harmonics1, voice) == doctest::Approx(x_bases[trio]));
            DOCTEST_CHECK(ui.GetValue(address.x, address.y, local) == doctest::Approx(x_bases[trio]));
        }
    }
}

DOCTEST_TEST_CASE("encoder_trios: track changes preserve bank families and global selection")
{
    GlobalEnv::ResetPerTest();
    TrioEncoderRig rig;
    for (auto family : {Encoders::Bank::Source, Encoders::Bank::FilterAndAmp,
             Encoders::Bank::PanningAndSequencing, Encoders::Bank::VoiceLFOs})
    {
        rig.m_encoders.SelectBank(family);
        for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
        {
            rig.m_encoders.SetTrack(trio);
            DOCTEST_CHECK(rig.m_encoders.m_selectedBank == Encoders::BankForTrio(family, trio));
            DOCTEST_CHECK(rig.m_encoders.GetSelectedMode() == Encoders::VoiceModeForTrio(trio));
        }
    }

    rig.m_encoders.SelectBank(Encoders::Bank::TheoryOfTime);
    rig.m_encoders.SetTrack(1);
    DOCTEST_CHECK(rig.m_encoders.m_selectedBank == Encoders::Bank::TheoryOfTime);
    rig.m_encoders.SelectBank(Encoders::Bank::FilterAndAmp);
    DOCTEST_CHECK(rig.m_encoders.m_selectedBank == Encoders::Bank::FilterAndAmpFire);
}

DOCTEST_TEST_CASE("encoder_trios: modulation is independent across modes and local voices")
{
    GlobalEnv::ResetPerTest();
    TrioEncoderRig rig;
    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        Cell& cell = rig.Carrier(trio);
        cell.FillModulators(&rig.m_context);
        cell.GetModulator(0)->SetValue(1.0f, true);
        auto& source = rig.m_encoders.GetModulatorValues(Encoders::VoiceModeForTrio(trio));
        for (size_t local = 0; local < Encoders::x_voicesPerTrio; ++local)
        {
            source.m_value[0][local] = static_cast<float>(trio * 3 + local + 1) / 10.0f;
        }

        cell.SetStateRecursive();
        cell.SetModulatorsAffecting();
    }

    rig.m_encoders.Process();
    for (int voice = 0; voice < 9; ++voice)
    {
        DOCTEST_CHECK(rig.m_encoders.GetValueNoSlew(Encoders::Param::Harmonics1, voice) == doctest::Approx(static_cast<float>(voice + 1) / 10.0f));
    }
}

DOCTEST_TEST_CASE("encoder_trios: bank reset preserves sibling trios and inactive scenes")
{
    GlobalEnv::ResetPerTest();
    TrioEncoderRig rig;
    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        rig.SetCarrier(trio, 0.7f);
    }

    rig.m_encoders.SetTrack(1);
    rig.m_encoders.ResetBank(Encoders::Bank::Source);
    DOCTEST_CHECK(rig.Carrier(1).m_values[0] == rig.Carrier(1).m_defaultValue);
    DOCTEST_CHECK(rig.Carrier(1).m_values[2] == 0.7f);
    DOCTEST_CHECK(rig.Carrier(0).m_values[0] == 0.7f);
    DOCTEST_CHECK(rig.Carrier(2).m_values[0] == 0.7f);
    rig.m_encoders.RevertToDefault(true);
    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        for (float value : rig.Carrier(trio).m_values)
        {
            DOCTEST_CHECK(value == rig.Carrier(trio).m_defaultValue);
        }
    }
}

DOCTEST_TEST_CASE("encoder_trios: new patches round trip named flat scenes and gesture activation")
{
    GlobalEnv::ResetPerTest();
    TrioEncoderRig rig;
    const char* x_names[] = {"Harmonics1Water", "Harmonics1Fire", "Harmonics1Earth"};
    const float x_bases[] = {0.2f, 0.6f, 0.8f};
    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        rig.SetCarrier(trio, x_bases[trio]);
    }

    Cell& fire = rig.Carrier(1);
    fire.m_modulators.AddGesture(&fire, 2);
    auto& gesture = *fire.m_modulators.m_gestures[2];
    gesture.SetActive(true, 3);
    gesture.SetAndRecordValue(0.9f, 3);

    JsonArena arena(JsonArena::kDefaultCapacity);
    JSON saved = rig.m_encoders.ToJSON(arena);
    DOCTEST_CHECK(saved.Get("Harmonics1").IsNull());
    DOCTEST_CHECK_FALSE(saved.Get("ExternalClockLoop").IsNull());
    DOCTEST_CHECK(saved.Get("ExternalClockLoopFire").IsNull());
    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        JSON scenes = saved.Get(x_names[trio]).Get("values").Get("values");
        DOCTEST_CHECK(scenes.Size() == 8);
        DOCTEST_CHECK(scenes.GetAt(0).NumberValue() == doctest::Approx(x_bases[trio]));
    }

    JSON active = saved.Get("Harmonics1Fire").Get("gestures").GetAt(2).Get("active");
    DOCTEST_CHECK(active.Size() == 8);
    DOCTEST_CHECK(active.GetAt(3).BooleanValue());
    rig.m_encoders.RevertToDefault(true);
    rig.m_encoders.FromJSON(saved);
    rig.m_encoders.Process();
    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        DOCTEST_CHECK(rig.Carrier(trio).m_values[0] == doctest::Approx(x_bases[trio]));
        DOCTEST_CHECK(rig.Carrier(trio).GetValueNoSlew(0) == doctest::Approx(x_bases[trio]));
    }

    DOCTEST_REQUIRE(fire.m_modulators.m_gestures[2]);
    DOCTEST_CHECK(fire.m_modulators.m_gestures[2]->m_isActive[3]);
    DOCTEST_CHECK(fire.m_modulators.m_gestures[2]->m_values[3] == doctest::Approx(0.9f));
    DOCTEST_CHECK_FALSE(rig.Carrier(0).m_modulators.m_gestures[2]);
    DOCTEST_CHECK_FALSE(rig.Carrier(2).m_modulators.m_gestures[2]);
}

DOCTEST_TEST_CASE("encoder_trios: short numeric scene arrays clear omitted entries")
{
    GlobalEnv::ResetPerTest();
    TrioEncoderRig rig;
    rig.SetCarrier(1, 0.25f);
    JsonArena arena(1024);
    JSON values = arena.Loads(R"({"values":[1,0.75]})");
    rig.Carrier(1).StateEncoderCell::FromJSON(values);
    rig.Carrier(1).SetForceUpdateRecursive();
    rig.Carrier(1).Compute();
    DOCTEST_CHECK(rig.Carrier(1).m_values[0] == 1.0f);
    DOCTEST_CHECK(rig.Carrier(1).m_values[1] == 0.75f);
    for (size_t scene = 2; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
    {
        DOCTEST_CHECK(rig.Carrier(1).m_values[scene] == 0.0f);
    }

    DOCTEST_CHECK(rig.Carrier(1).GetValueNoSlew(0) == 1.0f);
}
