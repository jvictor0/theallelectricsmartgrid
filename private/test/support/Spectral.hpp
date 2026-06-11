#pragma once

// Spectral.hpp -- FFT wrapper + transfer-function assertions for DSP tests.
//
// FROZEN API (WP-2). Built on the repo's own FFT
// (DiscreteFourierTransformGeneric<Bits> in AdaptiveWaveTable.hpp), NOT on
// ScopeWriter::WindowedFFT.
//
// NORMALIZATION of the repo FFT (verified by reading AdaptiveWaveTable.hpp and
// Math.hpp, and empirically in dsp_dft.cpp):
//   - Forward transform is a standard DFT with twiddle exp(-i*2*pi*j/len)
//     scaled by 1/N: m_components[k] = (1/N) * sum_i x[i] exp(-i 2pi k i / N).
//   - Therefore for a real cosine x[i] = A*cos(2*pi*k0*i/N) with 0 < k0 < N/2,
//     |m_components[k0]| = A/2 (energy splits between +k0 and -k0).
//   - For DC (x[i] = A), |m_components[0]| = A.
//   - m_components holds only the lower half: k in [0, N/2).
//
// We apply a Hann window before transforming (raw, un-normalized Hann). This
// scales broadband magnitudes by the window's mean (~0.5) but that scale
// cancels in transfer-function ratios (out/in), which is all we assert on.

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <vector>
#include <functional>

#include "doctest.h"

#include "BasicWaveTable.hpp"
#include "AdaptiveWaveTable.hpp"

#include "Signal.hpp"

