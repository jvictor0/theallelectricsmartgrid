# Absolute Time Coordinates Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Execution is not authorized by creation of this plan; follow the user's implementation request.

**Goal:** Make Theory of Time and its consumers use absolute coordinates, enabling fractional-speed playback across source cycles while preserving loop-periodic LFOs and trigger-captured AHD timing.

**Architecture:** Store unmodulated/modulated global double phases and evaluate loop phases and signed lattice positions from accepted topology. Accept parent/multiplier edits at simultaneous old/new parent cycle boundaries. Consumers wrap at output; AHD captures a ratio and origin instead of following a loop.

**Tech Stack:** C++17 headers, doctest, CMake standalone tests, JUCE/Xcode application, OpenSpec.

## Execution record

Implementation is complete in the working diff. Core tests are in `theory_of_time_absolute.cpp`; arithmetic, playback, LFO, MIDI/scope, recording, and sync regressions share `absolute_time_consumers.cpp`. Envelope/gate regressions are in `absolute_time_envelopes.cpp` and the existing DSP suites. The core no longer needs a separate `Preprocess` call.

Behavioral red/green checks included absolute core queries, stopped slot 0, negative/wide arp indices, and negative MIDI tick boundaries. The consumer migration removed all old query bridges and the now-unused circle trackers. The arp triangle fold was corrected to Euclidean modulo two so signed/wide page positions remain bounded.

A fresh core/consumer review and a separate AHD/gate review found no actionable regressions. Full-suite verification finds two preexisting startup-silence failures, reproduced from unchanged HEAD with the identical output peak (`0.000655346`). The clock is frozen in both versions. Verification counts are recorded in the OpenSpec task checklist after the final run.

The recipe below records the intended implementation steps. Completion is tracked in `openspec/changes/absolute-time-coordinates/tasks.md`. Per-task commits were consolidated into the final reviewable working diff; this work does not land or archive the change.

## Global Constraints

- Design authority: `openspec/changes/absolute-time-coordinates/design.md`; behavioral contracts: its six delta specs. Paths in this plan are relative to the repository root.
- Global loop is index 5 (`x_globalLoop`). Phase is absolute double cycles. Position is signed `int64_t` common-lattice ticks. Domains are `PhaseDomain::Unmodulated` and `PhaseDomain::Modulated`.
- Preserve existing transport/MIDI behavior, serialized keys, waveform controls, reverse-time semantics, and audio-thread allocation/locking discipline.
- Leave the existing unmodulated/modulated topology-boundary mismatch and PolyXFader automatic amplitude weights/slew unchanged.
- Active AHD captures source cycle ratio AND envelope period at trigger. No source-loop identity or topology reanchoring remains in the active envelope.
- Follow AGENTS.md: matched braces with opening brace on a new line; `m_` members, `x_` constants; HammerCase functions and enum values; structs, public members, explicit C++ casts; comments terminate with a separate `//` line; blank line after closing braces except adjacent closing braces or matching else.
- Use the existing managed worktree. Before execution, inspect current Git state; do not develop on main or create a redundant manual worktree. Stop and escalate broken agentic infrastructure.

## Build and verification commands

Configure a fresh standalone build so the test command matches the current CMake target, rather than the obsolete `private/build/tests` path in older plans:

```bash
cmake -S private/test -B /tmp/absolute-time-build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/absolute-time-build --target smartgrid_tests -j 4
/tmp/absolute-time-build/smartgrid_tests
```

Focused tests added by this plan use the prefix `AbsoluteTime:`. Run them with:

```bash
/tmp/absolute-time-build/smartgrid_tests --test-case='AbsoluteTime:*'
```

Final app compilation, without launching, deploying, or requesting provisioning updates:

```bash
xcodebuild -project JUCE/SmartGridOne/Builds/MacOSX/SmartGridOne.xcodeproj -scheme 'SmartGridOne - App' -configuration Debug -destination 'generic/platform=macOS' -derivedDataPath /tmp/absolute-time-app-build CODE_SIGNING_ALLOWED=NO build
```

## Task 1: Signed coordinate arithmetic and index consumers

**Files:** modify `private/src/PhaseUtils.hpp`, `private/src/IndexArp.hpp`, `private/src/TheNonagon.hpp`; create `private/test/unit/absolute_time_consumers.cpp`.

