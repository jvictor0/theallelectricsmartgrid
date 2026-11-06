#pragma once

#include "SampleTimer.hpp"
#include "ScopeWriter.hpp"
#include "SmartGridOneModulators.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <cassert>

struct WeightedModulators
{
    static constexpr size_t x_capacity = 15;

    struct Entry
    {
        ModulatorRef m_modulator{GlobalModulator::Noise};
        float m_weight = 0.0f;
    };

    std::array<Entry, x_capacity> m_entries;
    size_t m_count = 0;

    void Add(ModulatorRef modulator, float weight)
    {
        assert(std::isfinite(weight));
        if (weight != 0.0f)
        {
            assert(m_count < x_capacity);
            m_entries[m_count++] = {modulator, weight};
        }
    }

    void Normalize()
    {
        float magnitude = 0.0f;
        for (size_t i = 0; i < m_count; ++i)
        {
            magnitude += std::abs(m_entries[i].m_weight);
        }

        if (magnitude > 0.0f)
        {
            for (size_t i = 0; i < m_count; ++i)
            {
                m_entries[i].m_weight /= magnitude;
            }
        }
    }
};

struct ModulatorTimeAxis
{
    double m_start = 0.0;
    double m_end = 0.0;
    double m_transfer = 1.0;
    double m_wrappedStart = 0.0;
    double m_present = 1.0;
    bool m_predictive = false;

    double GetExtent() const
    {
        return std::max(0.0, m_end - m_start)
            + (m_transfer < 1.0 ? std::max(0.0, m_start - m_wrappedStart) : 0.0);
    }

    double GetSample(double position) const
    {
        if (position < m_transfer || m_transfer == 1.0)
        {
            return m_start + position / m_transfer * (m_end - m_start);
        }

        return m_wrappedStart + (position - m_transfer) / (1.0 - m_transfer)
            * (m_start - m_wrappedStart);
    }
};

