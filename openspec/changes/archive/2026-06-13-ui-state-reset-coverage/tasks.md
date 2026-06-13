## 1. Reproduce both bugs (test-first)

- [x] 1.1 Create `private/test/system/sys_encoder_reset.cpp` with the standard doctest + `SynthRig` includes, a `FindConnected` helper, a helper that reads `modulatorsAffecting`/`gesturesAffecting` for encoder (x,y) from `rig.UIState().m_squiggleBoyUIState.m_encoderBankUIState`, and `kSettleFrames`/`kTol` constants modeled on `sys_gestures.cpp`.
- [x] 1.2 Add reusable setup helpers: `setupModulator(rig, ex, ey)` (PressEncoder → IncEncoder depth cell (0,0) → deselect via PressEncoder(3,3)); `setupGesture(rig, ex, ey, gesture, weight)` (SetFader weight → PressGesturePad → IncEncoder → ReleaseGesturePad); and `assertResetTo(rig, ex, ey, expected)` that runs `kSettleFrames` then asserts value≈expected AND the relevant affecting mask is empty.
- [x] 1.3 Repro Bug 1 (encoder shift-reset): set up a modulator, assert the precondition that modulated `EncoderValue` ≠ base (switch modulator slot or run the sequencer if the live signal is ~0), then shift-press the encoder and assert mask empty AND value→base. Confirm this FAILS on unpatched source (mask clears, value frozen).
- [x] 1.4 Repro Bug 2 (gesture-button delete): set up a gesture so the encoder is driven off-base, assert the precondition (value ≠ base), then shift-press the gesture-selector pad to delete it (`WithShift([&]{ PressGesturePad(g); ReleaseGesturePad(g); })`), and assert gesturesAffecting bit clears AND value→base. Confirm this FAILS on unpatched source.
- [x] 1.5 Build/run `private/test` against unpatched source; confirm 1.3 and 1.4 value-assertions fail (proving both repros capture real bugs) while mask-assertions pass (proving the value/mask disagreement).

## 2. Fix both bugs

- [x] 2.1 Bug 1: add `SetForceUpdateRecursive();` to `BankedEncoderCell::HandleShiftPress` (`EncoderBank.hpp:656`) after `SetModulatorsAffectingRecursive();`.
- [x] 2.2 Bug 2: make the gesture-delete path set force-update — in `EncoderBankInternal::ClearGesture` (`EncoderBank.hpp:1349`) set force-update on each affected cell after `cell->ClearGesture(gesture)` (do NOT alter the `HandleChangedSceneManagerScene` caller of `SetAllModulatorsAffecting`, whose force-update is handled by the bulk scene refresh). Confirm no double-recompute.
- [x] 2.3 Rebuild; confirm 1.3 and 1.4 now pass; confirm `sys_gestures.cpp`, `sys_scenes.cpp`, `sys_patch_roundtrip.cpp`, `sys_theoryoftime_stress.cpp` still pass (no regressions).

## 3. Orthogonality matrix — reset/clear behavior

