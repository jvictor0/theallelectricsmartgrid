#include "doctest.h"

#include <cmath>
#include <memory>

#include "BitCrush.hpp"
#include "EncoderBankBank.hpp"
#include "TheNonagonSquiggleBoy.hpp"
#include "../support/GlobalEnv.hpp"

#ifdef SMARTGRID_TEST_OVERSAMPLE
static_assert(SampleTimer::x_oversample == SMARTGRID_TEST_OVERSAMPLE);
#endif

namespace
{

using Cell = SmartGrid::BankedEncoderCell;
using Distributor = SmartGrid::TickDistributor;
constexpr size_t x_ratio = SampleTimer::x_oversample;

struct HostRateRig
{
    SmartGridOneContext m_context;
    EncoderBankBank m_banks;

    HostRateRig()
        : m_banks(1, 1, 1, &m_context)
    {
        GlobalEnv::Init();
        GlobalEnv::ResetPerTest();
        m_banks.InitMode(0, 4);
        m_banks.InitBank(0, 0, SmartGrid::Color::Red);
        m_banks.CreateEncoder(0, 0, 0.5f, "Carrier", "CAR", SmartGrid::Color::Red, 0, false);
        m_banks.PlaceEncoder(0, 0, 0, 0);
        for (size_t i = 0; i < Cell::x_numModulators; ++i)
        {
            Sources().SetModulatorSkin(i, SmartGrid::Color::Red, nullptr);
        }
    }

    Cell& Carrier()
    {
        return *m_banks.GetEncoder(0);
    }

    Cell::ModulatorValues& Sources()
    {
        return m_banks.m_bankModes[0].m_modulatorValues;
    }

    Cell& Connect(size_t slot)
    {
        Carrier().FillModulators(&m_context);
        Cell& depth = *Carrier().m_modulators.m_modulators[slot];
        depth.SetValue(1.0f, true);
        depth.SetForceUpdateRecursive();
        return depth;
    }

    void Process()
    {
        if (SampleTimer::IsHostFrame())
        {
            m_context.m_sceneManager.Process();
        }

        m_banks.Process();
    }

    void Step()
    {
        SampleTimer::IncrementSample();
        Process();
    }

    void RunHostFrames(size_t frames)
    {
        for (size_t i = 0; i < frames * x_ratio; ++i)
        {
            Step();
        }
    }

    void Settle()
    {
        RunHostFrames(Distributor::x_maxSamplesToDistribute + 1);
    }
};

}

DOCTEST_TEST_CASE("encoder host rate: previous output weight ramps to zero across a host frame")
{
    DOCTEST_CHECK(Cell::GetPreviousOutputWeight(0, 4) == doctest::Approx(0.75f));
    DOCTEST_CHECK(Cell::GetPreviousOutputWeight(1, 4) == doctest::Approx(0.5f));
    DOCTEST_CHECK(Cell::GetPreviousOutputWeight(2, 4) == doctest::Approx(0.25f));
    DOCTEST_CHECK(Cell::GetPreviousOutputWeight(3, 4) == 0.0f);
    DOCTEST_CHECK(Cell::GetPreviousOutputWeight(0, 1) == 0.0f);
}

DOCTEST_TEST_CASE("encoder host rate: reads interpolate from the previous host frame output")
{
    HostRateRig rig;
    Cell& cell = rig.Carrier();
    size_t hostSample = SampleTimer::GetHostSample() + 1;
    cell.SetOutput(QuadFloat::Splat(0.2f), hostSample);
    cell.SetOutput(QuadFloat::Splat(0.6f), hostSample + 1);

    // A second write in the same host frame replaces the latest output and keeps
    // the previous frame's output.
    //
    cell.SetOutput(QuadFloat::Splat(1.0f), hostSample + 1);
    for (size_t subSample = 0; subSample < 4; ++subSample)
    {
        float weight = Cell::GetPreviousOutputWeight(subSample, 4);
        float expected = 0.2f * weight + 1.0f * (1.0f - weight);
        for (size_t channel = 0; channel < 4; ++channel)
        {
            DOCTEST_CHECK(cell.GetInterpolatedValue(channel, hostSample + 1, weight) == doctest::Approx(expected));
            DOCTEST_CHECK(cell.GetLatestValue(channel) == doctest::Approx(1.0f));
            cell.m_bipolar = true;
            DOCTEST_CHECK(cell.GetInterpolatedValue(channel, hostSample + 1, weight) == doctest::Approx(2 * expected - 1));
            cell.m_bipolar = false;
        }
    }

    // Once the matrix stops writing the cell, reads hold the latest output.
    //
    DOCTEST_CHECK(cell.GetInterpolatedValue(0, hostSample + 2, Cell::GetPreviousOutputWeight(0, 4)) == doctest::Approx(1.0f));
}

