#include "doctest.h"

#include "EncoderBankBank.hpp"
#include "../support/GlobalEnv.hpp"
#include "../support/HostFrames.hpp"

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

    // Advances whole host frames, the unit of tick distribution, ending where
    // interpolated reads equal the latest output.
    //
    void RunHostFrames(size_t hostFrames)
    {
        const size_t samples = hostframes::SamplesToHostFrameEnd(hostFrames);
        for (size_t i = 0; i < samples; ++i)
        {
            SampleTimer::IncrementSample();
            m_banks.Process();
        }
    }
};

}

DOCTEST_TEST_CASE("encoder distribution: bursts preserve pole state and reverse without a jump")
{
    GlobalEnv::ResetPerTest();
    TickRig rig;
    auto& cell = rig.Cell();
    cell.Increment(0.1f);
    rig.RunHostFrames(480);
    double expected = 0.35 + (0.25 - 0.35) * std::exp(-0.010 / 0.020);
    DOCTEST_CHECK(cell.m_values[0] == doctest::Approx(expected));
    double state = cell.m_smoothedValues[0];
    cell.Increment(0.1f);
    cell.Increment(0.1f);
    DOCTEST_CHECK(cell.m_targets[0] == doctest::Approx(0.55f));
    DOCTEST_CHECK(cell.m_smoothedValues[0] == state);
    rig.RunHostFrames(48);
    expected = 0.55 + (state - 0.55) * std::exp(-0.001 / 0.020);
    DOCTEST_CHECK(cell.m_values[0] == doctest::Approx(expected));
    state = cell.m_smoothedValues[0];
    cell.SetToValue(0.1f);
    DOCTEST_CHECK(cell.m_smoothedValues[0] == state);
    rig.RunHostFrames(48);
    expected = 0.1 + (state - 0.1) * std::exp(-0.001 / 0.020);
    DOCTEST_CHECK(cell.m_values[0] == doctest::Approx(expected));
    rig.RunHostFrames(Distributor::x_maxSamplesToDistribute);
    DOCTEST_CHECK(cell.m_values[0] == cell.m_targets[0]);
    DOCTEST_CHECK(cell.m_tickDistributor.IsDone());
    DOCTEST_CHECK(cell.m_samplesForceUpdate == 0);
}

