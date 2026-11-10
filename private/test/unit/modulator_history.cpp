#include "doctest.h"
#include "../support/SynthRig.hpp"
#include "SampleEventHistory.hpp"
#include <limits>
#include <thread>

namespace
{

constexpr size_t x_lfo1Scope = SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::LFO1), 0);
constexpr size_t x_lfo2Scope = SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::LFO2), 0);

void AdvanceControlIndex(ScopeWriter& writer)
{
    for (size_t i = 0; i < writer.m_decimation; ++i)
    {
        writer.AdvanceIndex();
    }
}

}

DOCTEST_TEST_CASE("Modulator time axes: warped scope and overlays read the same forward visit")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState;
    state.m_firstScopeSample.store(1000);
    TimeWarpInverter<1024> inverse;
    auto& writer = state.m_voiceScopeWriter;
    writer.SetTimeWarp(x_lfo1Scope, &inverse);
    writer.SetPeriodic(x_lfo1Scope, 0, {0.0, 20.0});
    for (size_t sample = 0; sample <= 80; ++sample)
    {
        double warped = sample <= 40 ? sample * 0.5 : 20.0 + (sample - 40) * 0.25;
        inverse.Write(warped, 1000.0 + sample);
        writer.Write(x_lfo1Scope, TrioFloat(static_cast<float>(sample) / 100.0f, 0, 0));
        writer.Write(x_lfo2Scope, TrioFloat(static_cast<float>(sample) / 200.0f, 0, 0));
        writer.AdvanceIndex();
    }

    writer.Publish();
    writer.AcceptSamples();
    auto axis = SmartGridOneModulatorHistory::GetTimeAxis(*ui, ModulatorRef(VoiceModulator::LFO1, 0));
    DOCTEST_CHECK(axis.GetSample(0.25) == 1060.0);
    DOCTEST_CHECK(axis.GetSample(0.75) == 1030.0);
    WeightedModulators weights;
    weights.Add(ModulatorRef(VoiceModulator::LFO2, 0), 1.0f);
    SmartGridOneModulatorHistory::PreparedModulatorReader overlay(*ui, weights, axis.OldestSample(), 1080.0);
    DOCTEST_CHECK(overlay.Get(axis.GetSample(0.25)) == doctest::Approx(0.3f));
    DOCTEST_CHECK(overlay.Get(axis.GetSample(0.75)) == doctest::Approx(0.15f));
}

DOCTEST_TEST_CASE("Weighted modulators: normalized signed depths mix recorded signals and hold the latest sample")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState;
    state.m_firstScopeSample.store(104);
    for (size_t index = 0; index < 4; ++index)
    {
        state.m_voiceScopeWriter.Write(x_lfo1Scope, TrioFloat(0.2f * (index + 1), 0.0f, 0.0f));
        state.m_voiceScopeWriter.Write(x_lfo2Scope, TrioFloat(0.4f, 0.0f, 0.0f));
        AdvanceControlIndex(state.m_voiceScopeWriter);
        state.m_quadScopeWriter.Write(static_cast<size_t>(SmartGridOne::QuadScopes::QuadLFO1), QuadFloat(0.25f, 0.0f, 0.0f, 0.0f));
        AdvanceControlIndex(state.m_quadScopeWriter);
    }

    state.m_voiceScopeWriter.Publish();
    state.m_voiceScopeWriter.AcceptSamples();
    state.m_quadScopeWriter.Publish();
    state.m_quadScopeWriter.AcceptSamples();
    WeightedModulators weights;
    weights.Add(ModulatorRef(VoiceModulator::LFO1, 0), 0.1f);
    weights.Add(ModulatorRef(VoiceModulator::LFO2, 0), -0.3f);
    weights.Add(ModulatorRef(VoiceModulator::Noise, 0), 0.0f);
    weights.Normalize();
    DOCTEST_REQUIRE(weights.m_count == 2);
    DOCTEST_CHECK(weights.m_entries[0].m_weight == doctest::Approx(0.25f));
    DOCTEST_CHECK(weights.m_entries[1].m_weight == doctest::Approx(-0.75f));
    using SmartGridOneModulatorHistory::GetWeightedModulatorValue;
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 104.0 + 1.0) == doctest::Approx(0.55f));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 104.0 + 1.5) == doctest::Approx(0.575f));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 107.0) == doctest::Approx(0.65f));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 10000.0) == doctest::Approx(0.65f));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 103.0) == 0.0f);
    state.m_voiceScopeWriter.m_oldestHistoryIndex = 2;
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 104.0 + 1.0) == 0.0f);

    WeightedModulators bipolar;
    bipolar.Add(ModulatorRef(QuadModulator::LFO1, 0), -1.0f);
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, bipolar, 104.0 + 2.0) == doctest::Approx(0.75f));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, bipolar, 103.0) == 0.0f);
    WeightedModulators missing;
    missing.Add(ModulatorRef(VoiceModulator::Sheaf1, 0), -1.0f);
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, missing, 104.0 + 2.0) == 0.0f);
    WeightedModulators noise;
    noise.Add(ModulatorRef(GlobalModulator::Noise), 1.0f);
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, noise, 10000.0)
        == ui->GetModulatorValue(ModulatorRef(GlobalModulator::Noise), 107.0));
}

