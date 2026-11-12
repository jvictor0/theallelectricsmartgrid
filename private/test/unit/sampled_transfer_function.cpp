#include "doctest.h"
#include "Filter.hpp"
#include "FIR.hpp"
#include "LadderFilter.hpp"
#include "MultibandSaturator.hpp"
#include "StateVariableFilter.hpp"
#include "../support/GlobalEnv.hpp"
#include <atomic>
#include <thread>

namespace
{
struct GainSnapshot : FilterResponseSnapshot
{
    float m_gain = 0;

    std::complex<float> ComplexResponse(const ResponseFrequency&, size_t) const override
    {
        return {0, m_gain};
    }
};

SampledTransferFunction::Config TestConfig(size_t capacity, size_t samples)
{
    SampledTransferFunction::Config config;
    config.m_capacity = capacity;
    config.m_numSamples = samples;
    config.m_lowerPercentile = 0;
    config.m_upperPercentile = 1;
    config.m_samplingDivisor = 1;
    return config;
}
}

DOCTEST_TEST_CASE("Sampled response: order statistics use the recent cohort and survive producer wrap")
{
    TransferFunctionGeneric<GainSnapshot> ui(TestConfig(8, 5));
    DOCTEST_CHECK(ui.Sample().empty());
    for (int value = 1; value <= 20; ++value)
    {
        GainSnapshot snapshot;
        snapshot.m_gain = static_cast<float>(value);
        DOCTEST_REQUIRE(ui.m_history.Push(snapshot));
    }

    auto samples = ui.Sample();
    DOCTEST_REQUIRE(samples.size() == 5);
    auto frequency = ResponseFrequency::Hz(1000, 48000);
    auto result = ui.Response(samples, frequency, 0);
    DOCTEST_CHECK(result.m_lower == 16);
    DOCTEST_CHECK(result.m_median == 18);
    DOCTEST_CHECK(result.m_upper == 20);
    for (int i = 0; i < 100; ++i)
    {
        GainSnapshot snapshot;
        snapshot.m_gain = 1000;
        ui.m_history.Push(snapshot);
    }

    result = ui.Response(samples, frequency, 0);
    DOCTEST_CHECK(result.m_lower == 16);
    DOCTEST_CHECK(result.m_upper == 20);
    samples = ui.Sample();
    result = ui.Response(samples, frequency, 0);
    DOCTEST_CHECK(result.m_lower == 1000);
    DOCTEST_CHECK(result.m_median == 1000);
    DOCTEST_CHECK(result.m_upper == 1000);

    ui.m_history.Clear();
    DOCTEST_CHECK(ui.Sample().empty());
}

DOCTEST_TEST_CASE("Sampled response: configured percentiles and frequency units agree")
{
    auto config = TestConfig(64, 11);
    config.m_lowerPercentile = 0.2f;
    config.m_upperPercentile = 0.8f;
    TransferFunctionGeneric<GainSnapshot> ui(config);
    for (int i = 0; i <= 10; ++i)
    {
        GainSnapshot snapshot;
        snapshot.m_gain = static_cast<float>(i);
        ui.m_history.Push(snapshot);
    }

    auto samples = ui.Sample();
    auto response = ui.ResponseVOct(samples, 0, 48000, 0);
    DOCTEST_CHECK(response.m_lower == 2);
    DOCTEST_CHECK(response.m_median == 5);
    DOCTEST_CHECK(response.m_upper == 8);

    OPLowPassFilter filter;
    filter.SetAlphaFromNatFreq(0.01f);
    OPLowPassFilter::UIState lowPass(TestConfig(4, 1));
    lowPass.m_history.Push(filter.MakeSnapshot(0));
    auto ids = lowPass.Sample();
    for (double rate : {48000.0, 192000.0})
    {
        for (double pitch : {-4.0, 0.0, 4.0})
        {
            auto log = lowPass.ResponseVOct(ids, pitch, rate, 0);
            auto hz = lowPass.ResponseHz(ids, Tuning::x_middleCHz * std::exp2(pitch), rate, 0);
            DOCTEST_CHECK(log.m_median == hz.m_median);
        }
    }
}

