#include "doctest.h"

#include "../support/GlobalEnv.hpp"

#include "AudioInputBuffer.hpp"
#include "SourceMixer.hpp"
#include "../support/SVFReference.hpp"
#include <cmath>

namespace
{

void ProcessConstant(SourceMixer::Source& source, SourceMixer::Source::Input& input, float left, float right)
{
    input.m_gain.m_expParam = 1.0f;
    input.m_input[0] = left;
    input.m_input[1] = right;
    for (size_t i = 0; i < 8; ++i)
    {
        source.Process(input);
    }
}

}

DOCTEST_TEST_CASE("SourceMixer input config maps mono left and silences right")
{
    GlobalEnv::ResetPerTest();

    SourceMixer::Input input;
    AudioInputBuffer audioInput;
    audioInput.m_numInputs = SourceMixer::x_numPhysicalInputChannels;

    for (size_t i = 0; i < audioInput.m_numInputs; ++i)
    {
        audioInput.m_input[i] = static_cast<float>(i + 1);
    }

    input.m_sources[2].m_config.m_width = SourceMixer::SourceWidth::Mono;
    input.SetInputs(audioInput);

    DOCTEST_CHECK(input.m_sources[2].m_input[0] == doctest::Approx(5.0f));
    DOCTEST_CHECK(input.m_sources[2].m_input[1] == doctest::Approx(0.0f));
}

DOCTEST_TEST_CASE("SourceMixer input config maps stereo left and right lanes")
{
    GlobalEnv::ResetPerTest();

    SourceMixer::Input input;
    AudioInputBuffer audioInput;
    audioInput.m_numInputs = SourceMixer::x_numPhysicalInputChannels;

    for (size_t i = 0; i < audioInput.m_numInputs; ++i)
    {
        audioInput.m_input[i] = static_cast<float>(i + 1);
    }

    input.m_sources[3].m_config.m_width = SourceMixer::SourceWidth::Stereo;
    input.SetInputs(audioInput);

    DOCTEST_CHECK(input.m_sources[3].m_input[0] == doctest::Approx(7.0f));
    DOCTEST_CHECK(input.m_sources[3].m_input[1] == doctest::Approx(8.0f));
}

DOCTEST_TEST_CASE("SourceMixer UIState exposes static DSP-owned source colors")
{
    GlobalEnv::ResetPerTest();

    SourceMixer mixer;
    SourceMixer::Input input;
    SourceMixer::UIState uiState;

    mixer.SetupUIState(&uiState, input);

    DOCTEST_CHECK(SourceMixer::GetColor(2) == SmartGrid::Color::Ocean);
    DOCTEST_CHECK(uiState.Color(2) == SourceMixer::GetColor(2));
    DOCTEST_CHECK(SmartGrid::Color::From32Bit(SourceMixer::GetColor(2).To32Bit()) == SourceMixer::GetColor(2));
}

DOCTEST_TEST_CASE("SourceMixer mono processing leaves right lane silent")
{
    GlobalEnv::ResetPerTest();

    SourceMixer mixer;
    SourceMixer::Input input;

    input.m_sources[0].m_config.m_width = SourceMixer::SourceWidth::Mono;
    ProcessConstant(mixer.m_sources[0], input.m_sources[0], 1.0f, 0.0f);

    float left = mixer.m_sources[0].m_output[0];
    float right = mixer.m_sources[0].m_output[1];

    DOCTEST_CHECK(left > 0.0f);
    DOCTEST_CHECK(right == doctest::Approx(0.0f));
}

DOCTEST_TEST_CASE("SourceMixer stereo processing keeps lanes independent")
{
    GlobalEnv::ResetPerTest();

    SourceMixer mixer;
    SourceMixer::Input input;

    input.m_sources[1].m_config.m_width = SourceMixer::SourceWidth::Stereo;
    ProcessConstant(mixer.m_sources[1], input.m_sources[1], 1.0f, 0.1f);

    float left = mixer.m_sources[1].m_output[0];
    float right = mixer.m_sources[1].m_output[1];

    DOCTEST_CHECK(left > right);
    DOCTEST_CHECK(right > 0.0f);
}

