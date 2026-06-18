## Context

The arena JSON migration replaced the prior dual JSON backends with `private/src/Json.hpp`. Encoder state saves every normalized base value as a JSON real node, but serialization uses `%.17g`, so exact whole-number floats such as `1.0` are emitted as JSON text `1`. On load, `ParseNumber` classifies `1` as an integer node because it has no decimal point or exponent. `StateEncoderCell::FromJSON` then calls `RealValue()`, whose current behavior is strict by node type and returns `0.0` for integer nodes.

The bug therefore lives at the boundary between JSON token spelling and a schema field that semantically expects a float. It is visible in patch load for default non-zero whole-number encoder params such as `MasterVolume`, `LPCutoff`, and `Source1LP`.

The fix must preserve the realtime save constraints: audio-thread serialization still builds only into the caller-owned arena and must not call the heap allocator.

## Goals / Non-Goals

**Goals:**

- Restore whole-number normalized float values from patch JSON without changing the on-disk patch format.
- Keep compatibility with existing patches containing either integer-looking numeric tokens (`1`) or real-looking numeric tokens (`1.0`, `1e0`).
- Keep strict integer handling for discrete byte/state fields that intentionally use `IntegerValue()`.
- Add regression coverage at both the JSON numeric boundary and the end-to-end patch load boundary.

**Non-Goals:**

- Do not redesign the JSON parser or collapse the internal integer/real node distinction.
- Do not change patch save scheduling, arena ownership, or the `StateInterchange` retry protocol.
- Do not migrate existing patch files.

## Decisions

1. Add an explicit numeric float accessor in `Json.hpp`.

   Add a method such as `double NumberValue() const` or `double NumericValue() const` that returns the stored value for both `JsonType::Real` and `JsonType::Integer`, and returns `0.0` for non-numeric or null nodes.

   Rationale: JSON syntax does not distinguish application-level "float field" from "integer field"; `1` and `1.0` are both valid numeric encodings for a normalized float. A dedicated accessor makes that contract explicit without changing the existing strict `RealValue()` and `IntegerValue()` behavior.

   Alternative considered: Make `RealValue()` accept integer nodes. This would be smaller, but broader: existing tests document strict getters and future callers may rely on `RealValue()` as a type check.

   Alternative considered: Force the dumper to emit `1.0` for real nodes. This would fix newly saved patches, but would not load existing files or hand-edited JSON containing `1` in float fields.

2. Update encoder float deserialization to use the numeric accessor.

   In `StateEncoderCell::FromJSON`, replace `sceneValues.GetAt(j).RealValue()` with the new numeric accessor. This is the schema point where JSON values are known to be normalized float parameter values.

   Rationale: The bug affects encoder float state, not discrete `StateSaver` byte arrays. Changing the consuming schema code is narrower than globally loosening integer reads.

3. Cover the failure at two levels.

   Keep the end-to-end patch regression in `sys_patch_roundtrip.cpp` that saves a fresh patch and loads it into a fresh rig, then asserts default whole-number float params survive as `1.0`.

   Add or update a JSON unit test to prove the numeric accessor reads both parsed `1` and parsed `1.0` as `1.0`, while strict `IntegerValue()` / `RealValue()` behavior remains documented if retained.

## Risks / Trade-offs

- [Risk] A future float field outside encoder state may still call strict `RealValue()` and mishandle whole-number JSON tokens. -> Mitigation: name the new accessor clearly and use the JSON unit test as guidance; audit current `RealValue()` call sites during implementation.
- [Risk] Changing the dumper alone would look appealing but leave existing integer-looking float tokens broken. -> Mitigation: implement the fix in the load path via numeric accessor.
- [Risk] Broadly changing `RealValue()` could mask type mistakes in code that wanted strict real nodes. -> Mitigation: keep strict accessors and opt into numeric coercion at float schema boundaries.

## Migration Plan

No file migration is required. Existing patches that encode normalized float fields as `1`, `1.0`, or exponent forms should load through the same patch path once encoder deserialization uses the numeric accessor.

Rollback is a normal code revert; no persistent data transformation is introduced.

## Open Questions

None.
