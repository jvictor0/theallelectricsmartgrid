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
        filtered = QuadFloat::Min(QuadFloat::Splat(1), QuadFloat::Max(QuadFloat(), filtered));
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
    JsonArena arena(1024 * 1024);
    JSON patch = first->ToJSON(arena);
    DOCTEST_CHECK(patch.Get("modulators").Size() == 15);
    DOCTEST_CHECK(patch.Get("group").IsNull());
    second->FromJSON(patch);
    DOCTEST_CHECK(second->m_modulatorValues == &banks.GetModulatorValues(1));
    DOCTEST_CHECK(second->m_modulators.m_modulators[8]->m_modulatorValues == second->m_modulatorValues);
    DOCTEST_REQUIRE(second->m_modulators.m_modulators[8]->m_modulators.m_gestures[2]);
    DOCTEST_CHECK(second->m_modulators.m_modulators[8]->m_modulators.m_gestures[2]->m_modulatorValues == second->m_modulatorValues);
}

DOCTEST_TEST_CASE("modulator manager: instrument defaults retain source slots and parameter scopes")
{
    SmartGridOneContext context;
    SmartGridOneEncoders encoders(&context);
    for (size_t group = 0; group < x_numGroups; ++group)
    {
        for (size_t slot = 0; slot < x_numSlots; ++slot)
        {
            DOCTEST_CHECK(x_groups[group][slot] == x_sourceIds[group][slot]);
            auto skin = GetSkin(static_cast<Group>(group), slot);
            const auto& values = encoders.GetModulatorValues(static_cast<Group>(group));
            DOCTEST_CHECK(values.m_modulatorColor[slot] == skin.m_color);
            DOCTEST_CHECK(values.m_modulatorConnected[slot]);
        }
    }

    encoders.m_encoderBankBank.ForEachNamedEncoder([](size_t, Cell* cell)
    {
        DOCTEST_CHECK(cell->m_modulatorValues->m_groupIx == cell->GetSharedEncoderState()->m_modeIx);
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
    for (const auto& group : encoders.GetModulatorManager().m_groups)
    {
        DOCTEST_CHECK(group.m_values.m_gestureWeights[2] == 0.75f);
        DOCTEST_CHECK(group.m_values.m_selectedGestures.Get(2));
        DOCTEST_CHECK(group.m_values.m_blendChanged);
    }
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