DOCTEST_TEST_CASE("Sampled response: audio sampling is host gated and captures coherent quad base-width state")
{
    GlobalEnv::ResetPerTest();
    QuadOPBaseWidthFilter filter;
    QuadOPBaseWidthFilter::UIState ui(TestConfig(32, 32));
    SampledSnapshotWriter<QuadOPBaseWidthFilter::UIState> writer;
    writer.Bind(&ui);
    for (size_t sample = 1; sample <= 16 * SampleTimer::x_oversample; ++sample)
    {
        SampleTimer::IncrementSample();
        float base = 0.001f * static_cast<float>(sample);
        filter.SetBaseWidth(QuadFloat(base, base * 2, base * 3, base * 4), QuadFloat::Splat(2));
        filter.PopulateUIStateSample(writer);
        DOCTEST_CHECK(ui.m_history.m_published.load() == sample / SampleTimer::x_oversample);
    }

    auto ids = ui.Sample();
    DOCTEST_REQUIRE(ids.size() == 16);
    auto frequency = ResponseFrequency::CyclesPerSample(0.08);
    for (size_t i = 0; i < ids.size(); ++i)
    {
        float base = 0.001f * static_cast<float>((16 - i) * SampleTimer::x_oversample);
        filter.SetBaseWidth(QuadFloat(base, base * 2, base * 3, base * 4), QuadFloat::Splat(2));
        for (size_t lane = 0; lane < 4; ++lane)
        {
            auto expected = filter.MakeSnapshot(lane);
            DOCTEST_CHECK(ui.MagnitudeResponse(ids[i], frequency, lane)
                == doctest::Approx(expected.MagnitudeResponse(frequency, 0)));
        }
    }

    writer.SetSource(1);
    DOCTEST_CHECK(ui.Sample().empty());
    filter.PopulateUIStateSample(writer);
    DOCTEST_CHECK(ui.Sample().size() == 1);
}

DOCTEST_TEST_CASE("Sampled response: random sampling avoids locking to a periodic modulation")
{
    GlobalEnv::ResetPerTest();
    auto config = TestConfig(4096, 512);
    config.m_samplingDivisor = 64;
    TransferFunctionGeneric<GainSnapshot> ui(config);
    SampledSnapshotWriter<decltype(ui)> writer;
    writer.Bind(&ui);
    for (size_t sample = 1; sample <= SampleTimer::x_sampleRate * 2; ++sample)
    {
        SampleTimer::IncrementSample();
        writer.PopulateUIStateSample([&]
        {
            GainSnapshot snapshot;
            snapshot.m_gain = SampleTimer::GetHostSample() % 64 < 32 ? 1 : 3;
            return snapshot;
        });
    }

    DOCTEST_CHECK(ui.m_history.m_published.load() > 1200);
    DOCTEST_CHECK(ui.m_history.m_published.load() < 1800);
    auto ids = ui.Sample();
    auto response = ui.ResponseHz(ids, 1000, 48000, 0);
    DOCTEST_CHECK(response.m_lower == 1);
    DOCTEST_CHECK(response.m_upper == 3);
}

DOCTEST_TEST_CASE("Sampled response: default history window averages one 60 Hz paint")
{
    GlobalEnv::ResetPerTest();
    TransferFunctionGeneric<GainSnapshot> ui;
    SampledSnapshotWriter<decltype(ui)> writer;
    writer.Bind(&ui);
    double ageSum = 0;
    size_t paints = 0;
    for (size_t sample = 1; sample <= SampleTimer::x_sampleRate * 2; ++sample)
    {
        SampleTimer::IncrementSample();
        writer.PopulateUIStateSample([&]
        {
            GainSnapshot snapshot;
            snapshot.m_gain = static_cast<float>(SampleTimer::GetHostSample());
            return snapshot;
        });
        if (sample > SampleTimer::x_sampleRate / 2 && sample % (SampleTimer::x_sampleRate / 60) == 0)
        {
            auto ids = ui.Sample();
            DOCTEST_REQUIRE(ids.size() == 32);
            double oldest = ui.m_readSnapshots.back().m_gain;
            ageSum += (SampleTimer::GetHostSample() - oldest) / SampleTimer::x_hostSampleRate;
            ++paints;
        }
    }

    DOCTEST_CHECK(ageSum / paints == doctest::Approx(1.0 / 60).epsilon(0.1));
}

