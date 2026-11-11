## Context

The landed inverse uses a 32-byte fallback/replacement cell and a global sequence counter. Every grid edge completes the previous cell, clears five next-cell fields, overwrites the next entry, and commits a frontier pair. Fast forward travel amplifies these stores. Current mathematical proofs assume sequentially consistent atomics; their progress condition is an eventual quiet reader attempt.

## Goals / Non-Goals

**Goals:** Preserve selected inverse values and availability after every Write, phase density, retention, partial-frontier behavior, seed handling, and replacement/coalescing semantics. Reduce work per crossed cell. Prove the transformations before native edits, then measure them serially on studio.local.

**Non-Goals:** No changes outside inverse computation in production; no LFO filtering, UI/scope/delay changes, transport fixes, density reduction, or deployment. Do not benchmark pre-concurrency historical versions.

## Decisions

1. Keep atomic payloads and the current sequence-counter read contract. First omit stores only when the read discriminant makes those fields unreachable; subsequent activation must initialize them before commit.
2. Handle the first edge with the existing partial/replacement logic. Fully crossed interior cells can be assigned their complete fallback endpoints and an invalid replacement directly. The final entered cell remains open against the frontier. Prove equivalence of direct assignment and enter/finish, including ring aliasing and endpoint behavior.
3. Publish a small fixed number of complete-cell transitions per revision. Each committed batch must equal a prefix of the original traversal, and the total write work per odd section must have a fixed bound. Keep sequential consistency for this experiment. Choose batch size using measured write cost and reader behavior.
4. Use the landed header as a frozen differential and performance reference. Keep cases identical, alternate implementations, preserve hashes, compiler flags, raw samples, and run all timings remotely. Reuse one remote checkout and stable Release directory; no local timing conclusions.
5. Any weaker-memory-order experiment requires its own checked event-order model before C++ changes. Keep only demonstrated improvements; do not weaken the existing proof silently.

## Risks / Trade-offs

- Inactive stored bytes differ → prove observational equivalence and require initialization on activation; compare query results rather than dead payload bytes.
- Direct writes can change boundary-touch or seed semantics → explicit endpoint, wrap and startup proofs and differential cases.
- Batching increases reader retries → bounded batch theorem, contention regressions and remote contention measurements.
- Background remote work distorts timings → record process activity; use repeated serial paired timings and report limitations.

## Measured capacity decision

The initial Studio sweep compared omitted stores alone and direct-cell batches of 4, 8 and 16 edges. Eight edges retained most of the throughput gain while doing better on ordinary fast traces and saturated-reader latency than sixteen. The sixteen-edge version saved another roughly 7–8% in the demanding traces, but ordinary linear/gentle/blend cases regressed relative to eight, and reader tail latencies increased. The selected capacity is eight; all accesses retain sequential consistency. The model is capacity-independent, and the final full-range measurement uses the selected native header.
