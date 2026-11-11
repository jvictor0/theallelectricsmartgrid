# Inverse write optimization — 2026-11-11

The selected eight-edge batch reduces the most expensive case from 70.705 to
30.090 ms of inverse CPU per audio second on `studio.local` (57.4%). Across the
120 cases, the median case reduction is 36.5%. All 113,307,324 differential
queries match the landed inverse exactly, including absent results and cutoffs.
Ten cheap cases have slightly higher medians; the largest increase is 1.7%, or
0.0126 ms per audio second. This is a substantial reduction in the regression,
not removal of the inverse's work proportional to crossed cells.

## Scope and method

- Baseline: landed `de51539eb16f929321c7a3dec275be5b051424ac`, frozen in
  `LandedTimeWarpInverter.hpp`. Only its type name and provenance comment differ.
- Production edits: `private/src/TimeWarpInverter.hpp` only. Density, retained
  history, branch selection, reads, and sequentially consistent atomics stay the
  same. G/H payloads remain 32/16 MiB.
- Host: `studio.local`, Apple M3 Ultra, 28 CPUs, macOS 26.2 arm64.
- Build: root `make perf-build`, stable `build/tools/perf/Release`, AppleClang
  17.0.0.17000604, `-O3 -DNDEBUG -funroll-loops`, LTO.
- All timing ran serially on the Studio. Existing `omlx` and system services
  were left running; process snapshots are retained. No iPad or whole-engine
  timing is inferred from these inverse microbenchmarks.
- Full sweep: 22 warp settings at periods 2^16, 2^18, 2^20, 2^24 engine samples,
  plus eight settings at 16.1, 17.3, 19.7, 23.9 period bits: 120 cases total.
  Each trace spans at least two audio seconds and 3.25 master cycles. Settings
  include linear, gentle, ordinary blends, all-2/3/4/5 ratio chains, full depth,
  multiplier 16, both extreme skews, global-weight bursts, and Fourier traces.
- Performance: five repeats, alternating landed/current order. Measurements use
  writer-thread CPU time, exclude initialization/trace generation and the first
  half-second, and include four delay-like reads per 1,024 host-frame block.
  Reported CPU values are medians over repeats. At 48 kHz host / 192 kHz engine,
  period 2^16 is 2.9296875 master cycles/second; 2^24 is 87.38 seconds/cycle.
- Correctness: both writers receive the same observations. Queries cover a
  periodic display, the live cell/frontier, four delay heads, and captured cutoffs.
  Values compare exactly, without a numeric tolerance; NaN matches NaN.

## Measured results

CPU units are milliseconds per audio second. Positive savings mean less CPU.

| Case | Landed | Selected | Saving |
| --- | ---: | ---: | ---: |
| `linear/period2^16` | 3.230 | 1.836 | 43.2% |
| `gentle/period2^16` | 2.881 | 1.828 | 36.6% |
| `blend/period2^18` | 1.706 | 1.272 | 25.5% |
| `bad_fours/period2^16` | 35.431 | 15.100 | 57.4% |
| `default_full_mult/period2^16` | 69.350 | 29.234 | 57.8% |
| `hard_twos_reverse_skew/period2^16` | 70.253 | 29.292 | 58.3% |
| `global_full_mult/period2^16` | 70.705 | 30.090 | 57.4% |
| `default_full_mult/period2^24` | 0.909 | 0.880 | 3.1% |
| `bad_fours/period2^24` | 0.747 | 0.759 | -1.7% |

The highest median per-run maximum block CPU across cases falls from 1.543 to
0.687 ms per 1,024 host-frame block. The absolute observed maxima across all
cases/repeats fall from 2.002 to 0.927 ms. These maxima are observations, not
execution-time guarantees.

The small slow-case cost is reported rather than dismissed as noise. A targeted
nine-repeat follow-up reproduces `bad_fours/period2^24` at 0.746887 → 0.757503
ms/s: +1.42%, or +0.010616 ms/s. Slow linear is effectively unchanged in that
follow-up: 0.591716 → 0.592122 ms/s (+0.069%). Checksums and availability still
match in every timing run.

## Which changes helped

Entering a cell now writes its base, missing fallback endpoint and missing cut.
It does not clear then overwrite the base, or clear two relative timestamps
hidden by the missing cut. A later replacement initializes those timestamps.
This alone saves about 26% in the demanding tuning cases.

A complete interior cell directly stores both fallback endpoints and its missing
cut. Up to eight edges share a revision and final frontier. The first edge still
finishes the existing partial/replacement cell; the final cell remains open.
Interpolation is prepared before the odd revision, using the original expression.
A typical full eight-edge batch uses 29 atomic stores, versus about 88 in the
landed loop, excluding rare seed retirement. The conservative proved bound is
37 stores per batch. There is no allocation or weaker-memory-order experiment.

