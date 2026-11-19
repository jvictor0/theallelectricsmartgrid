#include "doctest.h"
#include "../support/GlobalEnv.hpp"
#include "ConservativeReuppingLadder.hpp"
#include "ConservativeReuppingLadderShape.hpp"
#include "NaiveReuppingLadderShape.hpp"
#include <complex>

namespace
{
template<typename State>
std::complex<double> Reference(const State& state, size_t lane, double frequency)
{
    const double alpha = state.m_alpha[lane];
    const double slope = state.m_saturation.m_gain[lane] / state.m_saturation.m_tanhGain[lane];
    const double k = state.m_k[lane];
    const auto q = std::polar(1.0, -2 * M_PI * frequency);
    const auto pole = alpha / (1.0 - (1.0 - alpha) * q);
    return (1 + k) * std::pow(slope * pole, 4)
        / (1.0 + k * std::pow(slope, 3) * q * std::pow(pole, 4));
}

template<typename Number, size_t Size>
void CheckResponse()
{
    using Filter = ConservativeReuppingLadderGeneric<Number, Size>;
    using Vector = typename Filter::Vector;
    Filter filter;
    constexpr double x_cutoffs[] = {1e-6, 0.0001, 0.01, 0.125, 0.1959, 0.25, 0.49, 0.5};
    for (size_t step = 0; step < 24; ++step)
    {
        Vector cutoff;
        Vector resonance;
        Vector gain;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            cutoff[lane] = static_cast<Number>(x_cutoffs[(step + lane) % 8]);
            resonance[lane] = static_cast<Number>((step + lane) % 3) / 2;
            gain[lane] = static_cast<Number>(0.25 + 2.375 * ((step + lane) % 3));
        }

        filter.SetCutoffResonanceAndGain(cutoff, resonance, gain);
        const auto snapshot = filter.MakeSnapshot();
        for (double frequency : {0.0, 1e-6, 0.0001, 0.002, 0.01, 0.1, 0.1959, 0.25, 0.499, 0.5})
        {
            const auto prepared = ResponseFrequency::CyclesPerSample(frequency)
                .template Broadcast<MultiChannelNumber<double, Size>>();
            const auto actual = snapshot.MagnitudeSquaredResponses(prepared);
            for (size_t lane = 0; lane < Size; ++lane)
            {
                DOCTEST_CAPTURE(step);
                DOCTEST_CAPTURE(lane);
                DOCTEST_CAPTURE(frequency);
                const double expected = std::norm(Reference(filter.m_state, lane, frequency));
                DOCTEST_CHECK(std::isfinite(actual[lane]));
                DOCTEST_CHECK(actual[lane] >= 0);
                DOCTEST_CHECK(actual[lane] == doctest::Approx(expected).epsilon(0.001));
            }
        }

        filter.Process(Vector::Splat(static_cast<Number>(0.01)));
        const auto history = filter.m_state;
        filter.SetCutoffResonanceAndGain(cutoff, resonance, gain * static_cast<Number>(1.1));
        DOCTEST_CHECK(filter.m_state.m_stage1 == history.m_stage1);
        DOCTEST_CHECK(filter.m_state.m_stage2 == history.m_stage2);
        DOCTEST_CHECK(filter.m_state.m_stage3 == history.m_stage3);
        DOCTEST_CHECK(filter.m_state.m_stage4 == history.m_stage4);
        const auto retuned = filter.MakeSnapshot();
        DOCTEST_CHECK(retuned.m_slope != snapshot.m_slope);
        filter.Reset();
        DOCTEST_CHECK(filter.m_state.m_stage4 == Vector());
        DOCTEST_CHECK(filter.m_output.m_x == Vector());
        DOCTEST_CHECK(filter.MakeSnapshot().m_k == retuned.m_k);
        DOCTEST_CHECK(filter.MakeSnapshot().m_slope == retuned.m_slope);
    }
}
}

DOCTEST_TEST_CASE("Conservative ladder: generated UI matches the delayed transfer across tuning and lanes")
{
    GlobalEnv::ResetPerTest();
    CheckResponse<float, 1>();
    CheckResponse<float, 2>();
    CheckResponse<float, 3>();
    CheckResponse<float, 4>();
    CheckResponse<double, 1>();
    CheckResponse<double, 2>();
    CheckResponse<double, 3>();
    CheckResponse<double, 4>();
}

