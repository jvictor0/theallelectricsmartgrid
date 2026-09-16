# Effects repairs implementation plan

> **For agentic workers:** Use systematic debugging and test-driven development to execute each task. Review the combined change before finishing.

**Goal:** Repair the approved gain and wrap defects, replace weak effects tests with output-based regressions, and explain the remaining DSP questions.

**Architecture:** Keep the current effect topology and control curves. Exercise the actual mixer wiring, parameter provider, spectral synthesis, and delay processors. Changes exposed by stronger tests must preserve each component's mathematical contract.

**Tech Stack:** C++17, doctest, standalone CMake tests at 48 kHz.

## Global constraints

- Work in the existing Codex worktree; production `main` remains untouched.
- Use the repository's brace, member-name, and comment conventions.
- Keep assertions enabled and make tests fail on silence, wrong gain, incorrect pitch, discontinuities, or incomplete reconstruction.
- Do not change Partial Machine input topology or the feedback/diffuser tuning while repairing arithmetic defects.
- Retain the original audit measurements as historical evidence.

## Task 1: Return gain and four-lane continuity

- [x] Add a mixer wiring regression that checks all three return controls reach unity at full scale, and reverb closes at zero.
- [x] Add independent parameter fixtures for all four segments, both interpolation modes, negative coordinates, and continuity across every boundary.
- [x] Run these tests against the current code and record their failures.
- [x] Remove `/ 2` from `ReverbReturn` mapping in `private/src/SquiggleBoy.hpp`.
- [x] Use `x_numParameters` as the segment count in `private/src/FrequencyDependentParameter.hpp`.
- [x] Add dedicated `QuadReverb::Process` impulse, gain, and feedback-tail coverage in `private/test/unit/dsp_quadreverb.cpp`.
- [x] Run the targeted cases.

## Task 2: Audibly meaningful delay coverage

- [x] Replace the startup impulse test's silence-tolerant assertions with a warmed-up transient fixture and explicit nonzero output energy.
- [x] Add a steady-tone gain and octave-shift spectrum test through `QuadDelay::Process`.
- [x] Verify feedback stress tests actually feed the previous output back and retain modulation values between updates.
- [x] Run all quad-delay tests; fix any production defects demonstrated by the corrected fixtures.

## Task 3: Partial Machine synthesis and reconstruction

- [x] Add isolated overlap-add tests for pitch ratios at source frequencies whose hop phase increments are not whole cycles.
- [x] Preserve the original unshifted phase accumulator and multiply emitted phase by detune and pitch-shift ratio, as specified by the user.
- [x] Replace the residual test with production Hann-windowed input; check bin-centered and off-bin sinusoid reconstruction independently.
- [x] Establish the complex coefficient contract between `ExtractAnalysisAtoms` and `WriteWindowedPartial` before correcting subtraction.
- [x] Correct the Hann phase offset in analysis itself and test known input phase and direct reconstruction independently of residual fitting.
- [x] Test organic/synthetic mute behavior at the synthesis output and fix the dead gain application if covered by the repair scope.
- [x] Run spectral-model, DFT, Partial Machine, and wiring tests.

## Task 4: Integration failure, review, and explanation

- [x] Reproduce the saved-patch assertion and capture the value before and after phase wrapping.
- [x] Add a narrow regression for the demonstrated phase-boundary error, repair its source, and rerun the unchanged saved-patch test.
- [x] Run the full standalone suite and resolve relevant failures without weakening assertions.
- [x] Review the diff and update the audit with repair status and validation.
- [x] Explain nonlinear feedback, phase multiplication, the first Hann window and residual subtraction, and alternatives to summing quad audio to mono.

## Follow-up: One analysis estimate for tracking and residual subtraction

The user approved moving the residual coefficient fit into analysis and restoring subtraction through the existing `WriteWindowedPartial` helper.

- [x] Reproduce the inconsistent analysis/residual estimates and off-bin magnitude bias with regressions.
- [x] Refine the selected analysis atoms using the Hann-kernel coefficient fit, preserving the existing magnitude scale.
- [x] Subtract those returned atoms through `WriteWindowedPartial`; remove the separate residual fit and unnecessary spectrum copy.
- [x] Verify spectral, synthesis, and full-suite behavior and review the change.

The new regressions failed on 274 of 784 assertions before the change. The corrected implementation passed 29 targeted tests and 103,123 assertions. The full suite passed 381 of 383 tests and 2,527,139 of 2,527,141 assertions; only the same two baseline startup-silence checks failed, with the unchanged 0.000657712 peak. Independent read-only review found no actionable issues. This follow-up is for PR #5 and has not been deployed to the iPad.

## Final single-pass extraction

The final user-directed simplification renames extraction to `ExtractAndSubtractAnalysisAtoms`. It fits and subtracts each partial directly from its input DFT in the existing peak scan, then creates the atom. The copied DFT, duplicate amplitude/phase estimate, refinement pass, and caller subtraction loop are removed. Existing atom limiting remains afterward. Before merge, 29 targeted tests passed; the full suite passed 381/383 tests with only the two unchanged baseline failures. Independent review found no actionable regressions.

## Earlier completion evidence

The broad repair set was implemented and reviewed. Before the user's phase-accumulator correction, the standalone suite passed 378 of 380 tests; the two startup silence failures were independently reproduced unchanged at the original commit. All effects and new regression cases passed. The user then specified the narrower pitch fix: retain `atom.UpdatePhase()` and scale emitted phase by detune and pitch ratio. See `docs/audits/effects-repairs-2026-09-15.md` for current behavior and validation. The input-topology proposal was declined.

The corrected phase implementation passed 45 targeted tests and 102,403 assertions after the revised contract test reproduced the previous implementation's failure. Build and whitespace checks passed.

Analysis phase now removes the Hann offset before storing an atom. The direct-analysis regression failed before the fix, then 26 targeted tests and 102,348 assertions passed with the correction. The residual fitter was not changed in this step.

The user requested deployment. `make ios-deploy` built, installed, and launched the signed Release app on the paired iPad. The fresh app log confirmed 48 kHz, 512-frame, four-input/four-output audio startup through MAYA44 USB+.

PR preparation: the final C++ source passed 379 of 381 tests and 2,526,355 of 2,526,357 assertions. Only the two independently confirmed baseline startup-silence failures remain. Final read-only review found no actionable issues.
