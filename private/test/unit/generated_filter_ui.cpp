#include "doctest.h"
#include "../../../formal/DSP/generated/TPTSVFGeneric.hpp"
#include "../../../formal/DSP/generated/OnePoleZOHLowPass.hpp"
#include "../../../formal/DSP/generated/OnePoleTPTLowPassGeneric.hpp"
#include "../support/GlobalEnv.hpp"
#include <complex>

namespace
{
template<typename Number, size_t Size>
void CheckGeneratedViews()
{
    using Filter = TPTSVFGeneric<Number, Size>;
    using Vector = typename Filter::Vector;
    using Low = TPTSVFLowPassGeneric<Number, Size>;
    using High = TPTSVFHighPassGeneric<Number, Size>;
    using Band = TPTSVFBandPassGeneric<Number, Size>;
    using Notch = TPTSVFNotchGeneric<Number, Size>;
    using AllPass = TPTSVFAllPassGeneric<Number, Size>;
    Low low{};
    High high{};
    typename Filter::LogicalSpec spec{};
    for (size_t lane = 0; lane < Size; ++lane)
    {
        spec.m_cutoff[lane] = static_cast<Number>(0.02 + 0.03 * lane);
        spec.m_q[lane] = static_cast<Number>(0.6 + lane);
    }

    low.ComputeCoefficients(spec);
    high.ComputeCoefficients(spec);
    auto saved = low.MakeSnapshot();
    typename High::Snapshot hp = high.MakeSnapshot();
    typename Band::Snapshot bp = saved;
    typename Notch::Snapshot notch = saved;
    typename AllPass::Snapshot allPass = saved;
    static_assert(std::is_trivially_copyable_v<typename Low::Snapshot>);
    SampledTransferFunction::Config config;
    config.m_capacity = 8;
    config.m_numSamples = 4;
    config.m_samplingDivisor = 1;
    typename Low::UIState ui(config);
    SampledSnapshotWriter<typename Low::UIState> writer;
    writer.Bind(&ui);
    GlobalEnv::ResetPerTest();
    low.PopulateUIStateSample(writer);
    auto ids = ui.Sample();
    DOCTEST_REQUIRE(ids.size() == 1);
    DOCTEST_CHECK(ui.NumLanes() == Size);
    for (double cycles : {0.0, 0.005, 0.02, 0.1, 0.4, 0.5})
    {
        const auto frequency = ResponseFrequency::CyclesPerSample(cycles);
        const auto prepared = frequency.template Broadcast<MultiChannelNumber<double, Size>>();
        auto lpPower = saved.MagnitudeSquaredResponses(prepared);
        auto hpPower = hp.MagnitudeSquaredResponses(prepared);
        auto bpPower = bp.MagnitudeSquaredResponses(prepared);
        auto notchPower = notch.MagnitudeSquaredResponses(prepared);
        auto allPassPower = allPass.MagnitudeSquaredResponses(prepared);
        for (size_t lane = 0; lane < Size; ++lane)
        {
            using C = std::complex<double>;
            const C z = std::polar(1.0, -2 * std::acos(-1.0) * cycles);
            const C a = 1.0 - z;
            const C b = 1.0 + z;
            const double g = saved.m_g[lane];
            const double k = saved.m_k[lane];
            const C denominator = a * a + k * g * a * b + g * g * b * b;
            const C lp = g * g * b * b / denominator;
            const C h = a * a / denominator;
            const C band = g * a * b / denominator;
            DOCTEST_CHECK(lpPower[lane] == doctest::Approx(std::norm(lp)).epsilon(0.00002));
            DOCTEST_CHECK(hpPower[lane] == doctest::Approx(std::norm(h)).epsilon(0.00002));
            DOCTEST_CHECK(bpPower[lane] == doctest::Approx(std::norm(band)).epsilon(0.00002));
            DOCTEST_CHECK(notchPower[lane] == doctest::Approx(std::norm(lp + h)).epsilon(0.00002));
            DOCTEST_CHECK(allPassPower[lane] == doctest::Approx(std::norm(lp + h - k * band)).epsilon(0.00002));
            DOCTEST_CHECK(allPassPower[lane] == doctest::Approx(1.0).epsilon(0.00002));
            DOCTEST_CHECK(ui.MagnitudeSquaredResponse(ids.front(), frequency, lane) == lpPower[lane]);
        }
    }

    spec.m_cutoff = Vector::Splat(static_cast<Number>(0.3));
    low.ComputeCoefficients(spec);
    typename Filter::Input input{Vector::Splat(static_cast<Number>(1))};
    auto tuned = low.MakeSnapshot();
    low.Process(input);
    for (size_t lane = 0; lane < Size; ++lane)
    {
        DOCTEST_CHECK(saved.m_g[lane] != tuned.m_g[lane]);
        DOCTEST_CHECK(ui.m_readSnapshots.front().m_g[lane] == saved.m_g[lane]);
        DOCTEST_CHECK(low.MakeSnapshot().m_g[lane] == tuned.m_g[lane]);
        DOCTEST_CHECK(low.MakeSnapshot().m_k[lane] == tuned.m_k[lane]);
    }
}

// A handwritten outer snapshot can embed generated output-specific children.
// It needs neither virtual inheritance nor a complex-response interface.
//
struct ShapeSnapshot
{
    using Low = TPTSVFLowPassGeneric<double, 3>;
    using High = TPTSVFHighPassGeneric<double, 3>;
    using Frequency = Low::Snapshot::Frequency;
    using ResponseVector = Low::Snapshot::ResponseVector;
    static constexpr size_t x_numLanes = 3;
    Low::Snapshot m_low;
    High::Snapshot m_high;

