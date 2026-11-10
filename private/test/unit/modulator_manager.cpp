#include "doctest.h"
#include "EncoderBankBank.hpp"
#include "SmartGridOneEncoders.hpp"
#include "../support/GlobalEnv.hpp"

namespace
{
using Cell = SmartGrid::BankedEncoderCell;
using namespace SmartGridOneModulators;
}

DOCTEST_TEST_CASE("modulator manager: shared sources filter once and gather into independent local slots")
{
    ModulatorManager manager(2, 2);
    manager.Bind(0, 8, 0);
    manager.Bind(1, 3, 0);
    manager.Bind(1, 8, 1);
    ModulatorManager::Decimator reference;
    for (size_t sample = 0; sample < 96 * SampleTimer::x_oversample; ++sample)
    {
        QuadFloat raw = sample % 7 < 3 ? QuadFloat(0.1f, 0.3f, 0.6f, 0.9f) : QuadFloat::Splat(1.0f);
        QuadFloat amplitude(0.2f, 0.4f, 0.6f, 0.8f);
        manager.Set(0, raw);
        manager.GetSource(0).m_amplitude = amplitude;
        manager.Set(1, QuadFloat::Splat(0.25f));
        QuadFloat filtered = reference.Process(raw);
        auto previous = manager.m_groups[0].m_values.m_hostValue[8];
        manager.ProcessSample();
        DOCTEST_CHECK(manager.m_groups[0].m_values.m_hostValue[8] == previous);
        if (sample % SampleTimer::x_oversample != 0)
        {
            continue;
        }

        manager.Gather();
        auto& first = manager.m_groups[0].m_values;
        auto& second = manager.m_groups[1].m_values;
        DOCTEST_CHECK(first.m_hostValue[8] == filtered);
        DOCTEST_CHECK(second.m_hostValue[3] == filtered);
        DOCTEST_CHECK(first.m_value[8] == raw);
        DOCTEST_CHECK(second.m_amplitude[3] == amplitude);
        first.ComputeChanged();
        second.ComputeChanged();
        DOCTEST_CHECK(first.m_changedModulators.Get(8) == second.m_changedModulators.Get(3));
        DOCTEST_CHECK_FALSE(first.m_changedModulators.Get(3));
    }

    manager.m_groups[0].m_values.ComputeChanged();
    manager.GetSource(0).m_amplitude = QuadFloat::Splat(0.9f);
    manager.Gather();
    manager.m_groups[0].m_values.ComputeChanged();
    DOCTEST_CHECK(manager.m_groups[0].m_values.m_changedModulators.Get(8));
}

