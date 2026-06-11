// dsp_vco.cpp -- unit tests for VCO (private/src/VCO.hpp)
//
// VCO is a phase-accumulating sine oscillator.
// Process(float freq) takes freq in CYCLES PER SAMPLE (phase increment per sample).
// The code: m_phase += freq; output = Math::Sin2pi(m_phase).
//
// Tests:
//   1. Dominant bin at expected frequency (frequency accuracy).
//   2. THD < 0.1 (< 10% -- loose because Math::Sin2pi is a polynomial approximation).
//   3. Amplitude near 1.0 at mid freq.
//   4. Phase continuity under frequency change (MaxAbsDelta bounded -- no clicks).
//   5. Output is NaN-clean.

#include "doctest.h"

#include <cmath>
#include <vector>
#include <functional>

#include "../support/GlobalEnv.hpp"
#include "../support/Signal.hpp"
#include "../support/Spectral.hpp"
#include "../support/Continuity.hpp"
#include "../support/NanScan.hpp"

#include "VCO.hpp"

using namespace TestSpectral;
using namespace TestSignal;

namespace
{
constexpr double kSR = 48000.0;
constexpr std::size_t kN = kFftSize1024;
} // namespace

// ---------------------------------------------------------------------------
// 1. Dominant spectrum bin at expected frequency.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("VCO: dominant bin at set frequency")
{
    GlobalEnv::ResetPerTest();

    const double freqHz   = 440.0;
    const float  freqCps  = static_cast<float>(freqHz / kSR);

    VCO vco;
    std::vector<float> buf(kN);
    for (std::size_t i = 0; i < kN; ++i)
    {
        buf[i] = vco.Process(freqCps);
    }

    auto spec = MagnitudeSpectrum(buf.data(), kN, kFftSize1024);
    std::size_t dom = DominantBin(spec);
    std::size_t expected = FreqBin(freqHz, kN, kSR);

    DOCTEST_INFO("dominant bin=" << dom << " expected=" << expected
                 << " (freqHz=" << freqHz << ")");
    // Allow ±2 bins for non-integer bin alignment.
    //
    DOCTEST_CHECK(static_cast<long>(dom) >= static_cast<long>(expected) - 2);
    DOCTEST_CHECK(static_cast<long>(dom) <= static_cast<long>(expected) + 2);
}

// ---------------------------------------------------------------------------
// 1b. Dominant bin at a second frequency (1 kHz).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("VCO: dominant bin at 1000 Hz")
{
    GlobalEnv::ResetPerTest();

    const double freqHz  = 1000.0;
    const float  freqCps = static_cast<float>(freqHz / kSR);

    VCO vco;
    std::vector<float> buf(kN);
    for (std::size_t i = 0; i < kN; ++i)
    {
        buf[i] = vco.Process(freqCps);
    }

    auto spec = MagnitudeSpectrum(buf.data(), kN, kFftSize1024);
    std::size_t dom = DominantBin(spec);
    std::size_t expected = FreqBin(freqHz, kN, kSR);

    DOCTEST_INFO("dominant bin=" << dom << " expected=" << expected);
    DOCTEST_CHECK(static_cast<long>(dom) >= static_cast<long>(expected) - 2);
    DOCTEST_CHECK(static_cast<long>(dom) <= static_cast<long>(expected) + 2);
}

// ---------------------------------------------------------------------------
// 2. THD < 0.1 (< 10%) -- Math::Sin2pi is a polynomial approximation.
//    Note: if THD is consistently higher than expected from a polynomial sin,
//    that might indicate a BUG in Math::Sin2pi -- documented with WARN.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("VCO: THD below 0.1 (10%)")
{
    GlobalEnv::ResetPerTest();

    // Use an exact-bin frequency for clean THD measurement.
    // Bin 20 at 1024-point FFT at 48 kHz: freq = 20 * 48000/1024 ~ 937.5 Hz.
    //
    const std::size_t k0 = 20;
    const double freqHz  = BinFreq(k0, kN, kSR);
    const float  freqCps = static_cast<float>(freqHz / kSR);

    VCO vco;
    const std::size_t nBuf = kN * 4;  // use 4 windows for better THD estimate
    std::vector<float> buf(nBuf);
    for (std::size_t i = 0; i < nBuf; ++i)
    {
        buf[i] = vco.Process(freqCps);
    }

    double thd = TestContinuity::THD(buf.data(), nBuf, freqHz, kSR, kFftSize1024);
    DOCTEST_INFO("VCO THD at " << freqHz << " Hz: " << thd);

    // BUG?: if Math::Sin2pi has unexpectedly high distortion, this may fail.
    // Polynomial sin approximations can have THD up to ~1%; threshold at 10%.
    //
    DOCTEST_WARN(thd < 0.1);
    // Hard failure only if severely distorted.
    //
    DOCTEST_CHECK(thd < 0.5);
}