namespace SmartGridOneModulatorHistory
{
inline float Noise(ModulatorRef modulator, double sample)
{
    uint64_t hash = static_cast<uint64_t>(std::floor(sample));
    hash += 0x9e3779b97f4a7c15ULL * (1 + modulator.m_voice
        + 16 * static_cast<uint64_t>(modulator.m_mode));
    hash = (hash ^ (hash >> 30)) * 0xbf58476d1ce4e5b9ULL;
    hash = (hash ^ (hash >> 27)) * 0x94d049bb133111ebULL;
    hash ^= hash >> 31;
    return static_cast<float>(hash >> 40) / 16777216.0f;
}

// Sample positions use SampleTimer's absolute audio timeline. Scope-backed
// sources return the recorded display signal, interpolated at its selected LOD,
// including the bipolar delay/reverb scopes. Notes hold until the next note;
// random rounds reconstruct their curves; noise is stable UI-only noise.
// Unavailable, overwritten, unaccepted, or invalid queries return zero.
//
template<typename UIState>
bool ReadModulatorValue(const UIState& uiState, ModulatorRef modulator, double sample, size_t lod, float& value)
{
    value = 0.0f;
    const auto& state = uiState.m_squiggleBoyUIState;
    size_t accepted = state.m_voiceScopeWriter.m_acceptedIndex;
    double first = static_cast<double>(state.m_firstScopeSample.load());
    if (!std::isfinite(sample) || accepted == 0 || sample < first
        || sample > first + static_cast<double>(accepted - 1))
    {
        return false;
    }

    double scopeIndex = sample - first;
    using Mode = SmartGridOneLayout::ParamMode;
    switch (modulator.m_mode)
    {
        case Mode::Voice:
        {
            if (modulator.m_voice >= std::size(state.m_xPos))
            {
                return false;
            }

            size_t voicesPerTrack = std::size(state.m_gangedRandomLFOUIState[0][0].m_voices);
            size_t track = modulator.m_voice / voicesPerTrack;
            size_t voice = modulator.m_voice % voicesPerTrack;
            using Scope = SmartGridOne::VoiceScopes;
            switch (static_cast<VoiceModulator>(modulator.m_index))
            {
                case VoiceModulator::Random1:
                case VoiceModulator::Random2:
                case VoiceModulator::Random3:
                case VoiceModulator::Random4:
                    return state.m_gangedRandomLFOUIState[modulator.m_index][track]
                        .m_history.ReadValue(voice, sample, value);
                case VoiceModulator::ModulationEnvelope:
                    return state.m_voiceScopeWriter.ReadHistory(
                        SmartGridOne::VoiceScopeIndex(static_cast<size_t>(Scope::ModulationEnvelope), track), voice, scopeIndex, lod, value);
                case VoiceModulator::AmpEnvelope:
                    return state.m_voiceScopeWriter.ReadHistory(
                        SmartGridOne::VoiceScopeIndex(static_cast<size_t>(Scope::AmpEnvelope), track), voice, scopeIndex, lod, value);
                case VoiceModulator::LFO1:
                    return state.m_voiceScopeWriter.ReadHistory(
                        SmartGridOne::VoiceScopeIndex(static_cast<size_t>(Scope::LFO1), track), voice, scopeIndex, lod, value);
                case VoiceModulator::LFO2:
                    return state.m_voiceScopeWriter.ReadHistory(
                        SmartGridOne::VoiceScopeIndex(static_cast<size_t>(Scope::LFO2), track), voice, scopeIndex, lod, value);
                case VoiceModulator::Sheaf1:
                case VoiceModulator::Sheaf2:
                case VoiceModulator::Sheaf3:
                {
                    const auto* writer = uiState.m_nonagonUIState.m_noteWriter;
                    NonagonNoteWriter::TimbreHistory::Event event;
                    if (!writer || !writer->m_timbreHistory[modulator.m_voice].FindAtOrBefore(sample, event))
                    {
                        return false;
                    }

                    value = static_cast<float>(event.m_values[
                        modulator.m_index - static_cast<size_t>(VoiceModulator::Sheaf1)]);
                    return true;
                }
                case VoiceModulator::Spread:
                    value = static_cast<float>(voice) / (voicesPerTrack - 1);
                    return true;
                case VoiceModulator::Noise:
                    value = Noise(modulator, sample);
                    return true;
            }

            break;
        }
        case Mode::Quad:
        {
            if (modulator.m_voice >= std::size(state.m_quadGangedRandomLFOUIState[0].m_voices))
            {
                return false;
            }

            using Scope = SmartGridOne::QuadScopes;
            switch (static_cast<QuadModulator>(modulator.m_index))
            {
                case QuadModulator::Random1:
                case QuadModulator::Random2:
                case QuadModulator::Random3:
                case QuadModulator::Random4:
                    return state.m_quadGangedRandomLFOUIState[modulator.m_index]
                        .m_history.ReadValue(modulator.m_voice, sample, value);
                case QuadModulator::LFO1:
                    return state.m_quadScopeWriter.ReadHistory(
                        static_cast<size_t>(Scope::QuadLFO1), modulator.m_voice, scopeIndex, lod, value);
                case QuadModulator::LFO2:
                    return state.m_quadScopeWriter.ReadHistory(
                        static_cast<size_t>(Scope::QuadLFO2), modulator.m_voice, scopeIndex, lod, value);
                case QuadModulator::Spread:
                    value = static_cast<float>(modulator.m_voice) / 3.0f;
                    return true;
                case QuadModulator::Noise:
                    value = Noise(modulator, sample);
                    return true;
            }

            break;
        }
        case Mode::Global:
        {
            if (modulator.m_voice != 0)
            {
                return false;
            }

            switch (static_cast<GlobalModulator>(modulator.m_index))
            {
                case GlobalModulator::Random1:
                case GlobalModulator::Random2:
                    return state.m_globalGangedRandomLFOUIState[
                        modulator.m_index - static_cast<size_t>(GlobalModulator::Random1)]
                        .m_history.ReadValue(0, sample, value);
                case GlobalModulator::Noise:
                    value = Noise(modulator, sample);
                    return true;
            }

            break;
        }
    }

    return false;
}

template<typename UIState>
bool ReadModulatorValue(const UIState& uiState, ModulatorRef modulator, double sample, float& value)
{
    return ReadModulatorValue(uiState, modulator, sample, 0, value);
}

template<typename UIState>
float GetModulatorValue(const UIState& uiState, ModulatorRef modulator, double sample)
{
    float value = 0.0f;
    ReadModulatorValue(uiState, modulator, sample, value);
    return value;
}

template<typename UIState>
double GetLatestSample(const UIState& uiState)
{
    const auto& state = uiState.m_squiggleBoyUIState;
    size_t count = state.m_voiceScopeWriter.m_acceptedIndex;
    return count == 0
        ? -1.0
        : static_cast<double>(state.m_firstScopeSample.load()) + static_cast<double>(count - 1);
}

struct ControlScope
{
    const ScopeWriter* m_writer = nullptr;
    size_t m_lane = 0;
    size_t m_voice = 0;
};

template<typename UIState>
ControlScope GetControlScope(const UIState& uiState, ModulatorRef modulator)
{
    const auto& state = uiState.m_squiggleBoyUIState;
    using Mode = SmartGridOneLayout::ParamMode;
    if (modulator.m_mode == Mode::Voice && modulator.m_voice < 9)
    {
        switch (static_cast<VoiceModulator>(modulator.m_index))
        {
            case VoiceModulator::ModulationEnvelope: return {&state.m_voiceScopeWriter, SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::ModulationEnvelope), modulator.m_voice / 3), modulator.m_voice % 3};
            case VoiceModulator::AmpEnvelope: return {&state.m_voiceScopeWriter, SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::AmpEnvelope), modulator.m_voice / 3), modulator.m_voice % 3};
            case VoiceModulator::LFO1: return {&state.m_voiceScopeWriter, SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::LFO1), modulator.m_voice / 3), modulator.m_voice % 3};
            case VoiceModulator::LFO2: return {&state.m_voiceScopeWriter, SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::LFO2), modulator.m_voice / 3), modulator.m_voice % 3};
            default: break;
        }
    }
    else if (modulator.m_mode == Mode::Quad && modulator.m_voice < 4)
    {
        switch (static_cast<QuadModulator>(modulator.m_index))
        {
            case QuadModulator::LFO1: return {&state.m_quadScopeWriter, static_cast<size_t>(SmartGridOne::QuadScopes::QuadLFO1), modulator.m_voice};
            case QuadModulator::LFO2: return {&state.m_quadScopeWriter, static_cast<size_t>(SmartGridOne::QuadScopes::QuadLFO2), modulator.m_voice};
            default: break;
        }
    }

    return {};
}

