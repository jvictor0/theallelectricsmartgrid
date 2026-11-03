#pragma once

#include <cstddef>

#include "SampleTimer.hpp"

// Host-frame timing for tests that drive SampleTimer::IncrementSample directly.
//
namespace hostframes
{

// Engine samples to run so that at least hostFrames host frames elapse and the
// last processed sample is the final sub-sample of a host frame. Interpolated
// encoder reads there equal the latest host-frame output.
//
inline size_t SamplesToHostFrameEnd(size_t hostFrames)
{
    const size_t ratio = SampleTimer::x_oversample;
    const size_t samples = hostFrames * ratio;
    const size_t last = SampleTimer::GetSample() + samples;
    return samples + (ratio - 1 + ratio - last % ratio) % ratio;
}

// Engine samples to run so that the next sample is sub-sample 0 of a host
// frame, where the encoder matrix computes and recording frames are captured.
//
inline size_t SamplesBeforeNextHostFrame()
{
    return SamplesToHostFrameEnd(0);
}

}
