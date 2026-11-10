#include "doctest.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "../support/GlobalEnv.hpp"
#include "../support/NanScan.hpp"
#include "../support/Continuity.hpp"
#include "../support/Spectral.hpp"

#include "RandomLFO.hpp"
#include "GangedRandomLFO.hpp"

static constexpr double kSampleRate = 48000.0;

DOCTEST_TEST_CASE("RandomLFO: determinism — same seed produces identical sequence")
{
    const size_t n = 500;

    GlobalEnv::ResetPerTest(); // sets kDefaultSeed
    RandomLFO lfo1;
    std::vector<float> seq1;
    seq1.reserve(n);
    for (size_t i = 0; i < n; ++i) seq1.push_back(lfo1.Process());

    GlobalEnv::ResetPerTest(); // resets to same seed
    RandomLFO lfo2;
    std::vector<float> seq2;
    seq2.reserve(n);
    for (size_t i = 0; i < n; ++i) seq2.push_back(lfo2.Process());

    DOCTEST_REQUIRE(seq1.size() == seq2.size());
    bool identical = true;
    for (size_t i = 0; i < seq1.size(); ++i)
    {
        if (seq1[i] != seq2[i]) { identical = false; break; }
    }
    DOCTEST_CHECK(identical);
}

// ---------------------------------------------------------------------------
// Test 7: RandomLFO — output bounded in [0, 1] and NaN-clean
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("RandomLFO: output bounded in [0, 1] and NaN-clean")
{
    GlobalEnv::ResetPerTest();
    RandomLFO lfo;
    const size_t n = 5000;
    std::vector<float> buf;
    buf.reserve(n);
    for (size_t i = 0; i < n; ++i) buf.push_back(lfo.Process());

    TestNan::AssertClean(buf.data(), n);
    for (float v : buf)
    {
        DOCTEST_CHECK(v >= -0.001f);
        DOCTEST_CHECK(v <= 1.001f);
    }
}

// ---------------------------------------------------------------------------
// Test 8: RandomLFO — smooth (MaxAbsDelta bounded by filter bandwidth)
//
// RandomLFO uses a double one-pole at a 0.05 Hz nat-freq, so large jumps
// between adjacent samples are impossible (the filter effectively limits the
// slew rate). We check that MaxAbsDelta is well below 0.1 per sample.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("RandomLFO: smooth output — MaxAbsDelta bounded by filter")
{
    GlobalEnv::ResetPerTest();
    RandomLFO lfo;
    const size_t n = 5000;
    std::vector<float> buf;
    buf.reserve(n);
    for (size_t i = 0; i < n; ++i) buf.push_back(lfo.Process());

    // Skip very first samples (filter startup transient)
    //
    float maxDelta = TestContinuity::MaxAbsDelta(buf.data() + 100, n - 100);
    // The 0.05 Hz filter should limit jumps to << 0.01 per sample
    //
    DOCTEST_CHECK(maxDelta < 0.01f);
}

struct ScriptedGangedRandomLFODrawSource
{
    std::vector<double> m_normalResults;
    std::vector<float> m_uniformResults;
    size_t m_nextNormal{0};
    size_t m_nextUniform{0};

    double Normal(double, double)
    {
        return m_normalResults.at(m_nextNormal++);
    }

    float Uniform01()
    {
        return m_uniformResults.at(m_nextUniform++);
    }
};

