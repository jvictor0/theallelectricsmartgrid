#include "doctest.h"

#include <cmath>
#include <memory>

#include "Resynthesis.hpp"

namespace
{
template<size_t Size>
void AnalyzePhaseGraph(ResynthesizerGeneric<float, Size>& synth)
{
    for (size_t bin = 1; bin < ResynthesizerGeneric<float, Size>::x_maxComponents; ++bin)
    {
        synth.SetAnalysisOmega(static_cast<int>(bin));
    }

    for (size_t channel = 0; channel < Size; ++channel)
    {
        synth.m_pvdr[channel].Clear();
        synth.m_pvdr[channel].Analyze();
    }
}
}

DOCTEST_TEST_CASE("Resynthesizer Schmitt gate triggers once and retains both sides of the hysteresis band")
{
    using State = Resynthesizer::PVDR::GateState;
    constexpr int x_bin = 200;
    auto synth = std::make_unique<Resynthesizer>();
    const float trigger = static_cast<float>(Resynthesizer::PVDR::x_triggerMagnitudeRatio);
    const float low = static_cast<float>(Resynthesizer::PVDR::x_lowMagnitudeRatio);
    struct Frame
    {
        float m_magnitude;
        State m_expected;
    };

    const Frame frames[] =
    {
        {0, State::Low},
        {0.0005f, State::Low},
        {trigger, State::Low},
        {std::nextafter(trigger, 1.0f), State::Triggered},
        {0.5f, State::High},
        {0.0005f, State::High},
        {low, State::High},
        {std::nextafter(low, 0.0f), State::Low},
        {0.0005f, State::Low},
        {0.002f, State::Triggered},
        {0, State::Low}
    };

    synth->m_analysisMagnitudes[50] = 1;
    for (const Frame& frame : frames)
    {
        DOCTEST_CAPTURE(frame.m_magnitude);
        synth->PrimeFromPrevious();
        synth->m_analysisMagnitudes[x_bin] = frame.m_magnitude;
        AnalyzePhaseGraph(*synth);
        const auto& pvdr = synth->m_pvdr[0];
        DOCTEST_CHECK(pvdr.m_gate[x_bin] == frame.m_expected);
        const auto& result = pvdr.m_results[pvdr.m_resultIndices[x_bin]];
        DOCTEST_CHECK(result.m_small == (frame.m_expected == State::Low));
        if (result.m_small)
        {
            DOCTEST_CHECK(result.m_delta >= -0.5);
            DOCTEST_CHECK(result.m_delta < 0.5);
        }
    }
}

DOCTEST_TEST_CASE("Resynthesizer Schmitt reference includes the previous frame and is independent per channel")
{
    using Synth = ResynthesizerGeneric<float, 2>;
    using State = Synth::PVDR::GateState;
    constexpr int x_bin = 200;
    auto synth = std::make_unique<Synth>();
    synth->m_prevAnalysisMagnitudes[50] = Synth::Vector(1.0f, 0.1f);
    synth->m_analysisMagnitudes[50] = Synth::Vector(0.1f, 0.1f);
    synth->m_analysisMagnitudes[x_bin] = Synth::Vector(0.0002f, 0.0002f);
    AnalyzePhaseGraph(*synth);
    DOCTEST_CHECK(synth->m_pvdr[0].m_gate[x_bin] == State::Low);
    DOCTEST_CHECK(synth->m_pvdr[1].m_gate[x_bin] == State::Triggered);
    synth->PrimeFromPrevious();
    AnalyzePhaseGraph(*synth);
    DOCTEST_CHECK(synth->m_pvdr[0].m_gate[x_bin] == State::Triggered);
    DOCTEST_CHECK(synth->m_pvdr[1].m_gate[x_bin] == State::High);
}