template<typename UIState>
bool GetRandomRound(const UIState& uiState, ModulatorRef modulator, double knownThrough,
    double sample, float& value, double& start, double& end)
{
    const auto& state = uiState.m_squiggleBoyUIState;
    using Mode = SmartGridOneLayout::ParamMode;
    switch (modulator.m_mode)
    {
        case Mode::Voice:
            return modulator.m_index < 4 && modulator.m_voice < 9
                && state.m_gangedRandomLFOUIState[modulator.m_index][modulator.m_voice / 3]
                    .m_history.GetRound(modulator.m_voice % 3, knownThrough, sample, value, start, end);
        case Mode::Quad:
            return modulator.m_index < 4 && modulator.m_voice < 4
                && state.m_quadGangedRandomLFOUIState[modulator.m_index]
                    .m_history.GetRound(modulator.m_voice, knownThrough, sample, value, start, end);
        case Mode::Global:
            return modulator.m_index >= 2 && modulator.m_index <= 3 && modulator.m_voice == 0
                && state.m_globalGangedRandomLFOUIState[modulator.m_index - 2]
                    .m_history.GetRound(0, knownThrough, sample, value, start, end);
    }

    return false;
}

template<typename UIState>
ModulatorTimeAxis GetTimeAxis(const UIState& uiState, ModulatorRef modulator)
{
    ModulatorTimeAxis axis;
    double now = GetLatestSample(uiState);
    if (now < 0.0)
    {
        return axis;
    }

    float unusedValue = 0.0f;
    if (GetRandomRound(uiState, modulator, now, now, unusedValue, axis.m_start, axis.m_end))
    {
        axis.m_present = std::clamp((now - axis.m_start) / (axis.m_end - axis.m_start), 0.0, 1.0);
        axis.m_predictive = true;
        return axis;
    }

    auto scope = GetControlScope(uiState, modulator);
    if (!scope.m_writer)
    {
        return axis;
    }

    ScopeReader reader(scope.m_writer, scope.m_voice, scope.m_lane, 256, 1);
    if (reader.m_empty)
    {
        return axis;
    }

    double origin = static_cast<double>(uiState.m_squiggleBoyUIState.m_firstScopeSample.load());
    axis.m_start = origin + reader.m_startIndex;
    axis.m_end = origin + reader.m_transferIndex;
    axis.m_wrappedStart = origin + reader.m_postTransferIndex;
    axis.m_transfer = reader.m_transferXSample / reader.m_numXSamples;
    axis.m_present = std::clamp((reader.m_transferXSample - 1.0) / reader.m_numXSamples, 0.0, 1.0);
    return axis;
}

