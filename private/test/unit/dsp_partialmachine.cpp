// dsp_partialmachine.cpp  --  unit tests for PartialMachine
//
// PartialMachine drives spectral analysis (SpectralModel::ExtractAtoms) +
// synthesis (SynthesisContext + QuadDFT) + OLA read-back every x_H samples.
//
// Wiring (from SquiggleBoy.hpp):
//   m_partialMachine.Process(m_mixer.m_send[2], m_partialMachineState)
//   The Input struct is built by PartialMachine::InputSetter::SetInput().
//   For tests we build it directly from its default-constructed state, which
//   corresponds to: 1024 atoms, no synthetic harmonics, pass-all reduction,
//   organic gain = 1 (set in SetInput when useSyntheticHarmonics = false).
//
// SpectralModel constants (Bits=12):
//   x_tableSize = 4096,  x_H = x_tableSize / 4 = 1024,  x_maxAtoms = 8192
//
// OLA latency: at least one hop (x_H = 1024 samples) before any synthesis
// audio emerges; in practice 2-3 hops for atoms to reach usable magnitude.
// We warm up for 4 * x_H samples before making assertions.
//
// NOTE: DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES; use DOCTEST_ prefixes.

#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "../support/GlobalEnv.hpp"
#include "../support/NanScan.hpp"
#include "../support/Signal.hpp"

#include "PartialMachine.hpp"
#include "SampleTimer.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

// Build a basic PartialMachine::Input that allows synthesis:
//   - organic mode (no synthetic harmonics)
//   - organic gain = 1, synthetic gain = 0 (default from SetInput path)
//   - wide-open bandwidth (accept all frequencies)
//   - reductionFeedback = 0 (immediate gating)
//   - syntheticGain = 0, organicGain = 1
//
PartialMachine::Input MakeBasicInput()
{
    PartialMachine::Input inp;

    inp.m_spectralModelInput.m_numAtoms = 64;  // modest; keeps test fast
    inp.m_spectralModelInput.m_useSyntheticHarmonics = false;

    // Slew: immediate (alpha = 1 = instantaneous).
    //
    inp.m_spectralModelInput.m_slewUpAlpha   = FrequencyDependentParameter::Parameter(1.0f);
    inp.m_spectralModelInput.m_slewDownAlpha = FrequencyDependentParameter::Parameter(1.0f);
    inp.m_spectralModelInput.m_omegaPortamentoAlpha = FrequencyDependentParameter::Parameter(1.0f);

    // Wide omega density: accept all atoms.
    //
    inp.m_spectralModelInput.m_omegaDensity = FrequencyDependentParameter::Parameter(1.0f / 4096.0f);

    // Low gain threshold so low-amplitude partials are tracked.
    //
    inp.m_spectralModelInput.m_gainThreshold = 1e-4f;

    // SynthesisContext: wide-open filter (bwBase = very low, bwWidth = very large),
    // full volume, no bass cut, spread azimuth, full organic gain.
    //
    inp.m_synthesisContextInput.m_bwBaseFrequency = FrequencyDependentParameter::Parameter(1.0f / 4096.0f);
    inp.m_synthesisContextInput.m_bwWidth         = FrequencyDependentParameter::Parameter(4096.0f);
    inp.m_synthesisContextInput.m_volume          = FrequencyDependentParameter::Parameter(1.0f);
    inp.m_synthesisContextInput.m_bassCutoff      = FrequencyDependentParameter::Parameter(1.0f / 4096.0f);
    inp.m_synthesisContextInput.m_azimuthFactor   = FrequencyDependentParameter::Parameter(0.25f);
    inp.m_synthesisContextInput.m_syntheticGain   = FrequencyDependentParameter::Parameter(0.0f);
    inp.m_synthesisContextInput.m_organicGain     = FrequencyDependentParameter::Parameter(1.0f);
    inp.m_synthesisContextInput.m_reductionFeedback = FrequencyDependentParameter::Parameter(0.0f);
    inp.m_synthesisContextInput.m_unison          = FrequencyDependentParameter::Parameter(0.0f);
    inp.m_synthesisContextInput.m_pitchShiftDepth = FrequencyDependentParameter::Parameter(1.0f);
    inp.m_synthesisContextInput.m_pitchShift      = FrequencyDependentParameter::Parameter(0.0f);
    inp.m_synthesisContextInput.m_azimuthOffset   = 0.0;

    return inp;
}

