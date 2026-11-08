# Settling encoder controls: validation

Lean checkpoint: `81c383a`. Native implementation: `9688a9e`.

## Behavior and proof boundary

Manual scene values use a 20 ms time-constant one-pole at host rate. Double state
survives burst edits and reversals. At normalized residual at most `1e-6`, both
state and published scene value snap exactly to the target and the moving bit
clears. Idle scene values execute no pole arithmetic. Relevant descendants keep
the parent active through final publication, then the tree returns to its cached
path. Recursive traversal skips individually retired siblings so they cannot
block another moving child. Live modulation and output interpolation retain
their existing equations.

The historical linear model and current pole model have separate configurations
and runtime machines, with shared bounds, no-jump, recording, reset and finite
convergence contracts. Pole-specific proofs cover contraction, exact settlement,
masked execution and early retirement. The 15001-frame bound is conservative;
full-scale native settlement takes about 277 ms, not the full proof budget.
These are proofs of exact-arithmetic models plus reviewed source correspondence,
not a formal verification of C++ or IEEE rounding.

## Checks

- `make lean-test`: 73 contracts/classifications; 2733 theorems; only standard Lean axioms.
- `make core-test`: 838 primary tests and 36 passthrough-rate tests pass, with assertions enabled.
- Performance smoke and protocol checks pass, including deterministic knob bursts.
- Native regressions cover bursts, reversal, all scene endpoints, float-stall avoidance,
  idle publication, nested final publication, staggered sibling retirement, matching
  patch reload accumulator preservation, gesture capture and 1x/4x processing.
- The previously accepted unchanged-target idle gesture-capture defect is retained
  explicitly in both model and native regression.

System tests that inspect settled controls now wait for the new convergence bound,
without relaxed value tolerances. The benchmark protocol also had a pre-existing
catalogue mismatch: the immutable before binary already reports 333 loaded roots,
while the fixture contains 329. The protocol now compares the current catalogue
across full-rig fixtures and separately checks their 165/164 gesture/modulator split.

## Audio evidence

The isolated probe uses the production encoder matrix and VPS with a 100 Hz test
oscillator. It replays all 1720 V/D events from
`recording-2026-11-07T181531-516543-0.sgrec` (25.95848 s). These clips replay the
recorded controls; they are not full-synth reconstructions of the original mix.

Synthetic 1 Hz sweeps use 30 Hz edits with 1 ms follow-ups. After discarding the
first two seconds, the off-harmonic power metric excludes ±10 Hz around the
100 Hz harmonics and integrates 50 Hz–18 kHz after resampling to 48 kHz:

| Control | Before | One-pole | Reduction |
| --- | ---: | ---: | ---: |
| V | -38.16 dBFS | -50.97 dBFS | 12.80 dB |
| D | -45.18 dBFS | -57.90 dBFS | 12.73 dB |

The earlier deadline-preserving linear prototype reduced this particular periodic
metric by 25.61/24.81 dB. The fixed pole suppresses less in that test and provides
the requested cadence-independent response. These metrics do not prove inaudibility.
Before/after WAVs are retained for listening, with before first and a 0.5 s gap.

The 8 kHz live-modulation probe is bit-for-bit identical for both controls and audio
through all nine seconds after the one-second startup allowance. The manual depth
transition during startup deliberately follows the new filter.

## CPU evidence

Milliseconds of calling-thread CPU per second of audio, AppleClang Release/LTO,
three five-second runs per case, same replay, no concurrent builds/tests:

| Workload | Before | After |
| --- | ---: | ---: |
| matrix/flat/fixed | 90.27 | 85.42 |
| matrix/flat/knobs | 123.79 | 134.42 |
| matrix/nested/fixed | 225.03 | 196.30 |
| matrix/nested/knobs | 258.29 | 252.92 |
| matrix/gestures/fixed | 906.63 | 892.09 |
| matrix/gestures/knobs | 1101.90 | 1075.23 |

A second paired, shuffled five-repeat flat-case check measured settled controls
at 89.29 → 85.28 ms/sec and knob bursts at 123.15 → 132.01 ms/sec. The moving-case
increase is about 8.86 ms/sec (0.89% of one CPU core), with edits to every sixteenth
named parameter. These are desktop throughput measurements, not iPad callback
maxima; small timing differences should not be treated as precise speedups.

## Reproduction artifacts

Preserved outside the worktree in the main checkout's ignored
`build/results/settling-encoder-controls-20261107/`: audio renders and WAVs,
`analyze.py`, exact probe sources, raw benchmark runs, immutable before binary,
provenance, and validation logs. The original event/stem evidence remains in
`build/results/vps-knob-zipper-20261107/`.

The before benchmark binary was built against `81c383a` production headers
(unchanged from `abed397`), with the new burst harness. The runner captured its
working-tree metadata after source editing had begun; that dirty snapshot is
not the binary's source revision. Its immutable binary SHA-256 and explicit
source/harness provenance identify the actual baseline. The after run uses the
clean `9688a9e` implementation.