DOCTEST_TEST_CASE("modulator manager: parameters in one bank mode use independent inherited groups and unchanged patch slots")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    EncoderBankBank banks(1, 1, 2, 2, 2, &context);
    banks.InitMode(0, 4);
    for (size_t group = 0; group < 2; ++group)
    {
        banks.m_modulatorManager.Bind(group, 8, group);
        banks.GetModulatorValues(group).SetModulatorSkin(8, SmartGrid::Color::Red, "SRC");
        banks.CreateEncoder(group, 0, group, 0.2f, group == 0 ? "First" : "Second", "TEST", SmartGrid::Color::Red, 0, false);
        Cell& cell = *banks.GetEncoder(group);
        cell.FillModulators(&context);
        cell.m_modulators.m_modulators[8]->SetValue(1.0f, true);
        cell.SetModulatorsAffecting();
        banks.m_modulatorManager.Set(group, QuadFloat::Splat(group == 0 ? 0.25f : 0.75f));
    }

    for (size_t sample = 0; sample < (SmartGrid::TickDistributor::x_maxSamplesToDistribute + 2) * SampleTimer::x_oversample; ++sample)
    {
        SampleTimer::IncrementSample();
        banks.Process();
    }

    auto* first = banks.GetEncoder(0);
    auto* second = banks.GetEncoder(1);
    DOCTEST_CHECK(first->GetSharedEncoderState() == second->GetSharedEncoderState());
    DOCTEST_CHECK(first->m_modulatorValues != second->m_modulatorValues);
    DOCTEST_CHECK(first->m_gestureValues == &banks.m_modulatorManager.m_gestureValues);
    DOCTEST_CHECK(second->m_gestureValues == first->m_gestureValues);
    for (size_t lane = 0; lane < 4; ++lane)
    {
        DOCTEST_CHECK(first->GetLatestValue(lane) == doctest::Approx(0.25f));
        DOCTEST_CHECK(second->GetLatestValue(lane) == doctest::Approx(0.75f));
    }

    auto* depth = first->m_modulators.m_modulators[8].get();
    depth->FillModulators(&context);
    depth->m_modulators.AddGesture(depth, 2);
    depth->m_modulators.m_gestures[2]->SetActive(true);
    DOCTEST_CHECK(depth->m_modulatorValues == first->m_modulatorValues);
    DOCTEST_CHECK(depth->m_modulators.m_modulators[8]->m_modulatorValues == first->m_modulatorValues);
    DOCTEST_CHECK(depth->m_modulators.m_gestures[2]->m_modulatorValues == first->m_modulatorValues);
    DOCTEST_CHECK(depth->m_gestureValues == first->m_gestureValues);
    DOCTEST_CHECK(depth->m_modulators.m_modulators[8]->m_gestureValues == first->m_gestureValues);
    DOCTEST_CHECK(depth->m_modulators.m_gestures[2]->m_gestureValues == first->m_gestureValues);
    JsonArena arena(1024 * 1024);
    JSON patch = first->ToJSON(arena);
    DOCTEST_CHECK(patch.Get("modulators").Size() == 15);
    DOCTEST_CHECK(patch.Get("group").IsNull());
    second->FromJSON(patch);
    DOCTEST_CHECK(second->m_modulatorValues == &banks.GetModulatorValues(1));
    DOCTEST_CHECK(second->m_modulators.m_modulators[8]->m_modulatorValues == second->m_modulatorValues);
    DOCTEST_REQUIRE(second->m_modulators.m_modulators[8]->m_modulators.m_gestures[2]);
    DOCTEST_CHECK(second->m_modulators.m_modulators[8]->m_modulators.m_gestures[2]->m_modulatorValues == second->m_modulatorValues);
    DOCTEST_CHECK(second->m_modulators.m_modulators[8]->m_modulators.m_gestures[2]->m_gestureValues == first->m_gestureValues);
}