**Interfaces:** produce `PhaseUtils::FloorDiv(int64_t value, int64_t divisor)` and `PhaseUtils::FloorMod(int64_t value, int64_t divisor)` for positive divisors. Widen `IndexArp` total/motive coordinates, input arrays, and page-index arguments to `int64_t`; keep bounded rhythm indices suitable for array access.

- Write arithmetic and real IndexArp regressions, including these exact expectations:

```cpp
DOCTEST_TEST_CASE("AbsoluteTime: signed coordinate arithmetic")
{
    DOCTEST_CHECK(PhaseUtils::FloorDiv(-1, 8) == -1);
    DOCTEST_CHECK(PhaseUtils::FloorMod(-1, 8) == 7);
    DOCTEST_CHECK(PhaseUtils::FloorDiv(-8, 8) == -1);
    DOCTEST_CHECK(PhaseUtils::FloorMod(-8, 8) == 0);
    DOCTEST_CHECK(PhaseUtils::FloorDiv(4294967299LL, 8) == 536870912LL);
    DOCTEST_CHECK(PhaseUtils::FloorMod(4294967299LL, 8) == 3);
}

DOCTEST_TEST_CASE("AbsoluteTime: arp negative and wide step indices")
{
    IndexArp arp;
    IndexArp::Input input;
    input.m_clock = true;
    input.m_rhythmLength = 8;
    input.m_totalIndex = -1;
    arp.Process(input);
    DOCTEST_CHECK(arp.m_rhythmIndex == 7);
    DOCTEST_CHECK(arp.m_motiveIndex == -1);
    input.m_totalIndex = 4294967299LL;
    arp.Process(input);
    DOCTEST_CHECK(arp.m_rhythmIndex == 3);
    DOCTEST_CHECK(arp.m_motiveIndex == 536870912LL);
}
```

- Build/run the focused cases and record their failing assertions or missing helper/type failure before implementing.
- Implement floor arithmetic with positive-divisor assertions. Use division/remainder without negating the numerator (so minimum signed values are safe):

```cpp
int64_t quotient = value / divisor;
int64_t remainder = value % divisor;
return quotient - (remainder < 0 ? 1 : 0);
```

For modulo return `remainder < 0 ? remainder + divisor : remainder`. Propagate index width through input arrays and output page calculations; use double until the final bounded float output rather than multiplying a wide page coordinate in float. Preserve retro, inversion, cycle folding, and zone mapping.
- Run the arithmetic/arp cases and existing sequencer tests; commit the completed arithmetic/index migration after successful verification.

## Task 2: Absolute core, aligned topology edits, and canonical API

**Files:** create `private/src/TheoryOfTimeBase.hpp`, `private/test/unit/theory_of_time_absolute.cpp`, modify `private/src/TheoryOfTime.hpp`, `private/src/PolyXFader.hpp`, `private/test/support/TimeRig.hpp`, `private/test/unit/time_rig.cpp`, and time API callers in `private/src` and `private/test`.

**Interfaces:** the six query signatures in design.md are authoritative. Rename base input phase to `m_unmodulatedPhase`, retain `m_phaseOffset`, and expose `TheoryOfTimeBase::x_globalLoop`. Use the base processing entry points `Process(size_t, const Input&)` and `RolloverMicroblockBuffer()` so deterministic core tests can supply absolute inputs independently of the running oscillator.

- Introduce this small test fixture, then write core regressions against the new API:

```cpp
struct AbsoluteTimeRig
{
    TheoryOfTimeBase m_time;
    TheoryOfTimeBase::Input m_input;
    size_t m_sampleIndex = 0;

    AbsoluteTimeRig()
    {
        m_input.m_running = true;
        for (size_t i = 0; i < TheoryOfTimeBase::x_globalLoop; ++i)
        {
            m_input.m_input[i].m_parentIndex = TheoryOfTimeBase::x_globalLoop;
            m_input.m_input[i].m_parentMult = 1;
        }
    }

    void Step(double phase)
    {
        if (m_sampleIndex == TheoryOfTimeBase::x_microBlockSize)
        {
            m_time.RolloverMicroblockBuffer();
            m_sampleIndex = 0;
        }

        ++m_sampleIndex;
        m_input.m_unmodulatedPhase = phase;
        m_time.Process(m_sampleIndex, m_input);
    }
};
```

