// dsp_helpers.cpp -- self-test for the WP-2 Continuity / NanScan helpers and
// the 4096-point spectral path. Complements dsp_dft.cpp (which focuses on the
// FFT itself and the transfer-function machinery).

#include "doctest.h"

#include <cmath>
#include <vector>
#include <limits>

#include "../support/GlobalEnv.hpp"
#include "../support/Signal.hpp"
#include "../support/Spectral.hpp"
#include "../support/Continuity.hpp"
#include "../support/NanScan.hpp"

using namespace TestSignal;
using namespace TestSpectral;
using namespace TestContinuity;

namespace
{
constexpr double kSampleRate = 48000.0;
}

// ---------------------------------------------------------------------------
// 4096-point spectral path: dominant bin of an exact-bin sine.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("MagnitudeSpectrum: 4096-point path resolves the right bin")
{
    GlobalEnv::ResetPerTest();

    const std::size_t N = kFftSize4096;
    const std::size_t k0 = 200;
    const double freq = BinFreq(k0, N, kSampleRate);

    std::vector<float> buf(N);
    Sine sine(freq, kSampleRate, 1.0f);
    Fill(sine, buf.data(), N);

    std::vector<float> spec = MagnitudeSpectrum(buf.data(), N, kFftSize4096);
    DOCTEST_CHECK(spec.size() == N / 2);
    DOCTEST_CHECK(DominantBin(spec) == k0);
}

// ---------------------------------------------------------------------------
// Continuity: a smooth sine has small max delta; an injected jump is counted.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("Continuity: MaxAbsDelta / DiscontinuityCount")
{
    GlobalEnv::ResetPerTest();

    const std::size_t n = 1024;
    std::vector<float> buf(n);
    Sine sine(100.0, kSampleRate, 1.0f);  // low freq -> small step per sample
    Fill(sine, buf.data(), n);

    float smoothMax = MaxAbsDelta(buf.data(), n);
    DOCTEST_CHECK(smoothMax < 0.05f);
    DOCTEST_CHECK(DiscontinuityCount(buf.data(), n, 0.1f) == 0);

    // Inject a single large jump.
    buf[500] += 1.5f;
    // Jump appears on both the rising edge into 500 and falling edge out of it.
    DOCTEST_CHECK(DiscontinuityCount(buf.data(), n, 0.1f) >= 1);
    DOCTEST_CHECK(MaxAbsDelta(buf.data(), n) > 1.0f);
}

// ---------------------------------------------------------------------------
// THD: a pure sine has near-zero THD; a hard-clipped sine has substantial THD.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("THD: pure tone ~0, clipped tone elevated")
{
    GlobalEnv::ResetPerTest();

    const std::size_t N = kFftSize1024;
    const std::size_t k0 = 32;
    const double freq = BinFreq(k0, N, kSampleRate);

    std::vector<float> clean(N);
    Sine s1(freq, kSampleRate, 0.9f);
    Fill(s1, clean.data(), N);
    double thdClean = THD(clean.data(), N, freq, kSampleRate, kFftSize1024);
    DOCTEST_CHECK(thdClean < 0.02);  // pure tone

    // Hard clip the sine to generate odd harmonics.
    std::vector<float> clipped(N);
    Sine s2(freq, kSampleRate, 1.0f);
    for (std::size_t i = 0; i < N; ++i)
    {
        float x = s2.Next() * 2.0f;
        clipped[i] = std::max(-0.6f, std::min(0.6f, x));
    }
    double thdClipped = THD(clipped.data(), N, freq, kSampleRate, kFftSize1024);
    DOCTEST_CHECK(thdClipped > 0.1);
    DOCTEST_CHECK(thdClipped > thdClean);
}

// ---------------------------------------------------------------------------
// NanScan: clean buffer reports -1; NaN/Inf detected; denormals classified
// separately.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("NanScan: detects NaN/Inf, separates denormals")
{
    GlobalEnv::ResetPerTest();

    std::vector<float> buf(64, 0.25f);
    DOCTEST_CHECK(TestNan::ScanBuffer(buf.data(), buf.size()) == -1);
    TestNan::AssertClean(buf.data(), buf.size());

    buf[40] = std::numeric_limits<float>::quiet_NaN();
    DOCTEST_CHECK(TestNan::ScanBuffer(buf.data(), buf.size()) == 40);

    buf[40] = std::numeric_limits<float>::infinity();
    DOCTEST_CHECK(TestNan::ScanBuffer(buf.data(), buf.size()) == 40);

    // Denormal is finite -> not "bad", but flagged by IsDenormal.
    float denorm = std::numeric_limits<float>::denorm_min();
    DOCTEST_CHECK(TestNan::IsBad(denorm) == false);
    DOCTEST_CHECK(TestNan::IsDenormal(denorm) == true);
    DOCTEST_CHECK(TestNan::IsDenormal(0.0f) == false);
    DOCTEST_CHECK(TestNan::IsBad(std::numeric_limits<float>::quiet_NaN()) == true);
}

// ---------------------------------------------------------------------------
// Sweep generator: produces a finite, bounded signal across its span.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("Sweep: finite and bounded over its span")
{
    GlobalEnv::ResetPerTest();

    const std::size_t n = 4096;
    std::vector<float> buf(n);
    Sweep sweep(50.0, 12000.0, n, kSampleRate);
    Fill(sweep, buf.data(), n);

    DOCTEST_CHECK(TestNan::ScanBuffer(buf.data(), n) == -1);
    for (std::size_t i = 0; i < n; ++i)
    {
        DOCTEST_CHECK(std::abs(buf[i]) <= 1.0001f);
    }
}