DOCTEST_TEST_CASE("SourceMixer stereo filter pair matches independent mono filters through cutoff changes")
{
    GlobalEnv::ResetPerTest();

    SourceMixer::Source source;
    SourceMixer::Source::Input input;
    TPTSVFLowPassFilterGeneric<float, 1> lp[SourceMixer::x_numSourceLanes];
    TPTSVFHighPassFilterGeneric<float, 1> hp[SourceMixer::x_numSourceLanes];
    input.m_gain.m_expParam = 1.5f;
    for (size_t i = 0; i < 2048; ++i)
    {
        input.m_hpCutoff.m_expParam = i < 1024 ? 0.001f : 0.004f;
        input.m_lpFactor.m_expParam = i < 512 ? 40.0f : 12.0f;
        input.m_input[0] = std::sin(0.013f * static_cast<float>(i));
        input.m_input[1] = 0.5f * std::sin(0.071f * static_cast<float>(i)) + ((i / 97) % 2 == 0 ? 0.25f : -0.25f);
        source.Process(input);

        float hpCutoff = input.m_hpCutoff.m_expParam;
        float lpFactor = input.m_lpFactor.m_expParam;
        for (size_t lane = 0; lane < SourceMixer::x_numSourceLanes; ++lane)
        {
            hp[lane].SetCutoffAndResonance(hpCutoff, 0.0f);
            lp[lane].SetCutoffAndResonance(hpCutoff * lpFactor, 0.0f);
            lp[lane].Process(input.m_input[lane] * 1.5f);
            hp[lane].Process(lp[lane].GetLowPass());
            float expected = hp[lane].GetHighPass();
            DOCTEST_CHECK(source.m_output[lane] == doctest::Approx(expected).epsilon(0.0001).scale(1.0));
        }
    }
}

DOCTEST_TEST_CASE("SourceMixer generated snapshots match audio and retain each source cutoff")
{
    GlobalEnv::ResetPerTest();
    SourceMixer mixer;
    SourceMixer::Input input;
    std::complex<double> measured[4]{};
    for (size_t source = 0; source < 4; ++source)
    {
        auto& params = input.m_sources[source];
        params.m_gain.m_expParam = 1;
        params.m_hpCutoff.m_expParam = 0.003f * (source + 1);
        params.m_lpFactor.m_expParam = 2 + source;
    }

    for (size_t sample = 0; sample < 8000; ++sample)
    {
        for (size_t source = 0; source < 4; ++source)
        {
            const auto tone = std::polar(1.0, 2 * M_PI * 0.007 * (source + 1) * sample);
            input.m_sources[source].m_input[0] = static_cast<float>(tone.real());
            input.m_sources[source].m_input[1] = static_cast<float>(tone.imag());
        }

        mixer.Process(input);
        if (sample >= 4000)
        {
            for (size_t source = 0; source < 4; ++source)
            {
                const auto tone = std::polar(1.0, 2 * M_PI * 0.007 * (source + 1) * sample);
                const auto output = mixer.m_sources[source].m_output;
                measured[source] += std::complex<double>(output[0], output[1]) / tone / 4000.0;
            }
        }
    }

    const auto snapshot = mixer.MakeSnapshot();
    for (size_t source = 0; source < 4; ++source)
    {
        const double frequency = 0.007 * (source + 1);
        const auto& params = input.m_sources[source];
        const double hp = params.m_hpCutoff.m_expParam[0];
        const double lp = hp * params.m_lpFactor.m_expParam[0];
        const auto expected = TestSVF::Transfer(std::tan(M_PI * hp), 2, frequency).m_high
            * TestSVF::Transfer(std::tan(M_PI * lp), 2, frequency).m_low;
        const double power = snapshot.MagnitudeSquaredResponse(ResponseFrequency::CyclesPerSample(frequency), source);
        DOCTEST_CHECK(power == doctest::Approx(std::norm(expected)).epsilon(0.00002));
        DOCTEST_CHECK(std::norm(measured[source]) == doctest::Approx(power).epsilon(0.00002));
    }
}
