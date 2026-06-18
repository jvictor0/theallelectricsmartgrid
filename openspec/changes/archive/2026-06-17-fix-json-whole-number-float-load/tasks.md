## 1. Regression Tests

- [x] 1.1 Keep the end-to-end failing patch round-trip test for default whole-number float params in `private/test/system/sys_patch_roundtrip.cpp`.
- [x] 1.2 Add or update JSON unit coverage proving the numeric float read path returns `1.0` for parsed `1`, `1.0`, and exponent-form numeric tokens.
- [x] 1.3 Run the targeted end-to-end test and confirm it fails before the production fix with `MasterVolume`, `LPCutoff`, and `Source1LP` loading as `0.0`.

## 2. JSON Numeric Accessor

- [x] 2.1 Add an explicit numeric floating-point accessor to `private/src/Json.hpp` that accepts both `JsonType::Real` and `JsonType::Integer`.
- [x] 2.2 Preserve strict `IntegerValue()` and `RealValue()` semantics unless the implementation intentionally updates all dependent tests and callers.
- [x] 2.3 Verify the accessor performs no allocation and only reads the current JSON node.

## 3. Encoder Load Integration

- [x] 3.1 Update `StateEncoderCell::FromJSON` in `private/src/Encoder.hpp` to use the numeric float accessor for stored normalized values.
- [x] 3.2 Audit remaining `RealValue()` call sites and update only schema fields that semantically expect numeric floats from persisted JSON.
- [x] 3.3 Keep discrete `StateSaver` and config-grid integer reads on strict integer accessors.

## 4. Verification

- [x] 4.1 Run the new JSON numeric unit coverage and confirm it passes.
- [x] 4.2 Run `build/private-test/smartgrid_tests --test-case="sys_patch_roundtrip: default-nonzero params survive save/load"` and confirm it passes.
- [x] 4.3 Run the existing JSON and patch persistence target set: `sys_patch_roundtrip:*`, `sys_save_arena:*`, and `json_arena:*`.
- [x] 4.4 Run `git diff --check` and resolve any whitespace issues.
