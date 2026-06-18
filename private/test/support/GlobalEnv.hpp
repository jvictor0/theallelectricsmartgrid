#pragma once

// GlobalEnv: process-level test environment management for the standalone,
// JUCE-free smartgrid test binary.
//
// The DSP core relies on a handful of global singletons / static state that
// must be initialized before any code that touches them runs:
//
//   * SampleTimer::s_instance  - global sample/frame clock (SampleTimer.hpp).
//                                Init() re-news the singleton. Re-Init()ing
//                                resets the sample/frame counters to zero,
//                                which is what we want at the start of each
//                                test so timing-derived behavior is repeatable.
//
//   * AsyncLogQueue::s_instance - the INFO(...) log sink (AsyncLogger.hpp).
//                                 Tests configure explicit log directories in
//                                 logger-specific cases; the global queue still
//                                 exists for all tests.
//
//   * Static RNGs in the DSP core - several headers own static Mersenne
//                                 Twister generators that are otherwise seeded
//                                 nondeterministically from std::random_device.
//                                 SeedRandomness() reseeds them deterministically
//                                 so tests that exercise randomness are
//                                 reproducible. Found via grep over private/src:
//                                   - RandomLFO::s_rng        (RandomLFO.hpp)
//                                   - RGen::s_gen             (NormGen.hpp)
//                                   - RandomWaveTable::s_gen  (RandomWaveTable.hpp)
//                                 (Uuid.hpp and StateSaver.hpp use function-local
//                                  RNGs that cannot be reseeded from here, and
//                                  Noise::WhiteNoise is per-instance seeded.)

#include <cstddef>
#include <cstdint>

#include "SampleTimer.hpp"
#include "AsyncLogger.hpp"
#include "RandomLFO.hpp"
#include "NormGen.hpp"
#include "RandomWaveTable.hpp"

namespace GlobalEnv
{

// Default frame size used throughout the host (SampleTimer::x_samplesPerProcessFrame).
//
inline constexpr std::size_t kSamplesPerFrame = SampleTimer::x_samplesPerProcessFrame;

// Default deterministic seed for test runs.
//
inline constexpr std::uint64_t kDefaultSeed = 0x5EED5EED5EED5EEDull;

// Reseed every static RNG in the DSP core deterministically.
//
inline void SeedRandomness(std::uint64_t seed)
{
    RandomLFO::s_rng.seed(static_cast<std::mt19937::result_type>(seed));
    RGen::s_gen.seed(static_cast<std::mt19937::result_type>(seed + 1));
    RandomWaveTable::s_gen.seed(static_cast<std::mt19937::result_type>(seed + 2));
}

// Idempotent process-level init. Safe to call multiple times.
//
inline void Init()
{
    static bool s_initialized = false;
    if (s_initialized)
    {
        return;
    }
    s_initialized = true;

    SampleTimer::Init(kSamplesPerFrame);
    (void) &AsyncLogQueue::s_instance;
    SeedRandomness(kDefaultSeed);
}

// Per-test reset: fresh sample/frame counters and deterministic RNG state.
//
inline void ResetPerTest()
{
    // Re-Init the SampleTimer so m_sample / m_frame start fresh.
    //
    SampleTimer::Init(kSamplesPerFrame);
    SeedRandomness(kDefaultSeed);
}

} // namespace GlobalEnv
