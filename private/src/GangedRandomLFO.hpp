#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <utility>
#include "MultiChannelNumber.hpp"
#include "SampleEventHistory.hpp"
#include "SampleTimer.hpp"

inline constexpr size_t x_gangedRandomFrameSamples = 32;

struct GangedRandomLFOTiming
{
    double m_muSeconds{0.0};
    double m_sigmaSeconds{0.0};
    double m_internalSigmaHz{0.0};
};

struct GangedRandomLFOInput
{
    GangedRandomLFOTiming m_waiting;
    GangedRandomLFOTiming m_moving;
    float m_targetInternalSigma{0.0f};

    static GangedRandomLFOInput Standard(double waitingMeanSeconds, float targetInternalSigma)
    {
        GangedRandomLFOInput input;
        input.m_waiting.m_muSeconds = waitingMeanSeconds;
        input.m_waiting.m_sigmaSeconds = 0.3 * waitingMeanSeconds;
        input.m_waiting.m_internalSigmaHz = 0.2 / waitingMeanSeconds;
        input.m_moving.m_muSeconds = waitingMeanSeconds / 2.0;
        input.m_moving.m_sigmaSeconds = 0.15 * waitingMeanSeconds;
        input.m_moving.m_internalSigmaHz = 0.4 / waitingMeanSeconds;
        input.m_targetInternalSigma = targetInternalSigma;
        return input;
    }
};

inline float GangedRandomLFOShapedInterpolate(
    float source,
    float target,
    float shape,
    double progress)
{
    static constexpr double x_pi = 3.14159265358979323846;
    double clampedProgress = std::clamp(progress, 0.0, 1.0);
    float narrowedProgress = static_cast<float>(clampedProgress);
    float clampedShape = std::clamp(shape, 0.0f, 1.0f);
    float smoothProgress = 0.5f - 0.5f * std::cos(static_cast<float>(x_pi) * narrowedProgress);
    float shapedProgress = clampedShape * smoothProgress + (1.0f - clampedShape) * narrowedProgress;
    return target * shapedProgress + source * (1.0f - shapedProgress);
}

inline float GangedRandomLFOFrameInterpolate(
    float source,
    float target,
    float shape,
    double waitingSamples,
    double movingIncrement,
    double elapsed)
{
    double frameStart = std::floor(elapsed / x_gangedRandomFrameSamples) * x_gangedRandomFrameSamples;
    float startValue = GangedRandomLFOShapedInterpolate(
        source, target, shape, (frameStart - waitingSamples) * movingIncrement);
    float endValue = GangedRandomLFOShapedInterpolate(
        source, target, shape, (frameStart + x_gangedRandomFrameSamples - waitingSamples) * movingIncrement);
    float fraction = static_cast<float>((elapsed - frameStart) / x_gangedRandomFrameSamples);
    return startValue + fraction * (endValue - startValue);
}

struct GangedRandomLFOVoiceInput
{
    double m_waitingIncrement{0.0};
    double m_movingIncrement{0.0};
    float m_shape{0.0f};
};

struct GangedRandomLFOVoice
{
    enum class State
    {
        Waiting,
        Moving,
        Done,
    };

    State m_state{State::Done};
    double m_currentStateProgress{0.0};
    size_t m_currentStateSamples{0};
    float m_source{0.0f};
    float m_target{0.0f};
    float m_output{0.0f};

    void Reset(float newTarget)
    {
        m_state = State::Waiting;
        m_currentStateProgress = 0.0;
        m_currentStateSamples = 0;
        m_source = m_target;
        m_target = newTarget;
        m_output = m_source;
    }

    float Process(const GangedRandomLFOVoiceInput& input)
    {
        return Process(input, 1);
    }

