# Change Report — ui-state-reset-coverage

## Summary

Built a regression net for encoder/grid UI-state coherence and fixed the two
cache-invalidation bugs it exposed. Full suite: **170 test cases (+11 new),
~1.49M assertions, all green.** Two source fixes, two new system test files,
three delta specs.

## Bugs found and fixed

### Bug 1 — shift-press encoder leaves a stale value (the reported bug)
`BankedEncoderCell::HandleShiftPress` (`private/src/EncoderBank.hpp`) cleared the
modulators/linked gestures and recomputed the affecting bitmasks
(`SetModulatorsAffectingRecursive`) but never called `SetForceUpdateRecursive()`.
Because `Compute` is change-driven and an emptied affecting bitmask can never
re-trigger it, `m_output` stayed **frozen** at the pre-reset value forever — the
published UI mask read "no modulation" while the value still reflected it.
**Fix:** added `SetForceUpdateRecursive()` to `HandleShiftPress`, matching the
existing `RevertToDefault` / `EncoderSet` bookkeeping.

### Bug 2 — shift-press gesture button leaves a stale value (the "similar bug")
Predicted in review and **confirmed**: deleting a gesture via its selector pad
(`GestureSelectorCell::OnPress` with shift → `EncoderBankInternal::ClearGesture`)
recomputed the masks via `SetAllModulatorsAffecting()` but set no force-update,
so the encoder stayed frozen at its gestured value after deletion.
**Fix:** `ClearGesture` now calls `SetForceUpdateRecursive()` on each affected
cell after deactivating the gesture. Deliberately scoped to the gesture-delete
caller, not the shared `SetAllModulatorsAffecting` helper (its other caller,
`HandleChangedSceneManagerScene`, already gets force-update from the bulk scene
refresh — verified no double-recompute).

Both fixes were proven test-first: the repro tests fail on the unpatched source
(value frozen at 0.7 while the mask correctly clears) and pass after the fix.

## Candidate finding (NOT fixed — flagged per instructions)
While building the gesture tests, an *over-aggressive* setup that drove a gesture
target to its 1.0 ceiling (leaking the surplus into the base via clamp
compensation) exposed an inconsistency: with the clamped gesture present at
weight 0 the cell read base ≈ 0.42, but after `ClearGesture` the cell read ≈ 0.12
— i.e. deleting a clamped/leaked gesture re-derives a different base than the
"live" leaked base. This only manifests under gesture-target clamping during
setup and is tangential to the cache-coherence bug. **Low confidence, complex,
left unfixed.** The regression tests avoid it by using a moderate, non-clamping
gesture drive. Worth a closer look as a follow-up (clamp-compensation vs. scene
storage when a gesture is deleted).

## Pre-existing crash (not introduced here)
The fuzz **soak** mode (`SMARTGRID_FUZZ_SEEDS`/`SMARTGRID_FUZZ_FRAMES`) aborts in
a `LoadPatch` path (`sys_fuzz.cpp`). Verified by reverting the source fix and
re-running the identical soak: the crash reproduces on the baseline, so it is the
previously-known LoadPatch soak crash, unrelated to this change.

## New tests (assertive — value AND UI state after settle frames)

`private/test/system/sys_encoder_reset.cpp` (8 cases):
- Bug 1 repro: shift-press encoder clears the linked gesture, value → ungestured base, post-clear analog sweep is inert.
- Bug 2 repro: shift-press gesture button deletes the gesture, value → ungestured base.
- Orthogonality matrix: gesture clear correct at analog-input down/middle/up × {encoder-shift, gesture-button}.
- Shift-press with a gesture *selected* clears only that gesture (modulator survives).
- Modulator-slot path: assign → mask set; shift-reset → mask cleared (sequencer running, NaN-clean).
- Scene isolation: reset in scene 0 leaves scene 1's gesture functional.
- Persistence: cleared state round-trips through save/reset/load.
- Whole-system `ResetToDefaults` clears all masks (hardens the old WARN-only check).

`private/test/system/sys_theoryoftime_topology.cpp` (5 cases):
- Multiplier pad sets `m_parentMult` AND lights the correct pad White (others off).
- Multipliers survive save/reset/load (value + lights).
- Loaded non-default topology drives the clock (phasor advances, bounded, NaN-clean).
- Multipliers are **scene-banked** — each scene remembers its own, restored on switch.
- Per-scene multipliers round-trip through a patch ("remembers scenes across saved patches").

## Coverage notes (axes sampled vs. exhausted)
- Analog-input position × clear-method: exhausted ({0, 0.5, 1} × {encoder, gesture-button}).
- Config axis: gesture, modulator, modulator+gesture covered; true depth-2 nested
  modulation is not front-door reachable via `SynthRig` (matches `sys_gestures.cpp`),
  so the reachable depth-1 modulator/gesture clears are tested instead.
- Other Shift commands enumerated (`m_shift` consumers): the reset-relevant ones
  (encoder reset, gesture delete) are covered. Shift+scene-pad copy-to-scene and
  the WrldBldr/SquiggleBoy shift paths exist but don't clear modulation, so they
  carry no cache-coherence bug and are out of scope for this change.

## Specs
Delta specs updated for `encoder-parameter-system` (cache-invalidation contract +
"Shift-Press Reset Semantics" + "Gesture Removal Invalidates the Recompute Cache"),
`phasor-timebase` (topology grid control/lights/persistence), and
`scene-state-management` (cross-scene/patch coherence). Not yet synced to main
specs (`/opsx:sync`) — pending archive.