Core assertion seeds:

```cpp
AbsoluteTimeRig rig;
rig.m_input.m_input[4].m_parentMult = 3;
rig.Step(2.25);
DOCTEST_CHECK(rig.m_time.GetPhase(4, rig.m_sampleIndex, PhaseDomain::Modulated) == doctest::Approx(6.75));
DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, rig.m_sampleIndex) == 13);
DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, rig.m_sampleIndex, 5) == 1);
rig.Step(-0.25);
DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, rig.m_sampleIndex) == -2);
DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, rig.m_sampleIndex, 5) == 4);
```

The reparent regression starts loop 3 under loop 4, with loop 4 at ratio 2 to global. Request loop 3 parent 5 plus a new multiplier. Drive global phase from 0.49 to 0.51: old parent crosses, new does not, and the pair MUST remain unchanged. Drive to 0.99 then 1.01: both cross, so the pair MUST be accepted. Repeat for reverse motion and simultaneous ancestor/descendant requests using a pre-edit snapshot. Assert the accepted fields, not only a pending input or LED.

- Run those cases red, then extract the core and implement absolute phase/position derivation, Euclidean gates/indices, explicit startup/stop behavior, and the shared API. Delete global CircleTracker and per-loop winding. Derive child phases from global phase and cycle ratio; do not retain duplicate child coordinates solely for convenience.
- Implement topology acceptance as two passes: snapshot all eligibility under old topology, then apply eligible parent/multiplier pairs and recompute the lattice once. Derive previous/current comparison coordinates in common units so a remap is not counted as travel.
- Implement GetPhase by interpolating global phase before multiplying by the interval's ratio. Add assertions for ordinary 0.99-to-1.01 midpoint, [7,8] interpolation/rollover, negative phase, multi-cycle seeks, and a topology edit at a nonzero integer global phase. Do not interpolate through a child jump such as 20-to-30.
- Migrate call signatures and domain enums together with removal of bridge functions. During this task, wrap at old consumer boundaries where needed to keep their behavior buildable; Tasks 3 and 4 remove redundant reconstruction in their own consumers. Do not retain old time API names as cross-task compatibility aliases.
- Run core, existing TimeRig, MIDI, recording, and topology tests. Resolve expected old-representation assertions by replacing them with new observable contracts, never weakening assertions. Commit this coherent core migration.

## Task 3: Playback and PolyXFader output semantics

**Files:** modify `private/src/PhasorPlayHead.hpp`, `private/src/PolyXFader.hpp`, `private/src/TapeHead.hpp`, `private/src/QuadDelay.hpp`, `private/src/DualSampleSource.hpp`; create `private/test/unit/absolute_time_consumers.cpp`; extend `private/test/unit/dsp_samplesource.cpp`.

**Interfaces:** consume `GetPhase(loop, sample, domain)`, `GetCycleRatio(loop, sample)`, and `CrossedCycleBoundary(loop, sample, domain)`. PhasorPlayHead's public input controls and normalized return value remain compatible.

- Write the motivating real-playhead test before editing playback arithmetic:

```cpp
GlobalEnv::ResetPerTest();
TheoryOfTime time;
TheoryOfTimeBase::Input clockInput;
clockInput.m_running = true;
clockInput.m_unmodulatedPhase = 1.5;
time.TheoryOfTimeBase::Process(1, clockInput);
PhasorPlayHead head;
PhasorPlayHead::Input input;
input.m_theoryOfTime = &time;
input.m_loopIndex = TheoryOfTimeBase::x_globalLoop;
input.m_sampleIndex = 1;
input.m_start = 0.0f;
input.m_length = 1.0f;
input.m_speed = 0.5f;
DOCTEST_CHECK(head.Process(input) == doctest::Approx(0.75));
input.m_speed = -0.5f;
DOCTEST_CHECK(head.Process(input) == doctest::Approx(0.25));
```

Extend it with global phases 0.5 and 2.5, rates 1/4, 4/3, 3/2, zero, and negative counterparts, and nonzero start/short windows using `frac(start + length * frac(phase * speed))` as the independent expected value.