DOCTEST_TEST_CASE("encoder host rate: connected matrix computes only on host frames and interpolates every lane")
{
    HostRateRig rig;
    rig.Connect(3);
    rig.Settle();
    Cell& cell = rig.Carrier();
    cell.Increment(0.1f);
    size_t startHost = SampleTimer::GetHostSample();
    QuadFloat previous;
    QuadFloat latest = cell.m_output;
    for (size_t i = 0; i < 32 * x_ratio; ++i)
    {
        rig.Sources().m_value[3] = QuadFloat(0.1f, 0.3f, 0.6f, 0.9f) * (i % 7 < 3 ? 1.0f : 0.5f);
        SampleTimer::IncrementSample();
        rig.Process();
        size_t hostFrames = SampleTimer::GetHostSample() - startHost;
        DOCTEST_CHECK(cell.m_samplesForceUpdate == Distributor::x_maxSamplesToDistribute - hostFrames);
        if (SampleTimer::IsHostFrame())
        {
            previous = latest;
            latest = rig.Sources().m_hostValue[3];
        }

        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CHECK(cell.GetLatestValue(lane) == doctest::Approx(latest[lane]));
            float fraction = static_cast<float>(SampleTimer::GetSubSampleIndex() + 1) / x_ratio;
            float expected = hostFrames == 0 ? latest[lane] : previous[lane] + (latest[lane] - previous[lane]) * fraction;
            DOCTEST_CHECK(cell.GetValue(lane) == doctest::Approx(expected));
        }
    }
}

DOCTEST_TEST_CASE("encoder host rate: decimated steps stay normalized through the matrix and bit reduction")
{
    HostRateRig rig;
    rig.Connect(0);
    Cell::ModulatorValues::Decimator reference;
    QuadFloat initial(0.0f, 1.0f, 0.25f, 0.75f);
    for (size_t slot = 0; slot < Cell::x_numModulators; ++slot)
    {
        rig.Sources().m_value[slot] = initial;
    }

    for (size_t i = 0; i < (Distributor::x_maxSamplesToDistribute + 1) * x_ratio; ++i)
    {
        reference.Process(initial);
        rig.Step();
    }

    bool overshot = false;
    bool undershot = false;
    BitRateReducerGeneric<4> reducer;
    for (size_t i = 0; i < 256; ++i)
    {
        QuadFloat raw = i < 128 ? QuadFloat(1.0f, 0.0f, 0.75f, 0.25f) : initial;
        QuadFloat filtered = reference.Process(raw);
        for (size_t slot = 0; slot < Cell::x_numModulators; ++slot)
        {
            rig.Sources().m_value[slot] = raw;
        }

        rig.Step();
        QuadFloat amount;
        for (size_t lane = 0; lane < 4; ++lane)
        {
            overshot = overshot || filtered[lane] > 1.001f;
            undershot = undershot || filtered[lane] < -0.001f;
            for (size_t slot = 0; slot < Cell::x_numModulators; ++slot)
            {
                DOCTEST_REQUIRE(rig.Sources().m_hostValue[slot][lane] >= 0.0f);
                DOCTEST_REQUIRE(rig.Sources().m_hostValue[slot][lane] <= 1.0f);
            }

            amount[lane] = rig.Carrier().GetValue(lane);
            DOCTEST_REQUIRE(amount[lane] >= 0.0f);
            DOCTEST_REQUIRE(amount[lane] <= 1.0f);
        }

        reducer.SetAmount(amount);
        QuadFloat audio = reducer.Process(QuadFloat(0.1f, -0.2f, 0.4f, -0.8f));
        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_REQUIRE(std::isfinite(audio[lane]));
        }
    }

    if constexpr (x_ratio == 4)
    {
        DOCTEST_CHECK(overshot);
        DOCTEST_CHECK(undershot);
    }
}

