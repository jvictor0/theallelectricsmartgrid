## Why

The compact phase inverse performs excessive stores and publication work while crossing many grid cells. Preserve its density, memory budget, branch semantics and coherent UI reads while recovering audio-thread CPU time.

## What Changes

- Prove omitted inactive-field stores, direct complete-cell traversal and bounded publication batches before changing their C++ implementation.
- Optimize only inverse computation; retain the existing clocks, scope readers, UI, delay processing and phase density.
- Compare the landed inverse with each proposed optimization on studio.local, including the full normal-to-pathological range and reader contention.
- Retain only measured improvements, with source correspondence, regression tests and documented results.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `warped-scope-history`: permit proved bounded batches of inverse cell transitions while preserving selected mappings, publication cutoffs, fixed storage and nonblocking writers.

## Impact

Production changes are confined to `private/src/TimeWarpInverter.hpp`. Supporting changes affect its Lean model, source correspondence, inverse tests, benchmark harness and documentation. No deployment or landing is included in this task.
