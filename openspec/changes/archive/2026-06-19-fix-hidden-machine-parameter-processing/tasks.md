## 1. Shared-State Wiring Refactor

- [x] 1.1 Temporarily disable `sys_gestures: hidden machine-dependent detune catches gesture weight changes` so the wiring refactor can land before the behavior fix.
- [x] 1.2 Move `BankedEncoderCell::SharedEncoderState` ownership from `EncoderBankInternal` to `EncoderBankBank::BankMode`.
- [x] 1.3 Wire every named encoder cell to its mode-level shared state during `EncoderBankBank::CreateEncoder`.
- [x] 1.4 Update `EncoderBankInternal::PlaceEncoder` so placement remains a bank-view operation and does not become the only source of shared-state wiring.
- [x] 1.5 Build `smartgrid_tests` and run existing encoder/gesture focused tests to verify the transparent reflector does not change visible behavior yet.

## 2. Hidden-Parameter Regression Tests

- [x] 2.1 Add a focused helper in `private/test/system/sys_gestures.cpp` or a nearby system test file that configures Water as VCO, Earth as Sample, selects the Source bank, and exposes `OscillatorDetune` at `(1, 3)`.
- [x] 2.2 Add and run a failing processing test showing `OscillatorDetune` misses a gesture-2 Spread-depth weight change while hidden by Earth's Sample topology.
- [x] 2.3 Add and run a failing modulation-source test showing a hidden machine-dependent parameter misses a changed modulation source while hidden.
- [x] 2.4 Add and run a failing gesture-clear test showing `ClearGesture` does not remove an active gesture from a hidden machine-dependent parameter.
- [x] 2.5 Add and run a reset-scope test showing a hidden machine-dependent parameter is not reset by the selected bank's visible grid reset.
- [x] 2.6 Add and run a failing scene-copy test showing a hidden machine-dependent parameter keeps stale scene state after a copy operation.

## 3. Mode-Level Processing

- [x] 3.1 Change `EncoderBankBank::Process` to compute changed modulator/gesture masks per mode, then loop the flat `m_encoders[]` owner array and call `Compute()` for every connected named encoder cell.
- [x] 3.2 Keep bank topology processing for view metadata only, so selected-bank UI placement remains machine-filtered.
- [x] 3.3 Run the hidden processing and modulation-source tests and verify they pass.

## 4. Mode-Level Bulk Operations

- [x] 4.1 Change gesture clearing so `EncoderBankBank::ClearGesture` reaches all named encoder cells, including cells currently hidden from bank topology.
- [x] 4.2 Keep selected-bank grid reset scoped to visible cells, while full patch default reset still reaches all named encoder cells.
- [x] 4.3 Change scene copy paths so hidden machine-dependent parameters in the affected scope copy scene state consistently with visible parameters.
- [x] 4.4 Recompute affecting gesture/modulator summaries for hidden semantic state while keeping selected-bank UI summaries limited to visible cells.
- [x] 4.5 Run the hidden gesture-clear, reset-scope, and scene-copy tests and verify they pass.

## 5. Re-enable Repro and Verify

- [x] 5.1 Re-enable `sys_gestures: hidden machine-dependent detune catches gesture weight changes`.
- [x] 5.2 Run the full hidden machine-dependent parameter focused test set and verify all tests pass.
- [x] 5.3 Run `cmake --build private/test/build -j 8` and `private/test/build/smartgrid_tests`.
- [x] 5.4 Run `git diff --check` and inspect the final diff for unrelated changes.