DOCTEST_TEST_CASE("Weighted modulators: future random samples use only the published round and then hold its target")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState;
    state.m_firstScopeSample.store(100);
    state.m_voiceScopeWriter.m_acceptedIndex = 1 + 32;
    GangedRandomLFOVoice voices[3];
    GangedRandomLFOVoiceInput inputs[3];
    for (size_t voice = 0; voice < 3; ++voice)
    {
        voices[voice].m_source = 0.2f;
        voices[voice].m_target = 0.8f;
        inputs[voice] = {1.0 / 20.0, 1.0 / 40.0, 0.0f};
    }

    auto& history = state.m_gangedRandomLFOUIState[0][1].m_history;
    history.RecordRound(100, voices, inputs);
    for (auto& voice : voices)
    {
        voice.Reset(0.1f);
    }

    history.RecordRound(164, voices, inputs);
    ModulatorRef source(VoiceModulator::Random1, 4);
    WeightedModulators weights;
    weights.Add(source, 1.0f);
    using SmartGridOneModulatorHistory::GetWeightedModulatorValue;
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 110.0) == doctest::Approx(0.2f + 0.18f * 10 / 32));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 140.0) == doctest::Approx(0.38f + 0.42f * 8 / 32));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 10000.0) == doctest::Approx(0.8f));
    auto axis = SmartGridOneModulatorHistory::GetTimeAxis(*ui, source);
    DOCTEST_CHECK(axis.GetExtent() == 64.0);
    DOCTEST_CHECK(axis.GetSample(0.0) == 100.0);
    DOCTEST_CHECK(axis.GetSample(1.0) == 164.0);
    DOCTEST_CHECK(axis.m_present == doctest::Approx(32.0 / 64.0));
    DOCTEST_CHECK(axis.m_predictive);
}

DOCTEST_TEST_CASE("Modulator time axes: scope wrapping maps every point to its original absolute sample")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState;
    state.m_firstScopeSample.store(104);
    auto& scope = state.m_voiceScopeWriter;
    for (size_t index = 0; index < 12; ++index)
    {
        if (index == 0 || index == 8)
        {
            scope.RecordStart(x_lfo1Scope, 0);
        }

        scope.Write(x_lfo1Scope, TrioFloat(static_cast<float>(index) / 12.0f, 0.0f, 0.0f));
        AdvanceControlIndex(scope);
    }

    scope.Publish();
    scope.AcceptSamples();
    ModulatorRef source(VoiceModulator::LFO1, 0);
    auto axis = SmartGridOneModulatorHistory::GetTimeAxis(*ui, source);
    DOCTEST_CHECK(axis.GetExtent() == 8.0);
    DOCTEST_CHECK(axis.m_transfer == doctest::Approx(3.0 / 8.0));
    ScopeReader reader(&scope, 0, x_lfo1Scope, 256, 1);
    for (size_t point = 0; point < 256; ++point)
    {
        double sample = axis.GetSample(static_cast<double>(point) / 256.0);
        DOCTEST_CHECK(ui->GetModulatorValue(source, sample) == doctest::Approx(reader.Get(point)));
    }

    DOCTEST_CHECK(SmartGridOneModulatorHistory::GetTimeAxis(*ui,
        ModulatorRef(VoiceModulator::Sheaf1, 0)).GetExtent() == 0.0);
    DOCTEST_CHECK(SmartGridOneModulatorHistory::GetTimeAxis(*ui,
        ModulatorRef(VoiceModulator::Spread, 0)).GetExtent() == 0.0);
    DOCTEST_CHECK(SmartGridOneModulatorHistory::GetTimeAxis(*ui,
        ModulatorRef(GlobalModulator::Noise)).GetExtent() == 0.0);
}

