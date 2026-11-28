#include "doctest.h"

#include <cmath>

#include "../support/GlobalEnv.hpp"
#include "EncoderBankBank.hpp"
#include "ExponentialEncoderDelta.hpp"
#include "InteriorGrid.hpp"
#include "WrldBLDRMidi.hpp"

namespace
{
using namespace SmartGrid;

struct ExponentialEncoderRig
{
    SmartGridOneContext m_context;
    EncoderBankBank m_banks;

    ExponentialEncoderRig()
        : m_banks(1, 1, 1, 15, 1, &m_context)
    {
        m_banks.InitMode(0, 1);
        m_banks.InitBank(0, 0, Color::Red);
        m_banks.CreateEncoder(0, 0, 0, 0.5f, "Test", "TEST", Color::Red, 5, false);
        m_banks.PlaceEncoder(0, 0, 0, 0);
    }

    BankedEncoderCell& Cell()
    {
        return *m_banks.GetEncoder(0);
    }

    void Turn(uint8_t value, size_t timestamp)
    {
        m_banks.m_banks[0].Apply(WrldBLDRMidi::FromMidi(BasicMidi::CC(timestamp, -1, 8, 0, value)));
    }
};
}

DOCTEST_TEST_CASE("exponential encoder: decoder matches firmware vectors and every MIDI value")
{
    struct Vector
    {
        uint8_t m_value;
        double m_delta;
    };

    // Shared exponential-relative-v1 vectors from the YTX firmware handoff.
    //
    const Vector x_vectors[] =
    {
        {0, -0.04299070473158695},
        {1, -0.0394086576878187},
        {32, -0.002528864984210997},
        {56, -0.00016859099894739983},
        {63, -1.52587890625e-05},
        {64, 0.0},
        {65, 1.52587890625e-05},
        {72, 0.00016859099894739983},
        {96, 0.002528864984210997},
        {127, 0.0394086576878187}
    };

    for (const auto& vector : x_vectors)
    {
        DOCTEST_CHECK(std::abs(ExponentialEncoderDelta::Decode(vector.m_value) - vector.m_delta) < 1e-15);
    }

    for (int value = 0; value < 128; ++value)
    {
        double expected = 0.0;
        double term = 1.0 / 65536.0;
        for (int i = 0; i < std::abs(value - 64); ++i)
        {
            expected += term;
            term *= std::pow(2.0, 1.0 / 8.0);
        }

        if (value < 64)
        {
            expected = -expected;
        }

        DOCTEST_CHECK(std::abs(ExponentialEncoderDelta::Decode(static_cast<uint8_t>(value)) - expected) < 1e-15);
    }
}

DOCTEST_TEST_CASE("exponential encoder: WRLD routing validates the channel CC and value")
{
    for (int cc = 0; cc < 16; ++cc)
    {
        for (int value = 0; value < 128; ++value)
        {
            MessageIn msg = WrldBLDRMidi::FromMidi(BasicMidi::CC(1234, -1, 8,
                static_cast<uint8_t>(cc), static_cast<uint8_t>(value)));
            if (value == 64)
            {
                DOCTEST_CHECK(msg.NoMessage());
                continue;
            }

            DOCTEST_CHECK(msg.m_mode == MessageIn::Mode::EncoderExponentialDelta);
            DOCTEST_CHECK(msg.m_routeId == static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::Encoder));
            DOCTEST_CHECK(msg.m_timestamp == 1234);
            DOCTEST_CHECK(msg.m_x == cc % 4);
            DOCTEST_CHECK(msg.m_y == cc / 4);
            DOCTEST_CHECK(msg.m_amount == value);
        }
    }

    DOCTEST_CHECK(WrldBLDRMidi::FromMidi(BasicMidi::CC(0, -1, 8, 16, 65)).NoMessage());
    DOCTEST_CHECK(WrldBLDRMidi::FromMidi(BasicMidi::CC(0, -1, 8, 127, 65)).NoMessage());
    DOCTEST_CHECK(WrldBLDRMidi::FromMidi(BasicMidi::CC(0, -1, 8, 0, 128)).NoMessage());
    DOCTEST_CHECK(WrldBLDRMidi::FromMidi(BasicMidi::Note(0, -1, 8, 0, 65)).NoMessage());
    DOCTEST_CHECK(WrldBLDRMidi::FromMidi(BasicMidi::CC(0, -1, 9, 0, 65)).NoMessage());
    DOCTEST_CHECK(EncoderMidi::FromMidi(BasicMidi::CC(0, 4, 8, 0, 65)).NoMessage());

    const MessageIn legacy = WrldBLDRMidi::FromMidi(BasicMidi::CC(1234, -1, 0, 5, 63));
    DOCTEST_CHECK(legacy.m_mode == MessageIn::Mode::EncoderIncDec);
    DOCTEST_CHECK(legacy.m_amount == -1);
    DOCTEST_CHECK(legacy.m_x == 1);
    DOCTEST_CHECK(legacy.m_y == 1);
    DOCTEST_CHECK(WrldBLDRMidi::FromMidi(BasicMidi::CC(0, -1, 1, 0, 127)).m_mode == MessageIn::Mode::EncoderPush);
    DOCTEST_CHECK(WrldBLDRMidi::FromMidi(BasicMidi::CC(0, -1, 1, 0, 0)).m_mode == MessageIn::Mode::EncoderRelease);
}

