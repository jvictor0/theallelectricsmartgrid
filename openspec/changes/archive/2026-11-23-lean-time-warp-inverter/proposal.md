## Why

Theory of Time's inverse has checked contracts and a separately maintained C++ implementation. Authoring its read and write rules in Lean lets the same definitions drive the proofs and the app's generated core.

## What Changes

- Make the fixed-cell inverse executable in Lean and connect its transitions and lookups to the existing phase and publication contracts.
- Generate the inverse core for the existing native fixed-capacity atomic ring, with the current delay and scope interfaces.
- Check generated code and numeric parity against native Lean and the current C++ inverse. Measure writer cost and reader contention before switching the app.

## Capabilities

### New Capabilities

### Modified Capabilities

- `warped-scope-history` gains a checked executable inverse and generated runtime contract.

## Impact

PhaseTime proofs, Lean C++ code generation, the two Theory of Time inverse instances, and the inverse validation harness.
