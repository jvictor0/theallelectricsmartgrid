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

    // High-pass and SVF low-pass cutoffs stay below host Nyquist. The ladder
    // supplies its own upper limit to the shared low-pass pitch-tracking law.
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
        return LowPassFreq(baseFreq, hpCutoff, lpCutoff, x_maxCutoff);
    }

    template <typename Vector>
    static Vector LowPassFreq(const Vector& baseFreq, const Vector& hpCutoff, const Vector& lpCutoff, float maxCutoff)
    {
        Vector lpBaseFreq = ((baseFreq / x_lpTrackingPivot).Log2() * x_lpPitchTracking).Exp2() * x_lpTrackingPivot;
        return Vector::Min(Vector::Splat(maxCutoff), lpBaseFreq * hpCutoff * lpCutoff);
    }

    template <typename Vector>
    static Vector SampleRateReducerFreq(const Vector& baseFreq, const Vector& sampleRateReducerFreq)
    {
        return baseFreq * sampleRateReducerFreq;
    }
};