DOCTEST_TEST_CASE("exponential encoder: movement ignores arrival timing and retains fine selector movement")
{
    for (size_t interval : {size_t(0), size_t(1000), size_t(500000)})
    {
        ExponentialEncoderRig rig;
        rig.Cell().m_lastTimestamp = 99;
        rig.Cell().m_lastSpeed = EncoderCell::x_maxSpeed;
        rig.Cell().m_lastDeltaSign = -1;
        rig.Turn(65, 100);
        rig.Turn(65, 100 + interval);
        rig.Turn(63, 100 + interval * 2);
        DOCTEST_CHECK(rig.Cell().m_targets[0] == 0.5f + 1.0f / 65536.0f);
        DOCTEST_CHECK(rig.Cell().m_lastTimestamp == 99);
        DOCTEST_CHECK(rig.Cell().m_lastSpeed == EncoderCell::x_maxSpeed);
        DOCTEST_CHECK(rig.Cell().m_lastDeltaSign == -1);

        rig.Turn(127, 100 + interval * 3);
        DOCTEST_CHECK(rig.Cell().m_targets[0] == doctest::Approx(0.5394239164768812).epsilon(1e-6));
        rig.Turn(0, 100 + interval * 4);
        DOCTEST_CHECK(rig.Cell().m_targets[0] == doctest::Approx(0.4964332117452942).epsilon(1e-6));
    }
}

DOCTEST_TEST_CASE("exponential encoder: clamping and legacy increments retain existing behavior")
{
    ExponentialEncoderRig rig;
    rig.Cell().SetValue(0.99f, true);
    rig.Turn(127, 1);
    DOCTEST_CHECK(rig.Cell().m_targets[0] == 1.0f);
    rig.Cell().SetValue(0.01f, true);
    rig.Turn(0, 2);
    DOCTEST_CHECK(rig.Cell().m_targets[0] == 0.0f);

    rig.m_banks.m_banks[0].Apply(WrldBLDRMidi::FromMidi(BasicMidi::CC(1000, -1, 0, 0, 65)));
    DOCTEST_CHECK(rig.Cell().m_targets[0] == doctest::Approx(EncoderCell::x_minSpeed));
}

DOCTEST_TEST_CASE("encoder shift-turn: exponential MIDI adjusts descendants and leaves the parent unchanged")
{
    GlobalEnv::ResetPerTest();
    ExponentialEncoderRig rig;
    auto& sources = rig.m_banks.GetModulatorValues(0);
    sources.SetModulatorSkin(0, Color::Red, nullptr);
    sources.SetModulatorSkin(1, Color::Blue, nullptr);
    rig.Cell().FillModulators(&rig.m_context);
    auto& positive = *rig.Cell().m_modulators.m_modulators[0];
    auto& negative = *rig.Cell().m_modulators.m_modulators[1];
    positive.SetValue(0.75f, true);
    negative.SetValue(0.25f, true);
    rig.Cell().SetStateRecursive();
    rig.Cell().SetModulatorsAffecting();
    rig.m_banks.Process();
    rig.m_context.m_sceneManager.m_shift = true;

    rig.Turn(0, 1000);
    DOCTEST_CHECK(positive.m_targets[0] == doctest::Approx(0.7070092952684131).epsilon(1e-6));
    DOCTEST_CHECK(negative.m_targets[0] == doctest::Approx(0.29299070473158695).epsilon(1e-6));
    rig.Turn(127, 2000);
    DOCTEST_CHECK(positive.m_targets[0] == doctest::Approx(0.7464179529562318).epsilon(1e-6));
    DOCTEST_CHECK(negative.m_targets[0] == doctest::Approx(0.25358204704376825).epsilon(1e-6));
    DOCTEST_CHECK(rig.Cell().m_targets[0] == 0.5f);
    DOCTEST_CHECK(positive.m_targets[1] == 0.75f);
    DOCTEST_CHECK(negative.m_targets[1] == 0.25f);
    DOCTEST_CHECK(rig.Cell().m_modulators.m_modulators[2]->m_targets[0] == 0.5f);
}