DOCTEST_TEST_CASE("modulator manager: shared value and amplitude changes reach every assigned parameter")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    EncoderBankBank banks(1, 1, 2, 1, 2, &context);
    banks.InitMode(0, 4);
    banks.m_modulatorManager.GetSource(0).m_requiresDownsampling = false;
    for (size_t group = 0; group < 2; ++group)
    {
        size_t slot = group == 0 ? 8 : 3;
        banks.m_modulatorManager.Bind(group, slot, 0);
        banks.GetModulatorValues(group).SetModulatorSkin(slot, SmartGrid::Color::Red, "SRC");
        banks.CreateEncoder(group, 0, group, 0.2f, group == 0 ? "First" : "Second", "TEST", SmartGrid::Color::Red, 0, false);
        Cell& cell = *banks.GetEncoder(group);
        cell.FillModulators(&context);
        cell.m_modulators.m_modulators[slot]->SetValue(1.0f, true);
        cell.SetModulatorsAffecting();
    }

    auto advance = [&banks]()
    {
        for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
        {
            SampleTimer::IncrementSample();
            banks.Process();
        }
    };
    banks.m_modulatorManager.Set(0, QuadFloat::Splat(0.25f));
    for (size_t frame = 0; frame < SmartGrid::TickDistributor::x_maxSamplesToDistribute + 2; ++frame)
    {
        advance();
    }

    DOCTEST_CHECK(banks.GetModulatorValues(0).m_changedModulators.IsZero());
    DOCTEST_CHECK(banks.GetModulatorValues(1).m_changedModulators.IsZero());
    banks.m_modulatorManager.Set(0, QuadFloat(0.3f, 0.4f, 0.5f, 0.6f));
    advance();
    DOCTEST_CHECK(banks.GetModulatorValues(0).m_changedModulators.m_bits == (1 << 8));
    DOCTEST_CHECK(banks.GetModulatorValues(1).m_changedModulators.m_bits == (1 << 3));
    for (size_t group = 0; group < 2; ++group)
    {
        DOCTEST_CHECK(banks.GetEncoder(group)->m_output == QuadFloat(0.3f, 0.4f, 0.5f, 0.6f));
    }

    advance();
    DOCTEST_CHECK(banks.GetModulatorValues(0).m_changedModulators.IsZero());
    DOCTEST_CHECK(banks.GetModulatorValues(1).m_changedModulators.IsZero());
    banks.m_modulatorManager.GetSource(0).m_amplitude = QuadFloat::Splat(0.5f);
    advance();
    DOCTEST_CHECK(banks.GetModulatorValues(0).m_changedModulators.m_bits == (1 << 8));
    DOCTEST_CHECK(banks.GetModulatorValues(1).m_changedModulators.m_bits == (1 << 3));
    for (size_t group = 0; group < 2; ++group)
    {
        for (size_t lane = 0; lane < 4; ++lane)
        {
            float expected = 0.1f + banks.m_modulatorManager.GetSource(0).m_value[lane] * 0.5f;
            DOCTEST_CHECK(banks.GetEncoder(group)->m_output[lane] == doctest::Approx(expected));
        }
    }

    advance();
    DOCTEST_CHECK(banks.GetModulatorValues(0).m_changedModulators.IsZero());
    DOCTEST_CHECK(banks.GetModulatorValues(1).m_changedModulators.IsZero());
}

DOCTEST_TEST_CASE("modulator manager: instrument groups expose distinct pairs and preserve other sources")
{
    SmartGridOneContext context;
    SmartGridOneEncoders encoders(&context);
    for (size_t group = 0; group < x_numGroups; ++group)
    {
        for (size_t slot = 0; slot < x_numSlots; ++slot)
        {
            if (group == static_cast<size_t>(Group::QuadLFO) && (slot < 2 || slot == 4 || slot == 5))
            {
                DOCTEST_CHECK(x_groups[group][slot] == Source::NumSources);
            }

            if (x_groups[group][slot] != Source::NumSources)
            {
                DOCTEST_CHECK(encoders.GetModulatorManager().m_groups[group].m_sources[slot]
                    == static_cast<size_t>(x_groups[group][slot]));
            }

            auto skin = GetSkin(static_cast<Group>(group), slot);
            const auto& values = encoders.GetModulatorValues(static_cast<Group>(group));
            DOCTEST_CHECK(values.m_modulatorColor[slot] == skin.m_color);
            DOCTEST_CHECK(values.m_modulatorConnected[slot]);
        }
    }

    encoders.m_encoderBankBank.ForEachNamedEncoder([](size_t, Cell* cell)
    {
        DOCTEST_CHECK(x_groupLanes[cell->m_modulatorValues->m_groupIx] == cell->GetSharedEncoderState()->m_numVoices);
    });

    DOCTEST_CHECK(GetSource(VoiceModulator::LFO1, 0) != GetSource(VoiceModulator::LFO1, 1));
    DOCTEST_CHECK(GetSource(VoiceModulator::LFO1, 0) != GetSource(QuadModulator::LFO1));
    float gestures[Cell::x_numGestureParams]{};
    gestures[2] = 0.75f;
    encoders.GetModulatorManager().SetGestureWeights(gestures);
    BitSet16 selected;
    selected.Set(2, true);
    encoders.m_encoderBankBank.SelectGesture(selected);
    encoders.m_encoderBankBank.SetBlendChanged(true);
    DOCTEST_CHECK(encoders.GetGestureValues().m_gestureWeights[2] == 0.75f);
    DOCTEST_CHECK(encoders.GetGestureValues().m_selectedGestures.Get(2));
    encoders.m_encoderBankBank.ForEachNamedEncoder([&](size_t, Cell* cell)
    {
        DOCTEST_CHECK(cell->m_gestureValues == &encoders.GetGestureValues());
    });

    for (const auto& group : encoders.GetModulatorManager().m_groups)
    {
        DOCTEST_CHECK(group.m_values.m_blendChanged);
    }
}