constexpr size_t kHopSize   = 1024;  // SpectralModel::x_H for Bits=12
constexpr int    kWarmupHops = 5;    // warmup hops before assertions
constexpr int    kWarmup     = kWarmupHops * static_cast<int>(kHopSize);

}  // namespace

// ---------------------------------------------------------------------------
// 1. Silence in -> output finite (may be non-zero due to OLA ring but bounded).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("PartialMachine: silence in -> output finite and near-zero")
{
    GlobalEnv::ResetPerTest();

    PartialMachine pm;
    PartialMachine::Input inp = MakeBasicInput();

    const int N = kWarmup + 256;
    std::vector<float> ch0(static_cast<size_t>(N));

    for (int i = 0; i < N; ++i)
    {
        QuadFloat in(0.0f, 0.0f, 0.0f, 0.0f);
        QuadFloat out = pm.Process(in, inp);
        ch0[static_cast<size_t>(i)] = out[0];
    }

    TestNan::AssertClean(ch0.data(), ch0.size());

    // After warmup with silence in, output should be near zero (no atoms).
    //
    for (int i = kWarmup; i < N; ++i)
    {
        DOCTEST_INFO("silence@" << i << "=" << ch0[static_cast<size_t>(i)]);
        DOCTEST_CHECK(std::abs(ch0[static_cast<size_t>(i)]) < 0.01f);
    }
}

// ---------------------------------------------------------------------------
// 2. Sine input -> output is finite, bounded, not silent after latency.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("PartialMachine: sine input -> non-silent output, finite")
{
    GlobalEnv::ResetPerTest();

    PartialMachine pm;
    PartialMachine::Input inp = MakeBasicInput();

    // 440 Hz sine at 48 kHz, amplitude 0.5.
    //
    const double sampleRate = static_cast<double>(SampleTimer::x_sampleRate);
    TestSignal::Sine sine(440.0, sampleRate, 0.5f);

    const int N = kWarmup + 2 * static_cast<int>(kHopSize);
    std::vector<float> ch0(static_cast<size_t>(N));

    for (int i = 0; i < N; ++i)
    {
        float s = sine.Next();
        QuadFloat in(s, s, s, s);
        QuadFloat out = pm.Process(in, inp);
        ch0[static_cast<size_t>(i)] = out[0];
    }

    TestNan::AssertClean(ch0.data(), ch0.size());

    // Post-warmup: at least some non-zero output.
    //
    float rms = 0.0f;
    int count = 0;
    for (int i = kWarmup; i < N; ++i)
    {
        float v = ch0[static_cast<size_t>(i)];
        rms += v * v;
        ++count;
    }
    rms = std::sqrt(rms / std::max(count, 1));
    DOCTEST_INFO("PartialMachine sine RMS post-warmup=" << rms);
    DOCTEST_CHECK(rms > 1e-6f);  // not completely silent

    // Bounded.
    //
    for (size_t i = 0; i < ch0.size(); ++i)
    {
        DOCTEST_CHECK(std::abs(ch0[i]) < 5.0f);
    }
}