// The historical API stays strict. Display sampling holds each source at its
// latest recorded sample, or predicts only the random round known at 'now'.
// Bipolar quad scopes are mapped to 0..1 before mixing or inversion.
//
template<typename UIState>
bool ReadDisplayValue(const UIState& uiState, ModulatorRef modulator, double sample, double now, size_t lod, float& value)
{
    value = 0.0f;
    double start = 0.0;
    double end = 0.0;
    if (sample > now && GetRandomRound(uiState, modulator, now, sample, value, start, end))
    {
        return true;
    }

    auto scope = GetControlScope(uiState, modulator);
    if (scope.m_writer)
    {
        if (lod >= ScopeWriter::x_numLevels || scope.m_writer->LatestIndex(lod) < 0.0)
        {
            return false;
        }

        double last = static_cast<double>(uiState.m_squiggleBoyUIState.m_firstScopeSample.load())
            + scope.m_writer->LatestIndex(lod);
        if (!ReadModulatorValue(uiState, modulator, std::min(sample, std::min(last, now)), lod, value))
        {
            return false;
        }

        return true;
    }

    return ReadModulatorValue(uiState, modulator, std::min(sample, now), lod, value);
}

// Prepare once for the complete display interval. All point reads retain the
// selected LOD, even when the primary axis wraps into its previous cycle.
//
template<typename UIState>
struct PreparedModulatorReader
{
    const UIState& m_uiState;
    WeightedModulators m_modulators;
    double m_now;
    std::array<size_t, WeightedModulators::x_capacity> m_lods{};

    PreparedModulatorReader(const UIState& uiState, const WeightedModulators& modulators,
        double oldestSample, double now)
        : m_uiState(uiState)
        , m_modulators(modulators)
        , m_now(now)
    {
        double origin = static_cast<double>(uiState.m_squiggleBoyUIState.m_firstScopeSample.load());
        double index = std::max(0.0, (oldestSample - origin));
        for (size_t i = 0; i < m_modulators.m_count; ++i)
        {
            auto scope = GetControlScope(uiState, m_modulators.m_entries[i].m_modulator);
            if (scope.m_writer)
            {
                size_t accepted = scope.m_writer->m_acceptedIndex;
                m_lods[i] = scope.m_writer->GetLod(
                    std::max(0.0, static_cast<double>(accepted) - 1.0 - index), accepted);
            }
        }
    }

    float Get(double sample) const
    {
        if (!std::isfinite(sample) || m_now < 0.0
            || sample < static_cast<double>(m_uiState.m_squiggleBoyUIState.m_firstScopeSample.load()))
        {
            return 0.0f;
        }

        float result = 0.0f;
        for (size_t i = 0; i < m_modulators.m_count; ++i)
        {
            const auto& entry = m_modulators.m_entries[i];
            float value = 0.0f;
            if (ReadDisplayValue(m_uiState, entry.m_modulator, sample, m_now, m_lods[i], value))
            {
                result += std::abs(entry.m_weight) * (entry.m_weight < 0.0f ? 1.0f - value : value);
            }
        }

        return result;
    }
};

template<typename UIState>
float GetWeightedModulatorValue(const UIState& uiState, const WeightedModulators& modulators,
    double sample, double now)
{
    PreparedModulatorReader<UIState> reader(uiState, modulators, std::min(sample, now), now);
    return reader.Get(sample);
}

template<typename UIState>
float GetWeightedModulatorValue(const UIState& uiState, const WeightedModulators& modulators, double sample)
{
    return GetWeightedModulatorValue(uiState, modulators, sample, GetLatestSample(uiState));
}
}