The direct-cell and batching changes are coupled: fusion needs an interior cell
within a batch. They are not presented as independent attributable savings.

| Tuning case, ms CPU/s | Fewer stores only | Batch 4 | Batch 8 | Batch 16 |
| --- | ---: | ---: | ---: | ---: |
| Linear, fastest | 2.649 | 2.144 | 1.833 | 1.981 |
| Gentle, fastest | 2.326 | 2.010 | 1.828 | 1.888 |
| Bad fours, fastest | 26.162 | 19.191 | 15.094 | 13.879 |
| Hard twos reverse skew, fastest | 52.074 | 37.331 | 29.212 | 27.038 |
| Global full multiplier, fastest | 52.678 | 38.869 | 30.076 | 27.692 |

Eight was selected because sixteen's additional 7–8% saving on heavy traces came
with worse ordinary fast-case and saturated-reader latency results.

## Contended reads

A separate stress test runs an unthrottled reader concurrently with an
unthrottled writer. This is deliberately more contention than a UI paint loop.
It includes startup and has no half-second warmup. Reader latency includes its
native retry loop and scheduling; it is not a measurement of an odd section's
duration. Values below are medians of five runs' p99 latencies.

| Fastest-period case | Landed reader p99, µs | Batch 8 reader p99, µs | Landed writer ms/s | Batch 8 writer ms/s |
| --- | ---: | ---: | ---: | ---: |
| Gentle | 1.459 | 3.583 | 15.51 | 11.60 |
| Hard twos reverse skew | 148.333 | 14.917 | 239.12 | 64.54 |
| Global full multiplier | 235.459 | 3.417 | 242.22 | 69.91 |

Reader latency does not improve in every case: gentle's p99 rises by about 2 µs.
The heavy cases improve substantially. The selected version's largest observed
individual read in these runs is 126.292 µs. Progress still assumes an eventual
quiet attempt; neither the old nor new protocol guarantees a wall-clock bound.

## Proof and native validation

`formal/PhaseTime/WriteOptimization.lean` and its axiom audit passed before native
optimization. The model proves inactive-payload equivalence, initialization upon
activation, preservation through scalar traversals, complete-cell fusion, seed
retirement, arbitrary slot aliases, boundary touches, batch-prefix equivalence,
capacity/progress bounds, and reuse of the existing SC snapshot theorem.
Reviewed anchors and full-file hashes connect the model to C++. This remains a
mathematical model with reviewed correspondence, not machine-code verification.

- `make lean-test`: 35 phase contracts / 570 theorems, plus 73 encoder contracts;
  both axiom audits pass using only standard Lean axioms.
- `make core-test`: 918 main tests and 39 alternate-rate tests pass, four existing
  skips. New regressions cover startup, stale inactive payloads, replacement
  activation, exact touches, cutoff changes, large origins, negative phase,
  randomized jumps, tiny/non-power-of-two rings, and concurrent batch reads.
- Full remote differential: 120 cases, 113,307,324 queries, zero mismatches.
- Full remote performance: 120 cases × five repeats × two implementations;
  identical checksums and unavailable counts for every paired repeat.
- `openspec validate optimize-inverse-writes --strict`: passes.

## Reproduction and retained artifacts

The final inverse SHA-256 is
`c9cbbb93f5cdbf8ff3d845446fc0bb46a8d5e44917dbf00f2ab913c824685bb5`.
The final Studio binary SHA-256 is
`de9571251dedbdc212999277f8cd5f56effff93fefc8fc2f72e38fedeb5d4740`.

Raw JSONL, aggregate JSON, metadata, source snapshots, staged binaries and proof
logs are retained locally under `build/results/inverse-optimization/` and on
the Studio under
`/Users/joyo/benchmarks/smartgrid-inverse-writes/repo/build/results/inverse-optimization/`.
The source bundle and dedicated remote checkout retain the landed baseline.
The two new `write-*` modes compare against the landed inverse; the generic
`oracle` metadata label in the original saved run was inherited from the older
benchmark and has been corrected for future runs. Saved source snapshots preserve
the exact runner used for this measurement.

Run from the dedicated Studio checkout, serially:

```sh
export PATH=/opt/homebrew/bin:/usr/local/bin:$PATH
make perf-build JOBS=4
python3 private/test/perf/run_density.py --mode write-correctness --output build/results/inverse-repeat
python3 private/test/perf/run_density.py --mode write-performance --output build/results/inverse-repeat
python3 private/test/perf/run_inverse_stages.py --label repeat-eight
```

The runners refuse to replace completed result directories. The stage runner
measures the currently built header; retained per-stage source snapshots identify
the fewer-store and 4/8/16-edge variants.