DOCTEST_TEST_CASE("encoder fine tune scales exponential and legacy movement before routing")
{
    ExponentialEncoderRig rig;
    auto& scene = rig.m_context.m_sceneManager;
    const float delta = static_cast<float>(ExponentialEncoderDelta::Decode(127));
    scene.m_fineTune = true;
    rig.Turn(127, 1);
    DOCTEST_CHECK(rig.Cell().m_targets[0] == doctest::Approx(0.5f + delta / 100.0f).epsilon(1e-7));

    rig.Cell().SetValue(0.5f, true);
    rig.m_banks.m_banks[0].Apply(WrldBLDRMidi::FromMidi(BasicMidi::CC(1000, -1, 0, 0, 65)));
    DOCTEST_CHECK(rig.Cell().m_targets[0] == doctest::Approx(0.5f + EncoderCell::x_minSpeed / 100.0f).epsilon(1e-7));
    DOCTEST_CHECK(rig.Cell().m_lastSpeed == EncoderCell::x_minSpeed);

    rig.m_banks.GetModulatorValues(0).SetModulatorSkin(0, Color::Red, nullptr);
    auto* child = rig.Cell().m_modulators.GetOrAddModulator(&rig.Cell(), &rig.m_context, 0);
    child->SetValue(0.25f, true);
    scene.m_shift = true;
    rig.Turn(127, 2);
    DOCTEST_CHECK(child->m_targets[0] == doctest::Approx(0.25f - delta / 100.0f).epsilon(1e-7));

    scene.m_fineTune = false;
    rig.Turn(127, 3);
    DOCTEST_CHECK(child->m_targets[0] == doctest::Approx(0.25f - delta / 100.0f - delta).epsilon(1e-7));
}

DOCTEST_TEST_CASE("encoder Spread creates a signed depth and keeps it when crossing zero")
{
    ExponentialEncoderRig rig;
    auto& scene = rig.m_context.m_sceneManager;
    auto& modulators = rig.Cell().m_modulators;
    constexpr size_t x_spread = BankedEncoderCell::x_spreadModulator;
    constexpr float x_delta = 1.0f / 65536.0f;
    scene.m_spread = true;
    rig.Turn(65, 1);
    DOCTEST_CHECK(modulators.m_numActiveModulators == 0);

    rig.m_banks.GetModulatorValues(0).SetModulatorSkin(x_spread, Color::Grey, "SPRD");
    rig.Turn(65, 2);
    auto* spread = modulators.m_modulators[x_spread].get();
    DOCTEST_REQUIRE(spread);
    DOCTEST_CHECK(modulators.m_numActiveModulators == 1);
    DOCTEST_CHECK(spread->m_targets[0] == 0.5f + x_delta);
    rig.Turn(63, 3);
    DOCTEST_CHECK(modulators.m_modulators[x_spread].get() == spread);
    DOCTEST_CHECK(spread->m_targets[0] == 0.5f);
    rig.Turn(63, 4);
    DOCTEST_CHECK(spread->m_targets[0] == 0.5f - x_delta);
    rig.Turn(65, 5);
    rig.Turn(65, 6);
    DOCTEST_CHECK(spread->m_targets[0] == 0.5f + x_delta);

    scene.m_fineTune = true;
    rig.Turn(127, 7);
    const float fineDelta = static_cast<float>(ExponentialEncoderDelta::Decode(127)) / 100.0f;
    DOCTEST_CHECK(spread->m_targets[0] == doctest::Approx(0.5f + x_delta + fineDelta).epsilon(1e-7));
    DOCTEST_CHECK(spread->m_targets[1] == 0.5f);
    DOCTEST_CHECK(rig.Cell().m_targets[0] == 0.5f);
}