    // Forecast only this round. A finished voice holds its target until the
    // gang starts another round at a frame boundary.
    //
    float Process(const GangedRandomLFOVoiceInput& input, size_t samples)
    {
        if (m_state == State::Waiting)
        {
            double remaining = input.m_waitingIncrement > 0.0
                ? std::ceil(1.0 / input.m_waitingIncrement) - static_cast<double>(m_currentStateSamples)
                : INFINITY;
            if (remaining > static_cast<double>(samples))
            {
                m_currentStateSamples += samples;
                m_currentStateProgress = m_currentStateSamples * input.m_waitingIncrement;
                m_output = m_source;
                return m_output;
            }

            samples -= static_cast<size_t>(std::max(0.0, remaining));
            m_state = State::Moving;
            m_currentStateSamples = 0;
            m_currentStateProgress = 0.0;
        }

        if (m_state == State::Moving)
        {
            m_currentStateSamples += samples;
            m_currentStateProgress = m_currentStateSamples * input.m_movingIncrement;
            m_output = GangedRandomLFOShapedInterpolate(
                m_source, m_target, input.m_shape, m_currentStateProgress);
            if (m_currentStateProgress >= 1.0)
            {
                m_state = State::Done;
                m_output = m_target;
            }
        }
        else
        {
            m_output = m_target;
        }

        return m_output;
    }

};

struct GangedRandomLFOVoiceSnapshot
{
    GangedRandomLFOVoice::State m_state{GangedRandomLFOVoice::State::Done};
    double m_currentStateProgress{0.0};
    float m_source{0.0f};
    float m_target{0.0f};
    float m_output{0.0f};
    float m_shape{0.0f};
    double m_waitingIncrement{0.0};
    double m_movingIncrement{0.0};
};

template<size_t x_voiceCount>
struct GangedRandomLFOSnapshot
{
    double m_sampleRate{0.0};
    double m_roundElapsedSamples{0.0};
    GangedRandomLFOVoiceSnapshot m_voices[x_voiceCount];
};

struct GangedRandomLFOVoiceUIState
{
    std::atomic<GangedRandomLFOVoice::State> m_state{GangedRandomLFOVoice::State::Done};
    std::atomic<double> m_currentStateProgress{0.0};
    std::atomic<float> m_source{0.0f};
    std::atomic<float> m_target{0.0f};
    std::atomic<float> m_output{0.0f};
    std::atomic<float> m_shape{0.0f};
    std::atomic<double> m_waitingIncrement{0.0};
    std::atomic<double> m_movingIncrement{0.0};
};

template<size_t x_voiceCount>
struct GangedRandomLFOHistory : SampleEventHistory<5 * x_voiceCount + 1, 256>
{
    using History = SampleEventHistory<5 * x_voiceCount + 1, 256>;

    void RecordRound(
        size_t sample,
        const GangedRandomLFOVoice* voices,
        const GangedRandomLFOVoiceInput* inputs)
    {
        RecordRound(sample, voices, inputs, 0.0);
    }

    void RecordRound(
        size_t sample,
        const GangedRandomLFOVoice* voices,
        const GangedRandomLFOVoiceInput* inputs,
        double elapsedSamples)
    {
        typename History::Event event;
        event.m_sample = sample;
        event.m_values[5 * x_voiceCount] = elapsedSamples;
        for (size_t voice = 0; voice < x_voiceCount; ++voice)
        {
            size_t offset = voice * 5;
            event.m_values[offset] = voices[voice].m_source;
            event.m_values[offset + 1] = voices[voice].m_target;
            event.m_values[offset + 2] = inputs[voice].m_waitingIncrement;
            event.m_values[offset + 3] = inputs[voice].m_movingIncrement;
            event.m_values[offset + 4] = inputs[voice].m_shape;
        }

        this->Record(event);
    }

    bool ReadValue(size_t voice, double sample, float& value) const
    {
        value = 0.0f;
        typename History::Event event;
        if (voice >= x_voiceCount || !this->FindAtOrBefore(sample, event))
        {
            return false;
        }

        value = Evaluate(event, voice, sample);
        return true;
    }

    float GetValue(size_t voice, double sample) const
    {
        float value = 0.0f;
        ReadValue(voice, sample, value);
        return value;
    }

    static float Evaluate(const typename History::Event& event, size_t voice, double sample)
    {
        size_t offset = voice * 5;
        float source = static_cast<float>(event.m_values[offset]);
        float target = static_cast<float>(event.m_values[offset + 1]);
        double waitingSamples = std::ceil(1.0 / event.m_values[offset + 2]);
        double movingIncrement = event.m_values[offset + 3];
        float shape = static_cast<float>(event.m_values[offset + 4]);
        double elapsed = sample - static_cast<double>(event.m_sample) + event.m_values[5 * x_voiceCount];
        return GangedRandomLFOFrameInterpolate(source, target, shape, waitingSamples, movingIncrement, elapsed);
    }