DOCTEST_TEST_CASE("encoder host rate: target ramps and nested ancestors advance once per host frame")
{
    HostRateRig rig;
    Cell& root = rig.Carrier();
    root.FillModulators(&rig.m_context);
    Cell& depth = *root.m_modulators.m_modulators[0];
    depth.FillModulators(&rig.m_context);
    Cell& nested = *depth.m_modulators.m_modulators[1];
    rig.Sources().m_value[0] = QuadFloat::Splat(1);
    rig.Sources().m_value[1] = QuadFloat::Splat(1);
    rig.Settle();
    nested.Increment(0.25f);
    size_t startHost = SampleTimer::GetHostSample();
    for (size_t i = 0; i < Distributor::x_maxSamplesToDistribute * x_ratio; ++i)
    {
        float before = nested.m_values[0];
        size_t remaining = nested.m_tickDistributor.m_samplesToDistribute;
        root.Compute(false);
        nested.Compute(false);
        DOCTEST_REQUIRE(nested.m_values[0] == before);
        DOCTEST_REQUIRE(nested.m_tickDistributor.m_samplesToDistribute == remaining);
        rig.Step();
        size_t elapsed = SampleTimer::GetHostSample() - startHost;
        DOCTEST_REQUIRE(nested.m_values[0] == doctest::Approx(0.5f + 0.25f * elapsed / Distributor::x_maxSamplesToDistribute));
        DOCTEST_REQUIRE(nested.m_tickDistributor.m_samplesToDistribute == Distributor::x_maxSamplesToDistribute - elapsed);
        DOCTEST_REQUIRE(root.m_samplesForceUpdate == Distributor::x_maxSamplesToDistribute - elapsed);
        DOCTEST_REQUIRE(depth.m_samplesForceUpdate == root.m_samplesForceUpdate);
    }

    DOCTEST_CHECK(nested.m_values[0] == 0.75f);
    nested.Increment(-0.25f);
    rig.Settle();
    DOCTEST_CHECK(root.GetValue(0) == 0.5f);
    DOCTEST_CHECK(root.m_modulatorsAffecting.IsZero());
}

DOCTEST_TEST_CASE("encoder host rate: repeated target edits retain host-time ramp durations")
{
    HostRateRig rig;
    rig.Settle();
    Cell& cell = rig.Carrier();
    cell.Increment(0.25f);
    rig.RunHostFrames(480);
    DOCTEST_CHECK(cell.m_values[0] == doctest::Approx(0.55f));
    cell.Increment(0.1f);
    DOCTEST_CHECK(cell.m_targets[0] == doctest::Approx(0.85f));
    DOCTEST_REQUIRE(cell.m_tickDistributor.m_samplesToDistribute == 480);
    rig.RunHostFrames(479);
    DOCTEST_CHECK(cell.m_values[0] < cell.m_targets[0]);
    rig.RunHostFrames(1);
    DOCTEST_CHECK(cell.m_values[0] == cell.m_targets[0]);

    cell.Increment(-0.1f);
    rig.RunHostFrames(1);
    cell.Increment(-0.1f);
    DOCTEST_REQUIRE(cell.m_tickDistributor.m_samplesToDistribute == Distributor::x_minSamplesToDistribute);
    rig.RunHostFrames(Distributor::x_minSamplesToDistribute - 1);
    DOCTEST_CHECK(cell.m_values[0] != cell.m_targets[0]);
    rig.RunHostFrames(1);
    DOCTEST_CHECK(cell.m_values[0] == cell.m_targets[0]);
}

DOCTEST_TEST_CASE("encoder host rate: settled outputs hold across frames without recomputation")
{
    HostRateRig rig;
    rig.Carrier().SetToValue(0.75f);
    rig.Settle();
    size_t lastWrite = rig.Carrier().m_outputHostSample;
    DOCTEST_REQUIRE(rig.Carrier().m_samplesForceUpdate == 0);
    for (size_t i = 0; i < 4 * x_ratio; ++i)
    {
        rig.Step();
        DOCTEST_CHECK(rig.Carrier().m_outputHostSample == lastWrite);
        DOCTEST_CHECK(rig.Carrier().GetValue(0) == 0.75f);
    }
}

