# Physical Modeling Corrections Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Correct PM morphing and envelope events, and make comb DECAY specify fundamental T60 independently of timbral DAMP.

**Architecture:** Keep the oversampled excitation and short envelope ranges. Share quarter-circle SVF morph weights with the visualization. Replace the one-pole comb with a passive fundamental-normalized bandpass loop and apply the radial substitution `H(z/r)` to every delay/filter memory element, including fractional interpolation.

**Tech Stack:** Header-based C++17 DSP, doctest standalone tests, CMake/Clang.

## Global Constraints

- Preserve PM 4× oversampling and attack/decay ranges of 100 µs–2.5 s.
- Preserve inverted PM amplitude-depth semantics and base-rate amp/modulation envelope timing.
- DECAY retains the signed 0–1 knob: negative below 0.5, dry at 0.5, positive above; absolute distance from center maps exponentially from 1 ms to 5 s.
- DAMP becomes a normalized damping amount. Preserve persisted parameter IDs; document changed sound semantics.
- At settled parameters, fundamental pitch and T60 must hold for both feedback polarities, including fractional periods. Extra timbral damping must not lengthen any mode's lifetime beyond DECAY.
- Audio and UI must describe the same implemented transfer function. No allocation, locks, or unbounded work on the audio thread.
- Follow AGENTS.md C++ conventions: structs/public members, Allman braces, m_ members, x_ constants, HammerCase functions, no C-style casts or default function arguments, comments ending with a separate `//` line.
- Work in the existing managed detached worktree. Do not merge, push, or deploy. Preserve existing audit artifacts as historical evidence.

## Task 1: Correct morph, trigger delivery, and explicit-release timing

**Files:** `private/src/StateVariableFilter.hpp`, `private/src/PhysicalModelingSource.hpp`, `private/src/AHD.hpp`; tests in `private/test/unit/dsp_svf.cpp`, `private/test/unit/dsp_ahd.cpp`, and new `private/test/unit/dsp_physicalmodeling.cpp` as needed; update `docs/ahd-envelopes.md` and `openspec/specs/ahd-envelopes/spec.md`.

**Interfaces:** Keep `AHD::Process(Input&)` for base-rate callers, add meaningful overload `Process(Input&, float elapsedBaseSamples)` for explicit release advancement. PM passes `1.0f / x_oversample`. A shared `LinearStateVariableFilter::MorphWeights` value with LP/BP/HP fields and `GetMorphWeights(float blend)` supplies both audio and UI.

- [x] Write behavioral regressions for endpoints 0=LP, 0.5=BP, 1=HP; interior quarter-circle weights and continuity around 0.5; the PM UI uses the same intended response.

```cpp
filter.m_lowPass = 2.0f;
filter.m_bandPass = 3.0f;
filter.m_highPass = 5.0f;
DOCTEST_CHECK(filter.GetMorphedOutput(0.5f) == doctest::Approx(3.0f));
DOCTEST_CHECK(filter.GetMorphedOutput(0.25f) == doctest::Approx(3.535533906f));
```

- [x] Add PM integration regressions using real `ProcessUBlock`: a settled 10 ms attack should reach 7.75/480 after the first microblock; retrigger on the immediately next microblock must capture the previous level once. Compare base and 4× explicit release elapsed time and running decay; preserve the short-range behavior. Observe failures before production edits.
- [x] Use quarter-circle weights: first half `Cos2pi(blend * 0.5f)` / `Sin2pi(blend * 0.5f)`; second half offset blend by 0.5. Clamp normalized morph.
- [x] Deliver PM trigger and release events on internal sample zero. Do not clear shared AHD input flags globally: amp scope consumers read them after processing. PM may use a local input copy with its event flags set only on sample zero; keep timing and parameter slews intact. This must recognize new events on consecutive blocks, not just boolean rising edges.
- [x] Keep default release advancement one base sample; oversampled path decrements `input.m_decayIncrement * elapsedBaseSamples`. Do not rescale running attack/decay increments.
- [x] Run the focused test binary and existing envelope/morph tests. Update envelope documentation and commit only task files.

```sh
clang++ -std=c++17 -O2 -DDOCTEST_CONFIG_NO_SHORT_MACRO_NAMES=1 -Iprivate/src -Iprivate/test private/test/support/TestMain.cpp private/test/unit/dsp_svf.cpp private/test/unit/dsp_ahd.cpp private/test/unit/absolute_time_envelopes.cpp private/test/unit/dsp_physicalmodeling.cpp -o /tmp/pm-envelope-morph-tests
/tmp/pm-envelope-morph-tests
```

## Task 2: Implement and integrate the tuned decay comb

**Files:** replace `private/src/CombFilterWithOnePole.hpp` with clearly named `private/src/TunedDecayComb.hpp`; migrate `PhysicalModelingSource.hpp`, `private/test/unit/dsp_comb.cpp`, and concrete comb UI tests/callers; update `ForEachSmartGridOneParam.hpp` display description without changing its identifier. Update `docs/source-machine.md`, `docs/glossary.md`, `docs/drawing-filter-response.md`, and affected main specs.

