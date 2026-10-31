#include "doctest.h"
#include "../support/SynthRig.hpp"
#include "SampleEventHistory.hpp"
#include <limits>
#include <thread>

DOCTEST_TEST_CASE("Weighted modulators: normalized signed depths mix recorded signals and hold the latest sample")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState;
    state.m_firstAudioSample.store(100);
    state.m_audioScopeWriter.m_publishedIndex.store(33);
    state.m_firstControlSample.store(104);
    for (size_t index = 0; index < 4; ++index)
    {
        state.m_controlScopeWriter.Write(0, 0, 0.2f * (index + 1));
        state.m_controlScopeWriter.Write(1, 0, 0.4f);
        state.m_controlScopeWriter.AdvanceIndex();
        state.m_quadControlScopeWriter.Write(0, 0, -0.5f);
        state.m_quadControlScopeWriter.AdvanceIndex();
    }

    state.m_controlScopeWriter.Publish();
    state.m_quadControlScopeWriter.Publish();
    WeightedModulators weights;
    weights.Add(ModulatorRef(VoiceModulator::LFO1, 0), 0.1f);
    weights.Add(ModulatorRef(VoiceModulator::LFO2, 0), -0.3f);
    weights.Add(ModulatorRef(VoiceModulator::Noise, 0), 0.0f);
    weights.Normalize();
    DOCTEST_REQUIRE(weights.m_count == 2);
    DOCTEST_CHECK(weights.m_entries[0].m_weight == doctest::Approx(0.25f));
    DOCTEST_CHECK(weights.m_entries[1].m_weight == doctest::Approx(-0.75f));
    using SmartGridOneModulatorHistory::GetWeightedModulatorValue;
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 112.0) == doctest::Approx(0.55f));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 116.0) == doctest::Approx(0.575f));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 132.0) == doctest::Approx(0.65f));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 10000.0) == doctest::Approx(0.65f));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 103.0) == 0.0f);
    state.m_controlScopeWriter.m_oldestHistoryIndex.store(2);
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 112.0) == 0.0f);

    WeightedModulators bipolar;
    bipolar.Add(ModulatorRef(QuadModulator::DelayLFO, 0), -1.0f);
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, bipolar, 120.0) == doctest::Approx(0.75f));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, bipolar, 103.0) == 0.0f);
    WeightedModulators missing;
    missing.Add(ModulatorRef(VoiceModulator::Sheaf1, 0), -1.0f);
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, missing, 120.0) == 0.0f);
    WeightedModulators noise;
    noise.Add(ModulatorRef(GlobalModulator::Noise), 1.0f);
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, noise, 10000.0)
        == ui->GetModulatorValue(ModulatorRef(GlobalModulator::Noise), 132.0));
}

DOCTEST_TEST_CASE("Weighted modulators: future random samples use only the published round and then hold its target")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState;
    state.m_firstAudioSample.store(100);
    state.m_audioScopeWriter.m_publishedIndex.store(33);
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

    history.RecordRound(160, voices, inputs);
    ModulatorRef source(VoiceModulator::Random1, 4);
    WeightedModulators weights;
    weights.Add(source, 1.0f);
    using SmartGridOneModulatorHistory::GetWeightedModulatorValue;
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 110.0) == doctest::Approx(0.2f));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 140.0) == doctest::Approx(0.5f));
    DOCTEST_CHECK(GetWeightedModulatorValue(*ui, weights, 10000.0) == doctest::Approx(0.8f));
    auto axis = SmartGridOneModulatorHistory::GetTimeAxis(*ui, source);
    DOCTEST_CHECK(axis.GetExtent() == 60.0);
    DOCTEST_CHECK(axis.GetSample(0.0) == 100.0);
    DOCTEST_CHECK(axis.GetSample(1.0) == 160.0);
    DOCTEST_CHECK(axis.m_present == doctest::Approx(32.0 / 60.0));
    DOCTEST_CHECK(axis.m_predictive);
}

