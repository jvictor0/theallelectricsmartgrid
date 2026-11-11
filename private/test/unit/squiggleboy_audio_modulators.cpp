#include "doctest.h"
#include "../support/SynthRig.hpp"

namespace
{
using namespace SmartGridOneModulators;
using Machine = VoiceMachine::SourceMachine;

template<size_t Size>
void CheckSourceTaps()
{
    GlobalEnv::ResetPerTest();
    using Source = SquiggleBoySourceGeneric<Size>;
    using Vector = typename Source::Vector;
    auto source = std::make_unique<Source>();
    typename Source::Input input;
    auto writer = std::make_unique<ScopeWriterGeneric<Size,
        3 * static_cast<size_t>(SmartGridOne::VoiceScopes::NumScopes)>>(1, 512);
    source->SetScopeWriters(writer.get(), 1);
    auto process = [&]()
    {
        source->Process(input);
        for (size_t tap = 0; tap < 2; ++tap)
        {
            bool dual = input.m_sourceMachine == Machine::DualWaveShapingVCO;
            if (!dual && (input.m_sourceMachine != Machine::PhysicalModeling || tap == 0))
            {
                continue;
            }

            using Scope = SmartGridOne::VoiceScopes;
            auto logicalScope = dual ? (tap == 0 ? Scope::VCO1 : Scope::VCO2) : Scope::PhysicalModelingEnvelope;
            size_t scope = SmartGridOne::VoiceScopeIndex(static_cast<size_t>(logicalScope), 1);
            for (size_t lane = 0; lane < Size; ++lane)
            {
                float recorded = writer->ReadSample(scope, lane, writer->m_index);
                DOCTEST_CHECK((dual ? 0.5f + 0.5f * recorded : recorded) == source->m_modOut[tap][lane]);
                if (input.m_sourceMachine == Machine::DualWaveShapingVCO)
                {
                    const auto& top = source->m_dualWaveShapingVCO.m_vco[tap].m_top[lane];
                    if (top)
                    {
                        const auto& markers = writer->GetMarkers(scope, lane);
                        size_t count = markers.m_startIndexIndex;
                        DOCTEST_REQUIRE(count > 0);
                        DOCTEST_CHECK(markers.m_startIndices[(count - 1) % ScopeWriter::x_numStartIndices]
                            == doctest::Approx(writer->m_index + top.m_offset));
                    }
                }
                else if (input.m_sourceMachine == Machine::PhysicalModeling
                    && source->m_physicalModeling.m_ahd.m_channels[lane].m_triggered)
                {
                    const auto& markers = writer->GetMarkers(scope, lane);
                    size_t count = markers.m_startIndexIndex;
                    DOCTEST_REQUIRE(count > 0);
                    DOCTEST_CHECK(markers.m_startIndices[(count - 1) % ScopeWriter::x_numStartIndices] == writer->m_index);
                }
            }
        }

        writer->AdvanceIndex();
    };
    AdaptiveWaveTable table;
    for (size_t sample = 0; sample < BasicWaveTable::x_tableSize; ++sample)
    {
        double phase = 2.0 * M_PI * sample / BasicWaveTable::x_tableSize;
        table.m_waveTable.m_table[sample] = std::cos(phase) + std::cos(2.0 * phase);
    }

    table.Generate();
    for (size_t lane = 0; lane < Size; ++lane)
    {
        for (auto& vco : source->m_dualWaveShapingVCO.m_vco)
        {
            vco.SetLeft(lane, &table);
            vco.SetRight(lane, &table);
        }

        input.m_dualWaveShapingVCOInput.m_baseFreq[lane] = 0.001f * (lane + 1);
    }

    input.m_dualWaveShapingVCOInput.m_offsetFreqFactor = Vector::Splat(1.7f);
    input.m_dualWaveShapingVCOInput.m_morphHarmonics[0] = Vector::Splat(1.0f);
    input.m_dualWaveShapingVCOInput.m_morphHarmonics[1] = Vector::Splat(1.0f);
    bool sawExcursion = false;
    bool sawDistinctTaps = false;
    for (size_t sample = 0; sample < 256; ++sample)
    {
        input.m_dualWaveShapingVCOInput.m_fade = Vector::Splat(sample % 2);
        input.m_dualWaveShapingVCOInput.m_bitCrushAmount = Vector::Splat(sample % 3 == 0 ? 0.9f : 0.0f);
        process();
        for (size_t lane = 0; lane < Size; ++lane)
        {
            for (size_t tap = 0; tap < 2; ++tap)
            {
                float value = source->m_modOut[tap][lane];
                DOCTEST_CHECK(value == doctest::Approx(0.5f + 0.5f * source->m_dualWaveShapingVCO.m_vco[tap].m_out[lane]));
                sawExcursion |= value < 0 || value > 1;
            }

            sawDistinctTaps |= std::abs(source->m_modOut[0][lane] - source->m_modOut[1][lane]) > 0.1f;
        }
    }

    DOCTEST_CHECK(sawExcursion);
    DOCTEST_CHECK(sawDistinctTaps);

    TheoryOfTimeBase time;
    time.m_globalPeriodSamples = SampleTimer::x_sampleRate;
    time.m_sample.m_modulatedPhase = 1.0;
    input.m_physicalModelingInput.m_ahdInput.SetTheoryOfTime(&time);
    input.m_sourceMachine = Machine::PhysicalModeling;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        input.m_physicalModelingInput.m_mainSVFMorph[lane] = 0.25f * lane;
        auto& ahd = input.m_physicalModelingInput.m_ahdInput.m_channels[lane];
        ahd.m_amplitude = 0.2f + 0.1f * lane;
        ahd.m_trig = true;
    }

