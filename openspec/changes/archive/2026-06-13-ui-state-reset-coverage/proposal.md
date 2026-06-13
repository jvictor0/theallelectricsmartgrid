## Why

Smart Grid One recomputes encoder outputs lazily: a cell's post-modulation value is only recalculated on a control frame when its force-update flag is set or when an *affecting* modulator/gesture changed (`BankedEncoderCell::Compute`, `EncoderBank.hpp:755`). This is a deliberate optimization — "we don't check the encoder configuration every sample or frame." But it means any edit that clears or rewrites a cell's modulation configuration MUST invalidate that lazy cache, or the cell keeps publishing a stale value forever.

Two concrete bugs expose the gap, both the same omission in different reset paths:

- **Bug 1 — shift-press the encoder.** Shift-pressing a modulated encoder is supposed to reset it, and it *does* clear the modulators (and any linked gestures, via `ZeroModulators` → `DeactivateGesture`) and recompute the affecting bitmasks (`HandleShiftPress` → `SetModulatorsAffectingRecursive`, `EncoderBank.hpp:656`) — but it never calls `SetForceUpdateRecursive()`. Once the affecting bitmask is empty, nothing ever re-triggers `Compute`, and `m_output` stays frozen at the last modulated/gestured value.
- **Bug 2 — shift-press the gesture button (deleting a gesture).** The gesture-selector shift path (`GestureSelectorCell::OnPress` with shift → `…ClearGesture` → `EncoderBankInternal::ClearGesture`, `EncoderBank.hpp:1349`) deactivates the gesture and recomputes the affecting masks via `SetAllModulatorsAffecting()` (`EncoderBank.hpp:1301`) — but that helper, like `HandleShiftPress`, **never sets force-update**. Deleting a gesture leaves the encoder frozen at its gestured value instead of reverting to the ungestured base. This is the "similar bug" we set out to verify, and it is real.

In both cases the UI mask says "no modulation / no gesture" while the value (and `GetValue`) still reflects it — the two disagree forever. `RevertToDefault` (`EncoderBank.hpp:557`) and the `EncoderSet` path (`:1587`, with a comment naming the required `SetForceUpdateRecursive + SetModulatorsAffecting` bookkeeping) get it right; the two reset paths above do not.

There is currently no assertive test for either path (the existing `sys_gestures.cpp` Test 6 shift-presses a gesture pad but only checks NaN-cleanliness, not the value), so both regressions are invisible. This change fixes both bugs and — more importantly — builds the regression net the user asked for, structured as **orthogonality matrices**: each independent axis (reset method × what's configured × analog-input/gesture-weight position × scene/blend × patch round-trip) is varied independently, and every cell asserts BOTH the published UI state AND the actual computed value after settling frames, so this class of cache-coherence bug "can never happen again."

## What Changes

- **Fix both shift-reset cache bugs**: `BankedEncoderCell::HandleShiftPress` (Bug 1) and the gesture-button clear path `EncoderBankInternal::ClearGesture` (Bug 2) invalidate the lazy recompute cache (set force-update) after clearing modulation/gestures, so the published value and UI mask agree. Tighten the spec so EVERY config-mutating reset path carries this obligation.
- **Encoder reset/shift regression tests** (new `sys_encoder_reset.cpp`), structured as orthogonality matrices: shift-press encoder reset (modulator-only, gesture-only, both, nested), shift-press gesture-button delete, shift-press with a gesture selected, reset combined with scenes and A/B blend, reset interacting with save/load — each across analog-input/gesture-weight positions {down=0, middle=0.5, up=1}, each asserting `EncoderValue`/`GetValue` AND `EncoderBankUIState` (modulatorsAffecting / gesturesAffecting / brightness) after settle frames, AND that a post-clear analog-input sweep no longer moves the encoder (the gesture is truly gone).
- **Theory of Time topology grid tests** (new `sys_theoryoftime_topology.cpp`): pressing topology pads sets per-loop multipliers (`m_input[i].m_parentMult`), verifying the underlying clock topology value changed, the grid light/color reflects it, page/whole-grid reset behavior (if available), persistence across save/load, and behavior across scene switches.
- **Shift-command coverage matrix**: enumerate and test every Shift command reachable through `SynthRig` (shift+encoder reset, shift+gesture-pad clear, shift+scene-pad copy-to-scene, plus any others discovered during apply), asserting post-command value + UI state.
- **Spec hardening**: add requirements/scenarios pinning (a) cache invalidation on reset, (b) the topology-grid control/persistence contract, (c) discrete cell/grid state surviving scene switches and patch round-trips.

## Capabilities

### New Capabilities
<!-- none — all behaviors map to existing capabilities; this change hardens and tests them -->

### Modified Capabilities
- `encoder-parameter-system`: Strengthen "Change-Driven Recompute on Control Frames" so configuration-mutating resets (shift-press reset, gesture-button clear, revert, JSON load, absolute set) MUST set force-update; add an explicit "Shift-Press Reset Semantics" requirement covering the encoder shift-press branches (including clearing linked gestures); add a "Gesture Removal Invalidates the Recompute Cache" requirement covering the gesture-button delete path and correct behavior across analog-input/gesture-weight positions — all with scenarios that assert published value and affecting-bitmask coherence after settle frames.
- `phasor-timebase`: Add a "Theory of Time Topology Grid Control" requirement covering how topology pads set per-loop `m_parentMult`/`m_parentIndex`, that the grid light reflects the active value, page/grid reset behavior, and persistence across save/load.
- `scene-state-management`: Add scenarios pinning that encoder reset/shift operations and discrete grid-cell state remain coherent across scene switches and patch save/load round-trips (the cross-cutting combinations the user called out).

## Impact

- **Source**: two small behavioral fixes in `private/src/EncoderBank.hpp` — `HandleShiftPress` and the `ClearGesture` path each add the missing force-update so the published value follows the cleared configuration; no API or serialization changes. Any further bugs uncovered while writing tests are fixed in place and listed in the change report.
- **Tests**: new `private/test/system/sys_encoder_reset.cpp` and `private/test/system/sys_theoryoftime_topology.cpp`, built on the frozen `SynthRig`/`TimeRig` fixtures; possibly a small read-only accessor for topology-grid cell color if not already exposed.
- **Specs**: delta specs for `encoder-parameter-system`, `phasor-timebase`, `scene-state-management`.
- **Risk**: low. The fix matches the existing `RevertToDefault` bookkeeping; tests are additive and deterministic (single-threaded fixtures).
