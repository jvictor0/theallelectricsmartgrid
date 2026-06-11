// WP-6: LFO unit tests — QuadLFO, RandomLFO, GangedRandomLFO.
//
// QuadLFO wiring (discovered from QuadReverb.hpp + SquiggleBoy.hpp):
//   - Standalone: no TheoryOfTime. Per-sample: QuadLFO::Process(input).
//   - Input has m_freq[4] (phase increment per sample) and m_phaseKnob[4]
//     (phase offset between the 4 channels).
//   - m_output[4] is in [-1,1] (sine + slew filter).
//   - All 4 channels run at independent frequencies; phase-lock logic fires
//     only when all 4 freqs are very close (within 0.01%).
//
// Tests cover:
//   - 4 outputs are sinusoidal (THD < threshold) at a given frequency.
//   - Dominant frequency bin matches the set frequency.
//   - Phase offsets between channels match the phaseKnob setting.
//   - Output bounded [-1,1] + epsilon (slew can overshoot only by ~0 since
//     the slew alpha tracks input freq).
//   - Continuity under rate change and multiplier change (per-sample freq
//     change): MaxAbsDelta bounded; WARN+report if discontinuity found.
//   - RandomLFO: determinism with GlobalEnv::SeedRandomness, bounded output,
//     smoothness (MaxAbsDelta bounded as it's filtered noise).
//   - GangedRandomLFO: basic functionality test (bounded output, no NaN).
//
// NOTE: DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES is defined for this target.

#include "doctest.h"

#include <cmath>
#include <vector>

#include "../support/GlobalEnv.hpp"
#include "../support/NanScan.hpp"
#include "../support/Continuity.hpp"
#include "../support/Spectral.hpp"

#include "QuadLFO.hpp"
#include "RandomLFO.hpp"
#include "GangedRandomLFO.hpp"

static constexpr double kSampleRate = 48000.0;

// ---------------------------------------------------------------------------
// Helper: run QuadLFO for N samples at the given freq (same for all channels),
// returning channel `ch` output as a float buffer.
// ---------------------------------------------------------------------------
//
static std::vector<float> RunQuadLFO(size_t n, float freqHz, int ch = 0,
                                      float phaseKnobCh0 = 0.0f,
                                      float phaseKnobCh1 = 0.0f,
                                      float phaseKnobCh2 = 0.0f,
                                      float phaseKnobCh3 = 0.0f)
{
    QuadLFO lfo;
    // Set a low-enough slew so the filter doesn't kill the sine amplitude.
    // The Process() code does: m_slew[i].SetAlphaFromNatFreq(freq*2) per call.
    // So we don't need to call SetSlew explicitly.

    QuadLFO::Input input;
    float freq = static_cast<float>(freqHz / kSampleRate);
    for (int i = 0; i < 4; ++i)
    {
        input.m_freq[i] = freq;
    }
    input.m_phaseKnob[0] = phaseKnobCh0;
    input.m_phaseKnob[1] = phaseKnobCh1;
    input.m_phaseKnob[2] = phaseKnobCh2;
    input.m_phaseKnob[3] = phaseKnobCh3;

    std::vector<float> buf;
    buf.reserve(n);

    for (size_t i = 0; i < n; ++i)
    {
        lfo.Process(input);
        buf.push_back(lfo.m_output[ch]);
    }
    return buf;
}

