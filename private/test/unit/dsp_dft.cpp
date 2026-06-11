// dsp_dft.cpp -- self-test for the WP-2 spectral helpers AND the repo's own
// DiscreteFourierTransformGeneric FFT.
//
// Goals:
//   * Pin down and document the repo FFT normalization empirically.
//   * Exercise MagnitudeSpectrum / DominantBin / MeasureTransferFunction so the
//     frozen helper APIs are validated before other WPs build on them.
//
// Uses DOCTEST_ prefixed macros (DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES is set by
// the test target).

#include "doctest.h"

#include <cmath>
#include <vector>
#include <functional>

#include "../support/GlobalEnv.hpp"
#include "../support/Signal.hpp"
#include "../support/Spectral.hpp"

#include "BasicWaveTable.hpp"
#include "AdaptiveWaveTable.hpp"

using namespace TestSpectral;
using namespace TestSignal;

namespace
{

constexpr std::size_t kN = 1024;     // == DiscreteFourierTransform table size
constexpr double kSampleRate = 48000.0;

} // namespace

// ---------------------------------------------------------------------------
// Normalization: a pure cosine at an exact bin, run through the RAW repo FFT
// (no window), must give |m_components[k0]| == A/2, and DC gives |X[0]| == A.
// This documents the 1/N forward-FFT normalization.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("repo FFT normalization: cosine at exact bin => A/2, DC => A")
{
    GlobalEnv::ResetPerTest();

    const std::size_t k0 = 40;
    const float A = 0.7f;

    BasicWaveTableGeneric<10> table;
    for (std::size_t i = 0; i < kN; ++i)
    {
        table.m_table[i] = A * std::cos(2.0 * M_PI * static_cast<double>(k0) *
                                        static_cast<double>(i) / static_cast<double>(kN));
    }

    DiscreteFourierTransform dft;
    dft.Transform(table);

    // Bin k0 magnitude is A/2.
    //
    DOCTEST_CHECK(std::abs(dft.m_components[k0]) == doctest::Approx(A / 2.0f).epsilon(0.01));
    // Neighboring bins essentially zero.
    //
    DOCTEST_CHECK(std::abs(dft.m_components[k0 - 1]) < 1e-3f);
    DOCTEST_CHECK(std::abs(dft.m_components[k0 + 1]) < 1e-3f);

    // DC: constant level A => |X[0]| == A.
    //
    BasicWaveTableGeneric<10> dcTable;
    for (std::size_t i = 0; i < kN; ++i)
    {
        dcTable.m_table[i] = A;
    }
    DiscreteFourierTransform dcDft;
    dcDft.Transform(dcTable);
    DOCTEST_CHECK(std::abs(dcDft.m_components[0]) == doctest::Approx(A).epsilon(0.01));
}

// ---------------------------------------------------------------------------
// Windowed MagnitudeSpectrum: pure sine at an exact bin -> DominantBin is that
// bin. (The Hann window scales the magnitude but does not move the peak.)
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("MagnitudeSpectrum: sine at exact bin -> dominant bin is correct")
{
    GlobalEnv::ResetPerTest();

    const std::size_t k0 = 64;
    const double freq = BinFreq(k0, kN, kSampleRate);

    std::vector<float> buf(kN);
    Sine sine(freq, kSampleRate, 1.0f);
    Fill(sine, buf.data(), kN);

    std::vector<float> spec = MagnitudeSpectrum(buf.data(), kN, kFftSize1024);
    DOCTEST_CHECK(spec.size() == kN / 2);
    DOCTEST_CHECK(DominantBin(spec) == k0);
}

// ---------------------------------------------------------------------------
// Sine BETWEEN bins -> energy concentrated in the immediate neighborhood of
// the two straddling bins (Hann main lobe).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("MagnitudeSpectrum: between-bin sine concentrates energy locally")
{
    GlobalEnv::ResetPerTest();

    const double fracBin = 64.5;
    const double freq = fracBin * kSampleRate / static_cast<double>(kN);

    std::vector<float> buf(kN);
    Sine sine(freq, kSampleRate, 1.0f);
    Fill(sine, buf.data(), kN);

    std::vector<float> spec = MagnitudeSpectrum(buf.data(), kN, kFftSize1024);

    double total = 0.0;
    double local = 0.0;
    for (std::size_t k = 0; k < spec.size(); ++k)
    {
        double e = static_cast<double>(spec[k]) * spec[k];
        total += e;
        if (k >= 63 && k <= 66)
        {
            local += e;
        }
    }
    DOCTEST_CHECK(total > 0.0);
    DOCTEST_CHECK(local / total > 0.9);  // >90% of energy within the main lobe
    DOCTEST_CHECK(DominantBin(spec) >= 64);
    DOCTEST_CHECK(DominantBin(spec) <= 65);
}

