#include "doctest.h"
#include "PhysicalModelingSource.hpp"
#include <cmath>
#include <memory>

namespace
{

struct PhysicalEnvelopeRig
{
    static constexpr size_t x_frameSamples = 8;
    TheoryOfTimeBase m_time;
    double m_baseSample;
    std::unique_ptr<PhysicalModelingSource> m_source;
    PhysicalModelingSource::Input m_input;

    PhysicalEnvelopeRig(float attackSeconds, float decaySeconds)
        : m_baseSample(0.0)
        , m_source(std::make_unique<PhysicalModelingSource>(42))
    {
        m_input.m_ahdInput.SetTheoryOfTime(&m_time);
        m_source->m_ahdInputSetter.Set(
            std::log(attackSeconds / 0.0001f) / std::log(2.5f / 0.0001f),
            0.0f,
            std::log(decaySeconds / 0.0005f) / std::log(2.5f / 0.0005f),
            1.0f,
            false,
            m_input.m_ahdInput);
        SetClock(0.0);
    }

    AHD& Envelope()
    {
        return m_source->m_ahd.m_channels[0];
    }

    AHD::Input& EnvelopeInput()
    {
        return m_input.m_ahdInput.m_channels[0];
    }

    void SetClock(double baseSample)
    {
        m_baseSample = baseSample;
        SetClockPosition(0);
    }

    void SetClockPosition(size_t offset)
    {
        double phase = 1.0 + (m_baseSample + static_cast<double>(offset)) / 48000.0;
        m_time.m_sample.m_unmodulatedPhase = phase;
        m_time.m_sample.m_modulatedPhase = phase;
    }

    // Eight samples, at clock positions base through base + 7.
    //
    void ProcessFrame()
    {
        for (size_t i = 0; i < x_frameSamples; ++i)
        {
            SetClockPosition(i);
            m_source->ProcessWithAHD(m_input);
        }
    }

    void Trigger()
    {
        AHD::AHDControl control;
        control.m_trig = true;
        m_input.m_ahdInput.Set(0, control);
    }
};

}

DOCTEST_TEST_CASE("PhysicalModeling: consumes events once and accepts consecutive triggers")
{
    PhysicalEnvelopeRig rig(0.01f, 0.01f);
    rig.Trigger();
    rig.EnvelopeInput().m_release = true;
    rig.ProcessFrame();
    DOCTEST_CHECK(rig.Envelope().m_rawOutput == doctest::Approx(7.0f / 480.0f).epsilon(0.00001));
    DOCTEST_CHECK(rig.Envelope().m_startOutput == doctest::Approx(0.0f));
    DOCTEST_CHECK_FALSE(rig.EnvelopeInput().m_trig);
    DOCTEST_CHECK_FALSE(rig.EnvelopeInput().m_release);
    DOCTEST_CHECK(rig.Envelope().m_state == AHD::State::Running);

    rig.SetClock(8.0);
    rig.Trigger();
    rig.ProcessFrame();
    DOCTEST_CHECK(rig.Envelope().m_startOutput == doctest::Approx(7.0f / 480.0f).epsilon(0.00001));
    DOCTEST_CHECK(rig.Envelope().m_rawOutput == doctest::Approx(14.0f / 480.0f).epsilon(0.00001));
    DOCTEST_CHECK_FALSE(rig.EnvelopeInput().m_trig);
}

DOCTEST_TEST_CASE("PhysicalModeling: explicit release advances at the sample rate")
{
    PhysicalEnvelopeRig rig(0.01f, 0.01f);
    rig.Envelope().m_state = AHD::State::Running;
    rig.Envelope().m_rawOutput = 1.0f;
    rig.EnvelopeInput().m_release = true;

    AHD base;
    base.m_state = AHD::State::Running;
    base.m_rawOutput = 1.0f;
    AHD::Input baseInput = rig.EnvelopeInput();
    baseInput.m_releaseFactor = 1.0f;
    size_t releaseFrames = 0;
    for (; releaseFrames < 62 && rig.Envelope().m_state != AHD::State::Idle; ++releaseFrames)
    {
        rig.ProcessFrame();
        for (size_t sample = 0; sample < PhysicalEnvelopeRig::x_frameSamples; ++sample)
        {
            base.Process(baseInput);
        }

        DOCTEST_CHECK(rig.Envelope().m_rawOutput == doctest::Approx(base.m_rawOutput).epsilon(0.00001).scale(1.0));
        DOCTEST_CHECK_FALSE(rig.EnvelopeInput().m_release);
    }

    DOCTEST_CHECK(releaseFrames >= 60);
    DOCTEST_CHECK(releaseFrames <= 61);
    DOCTEST_CHECK(rig.Envelope().m_state == AHD::State::Idle);
    DOCTEST_CHECK(base.m_state == AHD::State::Idle);
}

DOCTEST_TEST_CASE("PhysicalModeling: running decay uses phase sample units")
{
    PhysicalEnvelopeRig rig(0.0001f, 0.01f);
    rig.Trigger();
    rig.ProcessFrame();
    DOCTEST_CHECK(rig.Envelope().m_rawOutput == doctest::Approx(1.0f - (7.0f - 4.8f) / 480.0f).epsilon(0.00001));
    rig.SetClock(56.0);
    rig.ProcessFrame();

    AHD base;
    base.m_state = AHD::State::Running;
    AHD::Input input = rig.EnvelopeInput();
    base.Process(input);
    DOCTEST_CHECK(base.m_rawOutput == doctest::Approx(1.0f - (63.0f - 4.8f) / 480.0f).epsilon(0.00001));
    DOCTEST_CHECK(rig.Envelope().m_rawOutput == doctest::Approx(base.m_rawOutput).epsilon(0.00001));
}

DOCTEST_TEST_CASE("PhysicalModeling: minimum attack is 100 microseconds and decay is 500 microseconds")
{
    PhysicalEnvelopeRig rig(0.0001f, 0.0005f);
    rig.Trigger();
    rig.ProcessFrame();
    DOCTEST_CHECK(rig.Envelope().m_rawOutput == doctest::Approx(1.0f - (7.0f - 4.8f) / 24.0f).epsilon(0.00001));
    rig.SetClock(8.0);
    rig.ProcessFrame();
    DOCTEST_CHECK(rig.Envelope().m_state == AHD::State::Running);
    DOCTEST_CHECK(rig.Envelope().m_rawOutput == doctest::Approx(1.0f - (15.0f - 4.8f) / 24.0f).epsilon(0.00001));
    rig.SetClock(24.0);
    rig.ProcessFrame();
    DOCTEST_CHECK(rig.Envelope().m_state == AHD::State::Idle);
    DOCTEST_CHECK(rig.Envelope().m_rawOutput == 0.0f);
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