DOCTEST_TEST_CASE("modulator manager: gesture changes reach every group once per host frame")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    EncoderBankBank banks(2, 2, 2, 0, 2, &context);
    for (size_t group = 0; group < 2; ++group)
    {
        banks.InitMode(group, 4);
        banks.CreateEncoder(group, group, group, 0.2f, "Param", "P", SmartGrid::Color::Red, 0, false);
        auto* root = banks.GetEncoder(group);
        root->m_modulators.AddGesture(root, 2);
        auto* gesture = root->m_modulators.m_gestures[2].get();
        gesture->SetActive(true);
        gesture->SetValue(group == 0 ? 0.6f : 1.0f, true);
        root->SetModulatorsAffecting();
    }

    for (size_t sample = 0; sample < (SmartGrid::TickDistributor::x_maxSamplesToDistribute + 2) * SampleTimer::x_oversample; ++sample)
    {
        SampleTimer::IncrementSample();
        banks.Process();
    }

    auto& gestures = banks.GetGestureValues();
    float weights[Cell::x_numGestureParams]{};
    weights[2] = 0.5f;
    banks.m_modulatorManager.SetGestureWeights(weights);
    BitSet16 selected;
    selected.Set(2, true);
    banks.SelectGesture(selected);
    for (size_t sample = 1; sample <= SampleTimer::x_oversample; ++sample)
    {
        SampleTimer::IncrementSample();
        banks.Process();
        DOCTEST_CHECK(gestures.m_changedGestures.Get(2) == (sample == SampleTimer::x_oversample));
    }

    DOCTEST_REQUIRE(banks.m_matrix.m_roots.size() == 2);
    for (size_t group = 0; group < 2; ++group)
    {
        auto* root = banks.GetEncoder(group);
        DOCTEST_CHECK(root->m_gestureValues == &gestures);
        DOCTEST_CHECK(banks.m_matrix.m_roots[group].m_gestureValues == &gestures);
        DOCTEST_CHECK(banks.m_matrix.m_roots[group].m_path == EncoderMatrix::Path::Graph);
        DOCTEST_CHECK(root->GetSelectedGestures().Get(2));
        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CHECK(root->m_output[lane] == doctest::Approx(group == 0 ? 0.4f : 0.6f));
        }
    }

    for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
    {
        SampleTimer::IncrementSample();
        banks.Process();
    }

    DOCTEST_CHECK(gestures.m_changedGestures.IsZero());
    banks.SelectGesture(BitSet16());
    for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
    {
        SampleTimer::IncrementSample();
        banks.Process();
    }

    DOCTEST_CHECK(gestures.m_changedGestures.Get(2));
    DOCTEST_CHECK(banks.GetEncoder(0)->GetSelectedGestures().IsZero());
    DOCTEST_CHECK(banks.GetEncoder(1)->GetSelectedGestures().IsZero());

    weights[2] = 0.75f;
    banks.m_modulatorManager.SetGestureWeights(weights);
    banks.GetEncoder(0)->SynchronizePreparedState();
    DOCTEST_CHECK(gestures.m_gestureWeights[2] == 0.75f);
    DOCTEST_CHECK(gestures.m_gestureWeightsPrev[2] == 0.5f);
    for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
    {
        SampleTimer::IncrementSample();
        banks.Process();
    }

    DOCTEST_CHECK(banks.GetEncoder(0)->m_output[0] == doctest::Approx(0.5f));
    DOCTEST_CHECK(banks.GetEncoder(1)->m_output[0] == doctest::Approx(0.8f));
}

