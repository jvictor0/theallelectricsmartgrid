#pragma once

// Continuity.hpp -- waveform continuity & harmonic-distortion helpers.
//
// FROZEN API (WP-2).

#include <cstddef>
#include <cmath>
#include <vector>

#include "Spectral.hpp"

namespace TestContinuity
{

// Largest absolute first difference |buf[i+1] - buf[i]| over the buffer.
// 0 for buffers shorter than 2 samples.
//
inline float MaxAbsDelta(const float* buf, std::size_t n)
{
    float maxd = 0.0f;
    for (std::size_t i = 0; i + 1 < n; ++i)
    {
        float d = std::abs(buf[i + 1] - buf[i]);
        if (d > maxd)
        {
            maxd = d;
        }
    }
    return maxd;
}

// Number of adjacent-sample jumps whose magnitude strictly exceeds threshold.
//
inline std::size_t DiscontinuityCount(const float* buf, std::size_t n, float threshold)
{
    std::size_t count = 0;
    for (std::size_t i = 0; i + 1 < n; ++i)
    {
        if (std::abs(buf[i + 1] - buf[i]) > threshold)
        {
            ++count;
        }
    }
    return count;
}

// THD -- total harmonic distortion as a power ratio
//   sqrt( sum_{h>=2} E(h*f) / E(f) )
// where E(.) is the summed magnitude-squared energy in a +/-1-bin neighborhood
// around each harmonic. The +/-1-bin window captures Hann main-lobe leakage so
// the measure is robust when the fundamental does not land exactly on a bin.
//
// Returns a linear ratio (0 = pure tone). Harmonics at or above Nyquist (i.e.
// past the spectrum's last bin) are ignored.
//
inline double THD(const float* buf, std::size_t n, double fundamentalHz,
                  double sampleRate, std::size_t fftSize = TestSpectral::kFftSize1024)
{
    std::vector<float> spec = TestSpectral::MagnitudeSpectrum(buf, n, fftSize);
    const std::size_t half = spec.size();

    auto neighborhoodEnergy = [&](std::size_t centerBin) -> double {
        double e = 0.0;
        std::size_t lo = (centerBin == 0) ? 0 : centerBin - 1;
        std::size_t hi = centerBin + 1;
        for (std::size_t k = lo; k <= hi && k < half; ++k)
        {
            double m = static_cast<double>(spec[k]);
            e += m * m;
        }
        return e;
    };

    std::size_t fundBin = TestSpectral::FreqBin(fundamentalHz, fftSize, sampleRate);
    double fundEnergy = neighborhoodEnergy(fundBin);
    if (fundEnergy <= 0.0)
    {
        return 0.0;
    }

    double harmEnergy = 0.0;
    for (int h = 2;; ++h)
    {
        std::size_t hb = TestSpectral::FreqBin(fundamentalHz * h, fftSize, sampleRate);
        if (hb >= half)
        {
            break;  // harmonic past Nyquist
        }
        harmEnergy += neighborhoodEnergy(hb);
    }

    return std::sqrt(harmEnergy / fundEnergy);
}

} // namespace TestContinuity