DOCTEST_TEST_CASE("Sampled response: optimized magnitudes match complex responses at DC through Nyquist")
{
    OPLowPassFilter lp;
    OPHighPassFilter hp;
    LinearStateVariableFilter svf;
    LadderFilterLP ladder;
    for (float cutoff : {0.00001f, 0.001f, 0.1f, 0.49f})
    {
        lp.SetAlphaFromNatFreq(cutoff);
        hp.SetAlphaFromNatFreq(cutoff);
        svf.SetCutoffAndResonance(cutoff, 0.8f);
        ladder.SetCutoff(cutoff);
        ladder.SetResonance(0.5f);
        ladder.Process(0.1f);
        auto low = lp.MakeSnapshot(0);
        auto high = hp.MakeSnapshot(0);
        auto state = svf.MakeSnapshot(0);
        auto ladderState = ladder.MakeSnapshot(0);
        for (double frequency : {0.0, 0.000001, 0.001, 0.03, 0.2, 0.499, 0.5})
        {
            auto prepared = ResponseFrequency::CyclesPerSample(frequency);
            for (float morph : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
            {
                state.m_weights = svf.GetMorphWeights(morph);
                for (const FilterResponseSnapshot* snapshot : std::array<const FilterResponseSnapshot*, 4>{&low, &high, &state, &ladderState})
                {
                    float magnitude = snapshot->MagnitudeResponse(prepared, 0);
                    DOCTEST_CHECK(std::isfinite(magnitude));
                    DOCTEST_CHECK(magnitude == doctest::Approx(std::abs(snapshot->ComplexResponse(prepared, 0))).epsilon(1e-5));
                }
            }
        }
    }
}

DOCTEST_TEST_CASE("Sampled history: concurrent wrap cannot tear a captured coefficient tuple")
{
    struct Snapshot
    {
        std::array<size_t, 64> m_values{};
    };

    SampledSnapshotHistory<Snapshot> history(8);
    std::atomic<bool> finished{false};
    std::thread producer([&]
    {
        for (size_t value = 1; value <= 50000; ++value)
        {
            Snapshot snapshot;
            snapshot.m_values.fill(value);
            history.Push(snapshot);
        }

        finished.store(true);
    });
    size_t reads = 0;
    bool coherent = true;
    do
    {
        size_t newest = history.m_published.load();
        for (size_t offset = 0; offset < std::min(size_t{8}, newest); ++offset)
        {
            Snapshot snapshot;
            if (history.Read(newest - offset, snapshot))
            {
                ++reads;
                for (size_t value : snapshot.m_values)
                {
                    coherent = coherent && value == snapshot.m_values[0];
                }
            }
        }
    }
    while (!finished.load());
    producer.join();
    DOCTEST_CHECK(reads > 0);
    DOCTEST_CHECK(coherent);

    size_t next = history.m_published.load() + 1;
    history.m_slots[next % history.m_capacity].m_owned.store(true);
    DOCTEST_CHECK_FALSE(history.Push(Snapshot{}));
    history.m_slots[next % history.m_capacity].m_owned.store(false);
    DOCTEST_CHECK(history.Push(Snapshot{}));
}

DOCTEST_TEST_CASE("Sampled response: concurrent machine switches cannot mix response histories")
{
    TransferFunctionGeneric<GainSnapshot> ui(TestConfig(64, 32));
    std::atomic<bool> finished{false};
    std::thread producer([&]
    {
        for (size_t machine = 1; machine <= 20000; ++machine)
        {
            ui.m_history.Clear();
            GainSnapshot snapshot;
            snapshot.m_gain = static_cast<float>(machine);
            for (size_t i = 0; i < 4; ++i)
            {
                ui.m_history.Push(snapshot);
            }
        }

        finished.store(true);
    });
    bool coherent = true;
    do
    {
        auto ids = ui.Sample();
        if (!ids.empty())
        {
            auto response = ui.ResponseHz(ids, 1000, 48000, 0);
            coherent = coherent && response.m_lower == response.m_upper;
        }
    }
    while (!finished.load());
    producer.join();
    DOCTEST_CHECK(coherent);
    DOCTEST_CHECK(ui.Sample().size() == 4);
}
