#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "Ladder4PoleShape.hpp"
#include "SVF2PoleShape.hpp"
#include <cmath>
#include <complex>

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

DOCTEST_TEST_CASE("Ladder4PoleShape: high-pass removes bass created by the sample rate reducer")
{
    CheckReducerBassRemoved<Ladder4PoleShapeGeneric<x_lanes>>(x_reducerHz);
}

DOCTEST_TEST_CASE("SVF2PoleShape: high-pass removes bass created by the sample rate reducer")
{
    CheckReducerBassRemoved<SVF2PoleShapeGeneric<x_lanes>>(x_reducerHz);
}