DOCTEST_TEST_CASE("encoder host rate: scene selection blend and endpoints survive every edit phase")
{
    for (size_t editPhase = 0; editPhase < x_ratio; ++editPhase)
    {
        DOCTEST_INFO("edit phase=" << editPhase);
        HostRateRig rig;
        Cell& cell = rig.Carrier();
        cell.SetAndRecordValue(0.2f, 0);
        cell.SetAndRecordValue(0.8f, 1);
        cell.SetAndRecordValue(0.6f, 2);
        rig.Settle();
        for (size_t i = 0; i < editPhase; ++i)
        {
            rig.Step();
        }

        auto& scene = rig.m_context.m_sceneManager;
        scene.m_scene1 = 2;
        const float blends[] = {0.25f, 0.5f, 1.0f, 0.0f};
        const float outputs[] = {0.65f, 0.7f, 0.8f, 0.6f};
        for (size_t change = 0; change < 4; ++change)
        {
            while (SampleTimer::GetSubSampleIndex() != editPhase)
            {
                rig.Step();
            }

            float previous = cell.GetLatestValue(0);
            scene.m_blendFactor = blends[change];
            do
            {
                rig.Step();
                if (!SampleTimer::IsHostFrame())
                {
                    DOCTEST_CHECK(cell.GetLatestValue(0) == previous);
                }
            }
            while (!SampleTimer::IsHostFrame());

            DOCTEST_CHECK(scene.m_changed);
            DOCTEST_CHECK(scene.m_changedScene == (change != 1));
            DOCTEST_CHECK(cell.GetLatestValue(0) == doctest::Approx(outputs[change]));
            for (size_t sub = 0; sub < x_ratio; ++sub)
            {
                if (sub > 0)
                {
                    rig.Step();
                }

                float expected = previous + (outputs[change] - previous) * (sub + 1) / x_ratio;
                DOCTEST_CHECK(cell.GetValue(0) == doctest::Approx(expected));
            }

            rig.RunHostFrames(1);
            DOCTEST_CHECK_FALSE(scene.m_changed);
            DOCTEST_CHECK_FALSE(scene.m_changedScene);
        }
    }
}

DOCTEST_TEST_CASE("encoder host rate: scene switches activate previously hidden routes")
{
    HostRateRig rig;
    Cell& root = rig.Carrier();
    root.FillModulators(&rig.m_context);
    Cell& depth = *root.m_modulators.m_modulators[2];
    depth.SetAndRecordValue(1.0f, 2);
    depth.SetForceUpdateRecursive();
    rig.Sources().m_value[2] = QuadFloat::Splat(0.25f);
    rig.Settle();
    DOCTEST_REQUIRE(root.m_modulatorsAffecting.IsZero());
    rig.m_context.m_sceneManager.m_scene1 = 2;
    rig.RunHostFrames(1);
    DOCTEST_CHECK_FALSE(root.m_modulatorsAffecting.IsZero());
    DOCTEST_CHECK(root.GetLatestValue(0) == doctest::Approx(0.25f));
    rig.m_context.m_sceneManager.m_scene1 = 0;
    rig.RunHostFrames(1);
    DOCTEST_CHECK(root.m_modulatorsAffecting.IsZero());
    DOCTEST_CHECK(root.GetLatestValue(0) == 0.5f);
}

DOCTEST_TEST_CASE("encoder host rate: amplitude-only changes are sampled on host frames")
{
    HostRateRig rig;
    rig.Connect(0);
    rig.Sources().m_value[0] = QuadFloat::Splat(1);
    rig.Settle();
    rig.Sources().m_amplitude[0] = QuadFloat(0, 0.25f, 0.5f, 1);
    for (size_t i = 0; i < x_ratio; ++i)
    {
        rig.Step();
        for (size_t lane = 0; lane < 4; ++lane)
        {
            float expected = SampleTimer::IsHostFrame() ? 0.5f + 0.5f * rig.Sources().m_amplitude[0][lane] : 1.0f;
            DOCTEST_CHECK(rig.Carrier().GetLatestValue(lane) == doctest::Approx(expected));
        }
    }
}

