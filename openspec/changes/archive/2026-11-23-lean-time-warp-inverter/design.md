## Context

The inverse selects a forward traversal in each fixed grid cell and translates logical phase to absolute engine sample time. The audio thread writes the inverse, and delay and scope readers consume it through `TimeWarpSource`. The existing proofs describe certified real-number pieces, bounded batches, and coherent scalar publication.

## Goals / Non-Goals

- Transfer the sampled-phase and publication contracts to executable Lean transitions and lookups.
- Generate native operations for both inverse capacities with their existing storage and interfaces.
- Use the filter project's proof and validation boundary. Lean checks the mathematical model and the connection to the executable algorithm. Native comparisons check floating-point execution and code generation. The atomics supply the sequentially consistent access model.

## Decisions

- Reuse `FixedCell` for the selection and interpolation contract and `WriteOptimization` for direct writes and batch prefixes. Prove the executable representation preserves the relevant invariants and maps its reads to certified pieces.
- Use signed grid indices, a fixed-capacity ring, and a batch of at most eight crossed edges. The storage lowering supplies bounded native scratch space and lock-free atomic scalar accesses under the existing revision protocol.
- Test the generated inverse side by side with the frozen C++ behavior. Include negative phases, wraparound, exact boundaries, partial cells, reversals, cutoffs, large timestamps, and concurrent reads.

## Risks / Trade-offs

- Cell occupancy and the incomplete frontier depend on traversal history. The proof must connect the physical slot and its active endpoints to the corresponding sampled traversal.
- Float timestamp deltas, endpoint arithmetic, and operation order can affect availability at cutoffs. Compare native values and missing results directly.
- Extra loads or larger revision sections can increase audio-thread cost or reader retries. Compare both against the existing implementation before wiring in the generated core.