DOCTEST_TEST_CASE("GangedRandomLFOVoice runs wait move and done states")
{
    GangedRandomLFOVoice voice;
    GangedRandomLFOVoiceInput input;
    input.m_waitingIncrement = 0.4;
    input.m_movingIncrement = 0.25;
    input.m_shape = 0.0f;

    DOCTEST_CHECK(voice.m_state == GangedRandomLFOVoice::State::Done);
    voice.Reset(0.75f);
    DOCTEST_CHECK(voice.m_state == GangedRandomLFOVoice::State::Waiting);
    DOCTEST_CHECK(voice.Process(input) == doctest::Approx(0.0f));
    DOCTEST_CHECK(voice.Process(input) == doctest::Approx(0.0f));
    DOCTEST_CHECK(voice.Process(input) == doctest::Approx(0.0f));
    DOCTEST_CHECK(voice.m_state == GangedRandomLFOVoice::State::Moving);
    DOCTEST_CHECK(voice.m_currentStateProgress == doctest::Approx(0.0));
    DOCTEST_CHECK(voice.Process(input) == doctest::Approx(0.1875f));

    input.m_movingIncrement = 0.8;
    input.m_shape = 1.0f;
    DOCTEST_CHECK(voice.Process(input) == doctest::Approx(0.75f));
    DOCTEST_CHECK(voice.m_state == GangedRandomLFOVoice::State::Done);
}

DOCTEST_TEST_CASE("GangedRandomLFO samples a correlated round in canonical order")
{
    ScriptedGangedRandomLFODrawSource draws;
    draws.m_normalResults = {2.0, 0.4, 0.6, 4.0, 0.2, 0.3, -0.2, 1.2};
    draws.m_uniformResults = {0.5f, 0.1f, 0.9f};
    GangedRandomLFO<2, ScriptedGangedRandomLFODrawSource> lfo(std::move(draws));

    GangedRandomLFOInput input;
    input.m_waiting = {2.0, 0.5, 0.125};
    input.m_moving = {4.0, 1.0, 0.25};
    input.m_targetInternalSigma = 0.25f;
    lfo.m_sampleRate = 10.0;
    lfo.SampleAndResetRound(input);

    DOCTEST_CHECK(lfo.m_waitingIncrement[0] == doctest::Approx(0.04));
    DOCTEST_CHECK(lfo.m_waitingIncrement[1] == doctest::Approx(0.06));
    DOCTEST_CHECK(lfo.m_movingIncrement[0] == doctest::Approx(0.02));
    DOCTEST_CHECK(lfo.m_movingIncrement[1] == doctest::Approx(0.03));
    DOCTEST_CHECK(lfo.m_target[0] == doctest::Approx(0.0f));
    DOCTEST_CHECK(lfo.m_target[1] == doctest::Approx(1.0f));
    DOCTEST_CHECK(lfo.m_shape[0] == doctest::Approx(0.1f));
    DOCTEST_CHECK(lfo.m_shape[1] == doctest::Approx(0.9f));
    DOCTEST_CHECK(lfo.m_draws.m_nextNormal == 8);
    DOCTEST_CHECK(lfo.m_draws.m_nextUniform == 3);
}

DOCTEST_TEST_CASE("GangedRandomLFO forecasts one round and redraws only on 32-sample boundaries")
{
    ScriptedGangedRandomLFODrawSource draws;
    draws.m_normalResults = {
        0.6, 0.2,
        1.0, 1.0, 0.5, 1.0, 1.0, 0.5, 0.25, 0.75,
        1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.4, 0.6,
    };
    draws.m_uniformResults = {0.4f, 0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.25f, 0.75f};
    GangedRandomLFO<2, ScriptedGangedRandomLFODrawSource> lfo(std::move(draws));
    GangedRandomLFOInput input;
    input.m_waiting = {1.0, 0.0, 0.0};
    input.m_moving = {1.0, 0.0, 0.0};
    input.m_targetInternalSigma = 0.2f;

    // Both voices finish within this frame; no second round is predicted.
    //
    for (size_t sample = 0; sample < 32; ++sample)
    {
        lfo.Process(1.0, input);
        DOCTEST_CHECK(lfo.m_draws.m_nextNormal == 10);
        DOCTEST_CHECK(lfo.Output(0) == doctest::Approx(0.6f + (0.25f - 0.6f) * sample / 32));
        DOCTEST_CHECK(lfo.Output(1) == doctest::Approx(0.2f + (0.75f - 0.2f) * sample / 32));
    }

    lfo.Process(1.0, input);
    DOCTEST_CHECK(lfo.m_draws.m_nextNormal == 18);
    DOCTEST_CHECK(lfo.Output(0) == doctest::Approx(0.25f));
    DOCTEST_CHECK(lfo.Output(1) == doctest::Approx(0.75f));
}