DOCTEST_TEST_CASE("Conservative ladder: tiny sinusoid settles to the plotted small-signal response")
{
    GlobalEnv::ResetPerTest();
    using Filter = ConservativeReuppingLadderGeneric<double, 1>;
    using Vector = Filter::Vector;
    constexpr size_t x_window = 32768;
    constexpr double x_amplitude = 1e-7;
    for (double gain : {0.25, 2.0, 5.0})
    {
        for (double resonance : {0.0, 0.7, 1.0})
        {
            for (size_t bin : {size_t(37), size_t(329), size_t(1001)})
            {
                Filter filter;
                filter.SetCutoffResonanceAndGain(Vector::Splat(0.04), Vector::Splat(resonance), Vector::Splat(gain));
                const double frequency = static_cast<double>(bin) / x_window;
                std::complex<double> sum{};
                double power = 0;
                for (size_t n = 0; n < 2 * x_window; ++n)
                {
                    const auto carrier = std::polar(1.0, 2 * M_PI * frequency * n);
                    const double y = filter.Process(Vector::Splat(x_amplitude * carrier.real()))[0];
                    if (n >= x_window)
                    {
                        sum += y * std::conj(carrier);
                        power += y * y;
                    }
                }

                const auto measured = sum * (2.0 / (x_window * x_amplitude));
                const auto expected = Reference(filter.m_state, 0, frequency);
                DOCTEST_CAPTURE(gain);
                DOCTEST_CAPTURE(resonance);
                DOCTEST_CAPTURE(frequency);
                DOCTEST_CHECK(std::abs(measured - expected) < 1e-7 * (1 + std::abs(expected)));
                const auto q = std::polar(1.0, -2 * M_PI * frequency);
                const Filter::Frequency exactFrequency{Vector::Splat(frequency),
                    Vector::Splat(std::sin(M_PI * frequency)), Vector::Splat(std::cos(M_PI * frequency)),
                    {Vector::Splat(q.real()), Vector::Splat(q.imag())}};
                const double exactPower = Filter::MagnitudeSquared(filter.MakeSnapshot(), exactFrequency)[0];
                const double measuredPower = 2 * power / (x_window * x_amplitude * x_amplitude);
                DOCTEST_CHECK(measuredPower == doctest::Approx(exactPower).epsilon(2e-6));
                const double plotted = filter.MakeSnapshot().MagnitudeSquaredResponse(
                    ResponseFrequency::CyclesPerSample(frequency), 0);
                // Prepared display frequencies use Math4096 interpolation.
                // Allow 0.001 dB of display error independently of the exact kernel.
                //
                DOCTEST_CHECK(std::abs(10 * std::log10(plotted / measuredPower)) < 0.001);
            }
        }
    }
}

DOCTEST_TEST_CASE("Conservative ladder shape: shared controls and sampled LP HP composition")
{
    GlobalEnv::ResetPerTest();
    using Shape = ConservativeReuppingLadderShapeGeneric<3>;
    using Vector = Shape::Vector;
    Shape shape;
    Shape::Input input;
    NaiveReuppingLadderShapeGeneric<3>::Input naiveInput;
    for (float knob : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
    {
        input.m_lpCutoffFactor.Update(Vector::Splat(knob));
        naiveInput.m_lpCutoffFactor.Update(Vector::Splat(knob));
        DOCTEST_CHECK(input.m_lpCutoffFactor.m_expParam == naiveInput.m_lpCutoffFactor.m_expParam);
    }

    shape.Process(input, Vector::Splat(0.01f));
    const auto snapshot = shape.MakeSnapshot();
    Shape::UIState ui;
    SampledSnapshotWriter<Shape::UIState> writer;
    writer.Bind(&ui);
    shape.PopulateUIStateSample(writer);
    const auto ids = ui.Sample();
    DOCTEST_REQUIRE(ids.size() == 1);
    DOCTEST_CHECK(ui.NumLanes() == 3);
    const auto frequency = ResponseFrequency::CyclesPerSample(0.01);
    const auto prepared = frequency.Broadcast<MultiChannelNumber<double, 3>>();
    const auto expected = snapshot.m_lp.MagnitudeSquaredResponses(prepared)
        * snapshot.m_hp.MagnitudeSquaredResponses(prepared);
    for (size_t lane = 0; lane < 3; ++lane)
    {
        DOCTEST_CHECK(ui.MagnitudeSquaredResponse(ids.front(), frequency, lane) == expected[lane]);
    }
}
