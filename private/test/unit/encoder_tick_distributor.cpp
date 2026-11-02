#include "doctest.h"

#include "EncoderBankBank.hpp"
#include "../support/GlobalEnv.hpp"

namespace
{

using Distributor = SmartGrid::TickDistributor;

struct TickRig
{
    SmartGridOneContext m_context;
    EncoderBankBank m_banks;

    TickRig()
        : m_banks(1, 1, 1, &m_context)
    {
        m_banks.InitMode(0, 1);
        m_banks.InitBank(0, 0, SmartGrid::Color::Red);
        m_banks.CreateEncoder(0, 0, 0.25f, "Test", "TEST", SmartGrid::Color::Red, 0, false);
        m_banks.PlaceEncoder(0, 0, 0, 0);
        for (size_t i = 0; i < SmartGrid::BankedEncoderCell::x_numModulators; ++i)
        {
            m_banks.GetModulatorValues(0).SetModulatorSkin(i, SmartGrid::Color::Red, nullptr);
        }
    }

    SmartGrid::BankedEncoderCell& Cell()
    {
        return *m_banks.GetEncoder(0);
    }

    void Run(size_t samples)
    {
        for (size_t i = 0; i < samples; ++i)
        {
            SampleTimer::IncrementSample();
            m_banks.Process();
        }
    }
};

}

DOCTEST_TEST_CASE("encoder distribution: timing stays bounded and same-sample scene writes share the ramp")
{
    Distributor ticks;
    DOCTEST_CHECK(ticks.IsDone());
    ticks.RecordChange();
    DOCTEST_CHECK(ticks.m_samplesToDistribute == Distributor::x_maxSamplesToDistribute);
    ticks.RecordChange();
    DOCTEST_CHECK(ticks.m_samplesToDistribute == Distributor::x_maxSamplesToDistribute);

    for (size_t i = 0; i < 480; ++i)
    {
        ticks.RecordSample();
    }

    ticks.RecordChange();
    DOCTEST_CHECK(ticks.m_samplesToDistribute == 480);
    ticks.RecordSample();
    ticks.RecordChange();
    DOCTEST_CHECK(ticks.m_samplesToDistribute == Distributor::x_minSamplesToDistribute);
    for (size_t i = 0; i < 2 * Distributor::x_maxSamplesToDistribute; ++i)
    {
        ticks.RecordSample();
    }

    DOCTEST_CHECK(ticks.IsDone());
    DOCTEST_CHECK(ticks.m_samplesToDistribute == 0);
    DOCTEST_CHECK(ticks.m_samplesSinceLastChange == Distributor::x_maxSamplesToDistribute);
}

DOCTEST_TEST_CASE("encoder distribution: new ticks accumulate against targets and land exactly")
{
    GlobalEnv::ResetPerTest();
    TickRig rig;
    auto& cell = rig.Cell();
    cell.SetValue(0.25f, true);
    DOCTEST_CHECK(cell.m_tickDistributor.IsDone());
    cell.Increment(0.1f);
    DOCTEST_CHECK(cell.m_values[0] == 0.25f);
    DOCTEST_CHECK(cell.m_targets[0] == doctest::Approx(0.35f));
    rig.Run(480);
    DOCTEST_CHECK(cell.m_values[0] == doctest::Approx(0.27f));
    cell.SetTarget(cell.m_targets[0], 0);
    DOCTEST_CHECK(cell.m_tickDistributor.m_samplesToDistribute == Distributor::x_maxSamplesToDistribute - 480);
    cell.Increment(0.1f);
    DOCTEST_CHECK(cell.m_targets[0] == doctest::Approx(0.45f));
    DOCTEST_CHECK(cell.m_tickDistributor.m_samplesToDistribute == 480);
    cell.SetToValue(0.6f);
    DOCTEST_CHECK(cell.m_targets[0] == doctest::Approx(0.6f));
    rig.Run(480);
    DOCTEST_CHECK(cell.m_values[0] == cell.m_targets[0]);

    rig.Run(Distributor::x_maxSamplesToDistribute);
    rig.m_context.m_sceneManager.m_blendFactor = 0.5f;
    cell.Increment(0.1f);
    DOCTEST_CHECK(cell.m_targets[0] == doctest::Approx(0.65f));
    DOCTEST_CHECK(cell.m_targets[1] == doctest::Approx(0.3f));
    DOCTEST_CHECK(cell.m_tickDistributor.m_samplesToDistribute == Distributor::x_maxSamplesToDistribute);
}

