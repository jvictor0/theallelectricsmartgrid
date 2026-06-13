## Context

Encoder outputs are recomputed lazily on control frames. `BankedEncoderCell::Compute` (`private/src/EncoderBank.hpp:755`) only runs the gesture/modulation mixing when:

```cpp
m_forceUpdate
  || !m_modulatorsAffecting.Intersect(modulatorValues->m_changedModulators).IsZero()
  || !m_gesturesAffecting.Intersect(modulatorValues->m_changedGestures).IsZero()
```

`m_modulatorsAffecting` / `m_gesturesAffecting` are **cached** bitmasks recomputed only by `SetModulatorsAffecting()` — the deliberate "we don't check the encoder configuration every frame" optimization. The DSP/UI bridge is one-directional: `PopulateUIState` (`EncoderBank.hpp:1597`) copies `m_output[k]` and `m_modulatorsAffectingPerTrack` into `EncoderBankUIState` atomics; tests read them via `SynthRig::EncoderValue` / `SynthRig::UIState()`.

**Bug 1 — shift-press the encoder.** `HandleShiftPress` (`EncoderBank.hpp:656`) clears modulation and any linked gestures (`ZeroModulators` → `SetValue(0)` + `DeactivateGesture` per gesture, lines 680–698) and recomputes the affecting masks (`SetModulatorsAffectingRecursive`, line 677) but never calls `SetForceUpdateRecursive()`. After the masks are cleared they are empty, so the `Intersect(...)` terms are permanently false; with `m_forceUpdate` also false, `Compute` never runs again for that cell and `m_output` stays frozen at the last modulated/gestured value.

**Bug 2 — shift-press the gesture button (delete a gesture).** A different path with the same root cause: `GestureSelectorCell::OnPress` with shift (`TheNonagonSquiggleBoy.hpp:481`) → `…ClearGesture` → `EncoderBankInternal::ClearGesture` (`EncoderBank.hpp:1349`), which deactivates the gesture per cell and then calls `SetAllModulatorsAffecting()` (`EncoderBank.hpp:1301`) — a generic "recompute masks" helper (also reused by `HandleChangedSceneManagerScene`, line 1403) that recomputes the affecting bitmasks but **sets no force-update on any cell**. So after a gesture is deleted, the gesture's bit clears from `gesturesAffecting`, the `Intersect` term goes false, and the encoder is left frozen at its gestured value. This is the "similar bug" the requester predicted; it is confirmed in source.

In both cases the UI mask correctly reads "no modulation / no gesture" while the value stays wrong — the two disagree forever. The correct pattern is visible elsewhere in the same file: `RevertToDefault` (line 557) calls `SetForceUpdateRecursive()`, and the `EncoderSet` path (line 1587) does too, with a comment (line 1580) stating the required bookkeeping is "`SetForceUpdateRecursive + SetModulatorsAffecting`."

**Fixtures (frozen).** `private/test/support/SynthRig.hpp` drives the full `TheNonagonSquiggleBoyInternal` single-threaded, mirroring `NonagonWrapper::Process`. Relevant verbs: `SetEncoder/IncEncoder/PressEncoder/ReleaseEncoder`, `PressPad/ReleasePad/TapPad`, `SetShift/WithShift/PressShiftPad`, `SetLeftScene/SetRightScene/SetBlend/SelectScene`, `SetFader`, `SavePatch/LoadPatch/SavePatchJSON/LoadPatchJSON/ResetToDefaults`, `RunFrames/RunSamples/RunSeconds`; observers `EncoderValue(x,y,k)`, `EncoderConnected`, `UIState()`, `Internal()`, `MasterPhasor`, `Blend/LeftScene/RightScene`, `SawNaN/OutputPeak`. `TimeRig` drives a bare `TheoryOfTime` for the lower-level loop checks. Test framework is doctest; system tests live in `private/test/system/`.

## Goals / Non-Goals

**Goals:**
- Fix BOTH shift-reset cache-coherence bugs (encoder shift-press and gesture-button delete) with minimal changes that match existing bookkeeping.
- Build an assertive regression net structured as orthogonality matrices (D7): every cell drives commands, runs settle frames, then checks BOTH the published UI state (`EncoderBankUIState` masks/value, grid LED color) AND the authoritative value (`EncoderValue` / `GetValue` / underlying `m_parentMult`).
- Cover the combinations the requester named: all Shift commands, nested modulation, modulation × scenes × blend, save/load round-trips, gesture deletion via the gesture button, correct behavior with the gesture-weight analog input down/middle/up, and the Theory of Time topology grid (set multiplier, reset, persist, scene-switch, verify lights).
- Make the tests fail loudly on regression (hard `CHECK`/`REQUIRE`, not `WARN`) so the bug "can never happen again."