// ---------------------------------------------------------------------------
// 3. Stress: random seeded parameter modulation + white-noise input.
//    Assert NaN-clean, bounded output, atom count within x_maxAtoms.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("PartialMachine: stress - random modulation, finite+bounded")
{
    GlobalEnv::ResetPerTest();

    PartialMachine pm;
    TestSignal::WhiteNoise audioNoise(0xDEADBEEF1234ULL);
    TestSignal::WhiteNoise rng(0xFEEDFACE5678ULL);

    // 1 second of audio at 48 kHz.
    //
    const int N = 48000;
    bool anyBad = false;
    float maxAbs = 0.0f;

    for (int i = 0; i < N; ++i)
    {
        // Modulate parameters every hop.
        //
        PartialMachine::Input inp = MakeBasicInput();
        if (i % static_cast<int>(kHopSize) == 0)
        {
            // Randomly scale bwWidth, volume, reductionFeedback.
            //
            float r = rng.Next() * 0.5f + 0.5f;  // [0,1]
            inp.m_synthesisContextInput.m_volume =
                FrequencyDependentParameter::Parameter(r);
            inp.m_synthesisContextInput.m_bwWidth =
                FrequencyDependentParameter::Parameter(1.0f + r * 100.0f);

            // Vary numAtoms between 16 and 64.
            //
            inp.m_spectralModelInput.m_numAtoms =
                16 + static_cast<size_t>(r * 48.0f);
        }

        float s = audioNoise.Next() * 0.3f;
        QuadFloat in(s, s, s, s);
        QuadFloat out = pm.Process(in, inp);

        for (size_t ch = 0; ch < 4; ++ch)
        {
            float v = out[ch];
            if (std::isnan(v) || std::isinf(v))
            {
                anyBad = true;
            }
            else
            {
                maxAbs = std::max(maxAbs, std::abs(v));
            }
        }

        // Probe atom count in bounds.
        //
        size_t atomCount = pm.m_spectralModel.m_atoms.Size();
        DOCTEST_INFO("atom count at i=" << i << " = " << atomCount);
        DOCTEST_CHECK(atomCount <= PartialMachine::SpectralModel::x_maxAtoms);
    }

    DOCTEST_INFO("stress maxAbs=" << maxAbs);
    DOCTEST_CHECK(!anyBad);
    DOCTEST_CHECK(maxAbs < 10.0f);
}

// ---------------------------------------------------------------------------
// 4. Few-partial input (two sines): output finite and bounded.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("PartialMachine: two-partial sine input -> finite output")
{
    GlobalEnv::ResetPerTest();

    PartialMachine pm;
    PartialMachine::Input inp = MakeBasicInput();

    const double sampleRate = static_cast<double>(SampleTimer::x_sampleRate);
    TestSignal::Sine sine1(440.0,  sampleRate, 0.3f);
    TestSignal::Sine sine2(880.0,  sampleRate, 0.2f);

    const int N = kWarmup + 512;
    std::vector<float> ch0(static_cast<size_t>(N));

    for (int i = 0; i < N; ++i)
    {
        float s = sine1.Next() + sine2.Next();
        QuadFloat in(s, s, s, s);
        QuadFloat out = pm.Process(in, inp);
        ch0[static_cast<size_t>(i)] = out[0];
    }

    TestNan::AssertClean(ch0.data(), ch0.size());

    for (size_t i = 0; i < ch0.size(); ++i)
    {
        DOCTEST_CHECK(std::abs(ch0[i]) < 5.0f);
    }
}

// ---------------------------------------------------------------------------
// 5. SpectralModel: consumed analysis atoms are not merge candidates.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("SpectralModel: consumed analysis atom is ignored during merge")
{
    using Model = PartialMachine::SpectralModel;

    Model model;
    Model::Input input;
    input.m_slewUpAlpha = FrequencyDependentParameter::Parameter(1.0f);
    input.m_slewDownAlpha = FrequencyDependentParameter::Parameter(1.0f);
    input.m_omegaPortamentoAlpha = FrequencyDependentParameter::Parameter(1.0f);
    input.m_omegaDensity = FrequencyDependentParameter::Parameter(0.01f);

    FrequencyDependentParameter::Index index(0.5f, 0);
    Model::AnalysisAtomArray analysisAtoms;
    analysisAtoms.Add(Model::AnalysisAtom(0.42f, 0.5f, index, false));

    Model::Atom first(0.42f, 0.5f, index, 0.42f, 1.0f, 0.0f);
    model.SearchAndMerge(analysisAtoms, first, input);
    DOCTEST_REQUIRE(analysisAtoms[0].m_analysisMagnitude < 0.0f);

    Model::Atom second(0.421f, 0.5f, index, 0.421f, -0.001f, 0.0f);
    model.SearchAndMerge(analysisAtoms, second, input);

    DOCTEST_CHECK(second.m_analysisMagnitude >= 0.0f);
    DOCTEST_CHECK(second.m_synthesisMagnitude >= 0.0f);
    DOCTEST_CHECK(std::isfinite(second.m_synthesisMagnitude));
}
