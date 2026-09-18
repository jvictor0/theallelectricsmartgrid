# Time-Warp Delay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Fix time-warp delay indexing, interpolation and incomplete grain reads while retaining musical reversals.

**Architecture:** Keep head selection in warped coordinates and enforce the live recording frontier at grain launch in real coordinates. Use two-point linear inverse-map interpolation. Restrict only the time-warp LFO to continuous shapes and moderate Skew.

**Tech Stack:** C++17 header-based DSP, doctest, CMake, Clang sanitizers.

## Global Constraints

- Preserve time reversals and forward-only inverse-map writing.
- Negative absolute positions wrap by the actual physical buffer size.
- Grain window N = 4096; hop H = 1024; cubic audio reads require two future support samples.
- Time-warp Skew attack fraction spans 0.1 to 0.9; midpoint remains 0.5.
- Time-warp Shape spans sine to triangle continuously; voice and quad LFO behavior stays unchanged.
- Keep cubic interpolation for audio; use linear interpolation for inverse-map timestamps.
- Follow AGENTS.md braces, public structs, m_ member names, x_ constants, and comment formatting.
- Existing managed worktree is detached and isolated. Leave edits uncommitted until the coordinator completes verification; do not create a second worktree or alter other tasks' files.

### Task 1: Delay indexing, inverse interpolation and complete grain windows

**Files:** Modify `private/src/DelayLine.hpp`; create `private/test/unit/dsp_delaywarp.cpp`.

**Interfaces:** Existing DelayLineMovableWriter public read/write APIs stay intact. Extend GrainManager::Process with optional `double latestSampleTime = std::numeric_limits<double>::infinity()`; QuadGrainManager passes its writer's `m_lastTime - 1.0`.

- [x] Write and run failing tests exercising actual delay storage. Hand-check fixtures: ascending points (0,0), (1,1), (101,2), (102,3) should map position 26 to 1.25; inverse entries at 0=100 and 1=101 with neighbors -1000 and 9000 must map 0.5 to 100.5; negative physical positions must match their positive wrapped counterparts.
- [x] Implement signed scatter endpoints and floor/modulo indexing. For adjacent ascending warped positions low/high and real timestamps realLow/realHigh, write each integer i in `[ceil(low), ceil(high))` using:

```cpp
double alpha = (static_cast<double>(i) - low) / (high - low);
double value = realLow + alpha * (realHigh - realLow);
```

  Store using `PhaseUtils::FloorMod(i, Size)`. Fill from the current and previous ascending sample starting at count two. Equality produces an empty scatter interval.
- [x] Replace inverse lookup with linear interpolation between floor(position) and its successor. Correct cubic audio and envelope indexing for negative coordinates without changing their interpolation formulas.
- [x] Write and run a failing real quad-grain sine test with write speed five and 4500 warped samples of delay. Its 900-real-sample lag must not cause periodic attenuation after warmup; include positive sample offsets and starts already safely behind the writer.
- [x] At grain launch apply:

```cpp
double startTime = m_audioBuffer->GetRealTime(warpedTime) + sampleOffset;
double latestStart = latestSampleTime - (Resynthesizer::x_N - 1) - 2;
startTime = std::min(startTime, latestStart);
```

  Pass each actual live frontier from QuadGrainManager. Immutable sample banks retain the default infinite frontier. Document the two-sample interpolation margin.
- [x] Run focused delay/sample-source tests and UBSan with float-cast-overflow. Record evidence in the progress ledger.

### Task 2: Continuous time-warp LFO and Skew limits

**Files:** Modify `private/src/PolyXFader.hpp` and `private/src/TheoryOfTime.hpp`; create `private/test/unit/dsp_timewarp_lfo.cpp`. No changes to DelayLine, TapeHead, QuadDelay, other tests, docs, or git state for this task.

**Interfaces:** Preserve existing PolyXFader Input fields and their semantics for callers other than TheoryOfTime. Introduce an explicit opt-in continuous Shape mode on PolyXFader Input, default off. TheoryOfTime activates it when evaluating its phase-modulation LFO. Interpret the time-warp input's m_attackFrac as a normalized Skew control and remap only the evaluation copy to 0.1 + 0.8 * knob, so mapping is applied exactly once per evaluation.