**Non-Goals:**
- No redesign of the lazy-recompute scheme; it stays, we just enforce its invalidation contract.
- No new serialization format or public API changes.
- Not chasing exhaustive coverage of every encoder parameter — representative connected cells via `FindConnected`, plus the specific reset/topology paths.

## Decisions

**D1 — Fix at each mutation site, not in `Compute`.** Add `SetForceUpdateRecursive()` to `HandleShiftPress` (Bug 1, alongside the existing `SetModulatorsAffectingRecursive()`), and make the gesture-delete path set force-update on the affected cells (Bug 2). For Bug 2 the narrow fix is in `EncoderBankInternal::ClearGesture`: after `cell->ClearGesture(gesture)`, set force-update on each cell (e.g. `cell->SetForceUpdateRecursive()` in the same loop, or have `SetAllModulatorsAffecting` accept/imply a force-update for the clear caller) — but NOT in `HandleChangedSceneManagerScene`, the other `SetAllModulatorsAffecting` caller, where a bulk refresh already handles force-update via `HandleChangedSceneManager`/`SetStateRecursive`. Apply will pick the least-surprising spot and confirm no double-work. Rationale: the invariant is "config-mutating edits invalidate the cache"; `RevertToDefault` and `EncoderSet` already honor it, so this restores consistency at the mutation sites. *Alternative considered:* make `Compute` always recompute when a mask just transitioned to empty — rejected: it reintroduces per-frame checking, defeats the optimization, and spreads the contract across many places.

**D2 — Assert value AND UI state after settle frames, every test.** The bug's signature is precisely that the mask and the value *disagree*, so a test that checks only one would miss it. Each test runs `RunFrames(kSettleFrames)` after a command (control-frame cadence + slew lag), then asserts the mask via `UIState().…GetModulatorsAffecting/GetGesturesAffecting` and the value via `EncoderValue`. *Alternative:* assert immediately — rejected: lazy recompute and slew mean the change isn't visible the same frame.

**D3 — Reproduce the bug as a guarded precondition + corrective assertion.** A modulated cell only differs from base when its modulator signal is non-zero at reset time. The repro test first establishes the precondition (modulated output ≠ base, asserted) so the test is meaningful, then shift-resets and asserts the value returns to base while the mask clears. If a chosen modulator can't establish the precondition, pick another modulator slot / run the sequencer so a live source is non-zero. *Alternative:* assert the raw `m_output` is frozen — rejected as too white-box; `EncoderValue` is the user-observable contract.

**D4 — Two new system test files, mirror existing style.** `sys_encoder_reset.cpp` (encoder shift/reset/scene/patch matrix) and `sys_theoryoftime_topology.cpp` (topology grid). Reuse `FindConnected`, `kSettleFrames`, `kTol`, and the modulation-setup recipe documented in `sys_gestures.cpp` (PressEncoder → IncEncoder on a depth cell → deselect via PressEncoder(3,3)). *Alternative:* extend `sys_gestures.cpp` — rejected: keep the new regression net discoverable and focused.

**D5 — Topology grid: assert underlying value + LED color.** Press `TapPad(RouteBottomRight, i, mult-2)` to set `m_input[i].m_parentMult`; read the value via `rig.Internal()`; read the LED by querying the topology page's cell color (the on-color pad is the one whose `mult` equals `m_parentMult`). If no read-only color accessor is reachable from `SynthRig`, add a minimal const accessor rather than reaching into private members. *Alternative:* infer the light from the value — rejected: the user explicitly wants the lights verified independently of the value.

**D6 — Enumerate Shift commands by reading source during apply.** Known: shift+encoder (reset, `HandleShiftPress`), shift+gesture-pad (delete, `GestureSelectorCell` → `ClearGesture`), shift+scene-pad (copy-to-scene, `CopyToScene`). Apply will grep for every `m_shift` / `ShiftedCell` consumer and add a row per command to the matrix, each asserting post-command value + UI state. Whole-page / whole-grid reset (`RevertToDefault(allScenes,allTracks)` via `ResetToDefaults`, and any per-page reset cell) is tested where it exists; if a "reset whole page" affordance is found for the topology grid it is covered too.

