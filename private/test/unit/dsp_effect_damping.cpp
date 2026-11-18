#include "doctest.h"
#include "QuadDelay.hpp"
#include "QuadReverb.hpp"
#include "../support/GlobalEnv.hpp"
#include <complex>
#include <type_traits>

namespace
{
using Generated = TPTBaseWidthGeneric<float, 4>;
static_assert(std::is_same_v<decltype(QuadDelay::UIState::m_dampingFilter), Generated::UIState>);
static_assert(std::is_same_v<decltype(QuadReverb::UIState::m_dampingFilter), Generated::UIState>);

double ExpectedPower(double highCutoff, double lowCutoff, double frequency)
{
    const auto z = std::polar(1.0, -2 * std::acos(-1.0) * frequency);
    const auto a = 1.0 - z;
    const auto b = 1.0 + z;
    const double highG = std::tan(std::acos(-1.0) * highCutoff);
    const double lowG = std::tan(std::acos(-1.0) * lowCutoff);
    return std::norm(a / (a + highG * b) * (lowG * b) / (a + lowG * b));
}

void Configure(QuadDelay::PostFeedbackFilter& filter)
{
    filter.SetBFFBaseWidth(QuadFloat(0.001f, 0.01f, 0.04f, 0.125f), QuadFloat(10, 4, 8, 2048));
}

void Configure(QuadReverb::PostFeedbackFilter& filter)
{
    filter.SetBFFBaseWidth(QuadFloat(0.001f, 0.01f, 0.04f, 0.125f), QuadFloat(10, 4, 8, 2048));
    filter.SetAPFGain(0.6f);
    for (size_t lane = 0; lane < 4; ++lane)
    {
        filter.m_apf1.SetDelaySamples(static_cast<int>(lane), static_cast<int>(3 + lane));
        filter.m_apf2.SetDelaySamples(static_cast<int>(lane), static_cast<int>(5 + lane * 2));
    }
}

template<typename PostFilter>
void CheckAudioAndPlot()
{
    GlobalEnv::ResetPerTest();
    const double high[4] = {0.001, 0.01, 0.04, 0.125};
    const double low[4] = {0.01, 0.04, 0.32, 0.499};
    for (double frequency : {0.0, 0.003, 0.02, 0.1, 0.4, 0.49, 0.5})
    {
        PostFilter real;
        PostFilter imaginary;
        Configure(real);
        Configure(imaginary);
        Generated::UIState ui;
        SampledSnapshotWriter<Generated::UIState> writer;
        writer.Bind(&ui);
        real.m_bff.PopulateUIStateSample(writer);
        const auto samples = ui.Sample();
        DOCTEST_REQUIRE(samples.size() == 1);
        DOCTEST_CHECK(ui.NumLanes() == 4);
        std::complex<double> measured[4]{};
        constexpr size_t x_warmup = 8192;
        constexpr size_t x_measure = 4096;
        for (size_t sample = 0; sample < x_warmup + x_measure; ++sample)
        {
            const auto input = std::polar(1.0, 2 * std::acos(-1.0) * frequency * static_cast<double>(sample));
            const auto re = real.Process(QuadFloat::Splat(static_cast<float>(input.real())));
            const auto im = imaginary.Process(QuadFloat::Splat(static_cast<float>(input.imag())));
            if (sample >= x_warmup)
            {
                for (size_t lane = 0; lane < 4; ++lane)
                {
                    measured[lane] += std::complex<double>(re[lane], im[lane]) / input / static_cast<double>(x_measure);
                }
            }
        }

        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CAPTURE(frequency);
            DOCTEST_CAPTURE(lane);
            const auto expected = ExpectedPower(high[lane], low[lane], frequency);
            const auto plotted = ui.MagnitudeSquaredResponse(samples.front(), ResponseFrequency::CyclesPerSample(frequency), lane);
            DOCTEST_CHECK(std::isfinite(plotted));
            DOCTEST_CHECK(plotted == doctest::Approx(expected).epsilon(0.00003));
            DOCTEST_CHECK(std::norm(measured[lane]) == doctest::Approx(expected).epsilon(0.00003));
            DOCTEST_CHECK(std::norm(measured[lane]) == doctest::Approx(plotted).epsilon(0.00003));
        }
    }
}
}

DOCTEST_TEST_CASE("Effect damping: delay TPT audio matches its sampled power response")
{
    CheckAudioAndPlot<QuadDelay::PostFeedbackFilter>();
}

DOCTEST_TEST_CASE("Effect damping: reverb all-pass chain preserves the plotted TPT power")
{
    CheckAudioAndPlot<QuadReverb::PostFeedbackFilter>();
}

DOCTEST_TEST_CASE("Effect damping: clamp before tuning and preserve histories on modulation")
{
    QuadTPTBaseWidthFilter filter;
    DOCTEST_CHECK(filter.Process(QuadFloat()) == QuadFloat());
    filter.SetBaseWidth(QuadFloat(0.01f, 0.02f, 0.6f, 0.8f), QuadFloat(1, 2048, 1, 2));
    auto snapshot = filter.MakeSnapshot();
    const float high[4] = {0.01f, 0.02f, 0.499f, 0.499f};
    const float low[4] = {0.01f, 0.499f, 0.499f, 0.499f};
    for (size_t lane = 0; lane < 4; ++lane)
    {
        DOCTEST_CHECK(snapshot.m_highPass.m_g[lane]
            == doctest::Approx(std::tan(std::acos(-1.0) * high[lane])).epsilon(0.00003));
        DOCTEST_CHECK(snapshot.m_lowPass.m_g[lane]
            == doctest::Approx(std::tan(std::acos(-1.0) * low[lane])).epsilon(0.00003));
        DOCTEST_CHECK(filter.m_state.m_highPass.m_lowPass.m_G[lane] > 0);
        DOCTEST_CHECK(filter.m_state.m_highPass.m_lowPass.m_G[lane] < 1);
        DOCTEST_CHECK(filter.m_state.m_lowPass.m_G[lane] > 0);
        DOCTEST_CHECK(filter.m_state.m_lowPass.m_G[lane] < 1);
    }

    filter.Process(QuadFloat(1, -1, 0.5f, -0.5f));
    const auto previous = filter.m_state;
    filter.SetBaseWidth(QuadFloat::Splat(0.03f), QuadFloat::Splat(4));
    DOCTEST_CHECK(filter.m_state.m_highPass.m_lowPass.m_s == previous.m_highPass.m_lowPass.m_s);
    DOCTEST_CHECK(filter.m_state.m_lowPass.m_s == previous.m_lowPass.m_s);
    DOCTEST_CHECK(filter.MakeSnapshot().m_highPass.m_g != snapshot.m_highPass.m_g);
}
