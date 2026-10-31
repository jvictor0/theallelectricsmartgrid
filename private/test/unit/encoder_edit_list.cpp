#include "doctest.h"

#include <cmath>

#include "../support/SynthRig.hpp"

namespace
{
using Encoders = SmartGridOneEncoders;
using Bank = Encoders::Bank;

struct EditRig
{
    SmartGridOneContext m_context;
    Encoders m_encoders;
    SmartGrid::BankedEncoderCell* m_delay;

    EditRig()
        : m_context()
        , m_encoders(&m_context)
        , m_delay(nullptr)
    {
        m_encoders.SetTrack(0);
        m_encoders.SelectBank(Bank::Delay);
        m_delay = GetBank().GetBase(3, 3);
        m_delay->SetValue(0.25f, true);
        m_delay->SetForceUpdateRecursive();
        Run(SmartGrid::EncoderEditList::x_maxRampSamples * 2);
    }

    SmartGrid::EncoderBankInternal& GetBank()
    {
        return m_encoders.m_encoderBankBank.m_banks[static_cast<size_t>(Encoders::BankForTrio(Bank::Delay, 0))];
    }

    SmartGrid::EncoderEditList& List()
    {
        return m_encoders.m_encoderBankBank.m_editList;
    }

    void Run(size_t samples)
    {
        for (size_t i = 0; i < samples; ++i)
        {
            List().Process(m_context.m_sceneManager);
        }
    }
};

}

DOCTEST_TEST_CASE("EncoderEditList: an isolated absolute set ramps linearly over the longest ramp")
{
    GlobalEnv::ResetPerTest();
    EditRig rig;
    rig.m_encoders.Apply(SmartGrid::MessageIn::EncoderSetMsg(3, 3, 0.75f));
    DOCTEST_CHECK(rig.m_delay->m_values[0] == 0.25f);

    float target = SmartGrid::MessageIn::EncoderSetMsg(3, 3, 0.75f).SetValueFloat();
    float step = (target - 0.25f) / static_cast<float>(SmartGrid::EncoderEditList::x_maxRampSamples);
    float previous = rig.m_delay->m_values[0];
    for (size_t i = 0; i < SmartGrid::EncoderEditList::x_maxRampSamples; ++i)
    {
        rig.Run(1);
        float value = rig.m_delay->m_values[0];
        DOCTEST_REQUIRE(value - previous == doctest::Approx(step).epsilon(0.01));
        previous = value;
    }

    DOCTEST_CHECK(rig.m_delay->m_values[0] == doctest::Approx(target).epsilon(1e-5));
    DOCTEST_CHECK(rig.List().m_numEdits == 0);
}

DOCTEST_TEST_CASE("EncoderEditList: relative ticks ramp over the gap since the previous tick")
{
    GlobalEnv::ResetPerTest();
    EditRig rig;
    constexpr size_t x_gap = 480;
    float delta = static_cast<float>(SmartGrid::ExponentialEncoderDelta::Decode(100));
    SmartGrid::MessageIn tick(SmartGrid::MessageIn::Mode::EncoderExponentialDelta, 3, 3, 100);

    rig.m_encoders.Apply(tick);
    rig.Run(x_gap);
    rig.m_encoders.Apply(tick);
    DOCTEST_REQUIRE(rig.List().m_numEdits == 1);
    DOCTEST_CHECK(rig.List().m_edits[0].m_samplesLeft == x_gap);

    float before = rig.m_delay->m_values[0];
    rig.Run(1);
    float firstStep = rig.m_delay->m_values[0] - before;
    DOCTEST_CHECK(firstStep > 0);
    DOCTEST_CHECK(firstStep < 2 * delta / static_cast<float>(x_gap));

    rig.Run(x_gap);
    DOCTEST_CHECK(rig.List().m_numEdits == 0);
    DOCTEST_CHECK(rig.m_delay->m_values[0] == doctest::Approx(0.25f + 2 * delta).epsilon(1e-4));
}

DOCTEST_TEST_CASE("EncoderEditList: flush lands the remainder and destroyed cells drop out")
{
    GlobalEnv::ResetPerTest();
    EditRig rig;
    rig.m_encoders.Apply(SmartGrid::MessageIn::EncoderSetMsg(3, 3, 0.5f));
    rig.Run(10);
    rig.m_encoders.m_encoderBankBank.FlushEdits();
    DOCTEST_CHECK(rig.List().m_numEdits == 0);
    DOCTEST_CHECK(rig.m_delay->m_values[0] ==
        doctest::Approx(SmartGrid::MessageIn::EncoderSetMsg(3, 3, 0.5f).SetValueFloat()).epsilon(1e-5));

    rig.GetBank().MakeSelection(3, 3, rig.m_delay);
    rig.m_encoders.Apply(SmartGrid::MessageIn::EncoderSetMsg(0, 1, 0.75f));
    DOCTEST_REQUIRE(rig.List().m_numEdits == 1);
    SmartGrid::BankedEncoderCell* modulator = rig.List().m_edits[0].m_cell;
    DOCTEST_REQUIRE(modulator != nullptr);
    DOCTEST_CHECK(modulator->m_editSlot == &rig.List().m_edits[0].m_cell);

    rig.GetBank().Deselect();
    rig.m_delay->m_modulators.ClearAll();
    DOCTEST_CHECK(rig.List().m_edits[0].m_cell == nullptr);
    rig.Run(1);
    DOCTEST_CHECK(rig.List().m_numEdits == 0);
}