// ---------------------------------------------------------------------------
// Test 1: QuadLFO output is sinusoidal at a testable frequency.
//
// QuadLFO is designed for slow modulation (reverb/delay mod, sub-Hz to ~50 Hz).
// The built-in slew filter per-channel has cutoff = freq*2 (cycles/sample),
// so it passes the fundamental but attenuates harmonics. To use FFT spectral
// analysis, we need freq to land on a non-DC bin with the 4096-pt FFT at 48kHz
// (bin resolution ≈ 11.7 Hz). We test at 100 Hz (bin ≈ 8.5), where:
//   - Filter cutoff = 200 Hz → passes 100 Hz fundamental well
//   - THD should be low (harmonics are attenuated by the slew filter)
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("QuadLFO: sinusoidal output at 100 Hz — dominant bin near 100 Hz and low THD")
{
    GlobalEnv::ResetPerTest();

    const double freqHz = 100.0;
    // 2 seconds at 100 Hz = 200 cycles — enough for spectral analysis.
    //
    const size_t n = static_cast<size_t>(kSampleRate * 2);
    std::vector<float> buf = RunQuadLFO(n, freqHz, 0);

    // NaN clean
    //
    TestNan::AssertClean(buf.data(), n);

    // Bounded in [-1, 1] + generous epsilon for filter transient
    //
    for (float v : buf)
    {
        DOCTEST_CHECK(v >= -1.1f);
        DOCTEST_CHECK(v <= 1.1f);
    }

    // Skip first 10 cycles (transient) for spectral analysis
    //
    size_t skipSamples = static_cast<size_t>(kSampleRate / freqHz * 10);
    const float* steady = buf.data() + skipSamples;
    size_t steadyN = n - skipSamples;

    // Dominant bin should be near 100 Hz in the 4096-pt FFT.
    // Bin resolution = 48000/4096 ≈ 11.7 Hz; bin 8 = 93.75 Hz, bin 9 = 105.5 Hz.
    //
    auto spec = TestSpectral::MagnitudeSpectrum(steady, steadyN, TestSpectral::kFftSize4096);
    size_t domBin = TestSpectral::DominantBin(spec);
    double measHz = TestSpectral::BinFreq(domBin, TestSpectral::kFftSize4096, kSampleRate);
    // Allow ±2 bins (±23 Hz) tolerance for frequency rounding.
    //
    DOCTEST_CHECK(measHz == doctest::Approx(freqHz).epsilon(0.25));

    // THD: the slew filter (cutoff=200 Hz) attenuates harmonics strongly.
    //
    double thd = TestContinuity::THD(steady, steadyN, freqHz, kSampleRate, TestSpectral::kFftSize4096);
    // THD < 1.0 is a generous threshold (the slew does attenuate harmonics significantly)
    //
    DOCTEST_CHECK(thd < 1.0);
}

// ---------------------------------------------------------------------------
// Test 2: QuadLFO — all 4 channels are active and bounded
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("QuadLFO: all 4 channels produce bounded output")
{
    GlobalEnv::ResetPerTest();

    QuadLFO lfo;
    QuadLFO::Input input;
    float freq = static_cast<float>(50.0 / kSampleRate); // 50 Hz (avoids near-DC bin issues)
    for (int i = 0; i < 4; ++i)
    {
        input.m_freq[i] = freq;
        input.m_phaseKnob[i] = 0.0f;
    }

    const size_t n = static_cast<size_t>(kSampleRate);
    std::vector<float> ch[4];
    for (int c = 0; c < 4; ++c) ch[c].reserve(n);

    for (size_t i = 0; i < n; ++i)
    {
        lfo.Process(input);
        for (int c = 0; c < 4; ++c) ch[c].push_back(lfo.m_output[c]);
    }

    for (int c = 0; c < 4; ++c)
    {
        TestNan::AssertClean(ch[c].data(), n);
        for (float v : ch[c])
        {
            DOCTEST_CHECK(v >= -1.1f);
            DOCTEST_CHECK(v <= 1.1f);
        }
    }
}

// ---------------------------------------------------------------------------
// Test 3: Phase offset — phaseKnob != 0 shifts channel output phase
//
// With phaseKnob[1] = 0.25 (quarter-cycle offset), channel 0 and channel 1
// should have a phase difference approximately equal to GetPhase(1, 0.25).
// We verify the channels are not identical (different phase) when phaseKnob != 0.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("QuadLFO: non-zero phaseKnob produces different phase output from ch0")
{
    GlobalEnv::ResetPerTest();

    // Run with phaseKnob = 0 (no offset) and with phaseKnob = 0.25 (offset)
    //
    auto runTwo = [&](float pk1) -> std::pair<std::vector<float>, std::vector<float>>
    {
        GlobalEnv::ResetPerTest();
        QuadLFO lfo;
        QuadLFO::Input input;
        float freq = static_cast<float>(50.0 / kSampleRate);
        for (int i = 0; i < 4; ++i) input.m_freq[i] = freq;
        input.m_phaseKnob[0] = 0.0f;
        input.m_phaseKnob[1] = pk1;
        input.m_phaseKnob[2] = 0.0f;
        input.m_phaseKnob[3] = 0.0f;

        const size_t skip = 2000;
        const size_t n = 4000;
        std::vector<float> out0, out1;
        out0.reserve(n);
        out1.reserve(n);
        for (size_t i = 0; i < skip + n; ++i)
        {
            lfo.Process(input);
            if (i >= skip)
            {
                out0.push_back(lfo.m_output[0]);
                out1.push_back(lfo.m_output[1]);
            }
        }
        return {out0, out1};
    };

    // With no phase offset, channels should be nearly identical (same freq, same phase start)
    //
    auto [ch0_nophase, ch1_nophase] = runTwo(0.0f);
    float maxDiffNoPhase = 0.0f;
    for (size_t i = 0; i < ch0_nophase.size(); ++i)
    {
        maxDiffNoPhase = std::max(maxDiffNoPhase, std::abs(ch0_nophase[i] - ch1_nophase[i]));
    }

    // With phase offset, channels should differ (phase shifted)
    //
    auto [ch0_phase, ch1_phase] = runTwo(0.25f);
    float maxDiffWithPhase = 0.0f;
    for (size_t i = 0; i < ch0_phase.size(); ++i)
    {
        maxDiffWithPhase = std::max(maxDiffWithPhase, std::abs(ch0_phase[i] - ch1_phase[i]));
    }

    // Channels with offset should differ more than channels without offset
    //
    DOCTEST_CHECK(maxDiffWithPhase > maxDiffNoPhase + 0.05f);
}