DOCTEST_TEST_CASE("encoder distribution: nested edits advance once per process and refresh ancestors through the last sample")
{
    GlobalEnv::ResetPerTest();
    TickRig rig;
    auto& root = rig.Cell();
    root.FillModulators(&rig.m_context);
    auto& child = *root.m_modulators.m_modulators[0];
    child.FillModulators(&rig.m_context);
    auto& nested = *child.m_modulators.m_modulators[1];
    rig.m_banks.GetModulatorValues(0).m_value[0] = QuadFloat::Splat(1.0f);
    rig.m_banks.GetModulatorValues(0).m_value[1] = QuadFloat::Splat(1.0f);
    rig.Run(Distributor::x_maxSamplesToDistribute);
    DOCTEST_REQUIRE(root.m_samplesForceUpdate == 0);
    nested.Increment(0.25f);
    for (size_t i = 0; i < 5; ++i)
    {
        root.Compute(false);
        nested.Compute(false);
    }

    DOCTEST_CHECK(nested.m_values[0] == 0.5f);
    DOCTEST_CHECK(nested.m_tickDistributor.m_samplesToDistribute == Distributor::x_maxSamplesToDistribute);
    DOCTEST_CHECK(root.m_samplesForceUpdate == Distributor::x_maxSamplesToDistribute);
    rig.Run(1);
    DOCTEST_CHECK(nested.m_values[0] > 0.5f);
    DOCTEST_CHECK(root.m_samplesForceUpdate == Distributor::x_maxSamplesToDistribute - 1);
    DOCTEST_CHECK(child.m_samplesForceUpdate == root.m_samplesForceUpdate);
    rig.Run(Distributor::x_maxSamplesToDistribute - 1);
    DOCTEST_CHECK(nested.m_values[0] == 0.75f);
    DOCTEST_CHECK(root.m_samplesForceUpdate == 0);
    DOCTEST_CHECK(root.GetValue(0) > 0.25f);

    nested.Increment(-0.25f);
    rig.Run(Distributor::x_maxSamplesToDistribute);
    DOCTEST_CHECK(root.GetValue(0) == 0.25f);
    DOCTEST_CHECK(root.m_modulatorsAffecting.IsZero());
    DOCTEST_CHECK(root.m_samplesForceUpdate == 0);
}

DOCTEST_TEST_CASE("encoder distribution: resetting a nested depth keeps unchanged ancestors computing")
{
    GlobalEnv::ResetPerTest();
    TickRig rig;
    auto& root = rig.Cell();
    root.FillModulators(&rig.m_context);
    auto& child = *root.m_modulators.m_modulators[0];
    child.FillModulators(&rig.m_context);
    auto& nested = *child.m_modulators.m_modulators[1];
    rig.m_banks.GetModulatorValues(0).m_value[0] = QuadFloat::Splat(1.0f);
    rig.m_banks.GetModulatorValues(0).m_value[1] = QuadFloat::Splat(1.0f);
    nested.Increment(0.25f);
    rig.Run(Distributor::x_maxSamplesToDistribute);
    DOCTEST_REQUIRE(child.m_targets[0] == 0.5f);
    DOCTEST_REQUIRE(child.m_samplesForceUpdate == 0);

    root.RevertToDefault(true);
    DOCTEST_CHECK(child.m_samplesForceUpdate == Distributor::x_maxSamplesToDistribute);
    rig.Run(1);
    DOCTEST_CHECK(nested.m_values[0] < 0.75f);
    DOCTEST_CHECK(nested.m_values[0] > 0.5f);
    rig.Run(Distributor::x_maxSamplesToDistribute - 1);
    DOCTEST_CHECK(nested.m_values[0] == 0.5f);
    DOCTEST_CHECK(root.GetValue(0) == 0.25f);
    DOCTEST_CHECK(root.m_modulatorsAffecting.IsZero());
    DOCTEST_CHECK(root.m_samplesForceUpdate == 0);
}

