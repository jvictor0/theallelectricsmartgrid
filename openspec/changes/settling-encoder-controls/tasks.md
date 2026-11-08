## 1. Lean first

- [x] 1.1 Prove the fixed-coefficient one-pole's bounds, contraction, exact finite settling and idle behavior.
- [x] 1.2 Separate historical/current control policies while preserving shared contracts and historical source correspondence.
- [x] 1.3 Update runtime, motion, reset, gesture and patch-load proofs and contract manifests; pass the Lean audit before editing production C++.

## 2. Implementation

- [x] 2.1 Replace adaptive tick ramps with double-precision 20 ms input smoothing and explicit moving-scene tracking.
- [x] 2.2 Preserve final publication and nested invalidation, and stop smoothing/recursive refresh when the tree settles.
- [x] 2.3 Update regression tests for burst edits, reversals, endpoints, patch/gesture state, idle behavior and modulation preservation.

## 3. Validation and completion

- [x] 3.1 Review source/model correspondence, update documentation and pass formal/core checks.
- [x] 3.2 Replay the recorded knob events and compare audible artifacts and audio-rate modulation.
- [x] 3.3 Measure settled and moving encoder CPU, preserve results, and finish the change through the repository workflow.

Validation and measurements: [report](../../../docs/encoder-control-smoothing-validation.md).
