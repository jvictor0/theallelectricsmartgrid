#include "doctest.h"
#include "PhysicalModelingSource.hpp"
#include <cmath>

namespace
{

struct PhysicalEnvelopeRig
{
    TheoryOfTimeBase m_time;
    PhysicalModelingSource m_source;
    PhysicalModelingSource::Input m_input;

    PhysicalEnvelopeRig(float attackSeconds, float decaySeconds)
    {
        m_input.m_ahdInput.m_theoryOfTime = &m_time;
        m_input.SetAHD(
            std::log(attackSeconds / 0.0001f) / std::log(2.5f / 0.0001f),
            0.0f,
            std::log(decaySeconds / 0.0005f) / std::log(2.5f / 0.0005f),
            1.0f);
        SetClock(0.0);
        for (size_t block = 0; block < 128; ++block)
        {
            m_source.ProcessUBlock(m_input);
        }
    }

    void SetClock(double baseSample)
    {
        for (size_t i = 0; i < m_time.m_samples.size(); ++i)
        {
            double phase = 1.0 + (baseSample + static_cast<double>(i)) / 48000.0;
            m_time.m_samples[i].m_unmodulatedPhase = phase;
            m_time.m_samples[i].m_modulatedPhase = phase;
        }
    }

    void Trigger()
    {
        AHD::AHDControl control;
        control.m_trig = true;
        m_input.m_ahdInput.Set(control);
    }
};

}

DOCTEST_TEST_CASE("PhysicalModeling: consumes events once per microblock and accepts consecutive triggers")
{
    PhysicalEnvelopeRig rig(0.01f, 0.01f);
    rig.Trigger();
    rig.m_input.m_ahdInput.m_release = true;
    rig.m_source.ProcessUBlock(rig.m_input);
    DOCTEST_CHECK(rig.m_source.m_ahd.m_rawOutput == doctest::Approx(7.75f / 480.0f).epsilon(0.00001));
    DOCTEST_CHECK(rig.m_source.m_ahd.m_startOutput == doctest::Approx(0.0f));
    DOCTEST_CHECK_FALSE(rig.m_input.m_ahdInput.m_trig);
    DOCTEST_CHECK_FALSE(rig.m_input.m_ahdInput.m_release);
    DOCTEST_CHECK(rig.m_source.m_ahd.m_state == AHD::State::Running);

    rig.SetClock(8.0);
    rig.Trigger();
    rig.m_source.ProcessUBlock(rig.m_input);
    DOCTEST_CHECK(rig.m_source.m_ahd.m_startOutput == doctest::Approx(7.75f / 480.0f).epsilon(0.00001));
    DOCTEST_CHECK(rig.m_source.m_ahd.m_rawOutput == doctest::Approx(15.5f / 480.0f).epsilon(0.00001));
    DOCTEST_CHECK_FALSE(rig.m_input.m_ahdInput.m_trig);
}

DOCTEST_TEST_CASE("PhysicalModeling: explicit release advances at the base sample rate")
{
    PhysicalEnvelopeRig rig(0.01f, 0.01f);
    rig.m_source.m_ahd.m_state = AHD::State::Running;
    rig.m_source.m_ahd.m_rawOutput = 1.0f;
    rig.m_input.m_ahdInput.m_release = true;

    AHD base;
    base.m_state = AHD::State::Running;
    base.m_rawOutput = 1.0f;
    AHD::Input baseInput = rig.m_input.m_ahdInput;
    baseInput.m_releaseFactor = 1.0f;
    size_t releaseBlocks = 0;
    for (; releaseBlocks < 62 && rig.m_source.m_ahd.m_state != AHD::State::Idle; ++releaseBlocks)
    {
        rig.m_source.ProcessUBlock(rig.m_input);
        for (size_t sample = 0; sample < 8; ++sample)
        {
            base.Process(baseInput);
        }

        DOCTEST_CHECK(rig.m_source.m_ahd.m_rawOutput == doctest::Approx(base.m_rawOutput).epsilon(0.00001).scale(1.0));
        DOCTEST_CHECK_FALSE(rig.m_input.m_ahdInput.m_release);
    }

    DOCTEST_CHECK(releaseBlocks >= 60);
    DOCTEST_CHECK(releaseBlocks <= 61);
    DOCTEST_CHECK(rig.m_source.m_ahd.m_state == AHD::State::Idle);
    DOCTEST_CHECK(base.m_state == AHD::State::Idle);
}