DOCTEST_TEST_CASE("Modulator time axes: scope wrapping maps every point to its original absolute sample")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState;
    state.m_firstAudioSample.store(100);
    state.m_audioScopeWriter.m_publishedIndex.store(100);
    state.m_firstControlSample.store(104);
    auto& scope = state.m_controlScopeWriter;
    for (size_t index = 0; index < 12; ++index)
    {
        if (index == 0 || index == 8)
        {
            scope.RecordStart(0, 0);
        }

        scope.Write(0, 0, static_cast<float>(index) / 12.0f);
        scope.AdvanceIndex();
    }

    scope.Publish();
    ModulatorRef source(VoiceModulator::LFO1, 0);
    auto axis = SmartGridOneModulatorHistory::GetTimeAxis(*ui, source);
    DOCTEST_CHECK(axis.GetExtent() == 64.0);
    DOCTEST_CHECK(axis.m_transfer == doctest::Approx(3.0 / 8.0));
    ScopeReader reader(&scope, 0, 0, 256, 1);
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

DOCTEST_TEST_CASE("Modulator history: scope reads interpolate only retained published samples")
{
    auto scope = std::make_unique<ScopeWriter>(1, 1);
    float value = 123.0f;
    DOCTEST_CHECK_FALSE(scope->ReadHistory(0, 0, 0.0, value));
    DOCTEST_CHECK(value == 0.0f);
    scope->Write(0, 0, 0.2f);
    scope->AdvanceIndex();
    scope->Write(0, 0, 0.8f);
    scope->AdvanceIndex();
    scope->Publish();
    DOCTEST_REQUIRE(scope->ReadHistory(0, 0, 0.5, value));
    DOCTEST_CHECK(value == doctest::Approx(0.5f));
    DOCTEST_REQUIRE(scope->ReadHistory(0, 0, 1.0, value));
    DOCTEST_CHECK(value == doctest::Approx(0.8f));
    for (double sample : {-1.0, 1.01, 2.0, std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN()})
    {
        DOCTEST_CHECK_FALSE(scope->ReadHistory(0, 0, sample, value));
        DOCTEST_CHECK(value == 0.0f);
    }

    DOCTEST_CHECK_FALSE(scope->ReadHistory(1, 0, 0.0, value));
    DOCTEST_CHECK_FALSE(scope->ReadHistory(0, 1, 0.0, value));
    scope->m_index = scope->MaxIndexes();
    scope->Write(0, 0, 0.9f);
    DOCTEST_CHECK_FALSE(scope->ReadHistory(0, 0, 0.0, value));
    DOCTEST_CHECK(value == 0.0f);
    DOCTEST_REQUIRE(scope->ReadHistory(0, 0, 1.0, value));
    DOCTEST_CHECK(value == doctest::Approx(0.8f));
    scope->AdvanceIndex();
    scope->Publish();
    DOCTEST_REQUIRE(scope->ReadHistory(0, 0, static_cast<double>(scope->MaxIndexes()), value));
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
    for (size_t elapsed = 1; elapsed <= 8; ++elapsed)
    {
        for (size_t voice = 0; voice < 2; ++voice)
        {
            float expected = voices[voice].Process(inputs[voice]);
            DOCTEST_CHECK(history.GetValue(voice, 100.0 + elapsed) == doctest::Approx(expected));
        }
    }

    DOCTEST_CHECK(history.GetValue(0, 102.5) == doctest::Approx(0.25f));
    DOCTEST_CHECK(history.GetValue(0, 107.0) == doctest::Approx(1.0f));
    voices[0].Reset(0.25f);
    voices[1].Reset(0.5f);
    history.RecordRound(108, voices, inputs);
    DOCTEST_CHECK(history.GetValue(0, 103.0) == doctest::Approx(0.5f));
    DOCTEST_CHECK(history.GetValue(0, 108.0) == doctest::Approx(1.0f));
    DOCTEST_CHECK(history.GetValue(0, 111.0) == doctest::Approx(0.625f));
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
        for (size_t slot = 0; slot < 4; ++slot)
        {
            for (size_t voice = 0; voice < 9; ++voice)
            {
                random.push_back({ModulatorRef(static_cast<VoiceModulator>(slot), voice),
                    static_cast<double>(sample), synth.m_gangedRandomLFO[slot][voice / 3].Output(voice % 3)});
            }

            for (size_t channel = 0; channel < 4; ++channel)
            {
                random.push_back({ModulatorRef(static_cast<QuadModulator>(slot), channel),
                    static_cast<double>(sample), synth.m_quadGangedRandomLFO[slot].Output(channel)});
            }
        }

        for (size_t slot = 0; slot < 2; ++slot)
        {
            random.push_back({ModulatorRef(static_cast<GlobalModulator>(slot + 2)),
                static_cast<double>(sample), synth.m_globalGangedRandomLFO[slot].Output(0)});
        }

        if (sample % 8 == 0)
        {
            for (size_t voice = 0; voice < 9; ++voice)
            {
                auto& source = synth.m_voices[voice];
                scopes.push_back({ModulatorRef(VoiceModulator::LFO1, voice),
                    static_cast<double>(sample), source.m_squiggleLFO[0].m_polyXFader.m_output});
                scopes.push_back({ModulatorRef(VoiceModulator::LFO2, voice),
                    static_cast<double>(sample), source.m_squiggleLFO[1].m_polyXFader.m_output});
                scopes.push_back({ModulatorRef(VoiceModulator::ModulationEnvelope, voice),
                    static_cast<double>(sample), source.m_amp.m_modulationAHD.m_output});
                scopes.push_back({ModulatorRef(VoiceModulator::AmpEnvelope, voice),
                    static_cast<double>(sample), source.m_amp.m_ahd.m_output});
            }

            for (size_t channel = 0; channel < 4; ++channel)
            {
                scopes.push_back({ModulatorRef(QuadModulator::LFO1, channel),
                    static_cast<double>(sample), synth.m_quadSquiggleLFO[0][channel].m_polyXFader.m_output});
                scopes.push_back({ModulatorRef(QuadModulator::LFO2, channel),
                    static_cast<double>(sample), synth.m_quadSquiggleLFO[1][channel].m_polyXFader.m_output});
                scopes.push_back({ModulatorRef(QuadModulator::DelayLFO, channel),
                    static_cast<double>(sample), synth.m_delay.m_lfo.m_output[channel]});
                scopes.push_back({ModulatorRef(QuadModulator::ReverbLFO, channel),
                    static_cast<double>(sample), synth.m_reverb.m_lfo.m_output[channel]});
            }
        }
    }

    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Spread, 1), 32.0) == 0.0f);
    rig.Internal().PopulateUIState();
    DOCTEST_CHECK(ui.m_squiggleBoyUIState.m_firstControlSample.load() == 8);
    for (const auto& observation : random)
    {
        DOCTEST_CAPTURE(observation.m_sample);
        DOCTEST_CAPTURE(observation.m_modulator.m_index);
        DOCTEST_CAPTURE(observation.m_modulator.m_voice);
        DOCTEST_CHECK(ui.GetModulatorValue(observation.m_modulator, observation.m_sample)
            == doctest::Approx(observation.m_value).epsilon(0.00001));
    }

    constexpr size_t x_scopeLanes = 52;
    for (size_t i = 0; i < scopes.size(); ++i)
    {
        const auto& observation = scopes[i];
        DOCTEST_CHECK(ui.GetModulatorValue(observation.m_modulator, observation.m_sample)
            == doctest::Approx(observation.m_value));
        if (i >= x_scopeLanes)
        {
            DOCTEST_CHECK(ui.GetModulatorValue(observation.m_modulator, observation.m_sample - 4.0)
                == doctest::Approx((observation.m_value + scopes[i - x_scopeLanes].m_value) * 0.5f));
        }
    }

    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::LFO1, 0), 7.99) == 0.0f);
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
    for (auto noise : x_noise)
    {
        float value = ui.GetModulatorValue(noise, 20.0);
        DOCTEST_CHECK(value >= 0.0f);
        DOCTEST_CHECK(value < 1.0f);
        DOCTEST_CHECK(ui.GetModulatorValue(noise, 20.99) == value);
        DOCTEST_CHECK(ui.GetModulatorValue(noise, 21.0) != value);
        for (double sample : {-1.0, 0.0, 64.01, std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN()})
        {
            DOCTEST_CHECK(ui.GetModulatorValue(noise, sample) == 0.0f);
        }
    }

    DOCTEST_CHECK(ui.GetModulatorValue(x_noise[0], 20.0) != ui.GetModulatorValue(x_noise[1], 20.0));
    DOCTEST_CHECK(ui.GetModulatorValue(x_noise[0], 20.0) != ui.GetModulatorValue(x_noise[2], 20.0));
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Spread, 9), 20.0) == 0.0f);
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(QuadModulator::Spread, 4), 20.0) == 0.0f);
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(static_cast<VoiceModulator>(12), 0), 20.0) == 0.0f);

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
    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(VoiceModulator::Sheaf1, 4), 64.0)
        == doctest::Approx(nonagon.m_nonagon.m_output.m_extraTimbre[4][0]));
}
