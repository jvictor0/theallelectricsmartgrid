#include "doctest.h"
#include "../support/GlobalEnv.hpp"
#include "AdaptiveWaveTable.hpp"
#include "OLA.hpp"

#include <cmath>
#include <complex>
#include <memory>

// InverseTransform evaluates a source spectrum at the output table's length:
// equal-size output, or interpolation through a larger IFFT. The reference is
// the band-limited Fourier sum of the stored components at the output time.
//
namespace
{

constexpr size_t x_sourceBits = 10;
constexpr size_t x_outputBits = 12;
constexpr size_t x_ratio = static_cast<size_t>(1) << (x_outputBits - x_sourceBits);

template<size_t Size>
using SourceDFT = DiscreteFourierTransformGeneric<float, Size, x_sourceBits>;

template<size_t Size>
using OutputDFT = DiscreteFourierTransformGeneric<float, Size, x_outputBits>;

template<size_t Size>
using OutputTable = BasicWaveTableGeneric<float, Size, x_outputBits>;

template<size_t Size>
using SourceTable = BasicWaveTableGeneric<float, Size, x_sourceBits>;

template<size_t Size>
double FourierSum(const SourceDFT<Size>& dft, size_t channel, size_t maxComponents, double sourcePosition)
{
    size_t componentsToUse = std::min(maxComponents + 1, SourceDFT<Size>::x_maxComponents);
    double result = dft.m_components[0][channel].real();
    for (size_t k = 1; k < componentsToUse; ++k)
    {
        std::complex<double> component(dft.m_components[k][channel].real(), dft.m_components[k][channel].imag());
        double angle = 2.0 * M_PI * static_cast<double>(k) * sourcePosition / static_cast<double>(SourceDFT<Size>::x_tableSize);
        result += 2.0 * (component * std::complex<double>(std::cos(angle), std::sin(angle))).real();
    }

    return result;
}

void SetComponent(SourceDFT<2>& dft, size_t bin, size_t channel, std::complex<float> value)
{
    dft.m_components[bin].SetChannel(channel, value);
}

std::unique_ptr<SourceDFT<2>> TwoChannelSpectrum()
{
    auto dft = std::make_unique<SourceDFT<2>>();
    SetComponent(*dft, 0, 0, std::complex<float>(0.1f, 0.0f));
    SetComponent(*dft, 3, 0, std::polar(0.2f, 0.7f));
    SetComponent(*dft, 17, 0, std::polar(0.05f, -1.3f));
    SetComponent(*dft, 200, 0, std::polar(0.03f, 2.1f));
    SetComponent(*dft, 0, 1, std::complex<float>(-0.04f, 0.0f));
    SetComponent(*dft, 5, 1, std::polar(0.15f, 0.2f));
    SetComponent(*dft, 99, 1, std::polar(0.07f, 1.1f));
    SetComponent(*dft, 511, 1, std::polar(0.01f, -0.4f));
    dft->WriteWindowedPartial(MultiChannelNumber<float, 2>(0.3f, 0.8f), MultiChannelNumber<float, 2>(0.06f, 0.09f), 41.37f / 1024.0f);
    return dft;
}

}

DOCTEST_TEST_CASE("InverseTransform: equal-size and 4x outputs match the band-limited Fourier sum")
{
    GlobalEnv::ResetPerTest();
    auto dft = TwoChannelSpectrum();
    for (size_t maxComponents : {SourceDFT<2>::x_maxComponents, static_cast<size_t>(100)})
    {
        DOCTEST_CAPTURE(maxComponents);
        auto same = std::make_unique<SourceTable<2>>();
        auto large = std::make_unique<OutputTable<2>>();
        dft->InverseTransform(*same, maxComponents);
        dft->InverseTransform(*large, maxComponents);
        for (size_t channel = 0; channel < 2; ++channel)
        {
            DOCTEST_CAPTURE(channel);
            for (size_t i = 0; i < SourceTable<2>::x_tableSize; ++i)
            {
                DOCTEST_CAPTURE(i);
                double expected = FourierSum(*dft, channel, maxComponents, static_cast<double>(i));
                DOCTEST_REQUIRE(same->m_table[i][channel] == doctest::Approx(expected).epsilon(1.0e-5).scale(1.0));
                DOCTEST_REQUIRE(large->m_table[i * x_ratio][channel] == doctest::Approx(same->m_table[i][channel]).epsilon(1.0e-5).scale(1.0));
            }

            for (size_t i = 0; i < OutputTable<2>::x_tableSize; ++i)
            {
                DOCTEST_CAPTURE(i);
                double position = static_cast<double>(i) / static_cast<double>(x_ratio);
                double expected = FourierSum(*dft, channel, maxComponents, position);
                DOCTEST_REQUIRE(large->m_table[i][channel] == doctest::Approx(expected).epsilon(1.0e-5).scale(1.0));
            }
        }
    }
}

