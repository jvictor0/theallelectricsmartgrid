# Whole-Tick Rhythm Review and Completion Plan

> **For agentic workers:** Use subagent-driven development or executing-plans to carry out the bounded tasks below. Review tasks are read-only; implementation stays in the existing Cursor worktree.

**Goal:** Review and complete the whole-tick rhythm change, with matching production behavior, tests, documentation, and specifications.

**Architecture:** Each loop cycle advances one signed 64-bit step on the undoubled LCM lattice. Per-loop gates sample the requested rhythm only on that loop's modulated tick; startup samples all rhythms and stopped gates are false. Rhythm edits do not generate ticks or global change notifications. Position updates and current crossing detection precede topology remapping and gate lookup.

**Tech Stack:** C++17, doctest/CMake, JUCE/Xcode, Markdown/OpenSpec.

## Global Constraints

- Preserve user changes and work directly in the specified existing worktree.
- Do not change MIDI/phase frequency or the separate voice-note gate duty cycle to compensate for removing half ticks.
- Retain signed 64-bit indices until bounded rhythm selection.
- Use explicit C++ arguments or meaningful overloads, public structs, matched braces, and repository comment style.
- Initial review was read-only with respect to publication. The user subsequently authorized cleanup, an explanatory commit, rebase on main, fast-forward of main, and push; no application deployment was requested.

## Task 1: Review and establish regression coverage

- [x] Independently review core timing/consumers and UI/state/navigation integration against HEAD.
- [x] Update existing absolute-time and voice-wiring expectations to whole ticks; replace removed half-step event queries.
- [x] Add behavioral coverage for startup/stop, same-value neighboring gates, negative and wide indices, own-tick edit acceptance, rollover, and accepted topology changes.
- [x] Exercise rhythm controls, ancestor reset selection, and state round trips through production classes.
- [x] Run focused cases before fixes and retain the failure evidence for any discovered defect.

## Task 2: Resolve review findings and verify production integration

- [x] Apply minimal fixes supported by review and failing cases.
- [x] Run the complete standalone suite; isolate and compare any unrelated failures with unchanged HEAD without altering this checkout.
- [x] Build the JUCE macOS app using the existing build configuration with signing disabled.
- [x] Re-review final production changes and fix remaining introduced defects.

## Task 3: Update the behavior contract

- [x] Update active Theory of Time, Nonagon, LaMeJuIS, gate/envelope, glossary, and controller documentation where affected.
- [x] Update canonical OpenSpec requirements and the unarchived absolute-time change to agree on whole ticks, per-loop gate latching, rhythm editing, and voice timing.
- [x] Delete the obsolete Theory of Time LaTeX/PDF and remove its live links (user request).
- [x] Check active documentation/specs for stale half-step APIs and formulas; preserve unrelated historical records.
- [x] Run applicable specification/whitespace checks and record final validation evidence.

## Review and verification evidence

- Corrected signed-index truncation in `TheoryOfTimeRhythm`: six cases beyond 32-bit range with sizes 3, 5, and 7 failed 18 assertions before the fix.
- Corrected `StateSaver::HandleBlendChanges`' exclusive upper bound, which skipped the final shuffled state. The new eight-scene round-trip test reproduced the omitted rhythm gate before the fix.
- Replaced stale half-tick test APIs and expectations and added 21 regression cases covering timing, edits, reset ancestry, consumers, controller pages, and scene persistence.
- Initial whole-tick focused run: 60 tests and 10,811 assertions passed.
- Before the frontend regressions were added, the complete standalone run passed 466/468 tests and 2,584,848/2,584,850 assertions. The two failures are pre-start silence checks in `sys_startup_stability.cpp`. Both reproduce identically against unchanged HEAD `8f745e5`, including output peak `0.000657712` and frozen phase `0 -> 0`; their assertions remain unchanged.
- The initial unsigned macOS Release app build passed through `make -C JUCE/SmartGridOne build`.
- Strict OpenSpec validation passed all 36 items. Active formula/API/link checks and `git diff HEAD --check` passed.
- Removed the obsolete Theory of Time PDF and LaTeX source at the user's request. Archived specs and earlier plans remain historical records.

## Frontend convergence follow-up

- Changed LameJuis configuration acceptance from gate-value changes to explicit modulated input ticks. Matrix and co-mute edits still wait for their own input; RHS and target edits wait for a relevant row input.
- Empty rows now stay false, even with RHS[0] enabled, and contribute to no accumulator. Section equality includes accumulator totals, allowing denominator-only same-pitch note changes on existing read/arp events.
- The randomized frontend test exposed a separate startup defect: the grid's coordinate converter did not receive the initial all-read lens. Initializing it from `m_coMuteState.GetLens()` fixed the discrepancy without waiting for a co-mute edit.
- Added deterministic randomized tests through the registered frontend controls and production Nonagon processing. An independent oracle checks all 64 sheaf sections, per-input acceptance, co-mute fibers, lane choices, read/trigger timing, timbres, topology, rhythms, signed reset indices, and eventual convergence after edits stop. A separate grid regression leaves co-mutes untouched to exercise startup initialization.
- Mutation checks confirmed the tests catch gate-change-only latching, acceptance on the wrong input, disabled rows contributing to totals, denominator-blind equality, early rhythm edits, doubled LCM, and ignored rhythm reset.
- The focused frontend run passed both tests and all 87,909,818 assertions after the startup fix. The full run then passed 468/470 tests and 90,494,666/90,494,668 assertions, with only the same two baseline startup-silence failures described above.
- Updated active LameJuis docs and canonical/delta specs to distinguish pending and accepted configuration, input counts and accumulator row counts, inactive rows, initial grid state, denominator-aware section identity, and trigger-time timbre capture.

Reproduction commands (from this worktree):

```sh
cmake -S private/test -B private/test/build
cmake --build private/test/build -j 6
private/test/build/smartgrid_tests '--test-case=WholeTick*,AbsoluteTime*,TimeRig*,TheoryOfTime rhythm*,TheoryOfTime WorldBuilder*,Frontend convergence:*'
ctest --test-dir private/test/build --output-on-failure
openspec validate --all --strict --no-interactive
```

The unchanged-HEAD comparison compiled the original startup test and production sources from `git archive HEAD` in a temporary directory, using the same C++17/arm64/SDK/optimization flags. No branch files were replaced for baseline testing.

## Landing validation (2026-09-26)

- Rebased onto main at `b50ce42`, preserving its `SampleTop` crossing events and fractional offsets while removing the separate half-step flag. The randomized oracle explicitly converts the event to Boolean when comparing tick presence.
- Rebuilt the standalone target and ran the complete rebased suite: 490/492 tests and 90,494,914/90,494,916 assertions passed. Only the same startup-silence assertions at `sys_startup_stability.cpp:95` and `:272` failed. All frontend convergence and current main's scope/timing regressions passed.
- Rebuilt the unsigned macOS Release app successfully after rebase.
- Revalidated all 36 OpenSpec items and the final diff whitespace check.
- Local editor settings remain outside the commit in both checkouts.
