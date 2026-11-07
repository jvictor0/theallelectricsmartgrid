#include "doctest.h"
#include "SmartGridOneEncoders.hpp"
#include "../support/GlobalEnv.hpp"

namespace
{
using Motion = SmartGridOneEncoders::VisibleMotion;

struct MotionRig
{
    SmartGridOneContext m_context;
    EncoderBankBank m_banks;
    Motion m_motion;
    EncoderBankUIState m_ui;

    MotionRig()
        : m_banks(2, 1, 2, &m_context)
    {
        GlobalEnv::ResetPerTest();
        m_banks.InitMode(0, 4);
        for (size_t i = 0; i < 2; ++i)
        {
            m_banks.InitBank(i, 0, SmartGrid::Color::Red);
            m_banks.CreateEncoder(i, 0, i == 0 ? 0.25f : 0.5f,
                i == 0 ? "First" : "Second", "TEST", SmartGrid::Color::Red, 0, i == 1);
            m_banks.PlaceEncoder(i, i, 0, 0);
        }

        m_banks.SelectGrid(0);
        m_context.m_sceneManager.Process();
        m_banks.ProcessHostFrame();
    }

    void Publish()
    {
        m_banks.PopulateUIState(&m_ui);
        m_motion.PopulateUIState(m_banks, m_ui);
    }
};
}

DOCTEST_TEST_CASE("encoder motion: audio rate motion becomes a stable center and per-voice RMS spread")
{
    Motion::SlotMotion motion;
    motion.Seed(QuadFloat::Splat(0.5f));
    QuadFloat amplitude(0.0f, 0.1f, 0.2f, 0.3f);
    for (size_t sample = 0; sample < SampleTimer::x_hostSampleRate / 2; ++sample)
    {
        float wave = std::sin(2.0 * 3.14159265358979323846 * 1000.0 * sample / SampleTimer::x_hostSampleRate);
        motion.Process(QuadFloat::Splat(0.5f) + amplitude * wave);
    }

    for (size_t voice = 0; voice < 4; ++voice)
    {
        DOCTEST_CHECK(motion.m_center[voice] == doctest::Approx(0.5f).epsilon(0.008));
        DOCTEST_CHECK(std::sqrt(motion.m_energy[voice])
            == doctest::Approx(amplitude[voice] / std::sqrt(2.0f)).epsilon(0.004));
    }

    for (size_t sample = 0; sample < SampleTimer::x_hostSampleRate / 4; ++sample)
    {
        motion.Process(QuadFloat::Splat(0.5f));
    }

    for (size_t voice = 0; voice < 4; ++voice)
    {
        DOCTEST_CHECK(motion.m_center[voice] == doctest::Approx(0.5f).epsilon(0.0001));
        DOCTEST_CHECK(motion.m_energy[voice] < 0.000001f);
    }
}

DOCTEST_TEST_CASE("encoder motion: slow modulation follows the knob with little blur")
{
    Motion::SlotMotion motion;
    motion.Seed(QuadFloat::Splat(0.5f));
    float maxError = 0;
    float maxSpread = 0;
    for (size_t sample = 0; sample < SampleTimer::x_hostSampleRate * 2; ++sample)
    {
        float value = 0.5f + 0.3f * std::sin(2.0 * 3.14159265358979323846 * sample / SampleTimer::x_hostSampleRate);
        motion.Process(QuadFloat::Splat(value));
        maxError = std::max(maxError, std::abs(value - motion.m_center[0]));
        maxSpread = std::max(maxSpread, std::sqrt(motion.m_energy[0]));
    }

    DOCTEST_CHECK(maxError < 0.031f);
    DOCTEST_CHECK(maxSpread < 0.031f);
}