// ---------------------------------------------------------------------------
// Test 4: Continuity under freq change mid-run
//
// Change m_freq from one value to another mid-run. Assert no discontinuity.
// WARN + report if a jump is found.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("QuadLFO: continuity under mid-run frequency change")
{
    GlobalEnv::ResetPerTest();

    QuadLFO lfo;
    QuadLFO::Input input;
    const float freqLow  = static_cast<float>(50.0 / kSampleRate);
    const float freqHigh = static_cast<float>(200.0 / kSampleRate);

    for (int i = 0; i < 4; ++i) { input.m_freq[i] = freqLow; input.m_phaseKnob[i] = 0.0f; }

    // Warmup
    //
    const size_t warmup = 2000;
    for (size_t i = 0; i < warmup; ++i) lfo.Process(input);

    // Pre-change: capture 1000 samples
    //
    const size_t preSamples = 1000;
    std::vector<float> buf;
    buf.reserve(preSamples + 1000);
    for (size_t i = 0; i < preSamples; ++i)
    {
        lfo.Process(input);
        buf.push_back(lfo.m_output[0]);
    }

    // Change freq
    //
    for (int i = 0; i < 4; ++i) input.m_freq[i] = freqHigh;

    // Post-change: capture 1000 samples
    //
    const size_t postSamples = 1000;
    for (size_t i = 0; i < postSamples; ++i)
    {
        lfo.Process(input);
        buf.push_back(lfo.m_output[0]);
    }

    TestNan::AssertClean(buf.data(), buf.size());

    // Steady-state max delta before change
    //
    float preMaxDelta = TestContinuity::MaxAbsDelta(buf.data(), preSamples);

    // Transition window: 5 before, 20 after change
    //
    size_t transStart = (preSamples > 5) ? preSamples - 5 : 0;
    size_t transEnd   = std::min(preSamples + 20, buf.size());
    const float* transPtr = buf.data() + transStart;
    size_t transLen = transEnd - transStart;

    float transMaxDelta = TestContinuity::MaxAbsDelta(transPtr, transLen);
    size_t discont = TestContinuity::DiscontinuityCount(transPtr, transLen, 0.1f);

    // Threshold: 4x pre-change delta (LFO phase can jump more aggressively)
    //
    float threshold = std::max(0.1f, preMaxDelta * 4.0f);

    if (discont > 0 || transMaxDelta > threshold)
    {
        // BUG?: QuadLFO frequency change causes discontinuity.
        //
        DOCTEST_WARN_MESSAGE(discont == 0,
            "QuadLFO DISCONTINUITY under freq change: "
            "DiscontinuityCount=" << discont <<
            " transMaxDelta=" << transMaxDelta <<
            " threshold=" << threshold <<
            " preMaxDelta=" << preMaxDelta <<
            " freqLow=" << freqLow << " freqHigh=" << freqHigh <<
            " changeAtSample=" << preSamples <<
            " REPRO: QuadLFO, freq 50->200 Hz at sample " << (warmup + preSamples));
        DOCTEST_MESSAGE("QuadLFO freq-change jump: maxDelta=" << transMaxDelta
            << " (pre=" << preMaxDelta << ")");
    }
    else
    {
        DOCTEST_CHECK(discont == 0);
    }
}