DOCTEST_TEST_CASE("encoder distribution: all scenes settle exactly without float stalls or idle work")
{
    GlobalEnv::ResetPerTest();
    TickRig rig;
    auto& cell = rig.Cell();
    for (size_t scene = 0; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
    {
        cell.SetTarget(scene % 2 ? 0.0f : 1.0f, scene);
    }

    size_t frames = 0;
    while (!cell.m_tickDistributor.IsDone() && frames < Distributor::x_maxSamplesToDistribute)
    {
        rig.RunHostFrames(1);
        ++frames;
    }

    DOCTEST_CHECK(frames > 12000);
    DOCTEST_CHECK(frames < 13500);
    DOCTEST_REQUIRE(cell.m_samplesForceUpdate == 0);
    auto stamp = cell.m_outputHostSample;
    for (size_t scene = 0; scene < SmartGrid::SceneManager::x_numScenes; ++scene)
    {
        DOCTEST_CHECK(cell.m_values[scene] == cell.m_targets[scene]);
        DOCTEST_CHECK(cell.m_smoothedValues[scene] == cell.m_targets[scene]);
    }

    rig.RunHostFrames(1000);
    DOCTEST_CHECK(cell.m_outputHostSample == stamp);
    DOCTEST_CHECK(cell.m_tickDistributor.m_movingScenes == 0);
    cell.SetTarget(1.0f - 0.000001f, 0);
    rig.RunHostFrames(100);
    DOCTEST_CHECK(cell.m_values[0] == cell.m_targets[0]);
    DOCTEST_CHECK(cell.m_samplesForceUpdate == 0);
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
    rig.RunHostFrames(Distributor::x_maxSamplesToDistribute);
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
    rig.RunHostFrames(1);
    DOCTEST_CHECK(nested.m_values[0] > 0.5f);
    DOCTEST_CHECK(root.m_samplesForceUpdate == Distributor::x_maxSamplesToDistribute - 1);
    DOCTEST_CHECK(child.m_samplesForceUpdate == root.m_samplesForceUpdate);
    rig.RunHostFrames(Distributor::x_maxSamplesToDistribute - 1);
    DOCTEST_CHECK(nested.m_values[0] == 0.75f);
    DOCTEST_CHECK(root.m_samplesForceUpdate == 0);
    DOCTEST_CHECK(root.GetValue(0) > 0.25f);

    nested.Increment(-0.25f);
    rig.RunHostFrames(Distributor::x_maxSamplesToDistribute);
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
    rig.RunHostFrames(Distributor::x_maxSamplesToDistribute);
    DOCTEST_REQUIRE(child.m_targets[0] == 0.5f);
    DOCTEST_REQUIRE(child.m_samplesForceUpdate == 0);

    root.RevertToDefault(true);
    DOCTEST_CHECK(child.m_samplesForceUpdate == Distributor::x_maxSamplesToDistribute);
    rig.RunHostFrames(1);
    DOCTEST_CHECK(nested.m_values[0] < 0.75f);
    DOCTEST_CHECK(nested.m_values[0] > 0.5f);
    rig.RunHostFrames(Distributor::x_maxSamplesToDistribute - 1);
    DOCTEST_CHECK(nested.m_values[0] == 0.5f);
    DOCTEST_CHECK(root.GetValue(0) == 0.25f);
    DOCTEST_CHECK(root.m_modulatorsAffecting.IsZero());
    DOCTEST_CHECK(root.m_samplesForceUpdate == 0);
}

DOCTEST_TEST_CASE("encoder distribution: a retired sibling cannot stop a longer moving route")
{
    GlobalEnv::ResetPerTest();
    TickRig rig;
    auto& root = rig.Cell();
    root.FillModulators(&rig.m_context);
    auto& shortRoute = *root.m_modulators.m_modulators[0];
    auto& longRoute = *root.m_modulators.m_modulators[1];
    shortRoute.SetValue(0.51f, true);
    shortRoute.SetForceUpdateRecursive();
    rig.RunHostFrames(Distributor::x_maxSamplesToDistribute);
    shortRoute.SetValue(0.5f, true);
    longRoute.SetValue(1.0f, true);
    root.SetForceUpdateRecursive();
    rig.RunHostFrames(9500);
    DOCTEST_REQUIRE(shortRoute.m_tickDistributor.IsDone());
    DOCTEST_REQUIRE_FALSE(longRoute.m_tickDistributor.IsDone());
    float before = longRoute.m_values[0];
    rig.RunHostFrames(100);
    DOCTEST_CHECK(longRoute.m_values[0] > before);
    rig.RunHostFrames(Distributor::x_maxSamplesToDistribute);
    DOCTEST_CHECK(longRoute.m_values[0] == 1.0f);
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
    rig.RunHostFrames(Distributor::x_maxSamplesToDistribute);
    DOCTEST_REQUIRE(root.m_modulatorsAffecting.IsZero());
    DOCTEST_REQUIRE(root.m_samplesForceUpdate == 0);

    rig.m_banks.CopyToScene(2);
    DOCTEST_CHECK(root.m_samplesForceUpdate == Distributor::x_maxSamplesToDistribute);
    DOCTEST_CHECK(child.m_samplesForceUpdate == Distributor::x_maxSamplesToDistribute);
    rig.RunHostFrames(480);
    DOCTEST_CHECK(nested.m_values[2] == doctest::Approx(0.5 + 0.25 * std::exp(-0.5)));
    float value = nested.m_values[2];
    size_t remaining = nested.m_tickDistributor.m_samplesToDistribute;
    root.Compute(false);
    root.Compute(false);
    DOCTEST_CHECK(nested.m_values[2] == value);
    DOCTEST_CHECK(nested.m_tickDistributor.m_samplesToDistribute == remaining);
    rig.RunHostFrames(Distributor::x_maxSamplesToDistribute);
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
    rig.RunHostFrames(Distributor::x_maxSamplesToDistribute);
    DOCTEST_CHECK(rig.Cell().m_values[0] == 0.75f);
    DOCTEST_CHECK_FALSE(recorder.m_paramEventsRing->Pop(event));

    JSON loaded = arena.Loads(R"({"values":[0.2]})");
    rig.Cell().StateEncoderCell::FromJSON(loaded);
    DOCTEST_CHECK(rig.Cell().m_values[0] == 0.75f);
    DOCTEST_CHECK(rig.Cell().m_targets[0] == 0.2f);
    rig.RunHostFrames(Distributor::x_maxSamplesToDistribute);
    DOCTEST_CHECK(rig.Cell().m_values[0] == 0.2f);
    DOCTEST_CHECK_FALSE(recorder.m_paramEventsRing->Pop(event));
}

DOCTEST_TEST_CASE("encoder distribution: gesture capture synchronizes a moving accumulator")
{
    GlobalEnv::ResetPerTest();
    TickRig rig;
    auto& root = rig.Cell();
    root.m_modulators.AddGesture(&root, 0);
    auto& gesture = *root.m_modulators.m_gestures[0];
    gesture.SetTarget(0.8f, 0);
    gesture.SetForceUpdateRecursive();
    rig.RunHostFrames(480);
    root.SetTarget(0.8f, 0);
    gesture.SetActive(true);
    DOCTEST_REQUIRE(gesture.m_values[0] == root.m_values[0]);
    DOCTEST_REQUIRE(gesture.m_smoothedValues[0] == root.m_values[0]);
    double captured = gesture.m_smoothedValues[0];
    gesture.Compute(true);
    DOCTEST_CHECK(gesture.m_values[0] == doctest::Approx(0.8 + (captured - 0.8) * std::exp(-1.0 / 960.0)));
}

DOCTEST_TEST_CASE("encoder distribution: retain the accepted unchanged-target idle capture defect")
{
    GlobalEnv::ResetPerTest();
    TickRig rig;
    auto& root = rig.Cell();
    root.m_modulators.AddGesture(&root, 0);
    auto& gesture = *root.m_modulators.m_gestures[0];
    root.SetTarget(0.0f, 0);
    gesture.SetActive(true);
    gesture.SetForceUpdateRecursive();
    DOCTEST_REQUIRE(gesture.m_values[0] == 0.25f);
    DOCTEST_REQUIRE(gesture.m_targets[0] == 0.0f);
    rig.RunHostFrames(Distributor::x_maxSamplesToDistribute);
    DOCTEST_CHECK(gesture.m_values[0] == 0.25f);
    DOCTEST_CHECK(gesture.m_tickDistributor.IsDone());
}