DOCTEST_TEST_CASE("encoder motion: page changes cold start and bipolar values remain display normalized")
{
    MotionRig rig;
    rig.m_motion.Process(rig.m_banks);
    rig.m_motion.m_slots[0].m_energy = QuadFloat::Splat(0.04f);
    rig.Publish();
    DOCTEST_CHECK(rig.m_ui.GetSpread(0, 0, 0) == doctest::Approx(0.2f));

    rig.m_banks.SelectGrid(1);
    rig.Publish();
    DOCTEST_CHECK(rig.m_ui.GetValue(0, 0, 0) == doctest::Approx(0.75f));
    DOCTEST_CHECK(rig.m_ui.GetSpread(0, 0, 0) == 0.0f);
    rig.m_motion.Process(rig.m_banks);
    rig.Publish();
    DOCTEST_CHECK(rig.m_ui.GetBipolar(0, 0));
    DOCTEST_CHECK(rig.m_ui.GetValue(0, 0, 0) == doctest::Approx(0.75f));
    DOCTEST_CHECK(rig.m_ui.GetSpread(0, 0, 0) == 0.0f);

    rig.m_motion.m_slots[0].m_energy = QuadFloat::Splat(0.01f);
    rig.Publish();
    DOCTEST_CHECK(rig.m_ui.GetSpread(0, 0, 0) == doctest::Approx(0.1f));
    rig.m_banks.m_banks[1].PlaceEncoder(0, 0, rig.m_banks.GetEncoder(0));
    rig.m_motion.Process(rig.m_banks);
    rig.Publish();
    DOCTEST_CHECK(rig.m_ui.GetValue(0, 0, 0) == doctest::Approx(0.25f));
    DOCTEST_CHECK(rig.m_ui.GetSpread(0, 0, 0) == 0.0f);

    rig.m_banks.m_banks[1].PlaceEncoder(0, 0, nullptr);
    rig.m_motion.Process(rig.m_banks);
    rig.Publish();
    DOCTEST_CHECK_FALSE(rig.m_ui.GetConnected(0, 0));
    for (size_t voice = 0; voice < 4; ++voice)
    {
        DOCTEST_CHECK(rig.m_ui.GetSpread(0, 0, voice) == 0.0f);
    }
}

DOCTEST_TEST_CASE("encoder motion: switches and disconnected controls never publish blur")
{
    MotionRig rig;
    rig.m_motion.Process(rig.m_banks);
    auto& cell = *rig.m_banks.GetEncoder(0);
    cell.m_switchValues = 4;
    cell.m_output = QuadFloat::Splat(1.0f);
    rig.m_motion.Process(rig.m_banks);
    rig.Publish();
    DOCTEST_CHECK(rig.m_ui.GetValue(0, 0, 0) == 1.0f);
    DOCTEST_CHECK(rig.m_ui.GetSpread(0, 0, 0) == 0.0f);
    cell.m_switchValues = 0;
    cell.m_connected = false;
    rig.m_motion.Process(rig.m_banks);
    rig.Publish();
    DOCTEST_CHECK_FALSE(rig.m_ui.GetConnected(0, 0));
    DOCTEST_CHECK(rig.m_ui.GetSpread(0, 0, 0) == 0.0f);
}

DOCTEST_TEST_CASE("encoder motion: SmartGridOne samples only on host frames and resets on patch load")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    SmartGridOneEncoders encoders(&context);
    encoders.SelectBank(SmartGridOneEncoders::Bank::Delay);
    context.m_sceneManager.Process();
    encoders.Process();
    auto& slots = encoders.m_visibleMotion.m_slots;
    auto slot = std::find_if(slots.begin(), slots.end(),
        [](const Motion::SlotMotion& value)
        {
            return value.m_initialized;
        });
    DOCTEST_REQUIRE(slot != slots.end());
    slot->m_center = QuadFloat::Splat(-1.0f);
    for (size_t sample = 1; sample < SampleTimer::x_oversample; ++sample)
    {
        SampleTimer::IncrementSample();
        encoders.Process();
        DOCTEST_REQUIRE(slot->m_center[0] == -1.0f);
    }

    SampleTimer::IncrementSample();
    context.m_sceneManager.Process();
    encoders.Process();
    DOCTEST_CHECK(slot->m_center[0] > -1.0f);

    JsonArena arena;
    JSON patch = encoders.ToJSON(arena);
    encoders.FromJSON(patch);
    DOCTEST_CHECK(encoders.m_visibleMotion.m_bank == -1);
    EncoderBankUIState ui;
    encoders.PopulateUIState(&ui);
    for (size_t x = 0; x < 4; ++x)
    {
        for (size_t y = 0; y < 4; ++y)
        {
            DOCTEST_CHECK(ui.GetSpread(x, y, 0) == 0.0f);
        }
    }
}

DOCTEST_TEST_CASE("encoder motion: display smoothing preserves immediate parameter readback")
{
    MotionRig rig;
    rig.m_motion.Process(rig.m_banks);
    rig.m_banks.GetEncoder(0)->m_output = QuadFloat::Splat(0.9f);
    rig.m_motion.Process(rig.m_banks);
    rig.Publish();
    DOCTEST_CHECK(rig.m_ui.GetValue(0, 0, 0) == doctest::Approx(0.9f));
    DOCTEST_CHECK(rig.m_ui.GetDisplayValue(0, 0, 0) > 0.25f);
    DOCTEST_CHECK(rig.m_ui.GetDisplayValue(0, 0, 0) < 0.3f);
    DOCTEST_CHECK(rig.m_ui.GetSpread(0, 0, 0) > 0.0f);
}