- Write PolyXFader tests using its real ComputePhase: multiplier 2.5, fixed shift/skew, and input phases differing by an integer MUST match before slew. Drive a real accepted topology switch with fixed weights and zero ToT modulation, checking both one-sided waveform values and maximum expected sample slope. Verify fractional interpolation around the switch does not add cycles. Use smooth unquantized shape for continuity assertions; retain separate expectations for quantization/S+H.
- Run the regressions red. Apply playback speed before its final wrapping. In PolyXFader, retain wrapping before waveform multiplier and all partial-lobe logic, but perform phase reduction in double before casting to float:

```cpp
double phase = inputPhase + phaseShift + 0.75;
float localPhase = static_cast<float>(phase - std::floor(phase));
```

- Keep delay projections in absolute coordinates, preserve write-head glue/stopped motion, and wrap only the circular-buffer addressing or normalized display adapter. Replace tracker access with the explicit sample-aware global phase query. Verify sample-window bounds, reverse playback, and delay resynthesis with the existing tests.
- Run focused consumer and sample/delay integration tests; commit after they pass. This task must demonstrate the half-speed 1.5-cycle result is 0.75, rather than the old 0.25.

## Task 4: Trigger-captured AHD and voice gate timing

**Files:** modify `private/src/AHD.hpp`, `private/src/MultiPhasorGate.hpp`, `private/src/TheNonagon.hpp`, `private/src/TheNonagonSquiggleBoy.hpp`, `private/src/SquiggleBoy.hpp`, `private/src/PhysicalModelingSource.hpp`; extend `private/test/unit/dsp_ahd.cpp` and create `private/test/unit/absolute_time_envelopes.cpp`.

**Interfaces:** add trigger input `AHDControl::m_phaseRatio` (double source cycles/global cycle); use `m_envelopePeriodSamples` for its period. AHD::Input captures `m_startGlobalPhase`, `m_phaseRatio`, and `m_envelopePeriodSamples` only on trigger. Remove AHD::Input::m_loopIndex and m_circleTracker. Caller computes the ratio via GetCycleRatio before Set(control). Existing production selection is loop 0; the voice gate ratio is NOT a replacement for that source selection.

- Add a matched-envelope regression: two AHDs capture the same ratio/period/global origin. Feed identical global phase afterward, but change and actually accept source topology in only one timebase. Compare their raw outputs throughout attack, hold, and decay. AHD inputs must receive ordinary non-trigger updates during the edit so the test catches accidental live-period replacement.
- Pin the numeric progress oracle: with origin 10, ratio 4, period 1000, and global phase 10.125, elapsed samples are 500. A topology edit to source ratio 6 and requested period 2000 must leave elapsed at 500. A new trigger at global 10.125 captures the new values, so phase 10.25 then means 1500 elapsed samples. Check raw attack output with attackIncrement 0.0001, no hold, and zero start amplitude: 0.05 for the old note and 0.15 for a fresh zero-level note. Retrigger-from-nonzero is a separate existing behavior check.
- Run the new cases red, then implement the captured-global formula:

```cpp
double phase = input.m_theoryOfTime->GetPhase(
    TheoryOfTimeBase::x_globalLoop,
    input.m_samplePosition,
    PhaseDomain::Modulated);
double elapsedSamples = std::abs(phase - input.m_startGlobalPhase)
    * input.m_phaseRatio * input.m_envelopePeriodSamples;
```

Capture at the actual trigger sample 0. Do not update captured fields on ordinary control refresh. Hold derives from the captured envelope period; retain live user hold/attack/decay controls. Keep release decrement and retrigger amplitude semantics unchanged.
- Change MultiPhasorGate bounds to global origin plus captured voice ratio; keep note-off threshold at 0.5. Capture the period consistently rather than combining an old bound ratio with a newly computed denominator. Supply the AHD source ratio in the integration before SetGates; new triggers use new topology without AHD retaining the source loop.
- Audit all AHDControl consumers, including DeepVocoder. Remove redundant elapsed-sample relay/filter fields only once every consumer is migrated. Remove CircleDistanceTracker use from these time consumers; retain the generic utility only if other real callers remain.
- Strengthen the old AHD continuity test: wait for the accepted parameter value, assert acceptance, use an envelope long enough to straddle it, and replace warning-only checks with required output/progress comparisons. Cover reparenting, reverse global phase, retrigger, and loop 0 production wiring plus a different test source ratio. Run AHD/gate/system stress cases; commit the completed migration.