**Interfaces:** `TunedDecayComb` owns its delay and loop filter, `Process(float)`, `Reset()`, `PopulateUIState(UIState*)`, and explicit normalized pitch/decay-knob/damp parameter setters. Choose concise combined or separate setters consistent with PM's once-per-microblock updates. Remove obsolete raw-feedback/one-pole interfaces rather than maintaining unused DSP.

- [x] Add failing audio regressions before replacement: maximum signed DECAY loses approximately 60 dB in 5 s at a low played note, for both polarities. Existing raw-feedback tests encode the old API/behavior; replace those tests explicitly when that API is removed.
- [x] Implement the stationary prototype `H0(z)=1/[1-s z^-N A(z) Fd(z)]`, with `Fd=(1-d)+dB`, `0<=d<=1`. `B` is a unity-peak bandpass at the played fundamental, Q=1/sqrt(2); hence `Fd` has unity, zero-phase fundamental response and magnitude at most one. A useful knob law is `d=knob²` for resolution at light damping. DAMP zero gives equal modal decay; higher settings remove upper partials faster.

```text
omega = 2*pi*f0/Fs
alpha = sin(omega)/(2*Q)
B(z) = alpha*(1-z^-2) / ((1+alpha)-2*cos(omega)*z^-1+(1-alpha)*z^-2)
r = exp(-ln(1000)/(Fs*T60))
H(z) = H0(z/r)
```

- [x] Include fractional delay with unity magnitude and exact fundamental phase. For example, use integer N plus first-order allpass `(a+z^-1)/(1+a*z^-1)` with fractional phase delay delta, choosing `a=(tan(omega/2)-tan(omega*delta/2))/(tan(omega/2)+tan(omega*delta/2))`. Reserving about one sample for the allpass avoids the pole approaching -1 at integer boundaries. Positive loop phase totals 2*pi; negative loop totals pi. Evaluate the bandpass at the played pitch for both.
- [x] Apply radial loss to all memories: integer read gain `r^N`, allpass one-step terms times r, and biquad z^-1/z^-2 coefficients times r/r². Use sufficient precision for low-note coefficient/state accuracy. Keep expensive coefficient calculations outside the per-oversample loop where practical; retain smooth finite parameter transitions, with bounded pitch and reset semantics. Test modulation and delay-integer boundaries. Any stationary exactness guarantee is for settled parameters; document that moving parameters change the trajectory.
- [x] Make UIState compute the actual integer delay + allpass + transformed damping response using atomic coefficient snapshots, not an ideal fractional delay approximation.
- [x] Integrate DAMP normalized amount, preserve serialized parameter identifier `PMCombDampenCutoff`, update its description. Preserve DECAY's polarity/time mapping and center dry behavior. Limit fundamental to representable delay/Nyquist bounds and document those bounds.
- [x] Verify rendered fundamental pitch and decay across 1×/4× rates, low/mid/high fractional pitches, both polarities, and several DAMP values. Use sinusoidal excitation followed by silence, sine projection/fit with time-window checks, or equivalent independent modal measurements. Check upper-partial attenuation increases with DAMP, DC decays, center is dry, reset silences memory, modulation remains finite, and measured UI transfer matches audio.
- [x] Run focused tests, then build/run the full standalone CMake suite once. If baseline/environment failures occur, distinguish them from this change. Update docs/specs with the formulas, assumptions, new knob meaning, and preserved envelope intent. Commit task files.

```sh
cmake -S private/test -B /tmp/pm-corrections-build
cmake --build /tmp/pm-corrections-build -j 4
ctest --test-dir /tmp/pm-corrections-build --output-on-failure
```

## Completion

- [x] Review task diffs for spec compliance and quality, then perform a separate whole-change review.
- [x] Confirm no production dependency still uses the removed comb API; historical audit probes remain explicitly historical.
- [x] Record test evidence and actual remaining limitations; leave the implemented work in this worktree for review.

## Follow-up: AHD owns event consumption

The user's subsequent API request supersedes Task 1's overload and caller-masking design: put `m_releaseFactor` in `AHD::Input`, and have `AHD::Process` clear both input event flags itself.

- [x] Reproduce repeated-trigger behavior with a failing test that reuses an input without manually clearing events.
- [x] Default release factor to 1, set PM to 0.25, and remove the second Process argument and PM's copied/masked input.
- [x] Consume both flags inside AHD, preserve trigger precedence, and expose the last call's consumed trigger for scope recording.
- [x] Update scope readers and add regression coverage for starts, retriggers, and ends with consumed inputs; preserve the shared control used by multiple envelopes.
- [x] Document release sources and update the current AHD contract.
- [x] Run the full suite and an independent review of this follow-up. CMake build and CTest passed; independent review found no issues. The 17 focused tests passed, and restoring the old scope readers in an isolated temporary header made the new scope test fail as expected.