- [x] 3.1 Define the axes as test data: reset method {shift-encoder(no-gesture), shift-encoder(gesture-selected), shift-gesture-button, ResetToDefaults}; config {modulator, gesture, modulator+gesture, nested}; analog-input/gesture-weight {0, 0.5, 1}; scene/blend {single, pair@0, pair@0.5, pair@1}; persistence {in-session, after save/load}.
- [x] 3.2 Reset-method × config (parameterized loop): for each (method, config) build the config, apply the method, assert value→expected (ungestured/unmodulated base, or remaining-modulation value) AND affecting masks match after settle.
- [x] 3.3 Reset-method × analog-input position: for each method that clears a gesture, run with gesture weight at 0/0.5/1; assert the encoder returns to the same ungestured base in all three, AND that a post-clear analog-input sweep is a no-op (gesture truly gone). (encoder-parameter-system: "Gesture deletion is correct at every analog-input position".)
- [x] 3.4 Encoder shift-press clears linked gestures: set up a gesture (no modulator), shift-press the ENCODER (not the gesture pad), assert gesturesAffecting clears and value→ungestured base — the linked gesture disappears with the encoder reset.
- [x] 3.5 Shift-press with gesture selected deactivates only the selected gesture: set up modulator + gesture, select the gesture, shift-press encoder; assert gesture deactivates while modulator survives (modulatorsAffecting still set, value reflects remaining modulation).
- [x] 3.6 Nested-modulation reset: build a depth cell that is itself modulated, shift-press the base encoder, assert the subtree is zeroed/garbage-collected, masks empty, value→base.
- [x] 3.7 Reset × scenes: modulate/gesture in scene 1 and scene 2, make scene 1 active (blend 0), apply the reset; assert scene-1→base, then switch to scene 2 and assert its config intact (scene-state-management: "Shift-reset in one scene leaves other scenes intact"). Repeat the value/mask checks at blend {0, 0.5, 1}.
- [x] 3.8 Reset × persistence: for representative (method, config) cells, apply reset → `SavePatch` → `ResetToDefaults` → `LoadPatch`; assert reloaded state matches the post-reset state (masks empty, value==base).
- [x] 3.9 Enumerate remaining Shift commands: grep `private/src` for `m_shift` / `ShiftedCell` consumers; for each reachable Shift command not yet covered (e.g. shift+scene-pad copy-to-scene) add a row asserting post-command value + UI state.
- [x] 3.10 Whole-system reset: `ResetToDefaults` after building modulation+gestures+scenes; assert all masks clear and values→defaults (replace the `WARN` checks in `sys_gestures.cpp` Test 5 with hard assertions). `log` any axis intentionally sampled rather than exhausted.

## 4. Theory of Time topology grid

- [x] 4.1 Create `private/test/system/sys_theoryoftime_topology.cpp` (doctest + `SynthRig`); confirm the topology grid route/coords (`TapPad(RouteBottomRight, i, mult-2)` per `sys_theoryoftime_stress.cpp` and `TheoryOfTimeTopologyPage::InitGrid`).
- [x] 4.2 Set multiplier: tap the value-`mult` pad for loop-bit `i`; assert `rig.Internal()...m_theoryOfTimeInput.m_input[i].m_parentMult == mult` after settle.
- [x] 4.3 Verify lights: assert the topology page publishes the value-`mult` pad as on-color (White) and the others off-color (Fuscia); add a minimal read-only color accessor to the topology page/grid if one isn't already reachable.
- [x] 4.4 Persistence: set non-default multipliers across several loop bits, `SavePatch` → `ResetToDefaults` → `LoadPatch`; assert every `m_parentMult` restored AND lights republished correctly.
- [x] 4.5 Scene behavior: determine empirically whether `"TheoryOfTimeMult"` is scene-banked (which `StateSaver` instance owns it via `m_stateSaver.Insert`), switch scenes, and pin the observed behavior with assertions.
- [x] 4.6 Loaded topology drives the clock: load a patch with non-default multipliers, run the sequencer, assert master phasor advances, loops gate, and `SawNaN`/`OutputPeak` invariants hold.
- [x] 4.7 Page/grid reset: grep for any "reset page/grid" affordance on the topology grid; if present, test it sets values + lights back to default; if only global `ResetToDefaults` exists, assert that path restores topology defaults and note the finding.

## 5. Verify, document, and finalize

- [x] 5.1 Run the full `private/test` suite; confirm all new and existing tests pass with both fixes and fail-loud (no `WARN`-only) assertions on the new coverage.
- [x] 5.2 Run the new repro/topology tests with the fuzz/heap env vars (per the test-suite-layout notes) to confirm no heap-allocation or NaN regressions.
- [x] 5.3 Write the change report: both identified bugs + their fixes, any additional bugs found while writing the matrix and how they were fixed (or flagged if complex), the orthogonality axes actually exhausted vs. sampled, and the final coverage list. Update relevant `docs/` if behavior wording changed.
- [x] 5.4 Sync delta specs to main specs (`/opsx:sync`) and prepare the change for archive.