DOCTEST_TEST_CASE("GangedRandomLFO starts at a shared random time between two random states")
{
    for (float position : {0.0f, 48.0f / 256.0f, 96.0f / 256.0f, 177.0f / 256.0f, 1.0f})
    {
        GlobalEnv::ResetPerTest();
        DOCTEST_CAPTURE(position);
        ScriptedGangedRandomLFODrawSource draws;
        draws.m_normalResults = {
            0.2, 0.8,
            1.0, 1.0 / 64.0, 1.0 / 128.0, 1.0, 1.0 / 64.0, 1.0 / 128.0, 0.9, 0.1,
            1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.4, 0.6,
        };
        draws.m_uniformResults = {0.3f, 0.6f, 0.0f, 1.0f, position, 0.5f, 0.25f, 0.75f};
        GangedRandomLFO<2, ScriptedGangedRandomLFODrawSource> lfo(draws);
        draws.m_uniformResults[4] = 0.0f;
        GangedRandomLFO<2, ScriptedGangedRandomLFODrawSource> reference(draws);
        GangedRandomLFOHistory<2> history;
        lfo.m_history = &history;
        GangedRandomLFOInput input = GangedRandomLFOInput::Standard(1.0, 0.2f);
        size_t initialElapsed = std::min(static_cast<size_t>(position * 256), size_t{255});
        for (size_t sample = 0; sample < initialElapsed; ++sample)
        {
            reference.Process(1.0, input);
        }

        for (size_t elapsed = initialElapsed; elapsed < 256; ++elapsed)
        {
            lfo.Process(1.0, input);
            reference.Process(1.0, input);
            DOCTEST_CHECK(lfo.m_roundElapsedSamples == static_cast<double>(elapsed));
            DOCTEST_CHECK(lfo.m_draws.m_nextNormal == 10);
            DOCTEST_CHECK(lfo.m_draws.m_nextUniform == 5);
            DOCTEST_CHECK(lfo.m_source[0] == doctest::Approx(0.2f));
            DOCTEST_CHECK(lfo.m_source[1] == doctest::Approx(0.8f));
            DOCTEST_CHECK(lfo.m_target[0] == doctest::Approx(0.9f));
            DOCTEST_CHECK(lfo.m_target[1] == doctest::Approx(0.1f));
            for (size_t voice = 0; voice < 2; ++voice)
            {
                DOCTEST_CHECK(lfo.Output(voice) == doctest::Approx(reference.Output(voice)));
                DOCTEST_CHECK(history.GetValue(voice, SampleTimer::GetSample()) ==
                    doctest::Approx(lfo.Output(voice)));
            }

            GangedRandomLFOUIState<2> uiState;
            lfo.PopulateUIState(&uiState);
            GangedRandomLFOSnapshot<2> snapshot;
            DOCTEST_REQUIRE(uiState.ReadSnapshot(snapshot));
            DOCTEST_CHECK(snapshot.m_roundElapsedSamples == static_cast<double>(elapsed));
            DOCTEST_CHECK(snapshot.m_voices[0].m_state == (elapsed < 64
                ? GangedRandomLFOVoice::State::Waiting
                : elapsed < 128 ? GangedRandomLFOVoice::State::Moving : GangedRandomLFOVoice::State::Done));
            DOCTEST_CHECK(snapshot.m_voices[1].m_state == (elapsed < 128
                ? GangedRandomLFOVoice::State::Waiting : GangedRandomLFOVoice::State::Moving));
            DOCTEST_CHECK(snapshot.m_voices[0].m_output == lfo.Output(0));
            DOCTEST_CHECK(snapshot.m_voices[1].m_output == lfo.Output(1));
            SampleTimer::IncrementSample();
        }

        float value = 0.0f;
        double start = 0.0;
        double end = 0.0;
        DOCTEST_REQUIRE(history.GetRound(0, 0.0, 0.0, value, start, end));
        DOCTEST_CHECK(start == -static_cast<double>(initialElapsed));
        DOCTEST_CHECK(end == static_cast<double>(256 - initialElapsed));
        DOCTEST_CHECK(value == doctest::Approx(history.GetValue(0, 0.0)));
        DOCTEST_CHECK(history.m_count.load() == 1);

        lfo.Process(1.0, input);
        DOCTEST_CHECK(lfo.m_roundElapsedSamples == 0.0);
        DOCTEST_CHECK(lfo.Output(0) == doctest::Approx(0.9f));
        DOCTEST_CHECK(lfo.Output(1) == doctest::Approx(0.1f));
        DOCTEST_CHECK(lfo.m_draws.m_nextNormal == 18);
        DOCTEST_CHECK(lfo.m_draws.m_nextUniform == 8);
        DOCTEST_CHECK(history.m_count.load() == 2);
        DOCTEST_REQUIRE(history.GetRound(0, SampleTimer::GetSample(), SampleTimer::GetSample(), value, start, end));
        DOCTEST_CHECK(start == static_cast<double>(256 - initialElapsed));
        DOCTEST_CHECK(end == start + 32.0);
    }
}