DOCTEST_TEST_CASE("Modulator history: sparse events retain held values and reject overwritten generations")
{
    SampleEventHistory<2, 3> history;
    SampleEventHistory<2, 3>::Event result;
    DOCTEST_CHECK_FALSE(history.FindAtOrBefore(0.0, result));
    for (size_t i = 0; i < 5; ++i)
    {
        history.Record({10 * i, {static_cast<double>(i), -static_cast<double>(i)}});
    }

    DOCTEST_CHECK_FALSE(history.FindAtOrBefore(19.99, result));
    DOCTEST_REQUIRE(history.FindAtOrBefore(20.0, result));
    DOCTEST_CHECK(result.m_values[0] == 2.0);
    DOCTEST_REQUIRE(history.FindAtOrBefore(39.5, result));
    DOCTEST_CHECK(result.m_values[0] == 3.0);
    DOCTEST_REQUIRE(history.FindAtOrBefore(10000.0, result));
    DOCTEST_CHECK(result.m_values[0] == 4.0);
    DOCTEST_CHECK_FALSE(history.Read(0, result));

    history.Record({40, {9.0, -9.0}});
    DOCTEST_REQUIRE(history.FindAtOrBefore(40.0, result));
    DOCTEST_CHECK(result.m_values[0] == 9.0);
}

DOCTEST_TEST_CASE("Modulator history: concurrent event overwrite never returns mixed fields")
{
    SampleEventHistory<2, 8> history;
    std::atomic<bool> finished{false};
    std::thread writer([&]()
    {
        for (size_t i = 0; i < 10000; ++i)
        {
            history.Record({i, {static_cast<double>(i), -static_cast<double>(i)}});
        }

        finished.store(true);
    });

    do
    {
        size_t count = history.m_count.load();
        SampleEventHistory<2, 8>::Event result;
        if (count > 0 && history.FindAtOrBefore(static_cast<double>(count - 1), result))
        {
            DOCTEST_CHECK(result.m_values[0] == static_cast<double>(result.m_sample));
            DOCTEST_CHECK(result.m_values[1] == -static_cast<double>(result.m_sample));
        }
    }
    while (!finished.load());

    writer.join();
}

DOCTEST_TEST_CASE("Modulator history: reads use accepted bounds and permit overwrite during a stalled frame")
{
    auto scope = std::make_unique<ScopeWriterGeneric<1, 1>>(1, 4096);
    float value = 123.0f;
    DOCTEST_CHECK_FALSE(scope->ReadHistory(0, 0, 0.0, 0, value));
    DOCTEST_CHECK(value == 0.0f);
    scope->Write(0, 0.2f);
    scope->AdvanceIndex();
    scope->Write(0, 0.8f);
    scope->AdvanceIndex();
    scope->Publish();
    scope->AcceptSamples();
    DOCTEST_REQUIRE(scope->ReadHistory(0, 0, 0.5, 0, value));
    DOCTEST_CHECK(value == doctest::Approx(0.5f));
    DOCTEST_REQUIRE(scope->ReadHistory(0, 0, 1.0, 0, value));
    DOCTEST_CHECK(value == doctest::Approx(0.8f));
    for (double sample : {-1.0, 1.01, 2.0, std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN()})
    {
        DOCTEST_CHECK_FALSE(scope->ReadHistory(0, 0, sample, 0, value));
        DOCTEST_CHECK(value == 0.0f);
    }

    DOCTEST_CHECK_FALSE(scope->ReadHistory(1, 0, 0.0, 0, value));
    DOCTEST_CHECK_FALSE(scope->ReadHistory(0, 1, 0.0, 0, value));
    scope->m_index = scope->MaxIndexes();
    scope->Write(0, 0.9f);
    DOCTEST_REQUIRE(scope->ReadHistory(0, 0, 0.0, 0, value));
    DOCTEST_CHECK(value == doctest::Approx(0.9f));
    DOCTEST_REQUIRE(scope->ReadHistory(0, 0, 1.0, 0, value));
    DOCTEST_CHECK(value == doctest::Approx(0.8f));
    scope->AdvanceIndex();
    scope->Publish();
    scope->AcceptSamples();
    DOCTEST_CHECK_FALSE(scope->ReadHistory(0, 0, 0.0, 0, value));
    DOCTEST_REQUIRE(scope->ReadHistory(0, 0, static_cast<double>(scope->MaxIndexes()), 0, value));
    DOCTEST_CHECK(value == doctest::Approx(0.9f));
}