    ResponseVector MagnitudeSquaredResponses(const Frequency& frequency) const
    {
        return m_low.MagnitudeSquaredResponses(frequency) * m_high.MagnitudeSquaredResponses(frequency);
    }

    float MagnitudeSquaredResponse(const ResponseFrequency& frequency, size_t lane) const
    {
        return MagnitudeSquaredResponses(frequency.Broadcast<MultiChannelNumber<double, 3>>())[lane];
    }
};
}

DOCTEST_TEST_CASE("Generated UI: output views, channel selection and captured history")
{
    CheckGeneratedViews<float, 1>();
    CheckGeneratedViews<float, 2>();
    CheckGeneratedViews<float, 3>();
    CheckGeneratedViews<float, 4>();
    CheckGeneratedViews<double, 1>();
    CheckGeneratedViews<double, 2>();
    CheckGeneratedViews<double, 3>();
    CheckGeneratedViews<double, 4>();
}

DOCTEST_TEST_CASE("Generated UI: handwritten composition embeds generated child snapshots")
{
    ShapeSnapshot::Low low{};
    ShapeSnapshot::High high{};
    using V = ShapeSnapshot::Low::Vector;
    low.ComputeCoefficients({V(0.1, 0.2, 0.3), V(0.5, 0.7, 1.0)});
    high.ComputeCoefficients({V(0.01, 0.03, 0.05), V(1.0, 0.7, 0.5)});
    ShapeSnapshot snapshot{low.MakeSnapshot(), high.MakeSnapshot()};
    TransferFunctionGeneric<ShapeSnapshot> ui;
    DOCTEST_REQUIRE(ui.m_history.Push(snapshot));
    auto ids = ui.Sample();
    auto f = ResponseFrequency::CyclesPerSample(0.04);
    auto lp = snapshot.m_low.MagnitudeSquaredResponses(f.Broadcast<MultiChannelNumber<double, 3>>());
    auto hp = snapshot.m_high.MagnitudeSquaredResponses(f.Broadcast<MultiChannelNumber<double, 3>>());
    for (size_t lane = 0; lane < 3; ++lane)
    {
        DOCTEST_CHECK(ui.Response(ids, f, lane).m_median == lp[lane] * hp[lane]);
    }
}

DOCTEST_TEST_CASE("Generated UI: single-output scalar and vector adapters need only power")
{
    OnePoleZOHLowPass scalar{};
    scalar.ComputeCoefficients({0.1});
    OnePoleZOHLowPass::UIState scalarUI;
    scalarUI.m_history.Push(scalar.MakeSnapshot());
    auto ids = scalarUI.Sample();
    DOCTEST_CHECK(scalarUI.Response(ids, ResponseFrequency::CyclesPerSample(0), 0).m_median == 1);
    using Filter = OnePoleTPTLowPassGeneric<float, 3>;
    Filter filter{};
    filter.ComputeCoefficients({Filter::Vector::Splat(0.1f)});
    Filter::UIState ui;
    ui.m_history.Push(filter.MakeSnapshot());
    ids = ui.Sample();
    DOCTEST_CHECK(ui.Response(ids, ResponseFrequency::CyclesPerSample(0.1), 2).m_median
        == doctest::Approx(0.5).epsilon(0.00002));
}

DOCTEST_TEST_CASE("Sampled response: power to decibels preserves the amplitude display")
{
    for (float amplitude : {0.0f, 0.000001f, 0.001f, 0.5f, 1.0f, 2.0f, 10.0f})
    {
        DOCTEST_CHECK(SampledResponsePlot::PowerToDb(amplitude * amplitude)
            == doctest::Approx(20.0f * std::log10(std::max(1e-5f, amplitude))));
    }
}