## Task 5: Remaining consumer integration and terminology cleanup

**Files:** modify `private/src/RecordingBuffer.hpp`, `private/src/ExternalClockSync.hpp`, `private/src/Phasor2Tick.hpp`, `private/src/ScopeWriter.hpp`, `private/src/TheNonagon.hpp`, `private/src/TheNonagonSquiggleBoy.hpp`, `private/src/SquiggleBoy.hpp`; adapt remaining callers found by the audit below. Update tests in `private/test/unit/external_clock_sync.cpp`, `private/test/system/midi_clock_sync.cpp`, `private/test/system/sys_recording_roundtrip.cpp`, and scope tests.

**Interfaces:** recording and sync use GetPhase(global, sample, Unmodulated); tape/music use Modulated; scopes reduce phase only when their schema requires a normalized position. Gate-step indices replace all monodromy call sites. Renamed global-period fields carry samples/global-cycle units.

- Add integration regressions with nonzero phase modulation: unmodulated timestamps must follow unmodulated global time even when modulated phase crosses a different cycle. Assert recording spans across zero, outgoing MIDI start/stop/division crossings, and normalized note-scope coordinates across global cycles.
- Run the cases red. Migrate timestamp and tick consumers to direct absolute phase; derive MIDI ticks by comparing signed floor division indices. Preserve one emitted tick event per processed crossing sample and division initialization on start. Keep scope/file schema meanings unchanged at adapters.
- Audit canonical naming and remove obsolete time APIs, state, and wrappers:

```bash
rg -n 'GetUnwoundMasterIndependent|GetDirectPhasor|GetIndirectPhasor|GetPhasorIndependent|GetInterpolatedDirectPhasor|GetInterpolatedIndirectPhasor|GetTheoryOfTimePhasor|GetTheoryOfTimeTop|GetLoopInternalMultiplier|GetLoopExternalMultiplier|MonodromyNumber|m_globalWinding|m_globalPhase|m_useIndirectPhasor' private/src private/test
rg -n 'CircleTracker|CircleDistanceTracker|UnWind|m_topIndependent|x_masterLoop|m_masterLoopSamples' private/src private/test
```

The first query must have no live legacy time matches. Inspect the second query individually: migrate ToT consumers, but do not rename unrelated generic DSP merely for matching a word. Preserve persistence strings such as TheoryOfTimeMult and TheoryOfTimeParentIx. Compile all changed call paths; do not leave aliases to hide missing migrations.
- Run all focused clock, recording, scope, arp, and envelope tests and commit this completed integration.

## Task 6: Documentation, final verification, and review

**Files:** update `docs/theory-of-time.md`, `docs/polyxfader-lfos.md`, `docs/ahd-envelopes.md`, `docs/multi-phasor-gate.md`, `docs/sampler-looper-recording.md`, `docs/quad-delay.md`, `docs/glossary.md`, `docs/tex/TheoryOfTime.tex`, and affected current API references. Keep archived specs and historical plans untouched. Reconcile delta specs with final names without syncing/archiving them prematurely.

- Document the mapping R to R followed by periodic output maps; distinguish absolute positions from event histories and LFO periodic shaping from playback speed. Explain old/new-parent acceptance and captured AHD timing. Describe the retained unmodulated/modulated mismatch plainly.
- Read the TeX source and update its matching mathematical claims; regenerate the tracked PDF using the repository's documented TeX process if available. Do not silently deliver an obsolete PDF as the new theory; report missing tooling rather than introducing a substitute rendering workflow.
- Build and run the full standalone suite using the commands above. New behavioral tests must fail on regressions and must verify topology was accepted. Record failures and resolve them without weakening assertions or masking infrastructure problems.
- Compile the JUCE application with signing disabled using the command above. Do not launch or deploy it as part of verification.
- Validate planning/spec consistency and the final diff:

```bash
openspec validate absolute-time-coordinates --strict
git diff --check
```

- Review against every design decision: absolute source and derived coordinates, signed gates/indices, simultaneous snapshot-based parent eligibility, interval-correct interpolation, playback output wrapping, preserved PolyXFader shaping, captured AHD ratio/period, consumer timestamp domains, and absence of duplicate APIs. Commit verified code/docs and report exact test/build results plus retained limitations. Do not merge or archive the change without the user's corresponding request.