DOCTEST_TEST_CASE("PhysicalModeling: running decay uses phase sample units without oversample scaling")
{
    PhysicalEnvelopeRig rig(0.0001f, 0.01f);
    rig.Trigger();
    rig.m_source.ProcessUBlock(rig.m_input);
    DOCTEST_CHECK(rig.m_source.m_ahd.m_rawOutput == doctest::Approx(1.0f - (7.75f - 4.8f) / 480.0f).epsilon(0.00001));
    rig.SetClock(56.0);
    rig.m_source.ProcessUBlock(rig.m_input);

    AHD base;
    base.m_state = AHD::State::Running;
    AHD::Input input = rig.m_input.m_ahdInput;
    base.Process(input);
    DOCTEST_CHECK(base.m_rawOutput == doctest::Approx(1.0f - (63.75f - 4.8f) / 480.0f).epsilon(0.00001));
    DOCTEST_CHECK(rig.m_source.m_ahd.m_rawOutput == doctest::Approx(base.m_rawOutput).epsilon(0.00001));
}

DOCTEST_TEST_CASE("PhysicalModeling: minimum attack is 100 microseconds and decay is 500 microseconds")
{
    PhysicalEnvelopeRig rig(0.0001f, 0.0005f);
    rig.Trigger();
    rig.m_source.ProcessUBlock(rig.m_input);
    DOCTEST_CHECK(rig.m_source.m_ahd.m_rawOutput == doctest::Approx(0.877083333f).epsilon(0.00001));
    rig.SetClock(8.0);
    rig.m_source.ProcessUBlock(rig.m_input);
    DOCTEST_CHECK(rig.m_source.m_ahd.m_state == AHD::State::Running);
    DOCTEST_CHECK(rig.m_source.m_ahd.m_rawOutput == doctest::Approx(0.54375f).epsilon(0.00001));
    rig.SetClock(24.0);
    rig.m_source.ProcessUBlock(rig.m_input);
    DOCTEST_CHECK(rig.m_source.m_ahd.m_state == AHD::State::Idle);
    DOCTEST_CHECK(rig.m_source.m_ahd.m_rawOutput == 0.0f);
}

DOCTEST_TEST_CASE("PhysicalModeling: UI morph response follows LP BP HP quarter-circle weights")
{
    PhysicalModelingSource::UIState ui;
    ui.m_mainSVFG.store(0.2f);
    ui.m_mainSVFK.store(1.0f);
    const float x_blends[] = {-0.2f, 0.0f, 0.25f, 0.499999f, 0.5f, 0.500001f, 0.75f, 1.0f, 1.2f};
    const float x_lp[] = {1.0f, 1.0f, 0.707106781f, 0.00000314159f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    const float x_bp[] = {0.0f, 0.0f, 0.707106781f, 1.0f, 1.0f, 1.0f, 0.707106781f, 0.0f, 0.0f};
    const float x_hp[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.00000314159f, 0.707106781f, 1.0f, 1.0f};
    for (float freq : {0.001f, 0.03f, 0.1f, 0.3f})
    {
        for (size_t i = 0; i < sizeof(x_blends) / sizeof(x_blends[0]); ++i)
        {
            ui.m_mainSVFMorph.store(x_blends[i]);
            auto expected = (
                x_lp[i] * LinearStateVariableFilter::LowPassTransferFunction(0.2f, 1.0f, freq) +
                x_bp[i] * LinearStateVariableFilter::BandPassTransferFunction(0.2f, 1.0f, freq) +
                x_hp[i] * LinearStateVariableFilter::HighPassTransferFunction(0.2f, 1.0f, freq)) *
                ui.m_combFilter.TransferFunctionValue(freq);
            DOCTEST_CHECK(std::abs(ui.TransferFunctionValue(freq) - expected) < 0.00001f);
        }
    }
}