DOCTEST_TEST_CASE("GangedRandomLFO holds completed lanes until the slowest lane's frame ends")
{
    GangedRandomLFO<2> lfo(123u);
    GangedRandomLFOInput input = GangedRandomLFOInput::Standard(1.0, 0.1f);
    for (size_t lane = 0; lane < 2; ++lane)
    {
        lfo.m_target[lane] = 1.0f;
        lfo.m_waitingIncrement[lane] = 1.0 / 16.0;
        lfo.m_movingIncrement[lane] = lane == 0 ? 1.0 / 8.0 : 1.0 / 64.0;
        lfo.m_shape[lane] = 1.0f;
    }

    lfo.ResetRound();
    for (size_t sample = 0; sample < 96; ++sample)
    {
        lfo.Process(1.0 / kSampleRate, input);
        if (sample >= 32)
        {
            DOCTEST_CHECK(lfo.Output(0) == 1.0f);
        }

        DOCTEST_CHECK(lfo.m_target[0] == 1.0f);
        DOCTEST_CHECK(lfo.m_target[1] == 1.0f);
        if (sample == 48)
        {
            // The chord between the cosine curve's samples at 32 and 64.
            //
            DOCTEST_CHECK(lfo.Output(1) == doctest::Approx(0.5f));
        }
    }

    lfo.Process(1.0 / kSampleRate, input);
    DOCTEST_CHECK(lfo.m_roundElapsedSamples == 0.0);
    DOCTEST_CHECK(lfo.Output(0) == 1.0f);
    DOCTEST_CHECK(lfo.Output(1) == 1.0f);
    DOCTEST_CHECK(lfo.m_target[1] != 1.0f);
}

DOCTEST_TEST_CASE("GangedRandomLFO is bounded NaN clean and deterministic")
{
    GangedRandomLFO<4> first(0x12345678u);
    GangedRandomLFO<4> second(0x12345678u);
    GangedRandomLFOInput input = GangedRandomLFOInput::Standard(0.05, 0.3f);
    std::vector<float> output;
    output.reserve(40000);

    for (size_t sample = 0; sample < 10000; ++sample)
    {
        first.Process(1.0 / kSampleRate, input);
        second.Process(1.0 / kSampleRate, input);
        for (size_t voice = 0; voice < 4; ++voice)
        {
            float value = first.Output(voice);
            output.push_back(value);
            DOCTEST_CHECK(value == second.Output(voice));
            DOCTEST_CHECK(value >= 0.0f);
            DOCTEST_CHECK(value <= 1.0f);
        }
    }

    TestNan::AssertClean(output.data(), output.size());
}

