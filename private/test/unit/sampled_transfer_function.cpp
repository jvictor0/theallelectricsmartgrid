#include "doctest.h"
#include "Filter.hpp"
#include "TPTBaseWidthFilter.hpp"
#include "FIR.hpp"
#include "LadderFilter.hpp"
#include "MultibandSaturator.hpp"
#include "TPTSVFFilter.hpp"
#include "../support/SVFReference.hpp"
#include "TrioUtils.hpp"
#include "TunedDecayComb.hpp"
#include "../support/GlobalEnv.hpp"
#include "../../../formal/DSP/generated/OnePoleZOHLowPassGeneric.hpp"
#include <atomic>
#include <thread>

namespace
{
template<size_t Size>
ResponseFrequencyGeneric<MultiChannelNumber<double, Size>> FrequencyLanes(const std::array<double, Size>& values)
{
    ResponseFrequencyGeneric<MultiChannelNumber<double, Size>> result{};
    for (size_t lane = 0; lane < Size; ++lane)
    {
        auto scalar = ResponseFrequency::CyclesPerSample(values[lane]);
        result.m_cyclesPerSample[lane] = scalar.m_cyclesPerSample[0];
        result.m_sinHalf[lane] = scalar.m_sinHalf[0];
        result.m_cosHalf[lane] = scalar.m_cosHalf[0];
        result.m_zInverse.SetChannel(lane, scalar.m_zInverse[0]);
    }

    return result;
}

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
    DOCTEST_CHECK(result.m_lower == 256);
    DOCTEST_CHECK(result.m_median == 324);
    DOCTEST_CHECK(result.m_upper == 400);
    for (int i = 0; i < 100; ++i)
    {
        GainSnapshot snapshot;
        snapshot.m_gain = 1000;
        ui.m_history.Push(snapshot);
    }

    result = ui.Response(samples, frequency, 0);
    DOCTEST_CHECK(result.m_lower == 256);
    DOCTEST_CHECK(result.m_upper == 400);
    samples = ui.Sample();
    result = ui.Response(samples, frequency, 0);
    DOCTEST_CHECK(result.m_lower == 1000000);
    DOCTEST_CHECK(result.m_median == 1000000);
    DOCTEST_CHECK(result.m_upper == 1000000);

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
    DOCTEST_CHECK(response.m_lower == 4);
    DOCTEST_CHECK(response.m_median == 25);
    DOCTEST_CHECK(response.m_upper == 64);