DOCTEST_TEST_CASE("InverseTransform: 4x output keeps bin indices, amplitudes, conjugates and the source bandlimit")
{
    GlobalEnv::ResetPerTest();
    auto dft = TwoChannelSpectrum();
    auto large = std::make_unique<OutputTable<2>>();
    dft->InverseTransform(*large, SourceDFT<2>::x_maxComponents);
    auto analysis = std::make_unique<OutputDFT<2>>();
    analysis->Transform(*large);
    for (size_t channel = 0; channel < 2; ++channel)
    {
        DOCTEST_CAPTURE(channel);
        for (size_t k = 0; k < OutputDFT<2>::x_maxComponents; ++k)
        {
            DOCTEST_CAPTURE(k);
            std::complex<float> expected = k < SourceDFT<2>::x_maxComponents ? dft->m_components[k][channel] : std::complex<float>();
            std::complex<float> actual = analysis->m_components[k][channel];
            DOCTEST_REQUIRE(actual.real() == doctest::Approx(expected.real()).epsilon(1.0e-6).scale(1.0));
            DOCTEST_REQUIRE(actual.imag() == doctest::Approx(expected.imag()).epsilon(1.0e-6).scale(1.0));
        }
    }
}

DOCTEST_TEST_CASE("InverseTransform: quad lanes match mono transforms at equal size and 4x")
{
    GlobalEnv::ResetPerTest();
    auto quad = std::make_unique<SourceDFT<4>>();
    auto mono = std::make_unique<SourceDFT<1>[]>(4);
    for (size_t lane = 0; lane < 4; ++lane)
    {
        float phase = 0.1f + 0.2f * static_cast<float>(lane);
        float magnitude = 0.05f + 0.01f * static_cast<float>(lane);
        float frequency = (7.25f + 13.5f * static_cast<float>(lane)) / 1024.0f;
        mono[lane].WriteWindowedPartial(phase, MultiChannelNumber<float, 1>(magnitude), frequency);
        mono[lane].m_components[0] = SourceDFT<1>::ComplexVector(std::complex<float>(0.01f * static_cast<float>(lane), 0.0f));
        for (size_t k = 0; k < SourceDFT<4>::x_maxComponents; ++k)
        {
            quad->m_components[k].SetChannel(lane, mono[lane].m_components[k][0]);
        }
    }

    auto quadSame = std::make_unique<SourceTable<4>>();
    auto quadLarge = std::make_unique<OutputTable<4>>();
    quad->InverseTransform(*quadSame, SourceDFT<4>::x_maxComponents);
    quad->InverseTransform(*quadLarge, SourceDFT<4>::x_maxComponents);
    for (size_t lane = 0; lane < 4; ++lane)
    {
        DOCTEST_CAPTURE(lane);
        auto monoSame = std::make_unique<SourceTable<1>>();
        auto monoLarge = std::make_unique<OutputTable<1>>();
        mono[lane].InverseTransform(*monoSame, SourceDFT<1>::x_maxComponents);
        mono[lane].InverseTransform(*monoLarge, SourceDFT<1>::x_maxComponents);
        for (size_t i = 0; i < SourceTable<4>::x_tableSize; ++i)
        {
            DOCTEST_REQUIRE(quadSame->m_table[i][lane] == doctest::Approx(monoSame->m_table[i][0]).epsilon(1.0e-6).scale(1.0));
        }

        for (size_t i = 0; i < OutputTable<4>::x_tableSize; ++i)
        {
            DOCTEST_REQUIRE(quadLarge->m_table[i][lane] == doctest::Approx(monoLarge->m_table[i][0]).epsilon(1.0e-6).scale(1.0));
        }
    }
}

