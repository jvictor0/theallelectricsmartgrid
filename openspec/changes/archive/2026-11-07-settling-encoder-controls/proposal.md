## Why

Recorded knob edits arrive in bursts. The current interval-based linear ramp retimes unfinished movement to as little as 2 ms when a small follow-up edit arrives, producing zipper sidebands in vector phase shaping. Manual controls need a fixed response that settles and stops consuming smoothing work while preserving audio-rate modulation.

## What Changes

- Replace adaptive linear tick distribution with a 20 ms one-pole on stored scene values, before modulation evaluation.
- Retain filter state across edits, use double precision internally, and land exactly when normalized error is at most `1e-6`.
- Process only moving scene values and return settled trees to the existing cached evaluation path after their final publication.
- Update and verify Lean first: preserve the pinned historical model, share behavioral requirements, and prove the current settling behavior before implementing C++.
- Verify burst edits, reversals, exact settling, nested controls, patch restoration, modulation preservation, and idle CPU behavior.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `encoder-parameter-system`: Fixed input smoothing, finite exact settling, and no smoothing work for settled controls replace the adaptive linear-ramp contract.

## Impact

Encoder leaf state and evaluator invalidation, encoder tests and benchmarks, the Lean models and source correspondence audit, and encoder documentation. Patch target serialization and modulation/output interpolation retain their existing formats and behavior. No new runtime dependency.
