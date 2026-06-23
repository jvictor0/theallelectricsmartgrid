#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "../support/SynthRig.hpp"

#include "SceneManager.hpp"
#include "SmartGridOneEncoders.hpp"
#include "TheNonagonSquiggleBoy.hpp"

namespace
{

void SetExternalClockLoopSwitch(TheNonagonSquiggleBoyInternal& system, int switchVal)
{
    using Param = SmartGridOneEncoders::Param;

    float value = static_cast<float>(switchVal) /
        static_cast<float>(TheoryOfTimeBase::x_numLoops - 1);
    SmartGrid::BankedEncoderCell* cell =
        system.m_squiggleBoy.m_encoders.m_encoderBankBank.GetEncoder(static_cast<size_t>(Param::ExternalClockLoop));

    DOCTEST_REQUIRE(cell != nullptr);
    cell->SetValueAllScenesAllTracks(value);
    cell->InitSlewState(value);
    for (size_t i = 0; i < 16; ++i)
    {
        cell->m_output[i] = value;
    }
}

}

DOCTEST_TEST_CASE("External clock loop selector publishes six switch values")
{
    using Param = SmartGridOneEncoders::Param;

    SmartGridOneEncoders::ParamSwitch paramSwitch = SmartGridOneEncoders::GetParamSwitch(Param::ExternalClockLoop);

    DOCTEST_CHECK(paramSwitch.IsSwitch());
    DOCTEST_CHECK(paramSwitch.m_numValues == TheoryOfTimeBase::x_numLoops);
}

DOCTEST_TEST_CASE("External clock loop selector maps fully counterclockwise to master")
{
    DOCTEST_CHECK(TheNonagonSquiggleBoyInternal::ExternalClockLoopIndexFromSwitch(0) == TheoryOfTimeBase::x_masterLoop);
    DOCTEST_CHECK(TheNonagonSquiggleBoyInternal::ExternalClockLoopIndexFromSwitch(3) == TheoryOfTimeBase::x_numLoops - 3 - 1);
    DOCTEST_CHECK(TheNonagonSquiggleBoyInternal::ExternalClockLoopIndexFromSwitch(TheoryOfTimeBase::x_numLoops - 1) == 0);
}

DOCTEST_TEST_CASE("External clock loop selector defaults to upper middle switch")
{
    using Param = SmartGridOneEncoders::Param;

    GlobalEnv::ResetPerTest();

    SmartGrid::SceneManager sceneManager;
    SmartGridOneEncoders encoders;
    encoders.Init(&sceneManager, 3, 3);
    encoders.Process();

    DOCTEST_CHECK(encoders.GetSwitchVal(Param::ExternalClockLoop) == 3);
    DOCTEST_CHECK(TheNonagonSquiggleBoyInternal::ExternalClockLoopIndexFromSwitch(encoders.GetSwitchVal(Param::ExternalClockLoop)) == TheoryOfTimeBase::x_numLoops - 3 - 1);
}

DOCTEST_TEST_CASE("External clock loop selector survives patch round trip")
{
    synthrig::SynthRig rig;
    TheNonagonSquiggleBoyInternal& system = rig.Internal();

    SetExternalClockLoopSwitch(system, 5);
    rig.RunFrames(2);

    DOCTEST_REQUIRE(system.m_squiggleBoy.m_encoders.GetSwitchVal(SmartGridOneEncoders::Param::ExternalClockLoop) == 5);

    std::string patch = rig.SavePatch();
    DOCTEST_REQUIRE_FALSE(patch.empty());

    rig.ResetToDefaults();
    rig.RunFrames(2);
    DOCTEST_REQUIRE(system.m_squiggleBoy.m_encoders.GetSwitchVal(SmartGridOneEncoders::Param::ExternalClockLoop) == 3);

    DOCTEST_REQUIRE(rig.LoadPatch(patch));
    rig.RunFrames(2);

    DOCTEST_CHECK(system.m_squiggleBoy.m_encoders.GetSwitchVal(SmartGridOneEncoders::Param::ExternalClockLoop) == 5);
    DOCTEST_CHECK(system.ExternalClockLoopIndex() == 0);
}