DOCTEST_TEST_CASE("encoder host rate: recording timestamps and saved values describe targets only")
{
    for (size_t editPhase = 0; editPhase < x_ratio; ++editPhase)
    {
        HostRateRig rig;
        rig.Settle();
        auto& recorder = rig.m_context.m_recorder;
        recorder.m_paramEventsRing = std::make_unique<CircularQueue<ParamEvent, StreamingRecorder::x_queueParamEvents>>();
        recorder.m_recordingStartSample = SampleTimer::GetHostSample();
        recorder.m_state = StreamingRecorder::State::Recording;
        rig.RunHostFrames(7);
        for (size_t i = 0; i < editPhase; ++i)
        {
            rig.Step();
        }

        rig.Carrier().SetToValue(0.75f);
        ParamEvent event;
        DOCTEST_REQUIRE(recorder.m_paramEventsRing->Pop(event));
        float recorded;
        std::memcpy(&recorded, event.m_value, sizeof(recorded));
        DOCTEST_CHECK(recorded == 0.75f);
        DOCTEST_CHECK(event.m_sample == 7);
        DOCTEST_CHECK(rig.Carrier().m_values[0] == 0.5f);
        JsonArena arena(4096);
        JSON saved = rig.Carrier().StateEncoderCell::ToJSON(arena);
        DOCTEST_CHECK(saved.Get("values").GetAt(0).NumberValue() == 0.75);
        rig.Settle();
        DOCTEST_CHECK(rig.Carrier().m_values[0] == 0.75f);
        DOCTEST_CHECK_FALSE(recorder.m_paramEventsRing->Pop(event));
    }
}

DOCTEST_TEST_CASE("encoder host rate: sequencer keeps the accepted read-before-matrix timing")
{
    GlobalEnv::ResetPerTest();
    auto system = std::make_unique<TheNonagonSquiggleBoyInternal>();
    auto& encoders = system->m_squiggleBoy.m_encoders;
    using Param = SmartGridOneEncoders::Param;
    Cell& cell = *encoders.m_encoderBankBank.GetEncoder(SmartGridOneEncoders::EncoderIndex(Param::SeqZoneHeight, 0));
    cell.SetValue(0.0f, true);
    for (size_t i = 0; i < (Distributor::x_maxSamplesToDistribute + 1) * x_ratio; ++i)
    {
        SampleTimer::IncrementSample();
        encoders.Process();
    }

    cell.SetAndRecordValue(1.0f, 1);
    for (size_t i = 0; i < Distributor::x_maxSamplesToDistribute * x_ratio; ++i)
    {
        SampleTimer::IncrementSample();
        encoders.Process();
    }

    system->SetBlendFactor(1.0f);
    AudioInputBuffer input;
    for (size_t i = 0; i < 2 * x_ratio; ++i)
    {
        SampleTimer::IncrementSample();
        system->ProcessSample(input);
        size_t sub = SampleTimer::GetSubSampleIndex();
        float expectedEarly = i < x_ratio ? 0.0f : static_cast<float>(sub + 1) / x_ratio;
        float expectedLate = i < x_ratio - 1 ? 0.0f : static_cast<float>(sub + 1) / x_ratio;
        if (i == 2 * x_ratio - 1)
        {
            expectedEarly = expectedLate = 1.0f;
        }

        DOCTEST_CHECK(system->m_nonagon.m_state.m_arpInput.m_zoneHeight[0] == doctest::Approx(expectedEarly));
        DOCTEST_CHECK(encoders.GetValue(Param::SeqZoneHeight, 0) == doctest::Approx(expectedLate));
    }
}

DOCTEST_TEST_CASE("encoder host rate: ratio 4 modulator decimation passes DC and rejects engine-rate content")
{
    using Decimator = DownsamplerGeneric<float, 4, Cell::ModulatorValues::x_decimatorOrder, 4>;
    Decimator dc;
    Decimator tone;
    float dcOut = 0.0f;
    float tonePeak = 0.0f;
    for (size_t i = 0; i < 4096; ++i)
    {
        dcOut = dc.Process(QuadFloat::Splat(0.5f))[0];
        float toneIn = static_cast<float>(std::sin(2.0 * M_PI * 0.3 * static_cast<double>(i)));
        float toneOut = tone.Process(QuadFloat::Splat(toneIn))[0];
        if (2048 <= i)
        {
            tonePeak = std::max(tonePeak, std::abs(toneOut));
        }
    }

    DOCTEST_CHECK(dcOut == doctest::Approx(0.5f));
    DOCTEST_CHECK(tonePeak < 0.1f);
}
