#pragma once

#include <cmath>
#include <complex>

// Independent digital transfer oracle for the SVF's natural-frequency tuning.
// Uses the bilinear polynomial in z^-1, not the generated power expression.
//
namespace TestSVF
{
struct Response
{
    std::complex<double> m_low;
    std::complex<double> m_band;
    std::complex<double> m_high;
};

inline Response Transfer(double g, double k, double frequency)
{
    const auto z = std::polar(1.0, -2 * std::acos(-1.0) * frequency);
    const auto a = 1.0 - z;
    const auto b = g * (1.0 + z);
    const auto denominator = a * a + k * a * b + b * b;
    return {b * b / denominator, a * b / denominator, a * a / denominator};
}

inline double LowPassFrequencyResponse(double g, double k, double frequency)
{
    return std::abs(Transfer(g, k, frequency).m_low);
}

inline double HighPassFrequencyResponse(double g, double k, double frequency)
{
    return std::abs(Transfer(g, k, frequency).m_high);
}

inline double BandPassFrequencyResponse(double g, double k, double frequency)
{
    return std::abs(Transfer(g, k, frequency).m_band);
}
}