    bool GetRound(size_t voice, double knownThrough, double sample, float& value,
        double& start, double& end) const
    {
        typename History::Event event;
        if (voice >= x_voiceCount || !this->FindAtOrBefore(knownThrough, event))
        {
            return false;
        }

        start = static_cast<double>(event.m_sample) - event.m_values[5 * x_voiceCount];
        end = start;
        for (size_t lane = 0; lane < x_voiceCount; ++lane)
        {
            double duration = std::ceil(1.0 / event.m_values[lane * 5 + 2])
                + std::ceil(1.0 / event.m_values[lane * 5 + 3]);
            end = std::max(end, start + duration);
        }

        end = start + std::ceil((end - start) / x_gangedRandomFrameSamples) * x_gangedRandomFrameSamples;
        value = Evaluate(event, voice, sample);
        return true;
    }
};

template<size_t x_voiceCount>
struct GangedRandomLFOUIState
{
    GangedRandomLFOHistory<x_voiceCount> m_history;
    std::atomic<uint32_t> m_revision{0};
    std::atomic<double> m_sampleRate{0.0};
    std::atomic<double> m_roundElapsedSamples{0.0};
    GangedRandomLFOVoiceUIState m_voices[x_voiceCount];

    bool ReadSnapshot(GangedRandomLFOSnapshot<x_voiceCount>& snapshot, unsigned maxRetries = 4) const
    {
        for (unsigned attempt = 0; attempt < maxRetries; ++attempt)
        {
            uint32_t startRevision = m_revision.load(std::memory_order_acquire);
            if ((startRevision & 1u) != 0u)
            {
                continue;
            }

            GangedRandomLFOSnapshot<x_voiceCount> candidate;
            candidate.m_sampleRate = m_sampleRate.load(std::memory_order_relaxed);
            candidate.m_roundElapsedSamples = m_roundElapsedSamples.load(std::memory_order_relaxed);
            for (size_t voice = 0; voice < x_voiceCount; ++voice)
            {
                const GangedRandomLFOVoiceUIState& source = m_voices[voice];
                GangedRandomLFOVoiceSnapshot& destination = candidate.m_voices[voice];
                destination.m_state = source.m_state.load(std::memory_order_relaxed);
                destination.m_currentStateProgress =
                    source.m_currentStateProgress.load(std::memory_order_relaxed);
                destination.m_source = source.m_source.load(std::memory_order_relaxed);
                destination.m_target = source.m_target.load(std::memory_order_relaxed);
                destination.m_output = source.m_output.load(std::memory_order_relaxed);
                destination.m_shape = source.m_shape.load(std::memory_order_relaxed);
                destination.m_waitingIncrement =
                    source.m_waitingIncrement.load(std::memory_order_relaxed);
                destination.m_movingIncrement =
                    source.m_movingIncrement.load(std::memory_order_relaxed);
            }

            uint32_t endRevision = m_revision.load(std::memory_order_acquire);
            if (startRevision == endRevision && (endRevision & 1u) == 0u)
            {
                snapshot = candidate;
                return true;
            }
        }

        return false;
    }
};

struct GangedRandomLFODrawSource
{
    std::mt19937 m_engine;
    std::normal_distribution<double> m_normal;
    std::uniform_real_distribution<float> m_uniform;

    GangedRandomLFODrawSource()
        : GangedRandomLFODrawSource(std::random_device{}())
    {
    }

    explicit GangedRandomLFODrawSource(uint32_t seed)
        : m_engine(seed)
        , m_normal(0.0, 1.0)
        , m_uniform(0.0f, 1.0f)
    {
    }

    double Normal(double mean, double sigma)
    {
        if (sigma == 0.0)
        {
            return mean;
        }

        using Parameters = std::normal_distribution<double>::param_type;
        return m_normal(m_engine, Parameters(mean, sigma));
    }

    float Uniform01()
    {
        return m_uniform(m_engine);
    }
};

template<size_t x_voiceCount, typename DrawSource = GangedRandomLFODrawSource>
struct GangedRandomLFO
{
    static_assert(x_voiceCount > 0, "a ganged random LFO requires at least one voice");
    using Vector = MultiChannelNumber<float, x_voiceCount>;