- [x] Write failing tests against the production TheoryOfTime path. At normalized attack knob 0.5, Mult 1 and source phase 0.125, Shape 1 must produce an unquantized triangle value 0.25. Shape 0 must produce `(1 - cos(pi/4))/2`, with Shape 0.5 halfway between those values. Drive through ProcessPhaseModLFO after initializing source phase/topology and inspect pre-slew component output to isolate waveform behavior.
- [x] Test Skew endpoints through source phases that distinguish attack fraction 0.1/0.9 from 0.001/0.999; repeated evaluation must not remap the stored knob value. Test a dense phase sweep at Mult 16 and Shape 1: avoid unit quantization jumps. Include a fractional Mult case and a default-mode PolyXFader case verifying its stepped shape remains available.
- [x] Implement the mode with sine/triangle blending over the entire shape range:

```cpp
float sine = (-Math::Cos2pi(in / 2) + 1) / 2;
return sine + shape * (in - sine);
```

  The mode bypasses Quantize. The default mode continues using the existing Shape and Quantize behavior. In TheoryOfTime use an evaluation copy for the Skew remap so UI/control input stays normalized and filtering remains at its existing boundary.
- [x] Compile and run `clang++ -std=c++17 -O2 -DDOCTEST_CONFIG_NO_SHORT_MACRO_NAMES=1 -I private/src -I private/test private/test/support/TestMain.cpp private/test/unit/dsp_timewarp_lfo.cpp -o /tmp/time-warp-audit/lfo_tests` then `/tmp/time-warp-audit/lfo_tests`. Record RED and GREEN evidence and any integration concerns in the task report. Leave edits uncommitted for coordinator integration.

### Task 3: Integration, documentation and review

**Files:** Extend `private/test/unit/dsp_delaywarp.cpp`; update `docs/quad-delay.md`, `docs/theory-of-time.md`, and `docs/polyxfader-lfos.md`.

- [x] Add real TimeRig -> QuadDelayInputSetter -> QuadGrainManager coverage at Mult 16 including negative startup, reversals, Shape endpoints, short delay and maximum Index. Assert finite sustained tone levels after warmup, and preserve negative write motion.
- [x] Update delay docs for linear inverse mapping, signed wrapping and the real-time grain guard. Correct existing descriptions of loop selection ownership. Update time-warp LFO Shape/Skew docs while retaining generic PolyXFader descriptions.
- [x] Build via `cmake -S private/test -B /tmp/time-warp-audit/build` and `cmake --build /tmp/time-warp-audit/build -j 4`. Run the full suite via ctest and focused Clang UBSan tests. Measure worst forward scatter increments under the final controls; report the distinction from iPad timing.
- [x] Obtain an independent code review of the final diff and tests. Resolve correctness findings, run affected checks, update this plan's checkboxes and ledger, and report results. No deployment or landing requested.

## Verification before rebase

Implemented and independently reviewed; no open review findings. Reversals and forward-only inverse-map writing remain intact. These results were recorded against original base `c113772a53ec919385ea59348966cd7e5be72a8e`.

- Focused delay/sample-source checks passed: 23 cases / 79,554 assertions before adding real-clock integration.
- Final rebuilt delay/LFO/quad-delay selection passed: 18 cases / 78,151 assertions. The integration test separately checks negative coordinates and actual backwards motion between observed samples.
- Final UBSan plus float-cast-overflow run passed: all 15 new cases / 69,697 assertions, including the default PolyXFader compatibility test.
- Four real-clock Mult 16 configurations sustained the expected 0.141421 RMS for a 750 Hz tone at amplitude 0.2, including full Index and Skew extremes. This verifies the reproduced level-dip mechanism for these signals and settings, not artifact-free behavior for every possible input.
- Removing the grain-start limit in a scratch source copy caused the explicit analysis-window regression to fail: the last audio query reached 103695.75 against a writer frontier of 100000. The production implementation passes the same check.
- CMake configuration and build succeeded. Unfiltered ctest aborts on the pre-existing `VectorPhaseShaper.hpp:340` assertion in `PartialMachine: espace etale patch remains finite after load`. With that case excluded, 382 cases passed and two startup-silence cases failed. All three failures were reproduced against an untouched archive of base commit `c113772a53ec919385ea59348966cd7e5be72a8e`; the startup failures reproduce the identical peak of 0.000657712. The full suite is therefore not green, but no new suite failures were observed.
- A 48-configuration steady-control sweep covered Shape endpoints, Skew 0/0.5/1, Mult 1/4/8/16, and both source-blend endpoints, at maximum Index and a 192000-sample global period. Largest measured forward advance: 503.418518 warped samples per audio sample; largest scatter: 504 entries per channel/sample. Largest mean scatter: 32.894167 entries per channel/sample. These are empirical workloads for the exercised controls, not a universal bound or iPad timing measurements.
- `git diff --check` passed.

