#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "BitCrush.hpp"
#include "NaiveReuppingLadderShape.hpp"
#include "SVF2PoleShape.hpp"
#include "SquiggleBoyShape.hpp"
#include <cmath>
#include <complex>
#include <fstream>

// The sample rate reducer aliases a high tone down to bass. Both filter
// machines run the reducer before the high-pass, so an engaged high-pass
// removes that bass.
//
namespace
{

constexpr size_t x_lanes = 3;
constexpr double x_toneHz = 5000.0;
constexpr double x_baseHz = 100.0;

struct AliasMeter
{
    std::complex<double> m_sum[x_lanes];
    size_t m_count;

    AliasMeter()
        : m_sum{}
        , m_count(0)
    {
    }

    void Add(size_t lane, double cyclesPerSample, size_t sample, double value)
    {
        double angle = -2.0 * M_PI * cyclesPerSample * static_cast<double>(sample);
        m_sum[lane] += value * std::complex<double>(std::cos(angle), std::sin(angle));
    }

    double Magnitude(size_t lane) const
    {
        return 2.0 * std::abs(m_sum[lane]) / static_cast<double>(m_count);
    }
};

template <typename Shape>
void SetCommonInputs(typename Shape::Input& input)
{
    using Vector = typename Shape::Vector;
    input.m_baseFreq = Vector::Splat(static_cast<float>(SampleTimer::HzToCycPerSamp(x_baseHz)));
    input.m_hpCutoffFactor.m_expParam = Vector::Splat(8.0f);
    input.m_lpCutoffFactor.m_expParam = Vector::Splat(1024.0f);
    input.m_lpResonance.m_expParam = Vector();
    input.m_hpResonance.m_expParam = Vector();
    input.m_saturationGain.m_expParam = Vector::Splat(0.25f);
}

// Reducer rates just below or above the tone alias it to |tone - rate|.
//
template <typename Shape>
void CheckReducerBassRemoved(const double (&reducerHz)[x_lanes][2])
{
    using Vector = typename Shape::Vector;
    GlobalEnv::Init();
    Shape shape;
    typename Shape::Input input;
    SetCommonInputs<Shape>(input);
    size_t settle = static_cast<size_t>(SampleTimer::SecondsToSamples(0.25));
    size_t measure = static_cast<size_t>(SampleTimer::SecondsToSamples(1.0));
    size_t sample = 0;
    for (size_t phase = 0; phase < 2; ++phase)
    {
        for (size_t lane = 0; lane < x_lanes; ++lane)
        {
            input.m_sampleRateReducerFreq.m_expParam[lane] = static_cast<float>(reducerHz[lane][phase] / x_baseHz);
        }

        AliasMeter reduced;
        AliasMeter output;
        for (size_t i = 0; i < settle + measure; ++i, ++sample)
        {
            double tone = 0.5 * std::sin(2.0 * M_PI * SampleTimer::HzToCycPerSamp(x_toneHz) * static_cast<double>(sample));
            Vector out = shape.Process(input, Vector::Splat(static_cast<float>(tone)));
            if (settle <= i)
            {
                for (size_t lane = 0; lane < x_lanes; ++lane)
                {
                    double alias = SampleTimer::HzToCycPerSamp(std::abs(x_toneHz - reducerHz[lane][phase]));
                    reduced.Add(lane, alias, sample, shape.m_sampleRateReducer.m_output[lane]);
                    output.Add(lane, alias, sample, out[lane]);
                }

                ++reduced.m_count;
                ++output.m_count;
            }
        }

        for (size_t lane = 0; lane < x_lanes; ++lane)
        {
            DOCTEST_CAPTURE(phase);
            DOCTEST_CAPTURE(lane);
            DOCTEST_CHECK(reduced.Magnitude(lane) > 0.05);
            DOCTEST_CHECK(output.Magnitude(lane) < 0.05 * reduced.Magnitude(lane));
        }
    }
}

constexpr double x_reducerHz[x_lanes][2] = {
    {4950.0, 4900.0},
    {4900.0, 5030.0},
    {5030.0, 4950.0},
};

}

DOCTEST_TEST_CASE("SampleRateReducer: each lane resumes its held sample and phase after bypass")
{
    using Vector = TrioSampleRateReducer::Vector;
    TrioSampleRateReducer reducer;
    reducer.SetFreq(Vector(0.5f, 0.75f, 1.25f));
    DOCTEST_CHECK(reducer.Process(Vector(10, 20, 30)) == Vector(0, 0, 30));
    DOCTEST_CHECK(reducer.Process(Vector(11, 21, 31)) == Vector(11, 21, 31));

    reducer.SetFreq(Vector(0.25f, 1.0f, 0.5f));
    DOCTEST_CHECK(reducer.Process(Vector(12, 22, 32)) == Vector(11, 22, 0));

    reducer.SetFreq(Vector(0.0f, 0.25f, 0.5f));
    DOCTEST_CHECK(reducer.Process(Vector(13, 23, 33)) == Vector(11, 21, 33));

    reducer.SetFreq(Vector(0.75f, 0.25f, 0.0f));
    DOCTEST_CHECK(reducer.Process(Vector(14, 24, 34)) == Vector(14, 24, 33));
}

DOCTEST_TEST_CASE("NaiveReuppingLadderShape: high-pass removes bass created by the sample rate reducer")
{
    CheckReducerBassRemoved<NaiveReuppingLadderShapeGeneric<x_lanes>>(x_reducerHz);
}

