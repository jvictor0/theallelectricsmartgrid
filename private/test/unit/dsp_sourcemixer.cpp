#include "doctest.h"

#include "../support/GlobalEnv.hpp"

#include "AudioInputBuffer.hpp"
#include "SourceMixer.hpp"

namespace
{

void FillUBlock(SourceMixer::Source::Input& input, float left, float right)
{
    input.m_gain.m_expParam = 1.0f;
    for (size_t i = 0; i < SampleTimer::x_controlFrameRate; ++i)
    {
        input.m_uBlockInput[0][i] = left;
        input.m_uBlockInput[1][i] = right;
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

    DOCTEST_CHECK(input.m_sources[2].m_uBlockInput[0][0] == doctest::Approx(5.0f));
    DOCTEST_CHECK(input.m_sources[2].m_uBlockInput[1][0] == doctest::Approx(0.0f));
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

    DOCTEST_CHECK(input.m_sources[3].m_uBlockInput[0][0] == doctest::Approx(7.0f));
    DOCTEST_CHECK(input.m_sources[3].m_uBlockInput[1][0] == doctest::Approx(8.0f));
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
    FillUBlock(input.m_sources[0], 1.0f, 0.0f);

    mixer.m_sources[0].ProcessUBlock(input.m_sources[0]);

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
    FillUBlock(input.m_sources[1], 1.0f, 0.1f);

    mixer.m_sources[1].ProcessUBlock(input.m_sources[1]);

    float left = mixer.m_sources[1].m_output[0];
    float right = mixer.m_sources[1].m_output[1];

    DOCTEST_CHECK(left > right);
    DOCTEST_CHECK(right > 0.0f);
}
