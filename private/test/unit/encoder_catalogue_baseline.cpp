#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "SmartGridOneEncoders.hpp"
#include <array>
#include <string>

namespace
{

using Encoders = SmartGridOneEncoders;
using Bank = Encoders::Bank;
using Mode = Encoders::BankMode;
using Param = Encoders::Param;

struct ExpectedParam
{
    const char* m_name;
    const char* m_shortName;
    Bank m_bank;
    int m_x;
    int m_y;
    float m_default;
    const char* m_description;
    SmartGrid::Color m_color;
    uint8_t m_sourceMask;
    uint8_t m_filterMask;
    int m_switchValues;
    bool m_bipolar;
};

const ExpectedParam x_parameters[] =
{
#define BASELINE_PARAM(name, shortName, bank, x, y, defaultValue, description, color, sourceMask, filterMask, switchValues, bipolar) \
    {#name, #shortName, Bank::bank, x, y, defaultValue, description, color, sourceMask, filterMask, switchValues, bipolar},
#include "../fixtures/SmartGridOneParamBaseline.inc"
#undef BASELINE_PARAM
};

const std::array<SmartGrid::Color, 20> x_bankColors =
{{
    SmartGrid::Color::Red, SmartGrid::Color::Red, SmartGrid::Color::Red,
    SmartGrid::Color::Green, SmartGrid::Color::Green, SmartGrid::Color::Green,
    SmartGrid::Color::Orange, SmartGrid::Color::Orange, SmartGrid::Color::Orange,
    SmartGrid::Color::Blue, SmartGrid::Color::Blue, SmartGrid::Color::Blue,
    SmartGrid::Color::Pink, SmartGrid::Color::Fuscia, SmartGrid::Color::Cyan,
    SmartGrid::Color::DarkPurple, SmartGrid::Color::Yellow, SmartGrid::Color::SeaGreen,
    SmartGrid::Color::White, SmartGrid::Color::Ocean
}};

std::string ExpectedName(const ExpectedParam& expected, size_t trio)
{
    std::string name(expected.m_name);
    if (Encoders::IsVoiceBank(expected.m_bank))
    {
        const char* x_suffixes[] = {"Water", "Fire", "Earth"};
        name += x_suffixes[trio];
    }

    return name;
}

bool Applies(const ExpectedParam& expected, int source, int filter)
{
    return (expected.m_sourceMask & (1 << source)) != 0 &&
        (expected.m_filterMask & (1 << filter)) != 0;
}

const ExpectedParam* ExpectedAt(Bank bank, int x, int y, int source, int filter)
{
    const ExpectedParam* result = nullptr;
    for (const ExpectedParam& expected : x_parameters)
    {
        for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
        {
            if (Encoders::BankForTrio(expected.m_bank, trio) == bank &&
                expected.m_x == x && expected.m_y == y &&
                (!Encoders::IsVoiceBank(bank) || Applies(expected, source, filter)))
            {
                result = &expected;
            }
        }
    }

    return result;
}

}

DOCTEST_TEST_CASE("encoder catalogue baseline: construction metadata")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    Encoders encoders(&context);
    DOCTEST_REQUIRE(std::size(x_parameters) == 176);
    DOCTEST_REQUIRE(Encoders::x_numParams == std::size(x_parameters));
    DOCTEST_CHECK(encoders.m_encoderBankBank.m_numEncoders == 176 * 3);
    DOCTEST_CHECK(encoders.m_encoderBankBank.m_numBanks == x_bankColors.size());
    DOCTEST_CHECK(encoders.m_encoderBankBank.m_numModes == 5);

    const size_t x_modeChannels[] = {3, 3, 3, 4, 1};
    for (size_t mode = 0; mode < std::size(x_modeChannels); ++mode)
    {
        DOCTEST_CHECK(encoders.m_encoderBankBank.m_bankModes[mode].m_numVoices == x_modeChannels[mode]);
    }

    for (size_t bank = 0; bank < x_bankColors.size(); ++bank)
    {
        auto expectedMode = bank < 12 ? static_cast<Mode>(bank % 3) :
            (bank < 16 ? Mode::Quad : Mode::Global);
        DOCTEST_CHECK(encoders.m_encoderBankBank.m_bankConfigs[bank].m_modeIx == static_cast<size_t>(expectedMode));
        DOCTEST_CHECK(encoders.m_encoderBankBank.m_bankConfigs[bank].m_color == x_bankColors[bank]);
    }

    for (size_t i = 0; i < std::size(x_parameters); ++i)
    {
        const ExpectedParam& expected = x_parameters[i];
        Param param = static_cast<Param>(i);
        auto address = encoders.GetParamAddress(param);
        DOCTEST_INFO(expected.m_name);
        DOCTEST_CHECK(address.bank == expected.m_bank);
        DOCTEST_CHECK(address.x == expected.m_x);
        DOCTEST_CHECK(address.y == expected.m_y);
        DOCTEST_CHECK(Encoders::GetParamSwitch(param).m_numValues == expected.m_switchValues);
        for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
        {
            auto* cell = encoders.m_encoderBankBank.GetEncoder(encoders.EncoderIndex(param, trio));
            if (trio != 0 && !Encoders::IsVoiceBank(expected.m_bank))
            {
                DOCTEST_CHECK(cell == nullptr);
                continue;
            }

            DOCTEST_REQUIRE(cell != nullptr);
            DOCTEST_CHECK(std::string(cell->m_name) == ExpectedName(expected, trio));
            DOCTEST_CHECK(std::string(cell->m_shortName) == expected.m_shortName);
            DOCTEST_CHECK(cell->m_defaultValue == doctest::Approx(expected.m_default));
            DOCTEST_CHECK(cell->m_switchValues == expected.m_switchValues);
            DOCTEST_CHECK(cell->m_bipolar == expected.m_bipolar);
            DOCTEST_CHECK(cell->m_color == expected.m_color);
            size_t channels = Encoders::IsVoiceBank(expected.m_bank) ? 3 :
                (static_cast<size_t>(expected.m_bank) < 16 ? 4 : 1);
            DOCTEST_CHECK(cell->m_sharedEncoderState->m_numVoices == channels);
        }
    }
}

DOCTEST_TEST_CASE("encoder catalogue baseline: every machine and trio grid")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    Encoders encoders(&context);
    for (int source = 0; source < 4; ++source)
    {
        for (int filter = 0; filter < 2; ++filter)
        {
            for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
            {
                encoders.SetTrack(trio);
                encoders.UpdateEncodersForMachine(
                    static_cast<VoiceMachine::SourceMachine>(source),
                    static_cast<VoiceMachine::FilterMachine>(filter));
            }

            for (int bank = 0; bank < 20; ++bank)
            {
                for (int y = 0; y < 4; ++y)
                {
                    for (int x = 0; x < 4; ++x)
                    {
                        auto* expected = ExpectedAt(static_cast<Bank>(bank), x, y, source, filter);
                        auto* actual = encoders.m_encoderBankBank.m_banks[bank].GetBase(x, y);
                        DOCTEST_INFO("source=", source, " filter=", filter,
                            " bank=", bank, " x=", x, " y=", y);
                        DOCTEST_CHECK((actual != nullptr) == (expected != nullptr));
                        if (actual && expected)
                        {
                            size_t cellTrio = bank < 12 ? static_cast<size_t>(bank % 3) : 0;
                            DOCTEST_CHECK(std::string(actual->m_name) == ExpectedName(*expected, cellTrio));
                            DOCTEST_CHECK(actual->m_color == expected->m_color);
                        }
                    }
                }
            }
        }
    }
}