DOCTEST_TEST_CASE("Modulator history: random curves hold for the gang and retain earlier rounds")
{
    GangedRandomLFOHistory<2> history;
    GangedRandomLFOVoice voices[2];
    GangedRandomLFOVoiceInput inputs[2];
    voices[0].Reset(1.0f);
    voices[1].Reset(0.8f);
    inputs[0] = {0.5, 0.5, 0.0f};
    inputs[1] = {0.25, 0.25, 1.0f};
    history.RecordRound(100, voices, inputs);
    for (size_t elapsed = 0; elapsed <= 32; ++elapsed)
    {
        DOCTEST_CHECK(history.GetValue(0, 100.0 + elapsed) == doctest::Approx(elapsed / 32.0f));
        DOCTEST_CHECK(history.GetValue(1, 100.0 + elapsed) == doctest::Approx(0.8f * elapsed / 32.0f));
    }

    DOCTEST_CHECK(history.GetValue(0, 102.5) == doctest::Approx(2.5f / 32));
    voices[0].Reset(0.25f);
    voices[1].Reset(0.5f);
    history.RecordRound(132, voices, inputs);
    DOCTEST_CHECK(history.GetValue(0, 116.0) == doctest::Approx(0.5f));
    DOCTEST_CHECK(history.GetValue(0, 132.0) == doctest::Approx(1.0f));
    DOCTEST_CHECK(history.GetValue(0, 148.0) == doctest::Approx(0.625f));
    DOCTEST_CHECK(history.GetValue(0, 99.99) == 0.0f);
}