// ---------------------------------------------------------------------------
// Impulse -> flat magnitude spectrum. We test on the RAW repo FFT (no window):
// a unit impulse at index 0 has X[k] = 1/N for every k. (The Hann-windowed
// helper would zero the impulse since the Hann window is 0 at i=0, so the flat
// property is most cleanly shown unwindowed -- documented choice.)
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("repo FFT: impulse -> flat spectrum at 1/N")
{
    GlobalEnv::ResetPerTest();

    BasicWaveTableGeneric<10> table;  // zeroed
    table.m_table[0] = 1.0f;

    DiscreteFourierTransform dft;
    dft.Transform(table);

    const float expected = 1.0f / static_cast<float>(kN);
    for (std::size_t k = 0; k < kN / 2; ++k)
    {
        DOCTEST_CHECK(std::abs(dft.m_components[k]) == doctest::Approx(expected).epsilon(0.01));
    }
}

// ---------------------------------------------------------------------------
// Parseval energy check on the RAW repo FFT.
//
// With 1/N-normalized forward FFT, Parseval is:
//   (1/N) sum_i x[i]^2  ==  sum_{k=0}^{N-1} |X[k]|^2
// m_components only stores k in [0, N/2). For a real signal with no DC and no
// Nyquist content, the full sum is 2 * sum_{k=1}^{N/2-1} |X[k]|^2. We use a
// sum of two exact-bin cosines (no DC, no Nyquist) so the identity is exact.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("repo FFT: Parseval energy identity (unwindowed)")
{
    GlobalEnv::ResetPerTest();

    BasicWaveTableGeneric<10> table;
    for (std::size_t i = 0; i < kN; ++i)
    {
        double t = static_cast<double>(i) / static_cast<double>(kN);
        table.m_table[i] = static_cast<float>(
            0.6 * std::cos(2.0 * M_PI * 40.0 * t) +
            0.3 * std::sin(2.0 * M_PI * 91.0 * t));
    }

    double timeEnergy = 0.0;
    for (std::size_t i = 0; i < kN; ++i)
    {
        timeEnergy += static_cast<double>(table.m_table[i]) * table.m_table[i];
    }
    timeEnergy /= static_cast<double>(kN);  // (1/N) sum x^2

    DiscreteFourierTransform dft;
    dft.Transform(table);

    // Full-spectrum energy: DC + 2 * lower-half (k=1..N/2-1). No Nyquist term.
    //
    double specEnergy = static_cast<double>(std::abs(dft.m_components[0])) *
                        std::abs(dft.m_components[0]);
    for (std::size_t k = 1; k < kN / 2; ++k)
    {
        double m = static_cast<double>(std::abs(dft.m_components[k]));
        specEnergy += 2.0 * m * m;
    }

    DOCTEST_CHECK(specEnergy == doctest::Approx(timeEnergy).epsilon(0.01));
}

// ---------------------------------------------------------------------------
// WhiteNoise determinism.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("WhiteNoise: same seed identical, different seeds differ")
{
    GlobalEnv::ResetPerTest();

    const std::size_t n = 256;
    std::vector<float> a(n), b(n), c(n);

    WhiteNoise wa(12345);
    WhiteNoise wb(12345);
    WhiteNoise wc(67890);
    Fill(wa, a.data(), n);
    Fill(wb, b.data(), n);
    Fill(wc, c.data(), n);

    bool identical = true;
    bool differs = false;
    for (std::size_t i = 0; i < n; ++i)
    {
        DOCTEST_CHECK(a[i] >= -1.0f);
        DOCTEST_CHECK(a[i] <= 1.0f);
        if (a[i] != b[i]) identical = false;
        if (a[i] != c[i]) differs = true;
    }
    DOCTEST_CHECK(identical);
    DOCTEST_CHECK(differs);
}

// ---------------------------------------------------------------------------
// MeasureTransferFunction self-check: identity system => |H| ~ 1 (0 dB) flat
// across mid bins. Proves the Welch averaging machinery.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("MeasureTransferFunction: identity is flat 0 dB")
{
    GlobalEnv::ResetPerTest();

    std::function<float(float)> identity = [](float x) { return x; };
    std::vector<float> H = MeasureTransferFunction(identity, kFftSize1024,
                                                   kSampleRate, 0xABCDEF);

    // Expect flat unity gain; check mid-band bins within 0.5 dB.
    //
    AssertResponseMatches(H, [](double) { return 1.0; },
                          kSampleRate, kFftSize1024, 0.5,
                          /*loBin*/ 16, /*hiBin*/ 480);
}

// ---------------------------------------------------------------------------
// MeasureTransferFunction self-check: a one-sample delay is all-pass, |H| == 1
// flat. Proves the magnitude estimate ignores phase as intended.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("MeasureTransferFunction: one-sample delay is flat 0 dB")
{
    GlobalEnv::ResetPerTest();

    float prev = 0.0f;
    std::function<float(float)> delay = [prev](float x) mutable {
        float out = prev;
        prev = x;
        return out;
    };
    std::vector<float> H = MeasureTransferFunction(delay, kFftSize1024,
                                                   kSampleRate, 0x13579);

    AssertResponseMatches(H, [](double) { return 1.0; },
                          kSampleRate, kFftSize1024, 0.5,
                          /*loBin*/ 16, /*hiBin*/ 480);
}