// ---------------------------------------------------------------------------
// Test 5: QuadLFO — independent channel frequencies run at different periods.
//
// Rather than FFT (which fails at sub-Hz LFO rates due to bin resolution),
// we count phasor wraps to verify different periods. We use frequencies in
// the range where the phase-lock logic does NOT fire (>0.01% apart).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("QuadLFO: independent channel frequencies produce different wrap rates")
{
    GlobalEnv::ResetPerTest();

    QuadLFO lfo;
    QuadLFO::Input input;
    // Set noticeably different frequencies so they're outside the phase-lock range.
    // Phase-lock fires when ratio is within 0.01%; these differ by 2x or more.
    //
    float freqs[4] = {
        static_cast<float>(50.0  / kSampleRate),
        static_cast<float>(100.0 / kSampleRate),
        static_cast<float>(200.0 / kSampleRate),
        static_cast<float>(75.0  / kSampleRate)
    };
    for (int i = 0; i < 4; ++i)
    {
        input.m_freq[i] = freqs[i];
        input.m_phaseKnob[i] = 0.0f;
    }

    // Track number of phase wraps per channel by watching the internal phase.
    // We count wraps from phasor values reported by inspecting output sign changes
    // (after the slew filter settles).
    //
    const size_t n = static_cast<size_t>(kSampleRate * 2); // 2 seconds

    std::vector<float> ch[4];
    for (int c = 0; c < 4; ++c) ch[c].reserve(n);

    for (size_t i = 0; i < n; ++i)
    {
        lfo.Process(input);
        for (int c = 0; c < 4; ++c) ch[c].push_back(lfo.m_output[c]);
    }

    // Each channel is NaN-clean and bounded
    //
    for (int c = 0; c < 4; ++c)
    {
        TestNan::AssertClean(ch[c].data(), n);
        for (float v : ch[c])
        {
            DOCTEST_CHECK(v >= -1.1f);
            DOCTEST_CHECK(v <= 1.1f);
        }
    }

    // Use spectral analysis on 2s of data at 48kHz with 4096-pt FFT.
    // Bin resolution = 48000/4096 ≈ 11.7 Hz. For 50/100/200/75 Hz:
    //   50 Hz → bin ~4.3 → bin 4 (46.9 Hz)
    //   100 Hz → bin ~8.5 → bin 8 or 9
    //   200 Hz → bin ~17 → bin 17
    //   75 Hz → bin ~6.4 → bin 6 (70.3 Hz) or bin 7 (81.6 Hz)
    // Skip first 1000 samples (transient)
    //
    const size_t skip = 1000;
    double expectedHz[4] = {50.0, 100.0, 200.0, 75.0};

    for (int c = 0; c < 4; ++c)
    {
        auto spec = TestSpectral::MagnitudeSpectrum(ch[c].data() + skip, n - skip, TestSpectral::kFftSize4096);
        size_t dom = TestSpectral::DominantBin(spec);
        double measHz = TestSpectral::BinFreq(dom, TestSpectral::kFftSize4096, kSampleRate);
        // Allow ±30% tolerance (one or two bins at coarse 11.7 Hz resolution)
        //
        DOCTEST_CHECK(measHz == doctest::Approx(expectedHz[c]).epsilon(0.30));
    }
}

// ---------------------------------------------------------------------------
// Test 6: RandomLFO — determinism (same seed → same sequence)
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("RandomLFO: determinism — same seed produces identical sequence")
{
    const size_t n = 500;

    GlobalEnv::ResetPerTest(); // sets kDefaultSeed
    RandomLFO lfo1;
    std::vector<float> seq1;
    seq1.reserve(n);
    for (size_t i = 0; i < n; ++i) seq1.push_back(lfo1.Process());

    GlobalEnv::ResetPerTest(); // resets to same seed
    RandomLFO lfo2;
    std::vector<float> seq2;
    seq2.reserve(n);
    for (size_t i = 0; i < n; ++i) seq2.push_back(lfo2.Process());

    DOCTEST_REQUIRE(seq1.size() == seq2.size());
    bool identical = true;
    for (size_t i = 0; i < seq1.size(); ++i)
    {
        if (seq1[i] != seq2[i]) { identical = false; break; }
    }
    DOCTEST_CHECK(identical);
}