DOCTEST_TEST_CASE("modulator manager: configured sources match each parameter's lane count")
{
    SmartGridOneContext context;
    SmartGridOneEncoders encoders(&context);
    auto& banks = encoders.m_encoderBankBank;
    banks.ForEachNamedEncoder([&](size_t, Cell* cell)
    {
        size_t group = cell->m_modulatorValues->m_groupIx;
        size_t parameterLanes = cell->GetSharedEncoderState()->m_numVoices;
        for (size_t slot = 0; slot < x_numSlots; ++slot)
        {
            size_t source = banks.m_modulatorManager.m_groups[group].m_sources[slot];
            if (source == ModulatorManager::x_noSource)
            {
                continue;
            }

            DOCTEST_INFO("parameter: ", cell->m_name, ", group: ", group,
                ", slot: ", slot, ", source: ", source);
            DOCTEST_REQUIRE(source < x_numSources);
            size_t sourceMode = static_cast<size_t>(x_sources[source].m_mode);
            size_t sourceLanes = banks.m_bankModes[sourceMode].m_numVoices;
            DOCTEST_CHECK(sourceLanes == parameterLanes);
        }
    });
}

DOCTEST_TEST_CASE("modulator manager: page families bind to independent random pairs")
{
    using Layout = SmartGridOneLayout;
    const Group tracks[3][5] =
    {
        {Group::WaterSource, Group::WaterShape, Group::WaterPanMelody, Group::WaterLFO, Group::WaterSub},
        {Group::FireSource, Group::FireShape, Group::FirePanMelody, Group::FireLFO, Group::FireSub},
        {Group::EarthSource, Group::EarthShape, Group::EarthPanMelody, Group::EarthLFO, Group::EarthSub}
    };
    const Group quad[] = {Group::QuadDelay, Group::QuadReverb, Group::QuadPartialMachine, Group::QuadLFO};
    DOCTEST_CHECK(x_numGroups == 20);
    std::array<bool, x_numSources> usedRandoms{};
    auto checkPair = [&](Group group, Source first, Source second, size_t lanes)
    {
        size_t index = static_cast<size_t>(group);
        DOCTEST_CHECK(x_groupLanes[index] == lanes);
        DOCTEST_CHECK(x_groups[index][2] == first);
        DOCTEST_CHECK(x_groups[index][3] == second);
        for (Source source : {first, second})
        {
            DOCTEST_CHECK_FALSE(usedRandoms[static_cast<size_t>(source)]);
            usedRandoms[static_cast<size_t>(source)] = true;
        }
    };
    for (size_t track = 0; track < 3; ++track)
    {
        for (size_t family = 0; family < 5; ++family)
        {
            checkPair(tracks[track][family], GetSource(GetVoiceRandom(family * 2), track),
                GetSource(GetVoiceRandom(family * 2 + 1), track), 3);
            size_t group = static_cast<size_t>(tracks[track][family]);
            DOCTEST_CHECK(x_groups[group][0] == GetSource(VoiceModulator::SourceMod1, track));
            DOCTEST_CHECK(x_groups[group][1] == GetSource(VoiceModulator::SourceMod2, track));
            DOCTEST_CHECK(x_groups[group][12] == GetSource(VoiceModulator::Sub, track));
            DOCTEST_CHECK(x_groups[group][13] == GetSource(VoiceModulator::Voice, track));
        }
    }

    for (size_t family = 0; family < 4; ++family)
    {
        checkPair(quad[family], GetSource(GetQuadRandom(family * 2)), GetSource(GetQuadRandom(family * 2 + 1)), 4);
    }

    for (const auto& member : Layout::x_pageMembers)
    {
        for (size_t track = 0; track < 3; ++track)
        {
            DOCTEST_INFO("parameter: ", static_cast<size_t>(member.m_param), ", track: ", track);
            if (Layout::GetParamMode(member.m_param) == Layout::ParamMode::Global)
            {
                DOCTEST_CHECK(GetGroup(member.m_param, track) == Group::Global);
                continue;
            }

            Group expected = Group::NumGroups;
            switch (member.m_page)
            {
                case Layout::Page::DualVCOSource:
                case Layout::Page::PhysicalModelingSource:
                case Layout::Page::SampleSource: expected = tracks[track][0]; break;
                case Layout::Page::Shape: expected = tracks[track][1]; break;
                case Layout::Page::PanAndMelody: expected = tracks[track][2]; break;
                case Layout::Page::VoiceLFOs: expected = tracks[track][3]; break;
                case Layout::Page::Sub: expected = tracks[track][4]; break;
                case Layout::Page::Delay: expected = Group::QuadDelay; break;
                case Layout::Page::Reverb: expected = Group::QuadReverb; break;
                case Layout::Page::PartialMachine: expected = Group::QuadPartialMachine; break;
                case Layout::Page::QuadLFOs: expected = Group::QuadLFO; break;
                case Layout::Page::FXMatrix:
                    if (Layout::GetParamMode(member.m_param) == Layout::ParamMode::Quad)
                    {
                        continue;
                    }

                    expected = tracks[track][1];
                    break;
                default: break;
            }

            DOCTEST_REQUIRE(expected != Group::NumGroups);
            DOCTEST_CHECK(GetGroup(member.m_param, track) == expected);
        }
    }
}

