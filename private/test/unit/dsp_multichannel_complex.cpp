#include "doctest.h"

#include <cmath>
#include <complex>
#include <random>
#include <type_traits>

#include "AdaptiveWaveTable.hpp"
#include "MultiChannelComplex.hpp"

namespace
{
    template<typename Number>
    void CheckComplexNear(std::complex<Number> actual, std::complex<Number> expected, double tolerance)
    {
        DOCTEST_CHECK(std::abs(actual - expected) <= tolerance * (1 + std::abs(expected)));
    }

    template<typename Number, size_t Size>
    void CheckArithmetic()
    {
        using Bundle = MultiChannelComplex<Number, Size>;
        using Vector = typename Bundle::Vector;
        using Complex = typename Bundle::Complex;
        constexpr double x_tolerance = std::is_same<Number, float>::value ? 2e-6 : 2e-14;

        Bundle zero;
        for (size_t channel = 0; channel < Size; ++channel)
        {
            DOCTEST_CHECK(zero[channel] == Complex(0, 0));
        }

        static_assert(!std::is_assignable<decltype(zero[0]), Complex>::value,
                      "lane reads must not silently accept assignments to temporaries");

        std::mt19937 random(1781);
        std::uniform_real_distribution<Number> distribution(-4, 4);
        for (size_t iteration = 0; iteration < 128; ++iteration)
        {
            Bundle a;
            Bundle b;
            Vector scale;
            for (size_t channel = 0; channel < Size; ++channel)
            {
                a.SetChannel(channel, Complex(distribution(random), distribution(random)));
                b.real()[channel] = distribution(random);
                b.imag()[channel] = distribution(random);
                scale[channel] = static_cast<Number>(channel + 1) * static_cast<Number>(-0.5);
            }

            Complex scalar(distribution(random), distribution(random));
            Bundle sum = a + b;
            Bundle difference = a - b;
            Bundle product = a * b;
            Bundle scaled = a * scale;
            Bundle divided = a / scale;
            Bundle conjugate = a.Conj();
            Bundle negative = -a;
            Bundle scalarProduct = a * scalar;
            Bundle scalarSum = a;
            scalarSum += scalar;
            Bundle selfProduct = a;
            selfProduct *= selfProduct;
            Bundle selfSum = a;
            selfSum += selfSum;
            Bundle selfDifference = a;
            selfDifference -= selfDifference;
            Bundle compoundScale = a;
            compoundScale *= scale;
            compoundScale /= scale;
            compoundScale *= static_cast<Number>(0.5);
            compoundScale /= static_cast<Number>(0.5);
            const Bundle& readOnly = a;

            for (size_t channel = 0; channel < Size; ++channel)
            {
                Complex x = a[channel];
                Complex y = b[channel];
                DOCTEST_CHECK(readOnly.real()[channel] == x.real());
                DOCTEST_CHECK(readOnly.imag()[channel] == x.imag());
                CheckComplexNear(sum[channel], x + y, x_tolerance);
                CheckComplexNear(difference[channel], x - y, x_tolerance);
                CheckComplexNear(product[channel], x * y, x_tolerance);
                CheckComplexNear(scaled[channel], x * scale[channel], x_tolerance);
                CheckComplexNear(divided[channel], x / scale[channel], x_tolerance);
                CheckComplexNear(conjugate[channel], std::conj(x), x_tolerance);
                CheckComplexNear(negative[channel], -x, x_tolerance);
                CheckComplexNear(scalarProduct[channel], x * scalar, x_tolerance);
                CheckComplexNear(scalarSum[channel], x + scalar, x_tolerance);
                CheckComplexNear(selfProduct[channel], x * x, x_tolerance);
                CheckComplexNear(selfSum[channel], x + x, x_tolerance);
                CheckComplexNear(compoundScale[channel], x, x_tolerance);
                DOCTEST_CHECK(selfDifference[channel] == Complex(0, 0));
            }
        }
    }

    template<typename Number, size_t Size>
    void CheckTransform()
    {
        using DFT = DiscreteFourierTransformGeneric<Number, Size, 6>;
        using MonoDFT = DiscreteFourierTransformGeneric<Number, 1, 6>;
        using Vector = typename DFT::Vector;
        constexpr size_t x_n = DFT::x_tableSize;
        constexpr double x_tolerance = 2e-6;
        const double tau = 2 * std::acos(-1.0);

        typename DFT::Table input;
        for (size_t sample = 0; sample < x_n; ++sample)
        {
            for (size_t channel = 0; channel < Size; ++channel)
            {
                double phase = tau * static_cast<double>(sample) / x_n;
                input.m_table[sample][channel] = static_cast<Number>(
                    0.1 * channel + 0.5 * std::cos(phase * (channel + 2))
                    + 0.25 * std::sin(phase * (channel + 3)));
            }
        }

        DFT dft;
        dft.Transform(input);
        for (size_t bin = 0; bin < DFT::x_maxComponents; ++bin)
        {
            for (size_t channel = 0; channel < Size; ++channel)
            {
                std::complex<double> expected(0, 0);
                for (size_t sample = 0; sample < x_n; ++sample)
                {
                    expected += static_cast<double>(input.m_table[sample][channel])
                        * std::polar(1.0, -tau * bin * sample / x_n);
                }

                expected /= x_n;
                CheckComplexNear(std::complex<double>(dft.m_components[bin][channel]), expected, x_tolerance);
            }
        }

        typename DFT::Table output;
        dft.InverseTransform(output, DFT::x_maxComponents - 1);
        for (size_t sample = 0; sample < x_n; ++sample)
        {
            for (size_t channel = 0; channel < Size; ++channel)
            {
                DOCTEST_CHECK(std::abs(output.m_table[sample][channel] - input.m_table[sample][channel]) < x_tolerance);
            }
        }

        Vector phase;
        Vector magnitude;
        for (size_t channel = 0; channel < Size; ++channel)
        {
            phase[channel] = static_cast<Number>(-0.31 + 0.17 * channel);
            magnitude[channel] = static_cast<Number>(0.2 * channel);
        }

        for (Number frequency : {static_cast<Number>(0.001), static_cast<Number>(0.213), static_cast<Number>(0.49)})
        {
            dft.Init();
            dft.WriteWindowedPartial(phase, magnitude, frequency);
            for (size_t channel = 0; channel < Size; ++channel)
            {
                MonoDFT mono;
                mono.WriteWindowedPartial(phase[channel], magnitude[channel], frequency);
                for (size_t bin = 0; bin < DFT::x_maxComponents; ++bin)
                {
                    CheckComplexNear(dft.m_components[bin][channel], mono.m_components[bin][0], x_tolerance);
                }
            }
        }
    }
}

DOCTEST_TEST_CASE("MultiChannelComplex: float arithmetic preserves independent channels")
{
    CheckArithmetic<float, 1>();
    CheckArithmetic<float, 2>();
    CheckArithmetic<float, 4>();
}

DOCTEST_TEST_CASE("MultiChannelComplex: double arithmetic preserves independent channels")
{
    CheckArithmetic<double, 1>();
    CheckArithmetic<double, 2>();
    CheckArithmetic<double, 4>();
}

DOCTEST_TEST_CASE("MultiChannelComplex: float FFT matches direct DFT and reconstructs every channel")
{
    CheckTransform<float, 1>();
    CheckTransform<float, 2>();
    CheckTransform<float, 4>();
}

DOCTEST_TEST_CASE("MultiChannelComplex: double FFT and windowed partials preserve every channel")
{
    CheckTransform<double, 1>();
    CheckTransform<double, 2>();
    CheckTransform<double, 4>();
}