DOCTEST_TEST_CASE("Shared-phase panned windowed partial matches mono lanes")
{
    GlobalEnv::ResetPerTest();
    auto quad = std::make_unique<SourceDFT<4>>();
    auto mono = std::make_unique<SourceDFT<1>[]>(4);
    auto perChannelPhase = std::make_unique<SourceDFT<4>>();
    QuadFloat pan(0.9f, 0.4f, 0.0f, 0.25f);
    const float phases[] = {0.13f, 0.61f, 0.97f};
    const float magnitudes[] = {0.2f, 0.07f, 0.11f};
    const float frequencies[] = {3.5f / 1024.0f, 77.123f / 1024.0f, 509.4f / 1024.0f};
    for (size_t partial = 0; partial < 3; ++partial)
    {
        quad->WriteWindowedPartial(phases[partial], pan * magnitudes[partial], frequencies[partial]);
        perChannelPhase->WriteWindowedPartial(QuadFloat::Splat(phases[partial]), pan * magnitudes[partial], frequencies[partial]);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            mono[lane].WriteWindowedPartial(phases[partial], MultiChannelNumber<float, 1>(magnitudes[partial] * pan[lane]), frequencies[partial]);
        }
    }

    for (size_t k = 0; k < SourceDFT<4>::x_maxComponents; ++k)
    {
        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CAPTURE(k);
            DOCTEST_CAPTURE(lane);
            std::complex<float> expected = mono[lane].m_components[k][0];
            DOCTEST_REQUIRE(quad->m_components[k][lane].real() == doctest::Approx(expected.real()).epsilon(1.0e-7).scale(1.0));
            DOCTEST_REQUIRE(quad->m_components[k][lane].imag() == doctest::Approx(expected.imag()).epsilon(1.0e-7).scale(1.0));
            DOCTEST_REQUIRE(perChannelPhase->m_components[k][lane].real() == doctest::Approx(expected.real()).epsilon(1.0e-7).scale(1.0));
            DOCTEST_REQUIRE(perChannelPhase->m_components[k][lane].imag() == doctest::Approx(expected.imag()).epsilon(1.0e-7).scale(1.0));
        }
    }
}

DOCTEST_TEST_CASE("Panned bin-centered residual write matches mono lanes")
{
    auto quad = std::make_unique<SourceDFT<4>>();
    auto mono = std::make_unique<SourceDFT<1>[]>(4);
    QuadFloat pan(0.3f, 1.0f, 0.55f, 0.0f);
    for (size_t bin : {static_cast<size_t>(1), static_cast<size_t>(40), SourceDFT<4>::x_maxComponents - 1})
    {
        std::complex<float> value = std::polar(0.5f, 0.1f * static_cast<float>(bin));
        quad->WriteBinCenteredWindowedPartial(bin, SourceDFT<4>::ComplexVector(value) * pan);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            mono[lane].WriteBinCenteredWindowedPartial(bin, SourceDFT<1>::ComplexVector(value * pan[lane]));
        }
    }

    for (size_t k = 0; k < SourceDFT<4>::x_maxComponents; ++k)
    {
        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CAPTURE(k);
            DOCTEST_CAPTURE(lane);
            DOCTEST_REQUIRE(quad->m_components[k][lane] == mono[lane].m_components[k][0]);
        }
    }
}

DOCTEST_TEST_CASE("OLAGeneric: overlapping frames accumulate across the ring and are cleared once read")
{
    using OLA = OLAGeneric<float, 2, 4>;
    constexpr size_t x_size = OLA::x_frameSize;
    constexpr size_t x_hop = 5;
    constexpr size_t x_frames = 6;
    constexpr size_t x_samples = x_hop * x_frames + x_size;
    OLA ola;
    double expected[x_samples][2] = {};
    for (size_t sample = 0; sample < x_samples; ++sample)
    {
        if (sample % x_hop == 0 && sample / x_hop < x_frames)
        {
            OLA::Frame frame;
            for (size_t i = 0; i < x_size; ++i)
            {
                float value = static_cast<float>(sample + 1) * 0.01f + static_cast<float>(i) * 0.001f;
                frame.m_table[i] = OLA::Vector(value, -2.0f * value);
                expected[sample + i][0] += value;
                expected[sample + i][1] += -2.0f * value;
            }

            ola.Write(frame);
        }

        DOCTEST_CAPTURE(sample);
        OLA::Vector output = ola.Process();
        DOCTEST_CHECK(output[0] == doctest::Approx(expected[sample][0]));
        DOCTEST_CHECK(output[1] == doctest::Approx(expected[sample][1]));
    }

    for (size_t sample = 0; sample < x_size; ++sample)
    {
        OLA::Vector output = ola.Process();
        DOCTEST_CHECK(output[0] == 0.0f);
        DOCTEST_CHECK(output[1] == 0.0f);
    }
}