    for (size_t sample = 0; sample < 64; ++sample)
    {
        auto envelope = source->m_physicalModeling.m_ahd;
        Vector expectedEnvelope = envelope.Process(input.m_physicalModelingInput.m_ahdInput);
        process();
        Vector filteredNoise = source->m_physicalModeling.m_mainSVF.GetMorphedOutput(input.m_physicalModelingInput.m_mainSVFMorph);
        for (size_t lane = 0; lane < Size; ++lane)
        {
            DOCTEST_CHECK(source->m_modOut[0][lane] == doctest::Approx(0.5f + 0.5f * filteredNoise[lane]));
            DOCTEST_CHECK(source->m_modOut[1][lane] == expectedEnvelope[lane]);
            DOCTEST_CHECK(source->m_modOut[1][lane] == source->m_physicalModeling.m_ahd.m_output[lane]);
            input.m_physicalModelingInput.m_ahdInput.m_channels[lane].m_trig = false;
        }

        time.m_sample.m_modulatedPhase += 1.0 / SampleTimer::x_sampleRate;
    }

    for (Machine active : {Machine::DualWaveShapingVCO, Machine::PhysicalModeling})
    {
        for (Machine placeholder : {Machine::Sample, Machine::Thru})
        {
            input.m_sourceMachine = active;
            process();
            DOCTEST_REQUIRE(source->m_modOut[1] != Vector());
            input.m_sourceMachine = placeholder;
            for (size_t sample = 0; sample < 2; ++sample)
            {
                process();
                DOCTEST_CHECK(source->m_modOut[0] == Vector());
                DOCTEST_CHECK(source->m_modOut[1] == Vector());
            }
        }
    }
}
}

DOCTEST_TEST_CASE("Audio modulators: source taps precede mixing and envelopes and clear on machine changes")
{
    CheckSourceTaps<1>();
    CheckSourceTaps<3>();
}

DOCTEST_TEST_CASE("Audio modulators: publication uses previous engine outputs without clipping")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    for (size_t track = 0; track < 3; ++track)
    {
        for (size_t lane = 0; lane < 3; ++lane)
        {
            float value = static_cast<float>(track * 3 + lane) * 0.2f - 0.3f;
            synth.m_tracks[track].m_source.m_modOut[0][lane] = value;
            synth.m_tracks[track].m_source.m_modOut[1][lane] = 1.0f - value;
            synth.m_tracks[track].m_amp.m_subModOut[lane] = value * 2.0f - 1.0f;
            synth.m_tracks[track].m_amp.m_voiceModOut[lane] = 1.0f - value * 2.0f;
            synth.m_tracks[track].m_amp.m_output[lane] = 99.0f;
        }
    }

    constexpr VoiceModulator x_taps[] = {VoiceModulator::SourceMod1, VoiceModulator::SourceMod2, VoiceModulator::Sub, VoiceModulator::Voice};
    for (size_t sample = 0; sample < 8; ++sample)
    {
        TrioFloat expected[3][4];
        for (size_t track = 0; track < 3; ++track)
        {
            auto& voice = synth.m_tracks[track];
            expected[track][0] = voice.m_source.m_modOut[0];
            expected[track][1] = voice.m_source.m_modOut[1];
            expected[track][2] = voice.m_amp.m_subModOut * 0.5f + 0.5f;
            expected[track][3] = voice.m_amp.m_voiceModOut * 0.5f + 0.5f;
        }

        rig.RunSamples(1);
        for (size_t track = 0; track < 3; ++track)
        {
            for (size_t tap = 0; tap < 4; ++tap)
            {
                const auto& published = synth.m_encoders.GetModulatorManager().GetSource(GetSource(x_taps[tap], track));
                DOCTEST_CHECK(published.m_requiresDownsampling);
                for (size_t lane = 0; lane < 3; ++lane)
                {
                    DOCTEST_CHECK(published.m_value[lane] == expected[track][tap][lane]);
                }
            }
        }
    }
}