    OPLowPassFilter filter;
    filter.SetAlphaFromNatFreq(0.01f);
    OPLowPassFilter::UIState lowPass(TestConfig(4, 1));
    lowPass.m_history.Push(filter.MakeSnapshot());
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
    QuadTPTBaseWidthFilter filter;
    QuadTPTBaseWidthFilter::UIState ui(TestConfig(32, 32));
    SampledSnapshotWriter<QuadTPTBaseWidthFilter::UIState> writer;
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
            QuadTPTBaseWidthFilter::UIState::SnapshotType expected = filter.MakeSnapshot();
            DOCTEST_CHECK(ui.MagnitudeSquaredResponse(ids[i], frequency, lane)
                == doctest::Approx(expected.MagnitudeSquaredResponse(frequency, lane)));
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
    DOCTEST_CHECK(response.m_upper == 9);
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
    MorphingTPTSVFFilterGeneric<float, 1> svf;
    LadderFilterLP ladder;
    for (float cutoff : {0.00001f, 0.001f, 0.1f, 0.49f})
    {
        lp.SetAlphaFromNatFreq(cutoff);
        hp.SetAlphaFromNatFreq(cutoff);
        ladder.SetCutoffAndResonance(cutoff, 0.5f);
        ladder.Process(0.1f);
        auto low = lp.MakeSnapshot();
        auto high = hp.MakeSnapshot();
        auto ladderState = ladder.MakeSnapshot();
        for (double frequency : {0.0, 0.000001, 0.001, 0.03, 0.2, 0.499, 0.5})
        {
            auto prepared = ResponseFrequency::CyclesPerSample(frequency);
            for (float morph : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
            {
                svf.SetCutoffResonanceAndMorph(cutoff, 0.8f, morph);
                const auto state = svf.MakeSnapshot();
                // Evaluate the independent z-polynomial at the same table-prepared angle.
                // Exact-frequency audio comparisons are covered by the DSP tests.
                //
                const double angle = std::atan2(prepared.m_sinHalf[0], prepared.m_cosHalf[0]) / M_PI;
                const auto reference = TestSVF::Transfer(state.m_svf.m_g[0], state.m_svf.m_k[0], angle);
                const auto expected = reference.m_low * static_cast<double>(state.m_low[0])
                    + reference.m_band * static_cast<double>(state.m_band[0])
                    + reference.m_high * static_cast<double>(state.m_high[0]);
                DOCTEST_CHECK(state.MagnitudeSquaredResponse(prepared, 0)
                    == doctest::Approx(std::norm(expected)).epsilon(0.00002));
                for (const FilterResponseSnapshot* snapshot : std::array<const FilterResponseSnapshot*, 3>{&low, &high, &ladderState})
                {
                    float magnitude = snapshot->MagnitudeSquaredResponse(prepared, 0);
                    DOCTEST_CHECK(std::isfinite(magnitude));
                    DOCTEST_CHECK(magnitude == doctest::Approx(std::norm(snapshot->ComplexResponse(prepared, 0))).epsilon(1e-5));
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
    size_t finalPublished = 0;
    std::thread producer([&]
    {
        for (size_t machine = 1; machine <= 20000; ++machine)
        {
            ui.m_history.Clear();
            finalPublished = 0;
            GainSnapshot snapshot;
            snapshot.m_gain = static_cast<float>(machine);
            for (size_t i = 0; i < 4; ++i)
            {
                // A UI-owned slot may reject a push. Count accepted samples
                // for the final cohort; join makes this count safe to read.
                //
                if (ui.m_history.Push(snapshot))
                {
                    ++finalPublished;
                }
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
    DOCTEST_CHECK(ui.Sample().size() == finalPublished);
}

namespace
{
struct TrioGainSnapshot : VectorResponseSnapshot<3>
{
    ResponseVector m_gain;

    ComplexVector ComplexResponses(const Frequency&) const override
    {
        return {m_gain.Convert<double>(), DoubleVector()};
    }
};

template<typename Snapshot, typename MonoSnapshot>
void CheckResponseLanes(const Snapshot& snapshot, const std::array<MonoSnapshot, 3>& mono)
{
    for (double frequency : {0.0, 0.000001, 0.001, 0.03, 0.2, 0.499, 0.5})
    {
        auto prepared = ResponseFrequency::CyclesPerSample(frequency);
        auto vectorFrequency = prepared.template Broadcast<MultiChannelNumber<double, Snapshot::x_numLanes>>();
        auto magnitude = snapshot.MagnitudeSquaredResponses(vectorFrequency);
        for (size_t lane = 0; lane < 3; ++lane)
        {
            if constexpr (std::is_base_of_v<FilterResponseSnapshot, Snapshot>)
            {
                auto reference = mono[lane].ComplexResponse(prepared, 0);
                DOCTEST_CHECK(std::abs(snapshot.ComplexResponses(vectorFrequency)[lane] - static_cast<std::complex<double>>(reference))
                    <= 0.00002 * std::max(1.0f, std::abs(reference)));
            }

            DOCTEST_CHECK(magnitude[lane] == doctest::Approx(mono[lane].MagnitudeSquaredResponse(prepared, 0)).epsilon(0.00002));
        }
    }

    auto prepared = FrequencyLanes<3>({0.0, 0.03, 0.5});
    auto magnitude = snapshot.MagnitudeSquaredResponses(prepared);
    for (size_t lane = 0; lane < 3; ++lane)
    {
        auto scalar = prepared.Channel(lane);
        if constexpr (std::is_base_of_v<FilterResponseSnapshot, Snapshot>)
        {
            auto reference = mono[lane].ComplexResponse(scalar, 0);
            DOCTEST_CHECK(std::abs(snapshot.ComplexResponses(prepared)[lane] - static_cast<std::complex<double>>(reference))
                <= 0.00002 * std::max(1.0f, std::abs(reference)));
        }

        DOCTEST_CHECK(magnitude[lane] == doctest::Approx(mono[lane].MagnitudeSquaredResponse(scalar, 0)).epsilon(0.00002));
    }
}
}

DOCTEST_TEST_CASE("Sampled vectors: generated ZOH uses the handwritten prepared frequency and nested snapshot")
{
    using Generated = OnePoleZOHLowPassGeneric<double, 3>;
    using Handwritten = OPLowPassFilterGeneric<float, 3>;
    static_assert(std::is_same_v<Generated::Frequency, Handwritten::Snapshot::Frequency>);
    static_assert(std::is_same_v<Generated::Frequency::Complex, MultiChannelComplex<double, 3>>);
    Handwritten::Snapshot snapshot;
    snapshot.m_alpha = TrioFloat(0.0f, 0.001f, 1.0f);
    Generated::Snapshot generated{snapshot.m_alpha.Convert<double>()};
    for (const auto& frequencies : {std::array<double, 3>{0, 0, 0.5}, {0.03, 0.2, 0.499}})
    {
        auto frequency = FrequencyLanes(frequencies);
        auto squared = Generated::MagnitudeSquared(generated, frequency);
        auto magnitude = snapshot.MagnitudeSquaredResponses(frequency);
        for (size_t lane = 0; lane < 3; ++lane)
        {
            DOCTEST_CHECK(std::isfinite(squared[lane]));
            DOCTEST_CHECK(squared[lane] == doctest::Approx(magnitude[lane]).epsilon(0.000002));
        }
    }
}

DOCTEST_TEST_CASE("Sampled vectors: DC endpoint selection is independent per frequency lane")
{
    auto frequency = FrequencyLanes<3>({0, 0.1, 0.5});
    OPHighPassFilterGeneric<float, 3>::Snapshot high;
    high.m_alpha = TrioFloat(1, 0.5f, 0.5f);
    TPTSVFLowPassFilterGeneric<float, 3>::Snapshot svf;
    svf.m_g = TrioFloat(0.01f, 0.1f, 0.2f);
    svf.m_k = TrioFloat::Splat(1);
    auto highComplex = high.ComplexResponses(frequency);
    auto highMagnitude = high.MagnitudeSquaredResponses(frequency);
    auto svfMagnitude = svf.MagnitudeSquaredResponses(frequency);
    DOCTEST_CHECK(highComplex[0] == std::complex<double>(0, 0));
    DOCTEST_CHECK(highMagnitude[0] == 0);
    DOCTEST_CHECK(svfMagnitude[0] == 1);
    for (size_t lane = 0; lane < 3; ++lane)
    {
        DOCTEST_CHECK(std::isfinite(highMagnitude[lane]));
        DOCTEST_CHECK(std::isfinite(svfMagnitude[lane]));
    }
}

DOCTEST_TEST_CASE("Sampled vectors: padded order statistics independently sort every lane")
{
    for (size_t count : {1, 2, 3, 5, 11, 17, 31, 32, 33, 63})
    {
        for (float lower : {0.0f, 0.1f, 0.5f})
        {
            auto config = TestConfig(64, count);
            config.m_lowerPercentile = lower;
            config.m_upperPercentile = 1 - lower;
            TransferFunctionGeneric<TrioGainSnapshot> ui(config);
            TransferFunctionGeneric<GainSnapshot> mono(config);
            std::array<std::vector<float>, 3> expected;
            for (size_t sample = 0; sample < count; ++sample)
            {
                TrioGainSnapshot snapshot;
                for (size_t lane = 0; lane < 3; ++lane)
                {
                    float value = static_cast<float>((sample * (11 + lane * 2) + lane * 7) % 37);
                    snapshot.m_gain[lane] = value;
                    expected[lane].push_back(value * value);
                }

                ui.m_history.Push(snapshot);
                GainSnapshot scalar;
                scalar.m_gain = snapshot.m_gain[0];
                mono.m_history.Push(scalar);
            }

            auto samples = ui.Sample();
            auto scalarSamples = mono.Sample();
            auto scalar = mono.ResponseHz(scalarSamples, 1000, 192000, 0);
            SampledTransferFunction::OrderStatistics actual[3];
            ui.ResponseAll(samples, ResponseFrequency::Hz(1000, 192000), actual);
            DOCTEST_CHECK(scalar.m_lower == actual[0].m_lower);
            DOCTEST_CHECK(scalar.m_median == actual[0].m_median);
            DOCTEST_CHECK(scalar.m_upper == actual[0].m_upper);
            for (size_t lane = 0; lane < 3; ++lane)
            {
                std::sort(expected[lane].begin(), expected[lane].end());
                DOCTEST_CHECK(actual[lane].m_lower == expected[lane][static_cast<size_t>(lower * (count - 1))]);
                DOCTEST_CHECK(actual[lane].m_median == expected[lane][(count - 1) / 2]);
                DOCTEST_CHECK(actual[lane].m_upper == expected[lane][static_cast<size_t>((1 - lower) * (count - 1))]);
            }
        }
    }
}

DOCTEST_TEST_CASE("Sampled vectors: captured filter coefficients and responses preserve independent voices")
{
    const TrioFloat cutoff(0.0001f, 0.01f, 0.42f);
    const TrioFloat resonance(0.1f, 0.6f, 0.9f);
    OPLowPassFilterGeneric<float, 3> lp;
    OPHighPassFilterGeneric<float, 3> hp;
    MorphingTPTSVFFilterGeneric<float, 3> svf;
    LadderFilterLPGeneric<float, 3> ladder;
    lp.SetAlphaFromNatFreq(cutoff);
    hp.SetAlphaFromNatFreq(cutoff);
    svf.SetCutoffResonanceAndMorph(cutoff, resonance, TrioFloat(0.0f, 0.3f, 0.8f));
    ladder.SetCutoffAndResonance(cutoff, resonance);
    ladder.Process(TrioFloat::Splat(0.1f));
    std::array<OPLowPassFilter::Snapshot, 3> monoLP;
    std::array<OPHighPassFilter::Snapshot, 3> monoHP;
    std::array<MorphingTPTSVFFilterGeneric<float, 1>::Snapshot, 3> monoSVF;
    std::array<LadderFilterLP::Snapshot, 3> monoLadder;
    auto state = svf.MakeSnapshot();
    for (size_t lane = 0; lane < 3; ++lane)
    {
        OPLowPassFilter oneLP;
        OPHighPassFilter oneHP;
        MorphingTPTSVFFilterGeneric<float, 1> oneSVF;
        LadderFilterLP oneLadder;
        oneLP.SetAlphaFromNatFreq(cutoff[lane]);
        oneHP.SetAlphaFromNatFreq(cutoff[lane]);
        oneSVF.SetCutoffResonanceAndMorph(cutoff[lane], resonance[lane], std::array<float, 3>{0.0f, 0.3f, 0.8f}[lane]);
        oneLadder.SetCutoffAndResonance(cutoff[lane], resonance[lane]);
        oneLadder.Process(0.1f);
        monoLP[lane] = oneLP.MakeSnapshot();
        monoHP[lane] = oneHP.MakeSnapshot();
        monoSVF[lane] = oneSVF.MakeSnapshot();
        monoLadder[lane] = oneLadder.MakeSnapshot();
    }

    DOCTEST_CHECK(lp.MakeSnapshot().m_alpha == lp.m_alpha);
    DOCTEST_CHECK(svf.MakeSnapshot().m_svf.m_k == svf.m_state.m_svf.m_k);
    DOCTEST_CHECK(ladder.MakeSnapshot().m_feedback == ladder.m_kEff);
    CheckResponseLanes(lp.MakeSnapshot(), monoLP);
    CheckResponseLanes(hp.MakeSnapshot(), monoHP);
    CheckResponseLanes(state, monoSVF);
    CheckResponseLanes(ladder.MakeSnapshot(), monoLadder);
}

DOCTEST_TEST_CASE("Sampled vectors: physical comb response retains independent delays and crossfade branches")
{
    for (double mix : {0.0, 0.25, 1.0})
    {
        TrioTunedDecayComb::Snapshot vector;
        std::array<TunedDecayComb::Snapshot, 3> mono;
        vector.m_mix = mix;
        for (size_t lane = 0; lane < 3; ++lane)
        {
            mono[lane].m_mix = mix;
            for (size_t branch = 0; branch < 2; ++branch)
            {
                auto coefficients = TunedDecayComb::ComputeCoefficients(
                    (110.0 + lane * 330 + branch * 25) / SampleTimer::x_sampleRate,
                    lane == 0 ? 0 : lane == 1 ? -1 : 1, std::log2(0.2 + lane),
                    1 + lane * 2, SampleTimer::x_sampleRate);
                vector.m_branch[branch].Set(lane, coefficients);
                mono[lane].m_branch[branch].Set(0, coefficients);
            }
        }

        CheckResponseLanes(vector, mono);
    }
}

DOCTEST_TEST_CASE("Sampled vectors: grouped FIR taps match the full complex polynomial")
{
    FIR<31> filter;
    for (float blend : {0.0f, 0.3f, 0.7f, 1.0f})
    {
        filter.SetParams(0.13f, blend, 0.37f);
        std::copy_n(filter.m_targetTaps, 31, filter.m_slewedTaps);
        auto snapshot = filter.MakeSnapshot();
        for (double normalized : {0.0, 0.001, 0.1, 0.3, 0.499, 0.5})
        {
            auto frequency = ResponseFrequency::CyclesPerSample(normalized);
            std::complex<double> expected{};
            std::complex<double> power{1, 0};
            for (float tap : filter.m_slewedTaps)
            {
                expected += power * static_cast<double>(tap);
                power *= frequency.m_zInverse[0];
            }

            auto actual = snapshot.ComplexResponses(frequency)[0];
            DOCTEST_CHECK(std::abs(actual - expected) < 1e-10);
        }
    }
}
