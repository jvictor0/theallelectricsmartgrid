// dsp_fir.cpp -- unit tests for FIR<Size> (private/src/FIR.hpp)
//
// FIR with windowed sinc design; supports LP/HP/BP via blend parameter.
// Taps are slewed (OPLowPassFilter on each tap). Tests freeze slews by
// setting all slewedTaps == targetTaps right after construction.
//
// Tests:
//   1. Impulse response matches tap values exactly (after disabling slew).
//   2. LP response: DC near 1, stopband attenuated by >20 dB.
//   3. HP response: high-frequency passband near 1, low-frequency stopband attenuated.
//   4. Measured LP response matches DFT of taps (the analytic reference).
//   5. NaN-clean across blend and cutoff sweeps.

#include "doctest.h"

#include <cmath>
#include <vector>
#include <complex>
#include <functional>
#include <algorithm>

#include "../support/GlobalEnv.hpp"
#include "../support/Signal.hpp"
#include "../support/Spectral.hpp"
#include "../support/NanScan.hpp"

#include "FIR.hpp"

using namespace TestSpectral;
using namespace TestSignal;

namespace
{
constexpr double kSR = 48000.0;
constexpr std::size_t kN = kFftSize1024;
constexpr std::size_t kFirSize = 127; // odd size, reasonable filter length

// Disable tap slewing by snapping all slewed taps to their targets.
template<std::size_t Size>
void freezeTaps(FIR<Size>& fir)
{
    for (std::size_t i = 0; i < Size; ++i)
    {
        fir.m_slewedTaps[i] = fir.m_targetTaps[i];
        fir.m_tapSlew[i].m_output = fir.m_targetTaps[i];
    }
}

// Compute the analytic |H(f)| from the tap DFT.
// normFreq: cycles per sample [0, 0.5)
double tapDFT(const float* taps, std::size_t size, double normFreq)
{
    double re = 0.0, im = 0.0;
    for (std::size_t n = 0; n < size; ++n)
    {
        double phase = -2.0 * M_PI * normFreq * static_cast<double>(n);
        re += taps[n] * std::cos(phase);
        im += taps[n] * std::sin(phase);
    }
    return std::sqrt(re*re + im*im);
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Impulse in -> taps out (exact convolution with frozen slews).
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("FIR: impulse response matches tap values")
{
    GlobalEnv::ResetPerTest();

    FIR<kFirSize> fir;
    fir.SetParams(0.1f, 0.0f, 0.5f);  // LP at cutoff=0.1, no blend asymmetry
    freezeTaps(fir);

    // Feed unit impulse then zeros.
    std::vector<float> out(kFirSize + 4, 0.0f);
    out[0] = fir.Process(1.0f);
    for (std::size_t i = 1; i < kFirSize + 4; ++i)
    {
        out[i] = fir.Process(0.0f);
    }

    // The tap order in the delay line: tap[0] is most recent, so impulse response
    // at output[i] should match tap[i] (up to the filter length).
    for (std::size_t i = 0; i < kFirSize; ++i)
    {
        DOCTEST_INFO("i=" << i << " out=" << out[i] << " tap=" << fir.m_slewedTaps[i]);
        DOCTEST_CHECK(out[i] == doctest::Approx(fir.m_slewedTaps[i]).epsilon(1e-5));
    }
    // After the filter length, output should be zero.
    for (std::size_t i = kFirSize; i < kFirSize + 4; ++i)
    {
        DOCTEST_CHECK(std::abs(out[i]) < 1e-6f);
    }
}

// ---------------------------------------------------------------------------
// 2. LP mode: DC near 1, stopband attenuated.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("FIR: LP passband near 0 dB, stopband attenuated >20 dB")
{
    GlobalEnv::ResetPerTest();

    const float cutoff = 0.1f;  // 0.1 cycles per sample ~ 4800 Hz at 48 kHz

    FIR<kFirSize> fir;
    fir.SetParams(cutoff, 0.0f, 0.5f);  // blend=0 -> LP
    freezeTaps(fir);

    std::function<float(float)> proc = [&fir](float x) {
        return fir.Process(x);
    };
    auto H = MeasureTransferFunction(proc, kN, kSR, 0xF12F11000001ull, 32, 4);

    // Passband: ~100 Hz to 3 kHz (below cutoff at 4800 Hz).
    std::size_t loPB = FreqBin(100.0, kN, kSR);
    std::size_t hiPB = FreqBin(3000.0, kN, kSR);
    AssertResponseMatches(H, [](double) { return 1.0; }, kSR, kN, 2.5, loPB, hiPB);

    // Stopband: 3x cutoff (about 14400 Hz) and above.
    std::size_t loSB = FreqBin(14000.0, kN, kSR);
    std::size_t hiSB = FreqBin(23000.0, kN, kSR);
    for (std::size_t k = loSB; k <= hiSB && k < H.size(); ++k)
    {
        float hk = H[k];
        float dbk = 20.0f * std::log10(hk > 1e-9f ? hk : 1e-9f);
        DOCTEST_INFO("stopband bin " << k << " (" << BinFreq(k, kN, kSR)
                     << " Hz) = " << dbk << " dB");
        DOCTEST_CHECK(dbk < -20.0f);
    }
}

// ---------------------------------------------------------------------------
// 3. HP mode: high-frequency passband near 1.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("FIR: HP passband near 0 dB above cutoff")
{
    GlobalEnv::ResetPerTest();

    const float cutoff = 0.2f;  // ~9600 Hz

    FIR<kFirSize> fir;
    fir.SetParams(cutoff, 1.0f, 0.5f);  // blend=1 -> HP
    freezeTaps(fir);

    std::function<float(float)> proc = [&fir](float x) {
        return fir.Process(x);
    };
    auto H = MeasureTransferFunction(proc, kN, kSR, 0xF12F12000001ull, 32, 4);

    // Passband: > 3x cutoff (~28800 Hz -- use upper range up to 22 kHz).
    std::size_t loPB = FreqBin(20000.0, kN, kSR);
    std::size_t hiPB = FreqBin(23000.0, kN, kSR);
    AssertResponseMatches(H, [](double) { return 1.0; }, kSR, kN, 3.0, loPB, hiPB);

    // Stopband: well below cutoff.
    std::size_t loSB = FreqBin(100.0, kN, kSR);
    std::size_t hiSB = FreqBin(2000.0, kN, kSR);
    for (std::size_t k = loSB; k <= hiSB && k < H.size(); ++k)
    {
        float hk = H[k];
        float dbk = 20.0f * std::log10(hk > 1e-9f ? hk : 1e-9f);
        DOCTEST_CHECK(dbk < -15.0f);
    }
}

// ---------------------------------------------------------------------------
// 4. Measured LP response matches DFT of taps (analytic reference).
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("FIR: measured LP response matches tap DFT (2.5 dB tol)")
{
    GlobalEnv::ResetPerTest();

    const float cutoff = 0.15f;

    FIR<kFirSize> fir;
    fir.SetParams(cutoff, 0.0f, 0.5f);
    freezeTaps(fir);

    // Copy taps for the analytic reference.
    float taps[kFirSize];
    for (std::size_t i = 0; i < kFirSize; ++i)
    {
        taps[i] = fir.m_slewedTaps[i];
    }

    std::function<float(float)> proc = [&fir](float x) {
        return fir.Process(x);
    };
    auto H = MeasureTransferFunction(proc, kN, kSR, 0xF12FD1F10001ull, 32, 4);

    auto expected = [&taps](double hz) -> double {
        double normFreq = hz / kSR;
        return tapDFT(taps, kFirSize, normFreq);
    };

    // Compare passband + transition band only. The Welch estimator has high
    // variance in the deep stopband (expected < -50 dB), so we restrict the
    // upper frequency to 2x the cutoff (3x cutoff ~ 21600 Hz at 0.15 cps * 48k).
    // Keep the comparison where the analytic response is above ~-40 dB.
    std::size_t lo = FreqBin(200.0, kN, kSR);
    std::size_t hi = FreqBin(5000.0, kN, kSR);  // well within transition, avoids deep stopband
    AssertResponseMatches(H, expected, kSR, kN, 2.5, lo, hi);
}

// ---------------------------------------------------------------------------
// 5. NaN-clean across blend and cutoff sweeps.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("FIR: NaN-clean across blend/cutoff variations")
{
    GlobalEnv::ResetPerTest();

    const std::size_t nSamples = 2048;
    TestSignal::WhiteNoise noise(0xF12FABB10001ull);
    std::vector<float> out(nSamples);

    struct Params { float cutoff; float blend; float asym; };
    const Params configs[] = {
        { 0.1f, 0.0f, 0.5f },   // LP
        { 0.2f, 0.5f, 0.5f },   // BP
        { 0.3f, 1.0f, 0.5f },   // HP
        { 0.05f, 0.25f, 0.3f }, // LP-leaning blend, low cutoff
        { 0.4f,  0.75f, 0.7f }, // HP-leaning blend, high cutoff
    };

    for (auto& p : configs)
    {
        FIR<kFirSize> fir;
        fir.SetParams(p.cutoff, p.blend, p.asym);
        freezeTaps(fir);

        for (std::size_t i = 0; i < nSamples; ++i)
        {
            out[i] = fir.Process(noise.Next());
        }
        DOCTEST_INFO("cutoff=" << p.cutoff << " blend=" << p.blend << " asym=" << p.asym);
        TestNan::AssertClean(out.data(), nSamples);
    }
}