    GangedRandomLFOVoice m_voices[x_voiceCount];
    GangedRandomLFOVoice m_nextVoices[x_voiceCount];
    Vector m_frameStart;
    Vector m_frameEnd;
    Vector m_output;
    size_t m_framePosition = 0;
    bool m_frameInitialized = false;
    GangedRandomLFOVoiceInput m_voiceInputs[x_voiceCount];
    DrawSource m_draws;
    double m_sampleRate{0.0};
    double m_roundElapsedSamples{0.0};
    GangedRandomLFOHistory<x_voiceCount>* m_history = nullptr;

    GangedRandomLFO() = default;

    explicit GangedRandomLFO(uint32_t seed)
        : m_draws(seed)
    {
    }

    explicit GangedRandomLFO(DrawSource draws)
        : m_draws(std::move(draws))
    {
    }

    void Process(double dt, const GangedRandomLFOInput& input)
    {
        if (!std::isfinite(dt) || dt <= 0.0)
        {
            return;
        }

        if (m_frameInitialized)
        {
            m_roundElapsedSamples += 1.0;
        }

        if (m_framePosition == 0)
        {
            m_sampleRate = 1.0 / dt;
            bool allDone = true;
            for (size_t voice = 0; voice < x_voiceCount; ++voice)
            {
                if (m_frameInitialized)
                {
                    m_voices[voice] = m_nextVoices[voice];
                }

                allDone = allDone && m_voices[voice].m_state == GangedRandomLFOVoice::State::Done;
            }

            if (allDone)
            {
                if (!m_frameInitialized)
                {
                    std::array<float, x_voiceCount> sources = SampleTargets(input);
                    for (size_t voice = 0; voice < x_voiceCount; ++voice)
                    {
                        m_voices[voice].m_target = sources[voice];
                    }
                }

                SampleAndResetRound(input);
                if (!m_frameInitialized)
                {
                    RandomizeInitialRoundPosition();
                }

                if (m_history)
                {
                    m_history->RecordRound(
                        SampleTimer::GetSample(), m_voices, m_voiceInputs, m_roundElapsedSamples);
                }
            }

            for (size_t voice = 0; voice < x_voiceCount; ++voice)
            {
                m_frameStart[voice] = m_voices[voice].m_output;
                m_nextVoices[voice] = m_voices[voice];
                m_frameEnd[voice] = m_nextVoices[voice].Process(m_voiceInputs[voice], x_gangedRandomFrameSamples);
            }

            m_frameInitialized = true;
        }

        float fraction = static_cast<float>(m_framePosition) / x_gangedRandomFrameSamples;
        m_output = m_frameStart + (m_frameEnd - m_frameStart) * fraction;
        m_framePosition = (m_framePosition + 1) % x_gangedRandomFrameSamples;
    }

    // One engine sample, the same Process shape as the other per-voice DSP.
    //
    Vector Process(const GangedRandomLFOInput& input)
    {
        Process(SampleTimer::x_samplePeriod, input);
        return GetOutput();
    }

    float Output(size_t voice) const
    {
        return m_output[voice];
    }

    Vector GetOutput() const
    {
        return m_output;
    }

    std::array<double, x_voiceCount> SampleCorrelatedIncrements(
        const GangedRandomLFOTiming& timing)
    {
        double sampledCenterSeconds = m_draws.Normal(
            timing.m_muSeconds,
            std::max(0.0, timing.m_sigmaSeconds));
        double centerSeconds = std::max(1.0 / m_sampleRate, std::abs(sampledCenterSeconds));
        double centerRateHz = 1.0 / centerSeconds;
        double epsilonIncrement = 1.0 / (m_sampleRate * 3600.0);

        std::array<double, x_voiceCount> increments{};
        for (size_t voice = 0; voice < x_voiceCount; ++voice)
        {
            double sampledRateHz = m_draws.Normal(
                centerRateHz,
                std::max(0.0, timing.m_internalSigmaHz));
            increments[voice] = std::max(
                epsilonIncrement,
                std::abs(sampledRateHz) / m_sampleRate);
        }

        return increments;
    }

    std::array<float, x_voiceCount> SampleTargets(const GangedRandomLFOInput& input)
    {
        float targetCenter = std::clamp(m_draws.Uniform01(), 0.0f, 1.0f);
        std::array<float, x_voiceCount> targets{};
        for (size_t voice = 0; voice < x_voiceCount; ++voice)
        {
            double sampledTarget = m_draws.Normal(
                static_cast<double>(targetCenter),
                static_cast<double>(std::max(0.0f, input.m_targetInternalSigma)));
            targets[voice] = static_cast<float>(std::clamp(sampledTarget, 0.0, 1.0));
        }

        return targets;
    }