DOCTEST_TEST_CASE("Resynthesizer snaps all oscillator roots and their followers then resumes detuned propagation")
{
    constexpr int x_bin = 64;
    auto synth = std::make_unique<Resynthesizer>();
    synth->m_analysisMagnitudes[x_bin] = 1;
    synth->m_analysisMagnitudes[x_bin - 1] = 0.3f;
    synth->m_analysisMagnitudes[x_bin + 1] = 0.2f;
    synth->m_analysisPhase[x_bin] = 0.2f;
    synth->m_analysisPhase[x_bin - 1] = -0.41f;
    synth->m_analysisPhase[x_bin + 1] = 0.39f;
    synth->PrimeFromPrevious();
    AnalyzePhaseGraph(*synth);
    auto& pvdr = synth->m_pvdr[0];
    DOCTEST_REQUIRE(pvdr.IsLeader(x_bin));
    for (int bin : {x_bin - 1, x_bin + 1})
    {
        DOCTEST_REQUIRE(pvdr.m_results[pvdr.m_resultIndices[bin]].m_parent == x_bin);
    }

    const float detunes[] = {1.0f, 1.01f, 0.99f};
    for (size_t oscillatorIndex = 0; oscillatorIndex < Resynthesizer::x_numOscillators; ++oscillatorIndex)
    {
        auto& oscillator = synth->m_oscillators[oscillatorIndex];
        for (int bin : {x_bin - 1, x_bin, x_bin + 1})
        {
            oscillator.m_synthesisPhase[bin] = 123.456 + oscillatorIndex;
        }

        oscillator.FixupPhases(detunes[oscillatorIndex], synth->m_pvdr);
        DOCTEST_CHECK(oscillator.m_synthesisPhase[x_bin][0] == synth->m_analysisPhase[x_bin][0]);
        for (int bin : {x_bin - 1, x_bin + 1})
        {
            const double error = Resynthesizer::PrincArg(oscillator.m_synthesisPhase[bin][0] - synth->m_analysisPhase[bin][0]);
            DOCTEST_CHECK(std::abs(error) < 1e-6);
        }
    }

    synth->PrimeFromPrevious();
    for (int bin : {x_bin - 1, x_bin, x_bin + 1})
    {
        synth->m_analysisPhase[bin][0] += 0.125f;
    }

    AnalyzePhaseGraph(*synth);
    DOCTEST_REQUIRE(pvdr.m_gate[x_bin] == Resynthesizer::PVDR::GateState::High);
    const double delta = pvdr.m_results[pvdr.m_resultIndices[x_bin]].m_delta;
    for (size_t oscillatorIndex = 0; oscillatorIndex < Resynthesizer::x_numOscillators; ++oscillatorIndex)
    {
        auto& oscillator = synth->m_oscillators[oscillatorIndex];
        const double expected = oscillator.m_synthesisPhase[x_bin][0] + delta * detunes[oscillatorIndex];
        oscillator.FixupPhases(detunes[oscillatorIndex], synth->m_pvdr);
        DOCTEST_CHECK(oscillator.m_synthesisPhase[x_bin][0] == expected);
    }
}

DOCTEST_TEST_CASE("Resynthesizer triggered followers retain their existing leader phase")
{
    constexpr int x_bin = 64;
    auto synth = std::make_unique<Resynthesizer>();
    synth->m_analysisMagnitudes[x_bin] = 1;
    synth->m_analysisPhase[x_bin] = 0.1f;
    synth->PrimeFromPrevious();
    synth->m_analysisMagnitudes[x_bin + 1] = 0.2f;
    synth->m_analysisPhase[x_bin + 1] = 0.3f;
    synth->m_pvdr[0].m_gate[x_bin] = Resynthesizer::PVDR::GateState::High;
    synth->m_oscillators[0].m_synthesisPhase[x_bin] = 0.4;
    AnalyzePhaseGraph(*synth);
    auto& pvdr = synth->m_pvdr[0];
    DOCTEST_REQUIRE(pvdr.m_gate[x_bin + 1] == Resynthesizer::PVDR::GateState::Triggered);
    const auto& result = pvdr.m_results[pvdr.m_resultIndices[x_bin + 1]];
    DOCTEST_REQUIRE(result.m_parent == x_bin);
    auto& oscillator = synth->m_oscillators[0];
    const double expected = 0.4 + pvdr.m_results[pvdr.m_resultIndices[x_bin]].m_delta;
    oscillator.FixupPhases(1.0f, synth->m_pvdr);
    DOCTEST_CHECK(oscillator.m_synthesisPhase[x_bin][0] == expected);
    DOCTEST_CHECK(oscillator.m_synthesisPhase[x_bin + 1][0] == expected + result.m_delta);
}

DOCTEST_TEST_CASE("Resynthesizer silence rearms phase reacquisition even when the reference is also silent")
{
    constexpr int x_bin = 64;
    auto synth = std::make_unique<Resynthesizer>();
    synth->m_analysisMagnitudes[x_bin] = 1;
    synth->m_analysisPhase[x_bin] = 0.2f;
    AnalyzePhaseGraph(*synth);
    synth->m_oscillators[0].FixupPhases(1.0f, synth->m_pvdr);
    for (int frame = 0; frame < 3; ++frame)
    {
        synth->PrimeFromPrevious();
        synth->m_analysisMagnitudes[x_bin] = 0;
        AnalyzePhaseGraph(*synth);
        auto& pvdr = synth->m_pvdr[0];
        for (size_t i = 0; i < pvdr.m_size; ++i)
        {
            DOCTEST_CHECK(pvdr.m_results[i].m_small);
            DOCTEST_CHECK(pvdr.m_gate[pvdr.m_results[i].m_bin] == Resynthesizer::PVDR::GateState::Low);
        }

        synth->m_oscillators[0].FixupPhases(1.0f, synth->m_pvdr);
    }

    synth->PrimeFromPrevious();
    synth->m_analysisMagnitudes[x_bin] = 0.5f;
    synth->m_analysisPhase[x_bin] = -0.3f;
    synth->m_oscillators[0].m_synthesisPhase[x_bin] = 321.123;
    AnalyzePhaseGraph(*synth);
    DOCTEST_REQUIRE(synth->m_pvdr[0].m_gate[x_bin] == Resynthesizer::PVDR::GateState::Triggered);
    synth->m_oscillators[0].FixupPhases(1.0f, synth->m_pvdr);
    DOCTEST_CHECK(synth->m_oscillators[0].m_synthesisPhase[x_bin][0] == synth->m_analysisPhase[x_bin][0]);
}