DOCTEST_TEST_CASE("encoder distribution: scene copies advance hidden targets through neutral ancestors")
{
    GlobalEnv::ResetPerTest();
    TickRig rig;
    auto& root = rig.Cell();
    root.FillModulators(&rig.m_context);
    auto& child = *root.m_modulators.m_modulators[0];
    child.FillModulators(&rig.m_context);
    auto& nested = *child.m_modulators.m_modulators[1];
    nested.SetAndRecordValue(0.75f, 2);
    nested.SetForceUpdateRecursive();
    rig.Run(Distributor::x_maxSamplesToDistribute);
    DOCTEST_REQUIRE(root.m_modulatorsAffecting.IsZero());
    DOCTEST_REQUIRE(root.m_samplesForceUpdate == 0);

    rig.m_banks.CopyToScene(2);
    DOCTEST_CHECK(root.m_samplesForceUpdate == Distributor::x_maxSamplesToDistribute);
    DOCTEST_CHECK(child.m_samplesForceUpdate == Distributor::x_maxSamplesToDistribute);
    rig.Run(Distributor::x_maxSamplesToDistribute / 2);
    DOCTEST_CHECK(nested.m_values[2] == doctest::Approx(0.625f));
    float value = nested.m_values[2];
    size_t remaining = nested.m_tickDistributor.m_samplesToDistribute;
    root.Compute(false);
    root.Compute(false);
    DOCTEST_CHECK(nested.m_values[2] == value);
    DOCTEST_CHECK(nested.m_tickDistributor.m_samplesToDistribute == remaining);
    rig.Run(Distributor::x_maxSamplesToDistribute / 2);
    DOCTEST_CHECK(nested.m_values[2] == 0.5f);
    DOCTEST_CHECK(root.GetValue(0) == 0.25f);
    DOCTEST_CHECK(root.m_samplesForceUpdate == 0);
    DOCTEST_CHECK(root.m_modulatorsAffecting.IsZero());
}

DOCTEST_TEST_CASE("encoder distribution: recording and patches store targets without recording interpolation")
{
    GlobalEnv::ResetPerTest();
    TickRig rig;
    auto& recorder = rig.m_context.m_recorder;
    recorder.m_paramEventsRing = std::make_unique<CircularQueue<ParamEvent, StreamingRecorder::x_queueParamEvents>>();
    recorder.m_recordingStartSample = SampleTimer::GetHostSample();
    recorder.m_state = StreamingRecorder::State::Recording;
    rig.Cell().SetToValue(0.75f);
    ParamEvent event;
    DOCTEST_REQUIRE(recorder.m_paramEventsRing->Pop(event));
    float recorded;
    std::memcpy(&recorded, event.m_value, sizeof(recorded));
    DOCTEST_CHECK(recorded == 0.75f);
    DOCTEST_CHECK(event.m_sample == 0);
    DOCTEST_CHECK(rig.Cell().m_values[0] == 0.25f);
    JsonArena arena(4096);
    JSON saved = rig.Cell().StateEncoderCell::ToJSON(arena);
    DOCTEST_CHECK(saved.Get("values").GetAt(0).NumberValue() == 0.75);
    rig.Run(Distributor::x_maxSamplesToDistribute);
    DOCTEST_CHECK(rig.Cell().m_values[0] == 0.75f);
    DOCTEST_CHECK_FALSE(recorder.m_paramEventsRing->Pop(event));

    JSON loaded = arena.Loads(R"({"values":[0.2]})");
    rig.Cell().StateEncoderCell::FromJSON(loaded);
    DOCTEST_CHECK(rig.Cell().m_values[0] == 0.75f);
    DOCTEST_CHECK(rig.Cell().m_targets[0] == 0.2f);
    rig.Run(Distributor::x_maxSamplesToDistribute);
    DOCTEST_CHECK(rig.Cell().m_values[0] == 0.2f);
    DOCTEST_CHECK_FALSE(recorder.m_paramEventsRing->Pop(event));
}