DOCTEST_TEST_CASE("GangedRandomLFO publishes a coherent predictive snapshot")
{
    GangedRandomLFO<2> lfo(123u);
    GangedRandomLFOInput input = GangedRandomLFOInput::Standard(0.5, 0.1f);
    lfo.Process(1.0 / kSampleRate, input);
    double initialElapsed = lfo.m_roundElapsedSamples;
    lfo.Process(1.0 / kSampleRate, input);

    GangedRandomLFOUIState<2> uiState;
    lfo.PopulateUIState(&uiState);
    GangedRandomLFOSnapshot<2> snapshot;
    DOCTEST_REQUIRE(uiState.ReadSnapshot(snapshot));
    DOCTEST_CHECK(snapshot.m_sampleRate == doctest::Approx(kSampleRate));
    DOCTEST_CHECK(snapshot.m_roundElapsedSamples == doctest::Approx(initialElapsed + 1.0));
    for (size_t voice = 0; voice < 2; ++voice)
    {
        DOCTEST_CHECK(snapshot.m_voices[voice].m_state == lfo.GetVoiceState(voice).m_state);
        DOCTEST_CHECK(snapshot.m_voices[voice].m_source == lfo.m_source[voice]);
        DOCTEST_CHECK(snapshot.m_voices[voice].m_target == lfo.m_target[voice]);
        DOCTEST_CHECK(
            snapshot.m_voices[voice].m_waitingIncrement ==
            lfo.m_waitingIncrement[voice]);
    }

    uiState.m_revision.store(1, std::memory_order_release);
    DOCTEST_CHECK_FALSE(uiState.ReadSnapshot(snapshot));
}

DOCTEST_TEST_CASE("GangedRandomLFO history matches frame interpolation across non-binary timing boundaries")
{
    GlobalEnv::ResetPerTest();
    GangedRandomLFO<3> lfo(427u);
    GangedRandomLFOHistory<3> history;
    lfo.m_history = &history;
    GangedRandomLFOInput input;
    input.m_waiting = {100.0 * SampleTimer::x_samplePeriod, 0.0, 0.0};
    input.m_moving = {28.0 * SampleTimer::x_samplePeriod, 0.0, 0.0};
    for (size_t sample = 0; sample < 3000; ++sample)
    {
        lfo.Process(input);
        for (size_t lane = 0; lane < 3; ++lane)
        {
            DOCTEST_CHECK(history.GetValue(lane, SampleTimer::GetSample()) == doctest::Approx(lfo.Output(lane)).epsilon(1e-6).scale(1));
        }

        SampleTimer::IncrementSample();
    }
}

