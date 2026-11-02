#pragma once

#include "SampleTimer.hpp"

#include <cstddef>

// Width-independent parts of the shape section: rate constants and the
// filter frequency mapping shared by the filter machines.
//
struct SquiggleBoyShapeBase
{
    static constexpr float x_lpPitchTracking = 0.707f;
    static constexpr float x_lpTrackingPivot = SampleTimer::HzToCycPerSamp(261.6256f);

    // Filter cutoffs stay below the host Nyquist.
    //
    static constexpr float x_maxCutoff = 0.5f / SampleTimer::x_oversample;

    // Frequencies are in cycles per sample.
    //
    template <typename Vector>
    static Vector HighPassFreq(const Vector& baseFreq, const Vector& hpCutoff)
    {
        return Vector::Min(Vector::Splat(x_maxCutoff), baseFreq * hpCutoff);
    }

    template <typename Vector>
    static Vector LowPassFreq(const Vector& baseFreq, const Vector& hpCutoff, const Vector& lpCutoff)
    {
        Vector lpBaseFreq = ((baseFreq / x_lpTrackingPivot).Log2() * x_lpPitchTracking).Exp2() * x_lpTrackingPivot;
        return Vector::Min(Vector::Splat(x_maxCutoff), lpBaseFreq * hpCutoff * lpCutoff);
    }

    template <typename Vector>
    static Vector SampleRateReducerFreq(const Vector& baseFreq, const Vector& sampleRateReducerFreq)
    {
        return baseFreq * sampleRateReducerFreq;
    }
};
