#pragma once

#include "SampleTimer.hpp"

#include <cstddef>

// Width-independent parts of the shape section: rate constants and the
// filter frequency mapping shared by the filter machines.
//
struct SquiggleBoyShapeBase
{
    static constexpr size_t x_oversample = 4;
    static constexpr float x_lpPitchTracking = 0.707f;
    static constexpr float x_lpTrackingPivot = SampleTimer::HzToCycPerSamp(261.6256f);

    // Frequencies come in cycles per base-rate sample and go out at the
    // oversampled rate, kept below the base-rate Nyquist.
    //
    template <typename Vector>
    static Vector HighPassFreq(const Vector& baseFreq, const Vector& hpCutoff)
    {
        return Vector::Min(Vector::Splat(0.5), baseFreq * hpCutoff) / x_oversample;
    }

    template <typename Vector>
    static Vector LowPassFreq(const Vector& baseFreq, const Vector& hpCutoff, const Vector& lpCutoff)
    {
        Vector lpBaseFreq = ((baseFreq / x_lpTrackingPivot).Log2() * x_lpPitchTracking).Exp2() * x_lpTrackingPivot;
        return Vector::Min(Vector::Splat(0.5), lpBaseFreq * hpCutoff * lpCutoff) / x_oversample;
    }

    template <typename Vector>
    static Vector SampleRateReducerFreq(const Vector& baseFreq, const Vector& sampleRateReducerFreq)
    {
        return baseFreq * sampleRateReducerFreq / x_oversample;
    }
};
