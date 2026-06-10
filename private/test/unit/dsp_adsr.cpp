// WP-6: ADSR envelope unit tests.
//
// ADSR is a standalone envelope (no TheoryOfTime). It is driven purely by
// per-sample gate/trig signals and per-sample increment/level parameters.
//
// Tests cover:
//   - Stage shape: attack rises to 1, decay falls to sustain, sustain holds,
//     release falls to 0.
//   - Sustain hold: output clamps at sustainLevel while gate=true.
//   - Gate-off during attack/decay triggers DecayNoGate path to 0.
//   - Re-trigger: new trig while gate resets to Attack.
//   - NaN-clean under extreme parameter values (0, 1, tiny increments).
//
// NOTE: DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES is defined for this target.

#include "doctest.h"

#include <cmath>
#include <vector>
#include <limits>

#include "../support/GlobalEnv.hpp"
#include "../support/NanScan.hpp"

#include "ADSR.hpp"

namespace
{

// Helper: set ADSR input from normalized [0,1] knob values.
// attack/decay/release in [0,1]: 0 = fastest, 1 = slowest.
void SetADSRInput(ADSR::Input& inp, ADSR::InputSetter& setter,
                  float attack, float decay, float sustain, float release,
                  bool gate, bool trig)
{
    setter.Set(attack, decay, sustain, release, inp);
    inp.m_gate = gate;
    inp.m_trig = trig;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Test 1: Full ADSR shape — attack, decay, sustain, release
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("ADSR: full shape — attack rises to 1, decay to sustain, sustain holds, release to 0")
{
    GlobalEnv::ResetPerTest();

    ADSR adsr;
    ADSR::Input inp;
    ADSR::InputSetter setter;

    const float sustainLevel = 0.5f;
    // Fast attack/decay/release so shape completes in hundreds of samples.
    SetADSRInput(inp, setter, 0.0f, 0.0f, sustainLevel, 0.0f, true, true);

    // === Attack phase ===
    float peak = 0.0f;
    const size_t maxAttack = 500;
    std::vector<float> buf;
    buf.reserve(2000);

    // First sample with trig=true starts the Attack state.
    buf.push_back(adsr.Process(inp));
    inp.m_trig = false; // one-shot

    for (size_t i = 1; i < maxAttack && adsr.m_state != ADSR::State::Decay && adsr.m_state != ADSR::State::Sustain; ++i)
    {
        buf.push_back(adsr.Process(inp));
        if (buf.back() > peak) { peak = buf.back(); }
    }

    DOCTEST_CHECK(peak >= 0.99f);  // must reach ~1.0
    DOCTEST_CHECK(adsr.m_state != ADSR::State::Idle);

    // Collect Decay phase
    for (size_t i = 0; i < 500 && adsr.m_state == ADSR::State::Decay; ++i)
    {
        buf.push_back(adsr.Process(inp));
    }

    // Should have moved to Sustain
    DOCTEST_CHECK(adsr.m_state == ADSR::State::Sustain);

    // Output near sustainLevel
    DOCTEST_CHECK(adsr.m_output == doctest::Approx(sustainLevel).epsilon(0.02f));

    // === Sustain phase: hold for 100 samples ===
    for (size_t i = 0; i < 100; ++i)
    {
        float v = adsr.Process(inp);
        buf.push_back(v);
        DOCTEST_CHECK(v == doctest::Approx(sustainLevel).epsilon(0.005f));
    }

    // === Release: drop gate ===
    inp.m_gate = false;
    for (size_t i = 0; i < 500 && adsr.m_state != ADSR::State::Idle; ++i)
    {
        buf.push_back(adsr.Process(inp));
    }

    DOCTEST_CHECK(adsr.m_state == ADSR::State::Idle);
    DOCTEST_CHECK(adsr.m_output == doctest::Approx(0.0f).epsilon(0.01f));

    // NaN-clean over whole run
    TestNan::AssertClean(buf.data(), buf.size());

    // Bounded in [0, 1]
    for (float v : buf)
    {
        DOCTEST_CHECK(v >= -0.001f);
        DOCTEST_CHECK(v <= 1.001f);
    }
}

// ---------------------------------------------------------------------------
// Test 2: Attack time scaling
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("ADSR: faster attack parameter reaches 1 in fewer samples")
{
    auto countSamplesToPeak = [](float attackParam) -> size_t
    {
        GlobalEnv::ResetPerTest();
        ADSR adsr;
        ADSR::Input inp;
        ADSR::InputSetter setter;
        SetADSRInput(inp, setter, attackParam, 0.0f, 0.5f, 0.0f, true, true);

        size_t count = 0;
        adsr.Process(inp);
        inp.m_trig = false;
        ++count;

        for (size_t i = 0; i < 10000; ++i)
        {
            adsr.Process(inp);
            ++count;
            if (adsr.m_state != ADSR::State::Attack) break;
        }
        return count;
    };

    size_t fast = countSamplesToPeak(0.0f);
    size_t slow = countSamplesToPeak(0.6f);
    DOCTEST_CHECK(fast < slow);
    DOCTEST_CHECK(fast * 4 < slow); // meaningfully faster
}

// ---------------------------------------------------------------------------
// Test 3: Sustain level is respected
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("ADSR: sustain level clamps output correctly")
{
    GlobalEnv::ResetPerTest();

    ADSR adsr;
    ADSR::Input inp;
    ADSR::InputSetter setter;

    const float sustainLevel = 0.3f;
    SetADSRInput(inp, setter, 0.0f, 0.0f, sustainLevel, 0.0f, true, true);

    // Run until Sustain
    adsr.Process(inp);
    inp.m_trig = false;
    for (size_t i = 0; i < 1000 && adsr.m_state != ADSR::State::Sustain; ++i)
    {
        adsr.Process(inp);
    }

    DOCTEST_CHECK(adsr.m_state == ADSR::State::Sustain);

    // Sustain should hold at sustainLevel
    for (size_t i = 0; i < 50; ++i)
    {
        float v = adsr.Process(inp);
        DOCTEST_CHECK(v == doctest::Approx(sustainLevel).epsilon(0.005f));
    }
}

// ---------------------------------------------------------------------------
// Test 4: Gate released during attack → skips Sustain → falls to 0
//
// Note: ADSR's DecayNoGate transition happens at the TOP of Process() and then
// the switch runs in the same call, so m_state may transition to Release in
// the same sample. We verify that Sustain is NEVER entered and that the output
// eventually reaches 0 (Idle) without going through Sustain.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("ADSR: gate released during attack skips Sustain and falls to 0")
{
    GlobalEnv::ResetPerTest();

    ADSR adsr;
    ADSR::Input inp;
    ADSR::InputSetter setter;

    // Use slow attack so we can cut the gate mid-attack
    SetADSRInput(inp, setter, 0.7f, 0.0f, 0.5f, 0.0f, true, true);
    adsr.Process(inp);
    inp.m_trig = false;

    // Run 20 samples of attack (with slow attack, output moves very slowly)
    for (size_t i = 0; i < 20; ++i)
    {
        adsr.Process(inp);
    }
    DOCTEST_CHECK(adsr.m_state == ADSR::State::Attack);
    float midAttackVal = adsr.m_output;
    DOCTEST_CHECK(midAttackVal > 0.0f);
    DOCTEST_CHECK(midAttackVal < 1.0f);

    // Drop gate
    inp.m_gate = false;

    // Should reach Idle without going through Sustain.
    // (DecayNoGate may transition to Release in the same Process call, so we
    // only check for Sustain absence and eventual Idle.)
    bool enteredSustain = false;
    bool reachedIdle = false;
    for (size_t i = 0; i < 5000; ++i)
    {
        adsr.Process(inp);
        if (adsr.m_state == ADSR::State::Sustain) enteredSustain = true;
        if (adsr.m_state == ADSR::State::Idle)
        {
            reachedIdle = true;
            break;
        }
    }

    DOCTEST_CHECK(!enteredSustain);
    DOCTEST_CHECK(reachedIdle);
    DOCTEST_CHECK(adsr.m_output == doctest::Approx(0.0f).epsilon(0.01f));
}

// ---------------------------------------------------------------------------
// Test 5: Re-trigger while in Sustain resets to Attack
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("ADSR: re-trigger in Sustain state restarts Attack")
{
    GlobalEnv::ResetPerTest();

    ADSR adsr;
    ADSR::Input inp;
    ADSR::InputSetter setter;

    SetADSRInput(inp, setter, 0.0f, 0.0f, 0.5f, 0.0f, true, true);
    adsr.Process(inp);
    inp.m_trig = false;

    // Run to Sustain
    for (size_t i = 0; i < 1000 && adsr.m_state != ADSR::State::Sustain; ++i)
    {
        adsr.Process(inp);
    }
    DOCTEST_CHECK(adsr.m_state == ADSR::State::Sustain);

    // Re-trigger
    inp.m_trig = true;
    adsr.Process(inp);
    inp.m_trig = false;

    DOCTEST_CHECK(adsr.m_state == ADSR::State::Attack);

    // Should rise to peak again
    float peak = adsr.m_output;
    for (size_t i = 0; i < 500 && adsr.m_state == ADSR::State::Attack; ++i)
    {
        float v = adsr.Process(inp);
        peak = std::max(peak, v);
    }
    DOCTEST_CHECK(peak >= 0.99f);
}

// ---------------------------------------------------------------------------
// Test 6: NaN-clean under extreme parameter values
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("ADSR: NaN-clean under extreme and edge-case parameters")
{
    struct Case { float a, d, s, r; };
    Case cases[] = {
        {0.0f, 0.0f, 0.0f, 0.0f},   // all fastest, sustain=0
        {1.0f, 1.0f, 1.0f, 1.0f},   // all slowest, sustain=1
        {0.0f, 0.0f, 0.5f, 0.0f},   // fast A/D/R, mid sustain
        {1.0f, 0.0f, 0.0f, 1.0f},   // slow A, fast D, no sustain, slow R
        {0.0f, 1.0f, 0.5f, 0.0f},   // fast A, slow D, mid sustain, fast R
    };

    for (auto& c : cases)
    {
        GlobalEnv::ResetPerTest();
        ADSR adsr;
        ADSR::Input inp;
        ADSR::InputSetter setter;

        SetADSRInput(inp, setter, c.a, c.d, c.s, c.r, true, true);
        std::vector<float> buf;
        buf.reserve(600);

        buf.push_back(adsr.Process(inp));
        inp.m_trig = false;

        for (size_t i = 1; i < 300; ++i)
        {
            buf.push_back(adsr.Process(inp));
        }

        // Drop gate for second half
        inp.m_gate = false;
        for (size_t i = 0; i < 300; ++i)
        {
            buf.push_back(adsr.Process(inp));
        }

        TestNan::AssertClean(buf.data(), buf.size());

        for (float v : buf)
        {
            // Note: ADSR Release can overshoot slightly below 0 by up to one
            // releaseIncrement before the clamp fires. We allow a small negative
            // margin proportional to the max decrement (1/(48000*decayTimeMin)).
            DOCTEST_CHECK(v >= -0.01f);
            DOCTEST_CHECK(v <= 1.001f);
        }
    }
}

// ---------------------------------------------------------------------------
// Test 7: Denormal-tiny increment values (attack knob near 1.0 = very slow)
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("ADSR: NaN-clean with near-zero attack increment (attack knob = 0.999)")
{
    GlobalEnv::ResetPerTest();

    ADSR adsr;
    ADSR::Input inp;
    ADSR::InputSetter setter;

    // attack=0.999 => attackIncrement near minimum => very small per-sample step
    SetADSRInput(inp, setter, 0.999f, 0.0f, 0.5f, 0.0f, true, true);

    std::vector<float> buf;
    buf.reserve(500);
    buf.push_back(adsr.Process(inp));
    inp.m_trig = false;
    for (size_t i = 1; i < 500; ++i)
    {
        buf.push_back(adsr.Process(inp));
    }

    TestNan::AssertClean(buf.data(), buf.size());

    // Output should be increasing (attack is just very slow)
    for (size_t i = 1; i < buf.size(); ++i)
    {
        DOCTEST_CHECK(buf[i] >= buf[i - 1] - 0.0001f);
    }
}