DOCTEST_TEST_CASE("Audio modulators: shared scopes supply normalized tap histories and physical noise uses its existing visualization")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& ui = rig.UIState();
    auto& writer = ui.m_squiggleBoyUIState.m_voiceScopeWriter;
    constexpr VoiceModulator x_taps[] = {VoiceModulator::SourceMod1, VoiceModulator::SourceMod2,
        VoiceModulator::Sub, VoiceModulator::Voice};
    rig.StartSequencer();
    for (Machine machine : {Machine::DualWaveShapingVCO, Machine::PhysicalModeling, Machine::Sample, Machine::Thru})
    {
        for (auto& config : synth.m_voiceConfig)
        {
            config.m_sourceMachine = machine;
        }

        rig.RunSamples(256);
        rig.Internal().PopulateUIState();
        writer.Publish();
        writer.AcceptSamples();
        const double now = SmartGridOneModulatorHistory::GetLatestSample(ui);
        for (size_t track = 0; track < 3; ++track)
        {
            const auto& voice = synth.m_tracks[track];
            TrioFloat expected[] = {voice.m_source.m_modOut[0], voice.m_source.m_modOut[1],
                voice.m_amp.m_subModOut * 0.5f + 0.5f, voice.m_amp.m_voiceModOut * 0.5f + 0.5f};
            for (size_t tap = 0; tap < 4; ++tap)
            {
                for (size_t lane = 0; lane < 3; ++lane)
                {
                    float actual = -1.0f;
                    ModulatorRef ref(x_taps[tap], track * 3 + lane);
                    DOCTEST_REQUIRE(SmartGridOneModulatorHistory::ReadModulatorValue(ui, ref, now, 0, actual));
                    if (machine == Machine::PhysicalModeling && tap == 0)
                    {
                        DOCTEST_CHECK(SmartGridOneModulatorHistory::GetControlScope(ui, ref).m_writer == nullptr);
                        DOCTEST_CHECK(actual == SmartGridOneModulatorHistory::Noise(ref, now));
                    }
                    else
                    {
                        DOCTEST_CHECK(actual == expected[tap][lane]);
                    }
                }
            }
        }
    }
}

DOCTEST_TEST_CASE("Audio modulators: full synth routes all four taps with bounded parameter caches")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& banks = synth.m_encoders.m_encoderBankBank;
    banks.ForEachNamedEncoder([](size_t, SmartGrid::BankedEncoderCell* cell)
    {
        if (cell->GetSharedEncoderState()->m_numVoices != 3)
        {
            return;
        }

        cell->FillModulators(cell->m_context);
        for (size_t slot : {0, 1, 12, 13})
        {
            cell->GetModulator(slot)->SetValue(slot % 2 ? 0.0f : 1.0f, true);
        }

        cell->SetModulatorsAffecting();
    });
    rig.StartSequencer();
    for (Machine machine : {Machine::DualWaveShapingVCO, Machine::PhysicalModeling, Machine::Sample, Machine::Thru})
    {
        for (auto& config : synth.m_voiceConfig)
        {
            config.m_sourceMachine = machine;
        }

        for (size_t frame = 0; frame < 128; ++frame)
        {
            rig.RunSamples(SampleTimer::x_oversample);
            for (const auto& root : banks.m_matrix.m_roots)
            {
                for (size_t lane = 0; lane < root.m_cell->GetSharedEncoderState()->m_numVoices; ++lane)
                {
                    DOCTEST_REQUIRE(root.m_cell->m_output[lane] >= 0.0f);
                    DOCTEST_REQUIRE(root.m_cell->m_output[lane] <= 1.0f);
                    for (const auto& cache : banks.m_valueCaches)
                    {
                        DOCTEST_REQUIRE(cache[root.m_index][lane] >= (root.m_cell->m_bipolar ? -1.0f : 0.0f));
                        DOCTEST_REQUIRE(cache[root.m_index][lane] <= 1.0f);
                    }
                }
            }
        }
    }

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}