DOCTEST_TEST_CASE("Modulator history: UI reconstructs every random lane and interpolates recorded scopes")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& ui = rig.UIState();
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Spread, 1), 0.0) == 0.0f);
    for (auto& input : synth.m_gangedRandomLFOInput)
    {
        input.m_waiting = {4.0 / 48000.0, 0.0, 0.0};
        input.m_moving = {8.0 / 48000.0, 0.0, 0.0};
    }

    for (auto& input : synth.m_globalGangedRandomLFOInput)
    {
        input.m_waiting = {4.0 / 48000.0, 0.0, 0.0};
        input.m_moving = {8.0 / 48000.0, 0.0, 0.0};
    }

    struct Observation
    {
        ModulatorRef m_modulator;
        double m_sample;
        float m_value;
    };

    std::vector<Observation> random;
    std::vector<Observation> scopes;
    for (size_t sample = 1; sample <= 64; ++sample)
    {
        rig.RunSamples(1);
        for (size_t slot = 0; slot < SmartGridOneModulators::x_numVoiceRandoms; ++slot)
        {
            for (size_t voice = 0; voice < 9; ++voice)
            {
                random.push_back({ModulatorRef(SmartGridOneModulators::GetVoiceRandom(slot), voice),
                    static_cast<double>(sample), synth.m_tracks[voice / 3].m_gangedRandomLFO[slot].Output(voice % 3)});
            }

            for (size_t channel = 0; channel < 4 && slot < SmartGridOneModulators::x_numQuadRandoms; ++channel)
            {
                random.push_back({ModulatorRef(SmartGridOneModulators::GetQuadRandom(slot), channel),
                    static_cast<double>(sample), synth.m_quadGangedRandomLFO[slot].Output(channel)});
            }
        }

        for (size_t slot = 0; slot < 2; ++slot)
        {
            random.push_back({ModulatorRef(static_cast<GlobalModulator>(slot + 2)),
                static_cast<double>(sample), synth.m_globalGangedRandomLFO[slot].Output(0)});
        }

        for (size_t voice = 0; voice < 9; ++voice)
        {
            auto& source = synth.m_tracks[voice / 3];
            const size_t lane = voice % 3;
            scopes.push_back({ModulatorRef(VoiceModulator::LFO1, voice),
                static_cast<double>(sample), source.m_squiggleLFO[0].m_polyXFader.m_output[lane]});
            scopes.push_back({ModulatorRef(VoiceModulator::LFO2, voice),
                static_cast<double>(sample), source.m_squiggleLFO[1].m_polyXFader.m_output[lane]});
            scopes.push_back({ModulatorRef(VoiceModulator::ModulationEnvelope, voice),
                static_cast<double>(sample), source.m_amp.m_modulationAHD.m_output[lane]});
            scopes.push_back({ModulatorRef(VoiceModulator::AmpEnvelope, voice),
                static_cast<double>(sample), source.m_amp.m_ahd.m_output[lane]});
        }

        for (size_t channel = 0; channel < 4; ++channel)
        {
            scopes.push_back({ModulatorRef(QuadModulator::LFO1, channel),
                static_cast<double>(sample), synth.m_quadSquiggleLFO[0].m_polyXFader.m_output[channel]});
            scopes.push_back({ModulatorRef(QuadModulator::LFO2, channel),
                static_cast<double>(sample), synth.m_quadSquiggleLFO[1].m_polyXFader.m_output[channel]});
        }
    }

    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Spread, 1), 32.0) == 0.0f);
    rig.Internal().PopulateUIState();
    ui.m_squiggleBoyUIState.AcceptScopeSamples();
    DOCTEST_CHECK(ui.m_squiggleBoyUIState.m_firstScopeSample.load() == 1);
    for (const auto& observation : random)
    {
        DOCTEST_CAPTURE(observation.m_sample);
        DOCTEST_CAPTURE(observation.m_modulator.m_index);
        DOCTEST_CAPTURE(observation.m_modulator.m_voice);
        DOCTEST_CHECK(ui.GetModulatorValue(observation.m_modulator, observation.m_sample)
            == doctest::Approx(observation.m_value).epsilon(0.00001));
    }

    constexpr size_t x_scopeLanes = 44;
    DOCTEST_REQUIRE(scopes.size() == 64 * x_scopeLanes);
    for (size_t i = 0; i < scopes.size(); ++i)
    {
        const auto& observation = scopes[i];
        DOCTEST_CHECK(ui.GetModulatorValue(observation.m_modulator, observation.m_sample)
            == doctest::Approx(observation.m_value));
        if (i >= x_scopeLanes)
        {
            DOCTEST_CHECK(ui.GetModulatorValue(observation.m_modulator, observation.m_sample - 0.5)
                == doctest::Approx((observation.m_value + scopes[i - x_scopeLanes].m_value) * 0.5f));
        }
    }

    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::LFO1, 0), 0.99) == 0.0f);
    for (size_t voice = 0; voice < 9; ++voice)
    {
        DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Spread, voice), 20.5)
            == doctest::Approx(static_cast<float>(voice % 3) / 2.0f));
    }

    for (size_t channel = 0; channel < 4; ++channel)
    {
        DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(QuadModulator::Spread, channel), 20.5)
            == doctest::Approx(static_cast<float>(channel) / 3.0f));
    }

    const ModulatorRef x_noise[] =
    {
        ModulatorRef(VoiceModulator::Noise, 0), ModulatorRef(VoiceModulator::Noise, 1),
        ModulatorRef(QuadModulator::Noise, 0), ModulatorRef(GlobalModulator::Noise)};
    const double beyondPresent = SmartGridOneModulatorHistory::GetLatestSample(ui) + 0.01;
    for (auto noise : x_noise)
    {
        float value = ui.GetModulatorValue(noise, 20.0);
        DOCTEST_CHECK(value >= 0.0f);
        DOCTEST_CHECK(value < 1.0f);
        DOCTEST_CHECK(ui.GetModulatorValue(noise, 20.99) == value);
        DOCTEST_CHECK(ui.GetModulatorValue(noise, 21.0) != value);
        for (double sample : {-1.0, 0.0, beyondPresent, std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN()})
        {
            DOCTEST_CHECK(ui.GetModulatorValue(noise, sample) == 0.0f);
        }
    }

    DOCTEST_CHECK(ui.GetModulatorValue(x_noise[0], 20.0) != ui.GetModulatorValue(x_noise[1], 20.0));
    DOCTEST_CHECK(ui.GetModulatorValue(x_noise[0], 20.0) != ui.GetModulatorValue(x_noise[2], 20.0));
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Spread, 9), 20.0) == 0.0f);
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(QuadModulator::Spread, 4), 20.0) == 0.0f);
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(static_cast<VoiceModulator>(SmartGridOneModulators::x_numSourceIndices), 0), 20.0) == 0.0f);

    auto& writer = rig.Internal().m_nonagon.m_nonagon.m_noteWriter;
    NonagonNoteWriter::EventData note;
    note.m_voiceIx = 4;
    note.m_startSample = 12;
    note.m_timbre[0] = 0.2f;
    note.m_timbre[1] = 0.4f;
    note.m_timbre[2] = 0.6f;
    writer.RecordNote(note);
    writer.RecordStartIndex();
    writer.RecordNoteEnd(4, 0.5f);
    DOCTEST_CHECK(writer.m_timbreHistory[4].m_count.load() == 1);
    note.m_startSample = 40;
    note.m_timbre[0] = 0.8f;
    writer.RecordNote(note);
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Sheaf1, 4), 11.99) == 0.0f);
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Sheaf1, 4), 39.99) == doctest::Approx(0.2f));
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Sheaf1, 4), 40.0) == doctest::Approx(0.8f));
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Sheaf2, 4), 24.0) == doctest::Approx(0.4f));
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Sheaf3, 4), 64.0) == doctest::Approx(0.6f));
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Sheaf1, 3), 24.0) == 0.0f);

    auto& nonagon = rig.Internal().m_nonagon;
    nonagon.m_nonagon.m_multiPhasorGate.m_ahdControl[4].m_trig = true;
    nonagon.m_nonagon.SetOutputs(nonagon.m_state);
    DOCTEST_CHECK(writer.Get(writer.GetIndex() - 1).m_startSample == SampleTimer::GetSample());
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Sheaf1, 4), static_cast<double>(SampleTimer::GetSample()))
        == doctest::Approx(nonagon.m_nonagon.m_output.m_extraTimbre[4][0]));
}
DOCTEST_TEST_CASE("Modulator history: prepared mixtures keep one LOD across long trio and quad histories")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState;
    size_t storage = state.m_voiceScopeWriter.SampleStorageBytes()
        + state.m_quadScopeWriter.SampleStorageBytes()
        + state.m_monoScopeWriter.SampleStorageBytes();
    DOCTEST_CHECK(storage == 232 * sizeof(float) * ScopeWriter::x_baseCapacity * ScopeWriter::x_numLevels);
    DOCTEST_CHECK(state.m_voiceScopeWriter.m_scopes == 30);
    DOCTEST_CHECK(state.m_quadScopeWriter.m_scopes == 27);
    DOCTEST_CHECK(state.m_monoScopeWriter.m_scopes == 4);
    DOCTEST_CHECK(state.m_voiceScopeWriter.m_voices == 3);
    DOCTEST_CHECK(state.m_quadScopeWriter.m_voices == 4);
    DOCTEST_CHECK(state.m_monoScopeWriter.m_voices == 1);
    constexpr size_t x_samples = 2 * ScopeWriter::x_baseCapacity;
    const size_t ampScope = SmartGridOne::VoiceScopeIndex(
        static_cast<size_t>(SmartGridOne::VoiceScopes::AmpEnvelope), 2);
    const size_t quadScope = static_cast<size_t>(SmartGridOne::QuadScopes::QuadLFO2);
    for (size_t index = 0; index < x_samples; ++index)
    {
        state.m_voiceScopeWriter.Write(ampScope, TrioFloat(0.0f, 0.0f, static_cast<float>(index) / x_samples));
        state.m_quadScopeWriter.Write(quadScope, QuadFloat(0.0f, 0.0f, 0.0f, 0.2f));
        state.AdvanceScopeIndices();
        if (index % 1024 == 1023)
        {
            state.m_voiceScopeWriter.Publish();
            state.m_quadScopeWriter.Publish();
            state.AcceptScopeSamples();
        }
    }

    state.m_voiceScopeWriter.Publish();
    state.m_voiceScopeWriter.AcceptSamples();
    state.m_quadScopeWriter.Publish();
    state.m_quadScopeWriter.AcceptSamples();
    state.m_firstScopeSample.store(0);
    WeightedModulators weights;
    weights.Add(ModulatorRef(VoiceModulator::AmpEnvelope, 8), 0.75f);
    weights.Add(ModulatorRef(QuadModulator::LFO2, 3), -0.25f);
    double now = (x_samples - 1);
    SmartGridOneModulatorHistory::PreparedModulatorReader reader(*ui, weights,
        ScopeWriter::x_baseCapacity / 4.0, now);
    DOCTEST_REQUIRE(reader.m_lods[0] == 1);
    DOCTEST_REQUIRE(reader.m_lods[1] == 1);
    for (double index : {ScopeWriter::x_baseCapacity / 4.0 + 0.5,
        ScopeWriter::x_baseCapacity + 0.25, x_samples - 1000.0})
    {
        DOCTEST_CHECK(reader.Get(index)
            == doctest::Approx(0.75 * index / x_samples + 0.2).epsilon(0.00001));
    }

    DOCTEST_CHECK(reader.m_lods[0] == 1);
    DOCTEST_CHECK(reader.m_lods[1] == 1);
}