template<size_t Size>
static void CheckPackedGangedRandomRound(size_t timingOffset)
{
    GangedRandomLFO<Size> lfo(123u);
    std::array<GangedRandomLFOVoice, Size> voices;
    std::array<GangedRandomLFOVoiceInput, Size> inputs;
    std::array<float, Size> starts;
    std::array<float, Size> ends;
    const double waiting[] = {15.0, 100.0, 1.0, 64.0};
    const double moving[] = {49.0, 28.0, 1.0, 127.0};
    for (size_t lane = 0; lane < Size; ++lane)
    {
        size_t timing = (lane + timingOffset) % 4;
        lfo.m_source[lane] = 0.1f + 0.2f * lane;
        lfo.m_target[lane] = 0.9f - 0.2f * lane;
        lfo.m_shape[lane] = static_cast<float>(timing) / 3.0f;
        lfo.m_waitingIncrement[lane] = 1.0 / waiting[timing];
        lfo.m_movingIncrement[lane] = 1.0 / moving[timing];
        inputs[lane] = lfo.GetVoiceInput(lane);
        voices[lane].m_target = lfo.m_source[lane];
        voices[lane].Reset(lfo.m_target[lane]);
    }

    lfo.ResetRound();
    const auto input = GangedRandomLFOInput::Standard(1.0, 0.1f);
    for (size_t sample = 0; sample < 512; ++sample)
    {
        DOCTEST_CAPTURE(Size);
        DOCTEST_CAPTURE(timingOffset);
        DOCTEST_CAPTURE(sample);
        if (sample % x_gangedRandomFrameSamples == 0)
        {
            bool allDone = true;
            for (const auto& voice : voices)
            {
                allDone = allDone && voice.m_state == GangedRandomLFOVoice::State::Done;
            }

            if (allDone)
            {
                DOCTEST_CHECK(lfo.m_roundSamples == static_cast<double>(sample));
                lfo.Process(input);
                DOCTEST_CHECK(lfo.m_roundElapsedSamples == 0.0);
                for (size_t lane = 0; lane < Size; ++lane)
                {
                    DOCTEST_CHECK(lfo.Output(lane) == voices[lane].m_target);
                }

                return;
            }

            for (size_t lane = 0; lane < Size; ++lane)
            {
                starts[lane] = voices[lane].m_output;
                ends[lane] = voices[lane].Process(inputs[lane], x_gangedRandomFrameSamples);
            }
        }

        lfo.Process(input);
        DOCTEST_CHECK(lfo.m_roundElapsedSamples == static_cast<double>(sample));
        float fraction = static_cast<float>(sample % x_gangedRandomFrameSamples) / x_gangedRandomFrameSamples;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            float expected = starts[lane] + (ends[lane] - starts[lane]) * fraction;
            DOCTEST_CHECK(lfo.Output(lane) == doctest::Approx(expected).epsilon(1e-6).scale(1));
        }
    }

    DOCTEST_FAIL("The round did not finish");
}

DOCTEST_TEST_CASE("GangedRandomLFO packed forecasts preserve scalar lane completion and round cadence")
{
    for (size_t offset = 0; offset < 4; ++offset)
    {
        CheckPackedGangedRandomRound<1>(offset);
        CheckPackedGangedRandomRound<2>(offset);
        CheckPackedGangedRandomRound<3>(offset);
        CheckPackedGangedRandomRound<4>(offset);
    }
}

DOCTEST_TEST_CASE("GangedRandomLFO vector cosine curve stays within three millionths of cosine")
{
    using Vector = MultiChannelNumber<float, 4>;
    using DoubleVector = MultiChannelNumber<double, 4>;
    const Vector source(0.0f, 1.0f, 0.3f, 0.8f);
    const Vector target(1.0f, 0.0f, 0.7f, 0.2f);
    const Vector shape(1.0f, 0.0f, 0.25f, 0.75f);
    for (size_t sample = 0; sample <= 10000; ++sample)
    {
        double progress = -0.1 + 1.2 * sample / 10000.0;
        auto output = GangedRandomLFOShapedInterpolate(source, target, shape, DoubleVector::Splat(progress));
        for (size_t lane = 0; lane < 4; ++lane)
        {
            double p = std::clamp(progress, 0.0, 1.0);
            double smooth = 0.5 - 0.5 * std::cos(M_PI * p);
            double shaped = shape[lane] * smooth + (1.0 - shape[lane]) * p;
            double expected = target[lane] * shaped + source[lane] * (1.0 - shaped);
            DOCTEST_CHECK(std::abs(output[lane] - expected) < 3e-6);
            DOCTEST_CHECK(output[lane] >= std::min(source[lane], target[lane]));
            DOCTEST_CHECK(output[lane] <= std::max(source[lane], target[lane]));
        }
    }
}