DOCTEST_TEST_CASE("Resynthesizer channel and full clears rearm Schmitt state without affecting other channels")
{
    using Synth = ResynthesizerGeneric<float, 2>;
    using State = Synth::PVDR::GateState;
    constexpr int x_bin = 64;
    auto synth = std::make_unique<Synth>();
    synth->m_analysisMagnitudes[x_bin] = Synth::Vector(1, 1);
    synth->m_analysisPhase[x_bin] = Synth::Vector(0.35f, -0.22f);
    AnalyzePhaseGraph(*synth);
    synth->PrimeFromPrevious();
    AnalyzePhaseGraph(*synth);
    for (auto& oscillator : synth->m_oscillators)
    {
        oscillator.m_synthesisPhase[x_bin] = Synth::DoubleVector(12.34, 56.78);
    }

    synth->ClearChannel(0);
    DOCTEST_CHECK(synth->m_pvdr[0].m_size == 0);
    DOCTEST_CHECK(synth->m_pvdr[0].m_gate[x_bin] == State::Low);
    DOCTEST_CHECK(synth->m_pvdr[1].m_gate[x_bin] == State::High);
    for (auto& oscillator : synth->m_oscillators)
    {
        DOCTEST_CHECK(oscillator.m_synthesisPhase[x_bin][0] == 0);
        DOCTEST_CHECK(oscillator.m_synthesisPhase[x_bin][1] == 56.78);
    }

    synth->m_analysisMagnitudes[x_bin][0] = 1;
    AnalyzePhaseGraph(*synth);
    DOCTEST_CHECK(synth->m_pvdr[0].m_gate[x_bin] == State::Triggered);
    DOCTEST_CHECK(synth->m_pvdr[1].m_gate[x_bin] == State::High);
    synth->Clear();
    for (const auto& pvdr : synth->m_pvdr)
    {
        DOCTEST_CHECK(pvdr.m_size == 0);
        for (auto state : pvdr.m_gate)
        {
            DOCTEST_CHECK(state == State::Low);
        }
    }

    synth->m_analysisMagnitudes[x_bin] = Synth::Vector(1, 1);
    AnalyzePhaseGraph(*synth);
    DOCTEST_CHECK(synth->m_pvdr[0].m_gate[x_bin] == State::Triggered);
    DOCTEST_CHECK(synth->m_pvdr[1].m_gate[x_bin] == State::Triggered);
}

DOCTEST_TEST_CASE("Resynthesizer tracks a stable partial without accumulating phase rounding drift")
{
    constexpr double x_sampleRate = 48000.0;
    constexpr size_t x_frames = 33750;
    for (double frequency : {750.0, 997.0, 7901.0, 17003.0})
    {
        DOCTEST_CAPTURE(frequency);
        auto synth = std::make_unique<Resynthesizer>();
        const int bin = static_cast<int>(std::round(frequency * Resynthesizer::x_N / x_sampleRate));
        const float initialPhase = 0.113f;
        float previousPhase = initialPhase;
        auto& oscillator = synth->m_oscillators[0];
        oscillator.m_synthesisPhase[bin] = initialPhase;
        Resynthesizer::PVDR pvdr(synth.get(), 0);
        pvdr.m_size = 1;
        pvdr.m_results[0].m_bin = bin;
        pvdr.m_results[0].m_parent = bin;
        for (size_t frame = 1; frame <= x_frames; ++frame)
        {
            const double cycles = initialPhase + frequency * static_cast<double>(frame * Resynthesizer::x_H) / x_sampleRate;
            const float phase = static_cast<float>(cycles - std::floor(cycles + 0.5));
            synth->m_prevAnalysisPhase[bin] = previousPhase;
            synth->m_analysisPhase[bin] = phase;
            synth->SetAnalysisOmega(bin);
            pvdr.m_results[0].m_delta = synth->m_omegaInstantaneous[bin] * Resynthesizer::x_H;
            oscillator.FixupPhases(1.0f, &pvdr);
            previousPhase = phase;
        }

        const double expected = initialPhase + frequency * 720.0;
        DOCTEST_CHECK(std::abs(oscillator.m_synthesisPhase[bin] - expected) < 1e-6);
    }
}