**D7 — Structure the gesture/reset coverage as orthogonality matrices.** Rather than ad-hoc scenarios, the tests vary independent axes and assert the same post-condition in every cell, so a regression on any single axis is caught:
- **Reset method**: shift-press encoder (no gesture selected → clears modulators + linked gestures); shift-press encoder (gesture selected → deactivates selected gesture only); shift-press gesture button (delete gesture); global `ResetToDefaults`.
- **Configuration under test**: modulator-only; gesture-only; modulator + gesture together; nested modulation (depth cell itself modulated).
- **Analog-input / gesture-weight position at reset**: down (`SetFader → 0`), middle (0.5), up (1). The requester's point: the encoder must respond correctly in all three, and after a gesture is cleared, sweeping the analog input must no longer move the output.
- **Scene/blend context**: single scene; two-scene pair with blend ∈ {0, 0.5, 1}.
- **Persistence**: assert in-session, and again after `SavePatch → ResetToDefaults → LoadPatch`.

Post-condition asserted in every cell: published value/`GetValue` == expected (ungestured/unmodulated base, or remaining-modulation value) within `kTol`; the corresponding affecting bitmask matches; and where a gesture was removed, a post-clear analog-input sweep is a no-op. Full cartesian product is large; apply implements it as parameterized loops over the axes (covering each axis fully and the high-value pairings — reset-method × config, reset-method × analog-position, reset-method × persistence) rather than every combination, and `log`s any axis intentionally sampled rather than exhausted. *Alternative:* a handful of hand-picked scenarios — rejected: misses the cross-axis regressions the matrix is designed to surface.

## Risks / Trade-offs

- **[Modulator signal may be ~0 at reset, hiding the bug]** → D3's guarded precondition: assert the modulated output actually diverged from base before resetting; otherwise switch modulator source or run the sequencer to guarantee a live non-zero signal.
- **[Topology LED color not reachable read-only from `SynthRig`]** → add a small const accessor on the topology page / grid; do not duplicate color logic in the test.
- **[`parentMult` scene-banking unknown]** → the test determines empirically whether `"TheoryOfTimeMult"` is scene-banked (which `StateSaver` instance owns it) and pins the observed behavior; the spec only asserts the save/load round-trip, which holds either way.
- **[Fix changes a hot path]** → `SetForceUpdateRecursive` runs only on the shift-press event (not per frame); negligible cost, and it mirrors `RevertToDefault`. The topology continuity stress test (`sys_theoryoftime_stress.cpp`) guards against regressions in clock behavior.
- **[Tests too strict on slew tolerance]** → reuse the established `kTol` (5e-3 for value round-trips, 1e-2 for gesture math) and `kSettleFrames`, matching existing passing tests.

## Migration Plan

Additive: two small source fixes (`HandleShiftPress` and the gesture-delete `ClearGesture` path) plus two new test files and three delta specs. No data migration, no rollback concern — reverting the commit restores prior behavior. Land after `private/test` builds green and both repro tests (encoder shift-reset, gesture-button delete) fail on the pre-fix code and pass after.

## Open Questions (resolved during apply)

- **Is `"TheoryOfTimeMult"` scene-banked?** YES. The topology test proved it: setting bit 0 → 5 in scene 0, then activating scene 1, reverts the live value to scene 1's own (default 2); switching back to scene 0 restores 5. Each scene remembers its own multiplier and both survive a save/load round-trip. (This is the "remembers scenes across saved patches" behavior the requester wanted.)
- **Reset-whole-page affordance?** No dedicated per-page reset button found. Topology defaults are restored by the global `ResetToDefaults` → `TheNonagonSmartGrid::RevertToDefault` → `StateSaver::RevertToDefaultForScene` path, verified by the topology save/load test's reset step.
- **Front-door nested modulation?** The depth cell of a base parameter is reachable (PressEncoder selection → depth cell → deselect), and shift-reset clears it. Modulating a depth cell *itself* (true depth-2 nesting) is not front-door reachable via `SynthRig` (matches the note in `sys_gestures.cpp`); the reachable modulator + gesture clears are tested instead.