DOCTEST_TEST_CASE("encoder Shift plus Spread adds fine signed depths to used modulator routes")
{
    ExponentialEncoderRig rig;
    auto& scene = rig.m_context.m_sceneManager;
    auto& sources = rig.m_banks.GetModulatorValues(0);
    constexpr size_t x_spread = BankedEncoderCell::x_spreadModulator;
    sources.SetModulatorSkin(0, Color::Red, nullptr);
    sources.SetModulatorSkin(1, Color::Blue, nullptr);
    sources.SetModulatorSkin(x_spread, Color::Grey, "SPRD");
    rig.Cell().FillModulators(&rig.m_context);
    auto& slots = rig.Cell().m_modulators.m_modulators;
    slots[0]->SetValue(0.75f, true);
    slots[x_spread]->SetValue(0.8f, true);
    auto* nested = slots[0]->m_modulators.GetOrAddModulator(slots[0].get(), &rig.m_context, 1);
    nested->SetValue(0.25f, true);
    auto* underSpread = slots[x_spread]->m_modulators.GetOrAddModulator(slots[x_spread].get(), &rig.m_context, 1);
    underSpread->SetValue(0.25f, true);

    scene.m_shift = true;
    scene.m_spread = true;
    scene.m_fineTune = true;
    rig.Turn(0, 1);
    const float expected = 0.5f + static_cast<float>(ExponentialEncoderDelta::Decode(0)) / 100.0f;
    for (auto* cell : {slots[0].get(), nested, underSpread})
    {
        const auto& spread = cell->m_modulators.m_modulators[x_spread];
        DOCTEST_REQUIRE(spread);
        DOCTEST_CHECK(spread->m_targets[0] == doctest::Approx(expected).epsilon(1e-7));
        DOCTEST_CHECK(spread->m_modulators.m_numActiveModulators == 0);
    }

    DOCTEST_CHECK_FALSE(slots[1]->m_modulators.m_modulators[x_spread]);
    DOCTEST_CHECK_FALSE(slots[x_spread]->m_modulators.m_modulators[x_spread]);
    DOCTEST_CHECK(slots[x_spread]->m_targets[0] == 0.8f);
    DOCTEST_CHECK(slots[0]->m_targets[0] == 0.75f);
    DOCTEST_CHECK(nested->m_targets[0] == 0.25f);
    DOCTEST_CHECK(rig.Cell().m_targets[0] == 0.5f);
}

DOCTEST_TEST_CASE("encoder Shift plus Spread ignores routes only browsed in the picker")
{
    ExponentialEncoderRig rig;
    auto& sources = rig.m_banks.GetModulatorValues(0);
    constexpr size_t x_spread = BankedEncoderCell::x_spreadModulator;
    sources.SetModulatorSkin(0, Color::Red, nullptr);
    sources.SetModulatorSkin(x_spread, Color::Grey, "SPRD");
    auto& bank = rig.m_banks.m_banks[0];
    bank.HandlePress(0, 0);
    bank.HandlePress(0, 0);
    bank.HandlePress(0, 0);
    bank.HandlePress(3, 3);

    const auto& route = rig.Cell().m_modulators.m_modulators[0];
    DOCTEST_REQUIRE(route);
    const auto& spread = route->m_modulators.m_modulators[x_spread];
    DOCTEST_REQUIRE(spread);
    DOCTEST_CHECK(spread->m_targets[0] == 0.5f);

    auto& scene = rig.m_context.m_sceneManager;
    scene.m_shift = true;
    scene.m_spread = true;
    rig.Turn(127, 1);
    DOCTEST_CHECK(spread->m_targets[0] == 0.5f);

    const auto& nested = route->m_modulators.m_modulators[0];
    DOCTEST_REQUIRE(nested);
    nested->SetValue(0.75f, true);
    rig.Turn(127, 2);
    DOCTEST_CHECK(spread->m_targets[0] > 0.5f);
    DOCTEST_CHECK(route->m_targets[0] == 0.5f);
    DOCTEST_CHECK(nested->m_targets[0] == 0.75f);
}

DOCTEST_TEST_CASE("exponential encoder: ignored values do not activate gestures and valid edits do")
{
    ExponentialEncoderRig rig;
    auto& gestures = rig.m_banks.GetGestureValues();
    gestures.m_selectedGestures.Set(0, true);
    gestures.m_gestureWeights[0] = 0.5f;
    rig.Turn(64, 1);
    for (int value : {-1, 64, 128})
    {
        rig.m_banks.m_banks[0].Apply(MessageIn(MessageIn::Mode::EncoderExponentialDelta, 0, 0, value));
    }

    DOCTEST_CHECK_FALSE(rig.Cell().m_modulators.m_gestures[0]);
    DOCTEST_CHECK(rig.Cell().m_targets[0] == 0.5f);
    rig.Turn(127, 2);
    DOCTEST_REQUIRE(rig.Cell().m_modulators.m_gestures[0]);
    DOCTEST_CHECK(rig.Cell().m_targets[0] == doctest::Approx(0.5197043288439094).epsilon(1e-6));
    DOCTEST_CHECK(rig.Cell().m_modulators.m_gestures[0]->m_targets[0]
        == doctest::Approx(0.5197043288439094).epsilon(1e-6));
}