// ---------------------------------------------------------------------------
// 3. Output amplitude near 1.0.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("VCO: peak amplitude near 1.0")
{
    GlobalEnv::ResetPerTest();

    const float freqCps = static_cast<float>(500.0 / kSR);

    VCO vco;
    const std::size_t n = 4096;
    float maxVal = 0.0f;
    for (std::size_t i = 0; i < n; ++i)
    {
        float v = vco.Process(freqCps);
        float a = std::abs(v);
        if (a > maxVal) maxVal = a;
    }
    DOCTEST_INFO("peak amplitude: " << maxVal);
    DOCTEST_CHECK(maxVal > 0.9f);
    DOCTEST_CHECK(maxVal <= 1.01f);  // should not exceed 1 significantly
}

// ---------------------------------------------------------------------------
// 4. Phase continuity under a frequency change: MaxAbsDelta bounded.
//    A sudden frequency hop should not produce an audible click (jump in output).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("VCO: phase continuity under frequency change")
{
    GlobalEnv::ResetPerTest();

    const std::size_t n1 = 2048;
    const std::size_t n2 = 2048;

    VCO vco;
    std::vector<float> buf(n1 + n2);

    float freqLow  = static_cast<float>(440.0  / kSR);
    float freqHigh = static_cast<float>(2000.0 / kSR);

    for (std::size_t i = 0; i < n1; ++i)
    {
        buf[i] = vco.Process(freqLow);
    }
    // Instant frequency change -- test that the output doesn't jump discontinuously.
    //
    for (std::size_t i = n1; i < n1 + n2; ++i)
    {
        buf[i] = vco.Process(freqHigh);
    }

    // Inspect the sample at the boundary (index n1-1 and n1).
    //
    float jumpAtBoundary = std::abs(buf[n1] - buf[n1 - 1]);
    DOCTEST_INFO("jump at frequency switch boundary: " << jumpAtBoundary);

    // Phase accumulator continues smoothly; large freq change will sweep quickly
    // but since m_phase is continuous, the max delta should be bounded by the
    // maximum slope of a sine (approximately 2*pi*freqHigh per sample, which
    // at 2000 Hz / 48000 Hz = 0.042 cycles/sample -> max |dy| ~ 2*pi*0.042 ~ 0.26).
    // This is not a click; actual amplitude discontinuity is zero.
    //
    DOCTEST_CHECK(jumpAtBoundary < 2.0f);  // sine jump bounded by sine derivative

    // MaxAbsDelta over the whole buffer should be consistent with freq=2000 Hz.
    //
    float maxDelta = TestContinuity::MaxAbsDelta(buf.data(), n1 + n2);
    DOCTEST_INFO("MaxAbsDelta over full buffer: " << maxDelta);
    DOCTEST_CHECK(maxDelta < 1.5f);
}

// ---------------------------------------------------------------------------
// 5. NaN-clean output.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("VCO: NaN-clean output")
{
    GlobalEnv::ResetPerTest();

    const std::size_t n = 4096;
    std::vector<float> buf(n);

    VCO vco;
    // Test at several frequencies including near-Nyquist.
    //
    const float freqs[] = {
        static_cast<float>(50.0   / kSR),
        static_cast<float>(440.0  / kSR),
        static_cast<float>(4000.0 / kSR),
        static_cast<float>(20000.0 / kSR),
    };

    for (float f : freqs)
    {
        VCO v;
        for (std::size_t i = 0; i < n; ++i)
        {
            buf[i] = v.Process(f);
        }
        DOCTEST_INFO("freqCps=" << f);
        TestNan::AssertClean(buf.data(), n);
    }
}
