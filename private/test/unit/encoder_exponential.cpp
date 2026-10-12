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
        : m_banks(1, 1, 1, &m_context)
    {
        m_banks.InitMode(0, 1);
        m_banks.InitBank(0, 0, Color::Red);
        m_banks.CreateEncoder(0, 0, 0.5f, "Test", "TEST", Color::Red, 5, false);
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
        DOCTEST_CHECK(rig.Cell().m_values[0] == 0.5f + 1.0f / 65536.0f);
        DOCTEST_CHECK(rig.Cell().m_lastTimestamp == 99);
        DOCTEST_CHECK(rig.Cell().m_lastSpeed == EncoderCell::x_maxSpeed);
        DOCTEST_CHECK(rig.Cell().m_lastDeltaSign == -1);

        rig.Turn(127, 100 + interval * 3);
        DOCTEST_CHECK(rig.Cell().m_values[0] == doctest::Approx(0.5394239164768812).epsilon(1e-6));
        rig.Turn(0, 100 + interval * 4);
        DOCTEST_CHECK(rig.Cell().m_values[0] == doctest::Approx(0.4964332117452942).epsilon(1e-6));
    }
}

DOCTEST_TEST_CASE("exponential encoder: clamping and legacy increments retain existing behavior")
{
    ExponentialEncoderRig rig;
    rig.Cell().SetValue(0.99f, true);
    rig.Turn(127, 1);
    DOCTEST_CHECK(rig.Cell().m_values[0] == 1.0f);
    rig.Cell().SetValue(0.01f, true);
    rig.Turn(0, 2);
    DOCTEST_CHECK(rig.Cell().m_values[0] == 0.0f);

    rig.m_banks.m_banks[0].Apply(WrldBLDRMidi::FromMidi(BasicMidi::CC(1000, -1, 0, 0, 65)));
    DOCTEST_CHECK(rig.Cell().m_values[0] == doctest::Approx(EncoderCell::x_minSpeed));
}

DOCTEST_TEST_CASE("exponential encoder: ignored values do not activate gestures and valid edits do")
{
    ExponentialEncoderRig rig;
    auto& sources = rig.m_banks.m_bankModes[0].m_modulatorValues;
    sources.m_selectedGestures.Set(0, true);
    sources.m_gestureWeights[0] = 0.5f;
    rig.Turn(64, 1);
    for (int value : {-1, 64, 128})
    {
        rig.m_banks.m_banks[0].Apply(MessageIn(MessageIn::Mode::EncoderExponentialDelta, 0, 0, value));
    }

    DOCTEST_CHECK_FALSE(rig.Cell().m_modulators.m_gestures[0]);
    DOCTEST_CHECK(rig.Cell().m_values[0] == 0.5f);
    rig.Turn(127, 2);
    DOCTEST_REQUIRE(rig.Cell().m_modulators.m_gestures[0]);
    DOCTEST_CHECK(rig.Cell().m_values[0] == doctest::Approx(0.5197043288439094).epsilon(1e-6));
    DOCTEST_CHECK(rig.Cell().m_modulators.m_gestures[0]->m_values[0]
        == doctest::Approx(0.5197043288439094).epsilon(1e-6));
}