Local verification artifacts are under `/tmp/time-warp-audit`: `final-targeted.log`, `ubsan-final.log`, `guard-mutation.log`, `ctest.log`, `suite-excluding-baseline.log`, `baseline-patch.log`, `baseline-startup.log`, and `final-scatter-sweep.csv`. Independent review reports and the task ledger are in `.superpowers/sdd/2026-09-17-time-warp-delay/`.

## PR preparation

Rebased onto `origin/main` at `ec866d8` for the requested PR. The only overlap was the existing quad-delay impulse test: main already replaced it with a stronger warmed-impulse test and added pitch-shift and feedback coverage, so that version was retained. The three production headers and both new regression files are unchanged by the rebase. The PR adds no separate change to `dsp_quaddelay.cpp`.

- CMake configuration and build passed on the rebased tree.
- Unfiltered ctest completed all 400 cases: 398 passed, two startup-silence cases failed, with no skipped cases. Both failures are the previously reproduced baseline failures and report the identical peak of 0.000657712. The former VectorPhaseShaper assertion is fixed by current main and no longer interrupts the suite. Total assertions: 2,596,853; two failed.
- Rebuilt UBSan plus float-cast-overflow checks passed: 15 cases / 69,697 assertions.
- Logs: `/tmp/time-warp-audit/pr-cmake-build.log`, `pr-ctest.log`, and `pr-ubsan.log`.

## Follow-up audit corrections

The user requested fixes for the two confirmed findings from the follow-up audit: an all-zero Center blend at the final-source boundary, and an inverse-map gap in the first forward interval after a reversal. Preserve the approved reversal policy and the existing analysis-window limit.

- [x] Add regressions before implementation. The Center tests cover the exact boundary, adjacent floating-point values, and filtered encoder movement through the real clock preparation cadence. The reversal test uses hand-calculated timestamps and verifies that backward motion preserves history, the first forward interval replaces it immediately, and positions not yet crossed remain unchanged. It includes negative coordinates crossing the physical wrap. All three new tests failed for the expected reasons before the changes (10 failed assertions).
- [x] Include equality in the final-source Center boundary. At the boundary, the final source now remains selected rather than allowing all source weights to vanish.
- [x] Seed the ascending interpolation history with the preceding recorded turnaround sample when leaving descending motion. This fills the first forward interval without writing descending intervals.
- [x] Run all focused delay-warp/LFO tests and UBSan plus float-cast-overflow: 18 cases / 69,716 assertions pass in both runs.
- [x] Recompile and replay the original audit probes. Center now has zero zero-weight samples and maximum local speed 65.01825 rather than 19,884.1482. At the production-capacity reversal reproduction's grain launch, the formerly stale cell now contains the required timestamp 17893997.076140527, eliminating the 11,348.50-sample start error.
- [x] Complete the full-suite run and independent review for the follow-up commit to PR #6. The CMake build passes. All 403 cases ran: 401 passed and the same two baseline startup-silence cases failed, with the identical peak of 0.000657712. There were 2,596,872 assertions, of which two failed. Independent specification and quality review passed with no actionable findings.

Follow-up evidence is in `/tmp/time-warp-audit-fixed`: `fix-red.log`, `fix-green.log`, `fix-ubsan.log`, `fix-build.log`, `fix-ctest.log`, `fix-review.md`, `map-after.log`, and `center-after.log`. The original map probe retains its diagnostic labels `omitted` and `stale`; after the correction, its printed mapped/stored values equal the required values and its timestamp error is zero.
