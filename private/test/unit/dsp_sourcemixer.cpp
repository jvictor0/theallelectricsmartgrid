#include "doctest.h"

#include "../support/GlobalEnv.hpp"

#include "AudioInputBuffer.hpp"
#include "SourceMixer.hpp"
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
    LinearStateVariableFilter lp[SourceMixer::x_numSourceLanes];
    LinearStateVariableFilter hp[SourceMixer::x_numSourceLanes];
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
