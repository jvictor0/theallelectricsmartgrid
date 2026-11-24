## 1. Executable inverse and proofs

- [x] 1.1 Implement the Lean lookup and writer transitions, including the live frontier, seed, signed ring addressing, and bounded scatter batches.
- [x] 1.2 Prove the lookup and writer transitions refine the certified-cell and batch models. Add the executable contracts to the PhaseTime audit.

## 2. Generate and compare

- [x] 2.1 Lower the Lean operations to native fixed-size storage and sequentially consistent atomic access. Check regeneration of the committed header.
- [x] 2.2 Compare native Lean, generated C++, and the frozen inverter across representative and adversarial traces.

## 3. Integrate and verify

- [x] 3.1 Measure writer cost and contended reads using the existing inverse benchmark. Verify the generated core preserves the native constraints.
- [x] 3.2 Wire both Theory of Time inverses to the generated core and pass the relevant Lean, core, visualizer, and OpenSpec checks.

## Validation

- The replay test matched 52,228 reads across native Lean, generated C++, and the frozen C++ reference.
- The paired inverse benchmark compared the generated core against the pre-change eight-edge writer from `6330bf8`. It matched 885,372 reads in 28 trace cases.
- Nine serial paired runs on the development Mac measured eight writer cases and three saturated-reader cases. The demanding writer medians stayed within 0.5%. The two blend cases took an extra 0.082 and 0.078 ms of writer CPU per audio second. Reader p99 varied under synthetic contention. Raw results are under `build/results/lean-time-warp-inverter/`.
- `make lean-test`, `make core-test`, `make visualizers-test`, and strict OpenSpec validation passed. The PhaseTime audit checked 37 contracts and 640 theorems.
- The paired Release microbenchmark on the iPad Air (M3) matched 600,216 exact inverse reads in each suite. Repeating all cases with a measured 2.030–2.101 second post-launch wait preserved the heaviest writer result at +0.71%, or about 0.23 ms of CPU per simulated audio second. The largest added writer cost across the delayed cases was 0.41 ms per simulated audio second. In the balanced 20-pair heavy-contention run after the wait, reader p99 was 438 µs before and 436 µs with the generated inverter. Raw reports are under `build/results/lean-time-warp-inverter/ipad/` in the benchmark worktree.