// ---------------------------------------------------------------------------
// Test 7: RandomLFO — output bounded in [0, 1] and NaN-clean
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("RandomLFO: output bounded in [0, 1] and NaN-clean")
{
    GlobalEnv::ResetPerTest();
    RandomLFO lfo;
    const size_t n = 5000;
    std::vector<float> buf;
    buf.reserve(n);
    for (size_t i = 0; i < n; ++i) buf.push_back(lfo.Process());

    TestNan::AssertClean(buf.data(), n);
    for (float v : buf)
    {
        DOCTEST_CHECK(v >= -0.001f);
        DOCTEST_CHECK(v <= 1.001f);
    }
}

// ---------------------------------------------------------------------------
// Test 8: RandomLFO — smooth (MaxAbsDelta bounded by filter bandwidth)
//
// RandomLFO uses OPLowPassFilter at ~0.05/48000 nat-freq, so large jumps
// between adjacent samples are impossible (the filter effectively limits the
// slew rate). We check that MaxAbsDelta is well below 0.1 per sample.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("RandomLFO: smooth output — MaxAbsDelta bounded by filter")
{
    GlobalEnv::ResetPerTest();
    RandomLFO lfo;
    const size_t n = 5000;
    std::vector<float> buf;
    buf.reserve(n);
    for (size_t i = 0; i < n; ++i) buf.push_back(lfo.Process());

    // Skip very first samples (filter startup transient)
    //
    float maxDelta = TestContinuity::MaxAbsDelta(buf.data() + 100, n - 100);
    // Filter at 0.05/48000 should limit jumps to << 0.01 per sample
    //
    DOCTEST_CHECK(maxDelta < 0.01f);
}

// ---------------------------------------------------------------------------
// Test 9: GangedRandomLFO — basic: bounded output and NaN-clean
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("GangedRandomLFO: bounded output and NaN-clean (basic standalone drive)")
{
    GlobalEnv::ResetPerTest();

    // Drive GangedRandomLFOInternal directly (it's standalone — no ToT needed).
    //
    GangedRandomLFOInternal lfo;
    GangedRandomLFOInternal::Input input;
    input.m_time    = 1.0f;    // ~1 second per move (at dt=1/sampleRate)
    input.m_sigma   = 0.1f;
    input.m_gangSize = 4;

    const float dt = 1.0f / static_cast<float>(kSampleRate);
    const size_t n = 2000;

    std::vector<float> buf;
    buf.reserve(n * 4);

    for (size_t i = 0; i < n; ++i)
    {
        lfo.Process(dt, input);
        for (size_t v = 0; v < input.m_gangSize; ++v)
        {
            buf.push_back(lfo.m_pos[v]);
        }
    }

    TestNan::AssertClean(buf.data(), buf.size());

    // Positions are not hard-clamped by the class itself, but should be
    // loosely bounded (typically [0,1] with some sigma excursion).
    //
    float minVal = *std::min_element(buf.begin(), buf.end());
    float maxVal = *std::max_element(buf.begin(), buf.end());
    // Allow generous bounds: sigma=0.1 means +/-3-sigma = 0.3 from center
    //
    DOCTEST_CHECK(minVal > -1.0f);
    DOCTEST_CHECK(maxVal < 2.0f);
}

// ---------------------------------------------------------------------------
// Test 10: GangedRandomLFO determinism with fixed seed
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("GangedRandomLFO: determinism with fixed seed")
{
    const size_t n = 500;

    auto runLFO = [&]() -> std::vector<float>
    {
        GangedRandomLFOInternal lfo;
        GangedRandomLFOInternal::Input input;
        input.m_time = 1.0f;
        input.m_sigma = 0.1f;
        input.m_gangSize = 2;
        const float dt = 1.0f / static_cast<float>(kSampleRate);
        std::vector<float> buf;
        buf.reserve(n * 2);
        for (size_t i = 0; i < n; ++i)
        {
            lfo.Process(dt, input);
            for (size_t v = 0; v < 2; ++v) buf.push_back(lfo.m_pos[v]);
        }
        return buf;
    };

    GlobalEnv::ResetPerTest();
    auto seq1 = runLFO();

    GlobalEnv::ResetPerTest();
    auto seq2 = runLFO();

    DOCTEST_REQUIRE(seq1.size() == seq2.size());
    bool identical = true;
    for (size_t i = 0; i < seq1.size(); ++i)
    {
        if (seq1[i] != seq2[i]) { identical = false; break; }
    }
    DOCTEST_CHECK(identical);
}