DOCTEST_TEST_CASE("SVF2PoleShape: high-pass removes bass created by the sample rate reducer")
{
    CheckReducerBassRemoved<SVF2PoleShapeGeneric<x_lanes>>(x_reducerHz);
}

DOCTEST_TEST_CASE("NaiveReuppingLadderShape: stage-corner knob preserves its octave span and opens to engine Nyquist")
{
    using Shape = NaiveReuppingLadderShapeGeneric<3>;
    using Vector = Shape::Vector;
    GlobalEnv::Init();
    Shape shape;
    Shape::Input input;
    PhaseUtils::ExpParam legacyCutoff(0.25f, 1024.0f);
    input.m_baseFreq = Vector::Splat(Shape::x_lpTrackingPivot);
    input.m_hpCutoffFactor.m_expParam = Vector::Splat(1);
    for (float knob : {0.0f, 0.1f, 0.2f, 0.3f})
    {
        const double oldHalfPower = Shape::x_lpTrackingPivot * legacyCutoff.Update(knob)[0];
        input.m_lpCutoffFactor.Update(Vector::Splat(knob));
        shape.Process(input, Vector());
        const double alpha = shape.m_lpLadder.m_state.m_alpha[0];
        const double sineHalfPower = std::sqrt((std::pow(2.0, 0.25) - 1) * alpha * alpha / (4 * (1 - alpha)));
        const double actualHalfPower = std::asin(sineHalfPower) / M_PI;
        DOCTEST_CHECK(actualHalfPower == doctest::Approx(oldHalfPower).epsilon(0.001));
    }

    input.m_lpCutoffFactor.Update(Vector::Splat(1));
    shape.Process(input, Vector());
    DOCTEST_CHECK(shape.m_lpLadder.MakeSnapshot().m_controls.m_cutoff == Vector::Splat(0.5f));
    DOCTEST_CHECK(input.m_lpCutoffFactor.m_expParam[0] == doctest::Approx(2354.134).epsilon(0.00001));
}

DOCTEST_TEST_CASE("NaiveReuppingLadderShape: cutoff retains partial pitch tracking and the shared high-pass factor")
{
    using Shape = NaiveReuppingLadderShapeGeneric<3>;
    using Vector = Shape::Vector;
    GlobalEnv::Init();
    Shape shape;
    Shape::Input input;
    input.m_baseFreq = Vector(Shape::x_lpTrackingPivot * 0.5f, Shape::x_lpTrackingPivot, Shape::x_lpTrackingPivot * 2);
    input.m_hpCutoffFactor.m_expParam = Vector::Splat(2);
    input.m_lpCutoffFactor.m_expParam = Vector::Splat(100);
    shape.Process(input, Vector());
    const double pivotCutoff = 200 * Shape::x_lpTrackingPivot;
    for (size_t lane = 0; lane < 3; ++lane)
    {
        const double expected = pivotCutoff * std::pow(2.0, (static_cast<double>(lane) - 1) * 0.707);
        DOCTEST_CHECK(shape.m_lpLadder.MakeSnapshot().m_controls.m_cutoff[lane] == doctest::Approx(expected).epsilon(0.00001));
        DOCTEST_CHECK(shape.m_lpLadder.MakeSnapshot().m_controls.m_cutoff[lane] > Shape::x_maxCutoff);
    }

    input.m_hpCutoffFactor.m_expParam = Vector::Splat(1);
    shape.Process(input, Vector());
    DOCTEST_CHECK(shape.m_lpLadder.MakeSnapshot().m_controls.m_cutoff[1] == doctest::Approx(pivotCutoff / 2).epsilon(0.00001));
}

DOCTEST_TEST_CASE("Generated naive ladder plots the audited table using applied per-lane controls")
{
    std::ifstream file(SMARTGRID_REPO_ROOT "/JUCE/SmartGridOne/Assets/NaiveReuppingLadderResponse.bin",
        std::ios::binary);
    const std::vector<unsigned char> bytes(std::istreambuf_iterator<char>(file), {});
    DOCTEST_REQUIRE(bytes.size() == NaiveLadderResponse::x_bytes);
    struct ScopedTable
    {
        ~ScopedTable()
        {
            NaiveLadderResponse::s_table.reset();
        }
    } installed;
    const auto& table = NaiveLadderResponse::s_table.emplace(bytes.data(), bytes.size());

    TrioGeneratedNaiveReuppingLadder filter;
    using Vector = TrioGeneratedNaiveReuppingLadder::Vector;
    filter.SetCutoffResonanceAndGain(Vector(20.f / 192000, 0.025f, 0.5f),
        Vector(0, 0.125f, 1), Vector(0.25f, std::sqrt(1.25f), 5.f));
    TrioSquiggleBoyShape::Snapshot snapshot;
    snapshot.m_naiveReuppingLadder.m_lp = filter.MakeSnapshot();
    using Snapshot = decltype(snapshot);
    Snapshot::PreparedResponse prepared;
    snapshot.PrepareResponse(prepared);
    const double configs[3][3] = {{0, 0, 20}, {0.5, 0.5, 4800}, {1, 1, 96000}};
    for (double hz : {23.4375, 300.0, 23500.0, 48000.0, 96000.0})
    {
        const auto frequency = ResponseFrequency::Hz(hz, 192000).Broadcast<Snapshot::DoubleVector>();
        const auto power = snapshot.PreSrrPower(frequency, prepared);
        for (size_t lane = 0; lane < 3; ++lane)
        {
            DOCTEST_CHECK(10 * std::log10(power[lane])
                == doctest::Approx(table.DisplayDb(configs[lane], hz)).epsilon(0.00002));
        }
    }
}