DOCTEST_TEST_CASE("modulator manager: only declared sources are downsampled")
{
    ModulatorManager manager(2, 1);
    manager.GetSource(0).m_requiresDownsampling = false;
    manager.Bind(0, 2, 0);
    manager.Bind(0, 4, 1);
    for (size_t sample = 0; sample < SampleTimer::x_oversample; ++sample)
    {
        manager.Set(0, QuadFloat::Splat(0.8f));
        manager.Set(1, QuadFloat::Splat(0.8f));
        manager.ProcessSample();
        DOCTEST_CHECK(manager.m_groups[0].m_values.m_hostValue[2] == QuadFloat());
    }

    manager.Gather();
    DOCTEST_CHECK(manager.m_groups[0].m_values.m_hostValue[2] == QuadFloat::Splat(0.8f));
    DOCTEST_CHECK(manager.m_groups[0].m_values.m_hostValue[4][0] < 0.8f);
    DOCTEST_CHECK(manager.GetSource(0).m_decimator.Process(QuadFloat()) == QuadFloat());
    SmartGridOneContext context;
    SmartGridOneEncoders encoders(&context);
    for (size_t source = 0; source < x_numSources; ++source)
    {
        auto glyph = x_sources[source].m_skin.m_glyph;
        auto ref = GetRef(static_cast<Source>(source), 0);
        auto quad = static_cast<QuadModulator>(ref.m_index);
        bool mixerEnvelope = ref.m_mode == SmartGridOneLayout::ParamMode::Quad
            && GetMixerScope(quad) != SmartGridOne::QuadScopes::NumScopes && !IsMixerAudio(quad);
        bool expected = !mixerEnvelope
            && (glyph == SmartGridOne::ModulationGlyphs::ADSR || glyph == SmartGridOne::ModulationGlyphs::LFO);
        DOCTEST_CHECK(x_sources[source].m_requiresDownsampling == expected);
        DOCTEST_CHECK(encoders.GetModulatorManager().GetSource(source).m_requiresDownsampling == expected);
    }
}