    void SampleAndResetRound(const GangedRandomLFOInput& input)
    {
        std::array<double, x_voiceCount> waitingIncrements =
            SampleCorrelatedIncrements(input.m_waiting);
        std::array<double, x_voiceCount> movingIncrements =
            SampleCorrelatedIncrements(input.m_moving);
        std::array<float, x_voiceCount> targets = SampleTargets(input);
        float shapes[x_voiceCount];
        for (size_t voice = 0; voice < x_voiceCount; ++voice)
        {
            shapes[voice] = std::clamp(m_draws.Uniform01(), 0.0f, 1.0f);
        }

        for (size_t voice = 0; voice < x_voiceCount; ++voice)
        {
            m_voiceInputs[voice].m_waitingIncrement = waitingIncrements[voice];
            m_voiceInputs[voice].m_movingIncrement = movingIncrements[voice];
            m_voiceInputs[voice].m_shape = shapes[voice];
            m_voices[voice].Reset(targets[voice]);
        }

        m_roundElapsedSamples = 0.0;
    }

    void RandomizeInitialRoundPosition()
    {
        double durationSamples = 0.0;
        for (const GangedRandomLFOVoiceInput& input : m_voiceInputs)
        {
            durationSamples = std::max(durationSamples,
                std::ceil(1.0 / input.m_waitingIncrement) + std::ceil(1.0 / input.m_movingIncrement));
        }

        durationSamples = std::ceil(durationSamples / x_gangedRandomFrameSamples) * x_gangedRandomFrameSamples;
        double position = std::clamp(static_cast<double>(m_draws.Uniform01()), 0.0, 1.0);
        size_t elapsedSamples = static_cast<size_t>(std::min(
            std::floor(position * durationSamples), durationSamples - 1.0));
        m_roundElapsedSamples = static_cast<double>(elapsedSamples);
        m_framePosition = elapsedSamples % x_gangedRandomFrameSamples;

        // Keep the enclosing frame so startup uses the same interpolated curve
        // as an already-running gang, including the holds of completed voices.
        //
        size_t frameStart = elapsedSamples - m_framePosition;
        for (size_t voice = 0; voice < x_voiceCount; ++voice)
        {
            m_voices[voice].Process(m_voiceInputs[voice], frameStart);
        }
    }

    void PopulateUIState(GangedRandomLFOUIState<x_voiceCount>* uiState) const
    {
        if (!uiState)
        {
            return;
        }

        uint32_t startRevision = uiState->m_revision.fetch_add(1, std::memory_order_acq_rel);
        uiState->m_sampleRate.store(m_sampleRate, std::memory_order_relaxed);
        uiState->m_roundElapsedSamples.store(m_roundElapsedSamples, std::memory_order_relaxed);
        for (size_t voice = 0; voice < x_voiceCount; ++voice)
        {
            GangedRandomLFOVoice source = m_voices[voice];
            const GangedRandomLFOVoiceInput& input = m_voiceInputs[voice];
            if (m_frameInitialized)
            {
                source.Process(input, (m_framePosition + x_gangedRandomFrameSamples - 1) % x_gangedRandomFrameSamples);
            }

            source.m_output = m_output[voice];
            GangedRandomLFOVoiceUIState& destination = uiState->m_voices[voice];
            destination.m_state.store(source.m_state, std::memory_order_relaxed);
            destination.m_currentStateProgress.store(
                source.m_currentStateProgress,
                std::memory_order_relaxed);
            destination.m_source.store(source.m_source, std::memory_order_relaxed);
            destination.m_target.store(source.m_target, std::memory_order_relaxed);
            destination.m_output.store(source.m_output, std::memory_order_relaxed);
            destination.m_shape.store(input.m_shape, std::memory_order_relaxed);
            destination.m_waitingIncrement.store(
                input.m_waitingIncrement,
                std::memory_order_relaxed);
            destination.m_movingIncrement.store(
                input.m_movingIncrement,
                std::memory_order_relaxed);
        }

        uiState->m_revision.store(startRevision + 2u, std::memory_order_release);
    }
};

using TrioGangedRandomLFO = GangedRandomLFO<3>;
using QuadGangedRandomLFO = GangedRandomLFO<4>;