namespace TestSpectral
{

// Supported FFT sizes map to the repo's two DFT typedefs.
//   1024 -> DiscreteFourierTransformGeneric<10>
//   4096 -> DiscreteFourierTransformGeneric<12>
//
inline constexpr std::size_t kFftSize1024 = 1024;
inline constexpr std::size_t kFftSize4096 = 4096;

// ---------------------------------------------------------------------------
// MagnitudeSpectrumT<Bits> -- templated core. Applies a Hann window, copies
// (zero-padding / truncating) into the repo wavetable, runs the repo FFT, and
// returns |m_components[k]| for k in [0, N/2).
// ---------------------------------------------------------------------------
//
template <std::size_t Bits>
inline std::vector<float> MagnitudeSpectrumT(const float* buf, std::size_t n)
{
    constexpr std::size_t N = BasicWaveTableGeneric<Bits>::x_tableSize;
    constexpr std::size_t half = N / 2;

    BasicWaveTableGeneric<Bits> table;  // zero-initialized
    const std::size_t m = (n < N) ? n : N;
    for (std::size_t i = 0; i < m; ++i)
    {
        // Hann window over the table length N (periodic-ish, formula uses N-1).
        //
        double w = 0.5 * (1.0 - std::cos(2.0 * M_PI * static_cast<double>(i) /
                                         static_cast<double>(N - 1)));
        table.m_table[i] = static_cast<float>(buf[i] * w);
    }
    // Remaining [m, N) stay zero (zero-pad).

    DiscreteFourierTransformGeneric<Bits> dft;
    dft.Transform(table);

    std::vector<float> mag(half);
    for (std::size_t k = 0; k < half; ++k)
    {
        mag[k] = std::abs(dft.m_components[k]);
    }
    return mag;
}

// Runtime-dispatched magnitude spectrum. fftSize must be 1024 or 4096.
//
inline std::vector<float> MagnitudeSpectrum(const float* buf, std::size_t n,
                                            std::size_t fftSize = kFftSize1024)
{
    if (fftSize == kFftSize4096)
    {
        return MagnitudeSpectrumT<12>(buf, n);
    }
    return MagnitudeSpectrumT<10>(buf, n);
}

// ---------------------------------------------------------------------------
// Bin <-> frequency helpers
// ---------------------------------------------------------------------------

// Center frequency (Hz) of bin k.
//
inline double BinFreq(std::size_t k, std::size_t fftSize, double sampleRate)
{
    return static_cast<double>(k) * sampleRate / static_cast<double>(fftSize);
}

// Nearest bin index to frequency hz (may exceed N/2-1 for hz >= Nyquist).
//
inline std::size_t FreqBin(double hz, std::size_t fftSize, double sampleRate)
{
    double b = hz * static_cast<double>(fftSize) / sampleRate;
    if (b < 0.0)
    {
        b = 0.0;
    }
    return static_cast<std::size_t>(b + 0.5);
}

// Argmax over the spectrum.
//
inline std::size_t DominantBin(const std::vector<float>& spectrum)
{
    std::size_t best = 0;
    float bestMag = -1.0f;
    for (std::size_t k = 0; k < spectrum.size(); ++k)
    {
        if (spectrum[k] > bestMag)
        {
            bestMag = spectrum[k];
            best = k;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// MeasureTransferFunction -- drive seeded white noise through processFn and
// estimate |H(f)| = avg|Out[k]| / avg|In[k]| using Welch-style averaging of
// magnitude spectra across many frames. Averaging is essential: a single
// white-noise frame has ~100% magnitude variance per bin; averaging 32+
// independent frames tames it to a usable estimate.
//
// processFn: any callable float(float). A fresh warmup frame is discarded so
// filter state settles before measurement.
// ---------------------------------------------------------------------------
//
template <typename ProcessFn>
inline std::vector<float> MeasureTransferFunctionT(ProcessFn processFn,
                                                   std::size_t fftSize,
                                                   double sampleRate,
                                                   std::uint64_t seed,
                                                   int numFrames = 48,
                                                   int warmupFrames = 1)
{
    const std::size_t N = fftSize;
    const std::size_t half = N / 2;

    std::vector<float> avgIn(half, 0.0f);
    std::vector<float> avgOut(half, 0.0f);

    std::vector<float> in(N), out(N);
    TestSignal::WhiteNoise noise(seed);

    const int totalFrames = numFrames + warmupFrames;
    for (int f = 0; f < totalFrames; ++f)
    {
        for (std::size_t i = 0; i < N; ++i)
        {
            float x = noise.Next();
            in[i] = x;
            out[i] = processFn(x);
        }
        if (f < warmupFrames)
        {
            continue;  // discard while filter state settles
        }
        std::vector<float> mIn = MagnitudeSpectrum(in.data(), N, fftSize);
        std::vector<float> mOut = MagnitudeSpectrum(out.data(), N, fftSize);
        for (std::size_t k = 0; k < half; ++k)
        {
            avgIn[k] += mIn[k];
            avgOut[k] += mOut[k];
        }
    }

    std::vector<float> H(half, 0.0f);
    for (std::size_t k = 0; k < half; ++k)
    {
        H[k] = (avgIn[k] > 1e-12f) ? (avgOut[k] / avgIn[k]) : 0.0f;
    }
    return H;
}

// std::function overload (the documented primary signature).
//
inline std::vector<float> MeasureTransferFunction(const std::function<float(float)>& processFn,
                                                  std::size_t fftSize,
                                                  double sampleRate,
                                                  std::uint64_t seed,
                                                  int numFrames = 48,
                                                  int warmupFrames = 1)
{
    return MeasureTransferFunctionT(processFn, fftSize, sampleRate, seed,
                                    numFrames, warmupFrames);
}

// ---------------------------------------------------------------------------
// AssertResponseMatches -- compare measured |H| against an expected response
// (callable hz -> linear gain), in dB, per bin within tolDb. Bins below loBin
// or above hiBin are skipped (window leakage corrupts DC/Nyquist neighborhoods).
//
// To keep output readable: only the first kMaxReports failing bins emit a
// detailed CHECK; a final CHECK asserts the total failure count is zero.
// ---------------------------------------------------------------------------
//
template <typename ExpectedFn>
inline void AssertResponseMatches(const std::vector<float>& measuredH,
                                  ExpectedFn expectedHFn,
                                  double sampleRate,
                                  std::size_t fftSize,
                                  double tolDb,
                                  std::size_t loBin,
                                  std::size_t hiBin)
{
    constexpr int kMaxReports = 6;
    const std::size_t half = measuredH.size();
    if (hiBin >= half)
    {
        hiBin = half ? half - 1 : 0;
    }

    int failures = 0;
    int reported = 0;
    for (std::size_t k = loBin; k <= hiBin && k < half; ++k)
    {
        double hz = BinFreq(k, fftSize, sampleRate);
        double expLin = static_cast<double>(expectedHFn(hz));
        double measLin = static_cast<double>(measuredH[k]);

        // Guard against log of zero.
        //
        double expDb = 20.0 * std::log10(expLin > 1e-9 ? expLin : 1e-9);
        double measDb = 20.0 * std::log10(measLin > 1e-9 ? measLin : 1e-9);
        double errDb = std::abs(measDb - expDb);

        if (errDb > tolDb)
        {
            ++failures;
            if (reported < kMaxReports)
            {
                ++reported;
                DOCTEST_INFO("bin " << k << " (" << hz << " Hz): expected "
                             << expDb << " dB, measured " << measDb
                             << " dB, |err|=" << errDb << " dB > tol " << tolDb);
                DOCTEST_CHECK(errDb <= tolDb);
            }
        }
    }

    // Final tally so the count is asserted even past the report cap.
    //
    DOCTEST_INFO("total bins out of tolerance: " << failures
                 << " (band [" << loBin << ", " << hiBin << "])");
    DOCTEST_CHECK(failures == 0);
}

} // namespace TestSpectral
