# Tape inverse versus current, both at 192 kHz — 2026-11-11

This is the requested comparison: the original tape-based inverse algorithm
at **192 kHz**, versus the landed phase inverse at **192 kHz**, on `studio.local`.
It supersedes the 48 kHz historical comparison for this question. The baseline
uses the original plain-double tape algorithm, without the shared-inverse
metadata and atomics in the intermediate `LegacyTimeWarpInverter` reference.

The median matched-case ratio across 120 cases is **2.83×**. The largest ratio is
**16.45×** (`default_full_mult/period2^16`, 1.764 → 29.010 ms CPU per audio second).
The most expensive current case is **1.863 → 30.024 ms/s (16.11×)**. Current is
cheaper in **18 of 120** cases, mostly demanding modulation at slow tempo.

## Paired Studio measurements

Milliseconds of inverse CPU per audio second; median of five repeats. Each
paired run receives the same complete 192 kHz trajectory and performs four reads
per 1,024 host-frame hop. Initialization and trajectory generation are excluded.

| Case | Tape at 192 kHz, ms CPU/s | Current at 192 kHz, ms CPU/s | Current/tape |
| --- | ---: | ---: | ---: |
| `linear/period2^16` | 0.321 | 1.845 | 5.75x |
| `gentle/period2^16` | 0.340 | 1.844 | 5.42x |
| `blend/period2^18` | 0.397 | 1.271 | 3.20x |
| `bad_fours/period2^16` | 1.009 | 15.080 | 14.95x |
| `default_full_mult/period2^16` | 1.764 | 29.010 | 16.45x |
| `global_full_mult/period2^16` | 1.863 | 30.024 | 16.11x |
| `hard_fours/period2^16` | 1.774 | 27.645 | 15.58x |
| `linear/period2^24` | 0.342 | 0.592 | 1.73x |
| `default_full_mult/period2^24` | 1.805 | 0.879 | 0.49x |

At the most expensive current case, the absolute difference is +28.161 ms CPU
per audio second, about 2.82 percentage points of one CPU core. Fast gentle's
increase is +1.504 ms/s. These are inverse-only measurements, not whole-engine
or iPad measurements.

The largest observed 1,024-host-frame CPU blocks across all cases/repeats are
**1.768 ms for tape** and **0.960 ms for current**. They occur in different cases
and are individual observed maxima, not guaranteed execution-time bounds.
The full data retains per-case p99, maxima, wall maxima, and all five repeats.

## Exact baseline configuration

- Tape: `OriginalTimeWarpInverter<1 << 26>`, 512 MiB of plain doubles, one grid
  point per warped engine sample, one write per engine sample. Input phase is
  multiplied by the master period in engine samples. Original interpolation,
  index initialization, reversal handling and internal timestamp increment remain.
- Current: landed `d157dd5902286f5b0e2b8ef153f998b6c2aef77b`, 32 MiB for G,
  2^18 cells per master cycle, one write per engine sample, eight-edge batches.
- Both use the identical observation sequence; there is no 48 kHz downsampling
  in this comparison. The old 266× aliasing comparison does not apply here.
- The original tape methods were extracted from `5c3cb41:private/src/DelayLine.hpp`;
  only surrounding audio/envelope writes were removed. This run explicitly sets
  their cadence, coordinates and capacity to 192 kHz engine units as requested.
  It is an algorithm comparison at the requested rate, not a whole historical
  application's performance replay.
- No production implementation was changed for this benchmark. The test does not
  assert output equivalence: the two inverses intentionally have different
  branch, initialization, availability and publication semantics.

At the fastest master tempo, tape has 2^16 cells per master cycle and current
has 2^18, so current traverses roughly four times as many cells for the same
sampled phase travel, plus exact-boundary reentries. It also maintains branch
endpoints and coherent publication. This explains the direction of the remaining
cost; the paired test does not independently attribute each component.

## Reproduction and validation

- Host: Apple M3 Ultra, 28 CPUs, macOS 26.2 arm64, `studio.local`. Other existing
  system/omlx processes remained active; process snapshots are retained.
- Standard `make perf-build`, stable `build/tools/perf/Release`, AppleClang
  17.0.0.17000604, `-O3 -DNDEBUG -funroll-loops`, LTO.
- 120-case matrix, five repeats per method, alternating execution order: 1,200
  timed runs. Same cases and trace generator as the earlier comparisons. Each
  trace spans at least two seconds and 3.25 master cycles. Half a second of
  warmup is excluded. Host rate is 48 kHz; both inverse writers run at 192 kHz.
- The frozen tape extraction passed native checks of interpolation, clock origin,
  index initialization and reversal handling. Extraction body equality was
  previously checked against the historical source.
- Current inverse SHA-256:
  `c9cbbb93f5cdbf8ff3d845446fc0bb46a8d5e44917dbf00f2ab913c824685bb5`.
- Measured binary SHA-256:
  `192502a2e551eedf36c18273b0a4e7b43a0eed79fd4a51b4d672edf69c8d26b8`.

```sh
export PATH=/opt/homebrew/bin:/usr/local/bin:$PATH
make perf-build JOBS=4
build/tools/perf/Release/perf_density self-test
python3 private/test/perf/run_density.py --mode tape-performance --output build/results/tape192-repeat
```

Raw runs, metadata, source snapshots, the measured binary and summary are retained
in `build/results/tape192-inverse-studio/`, on Studio under the same relative path
in its dedicated benchmark checkout, and in the main checkout at
`build/results/tape192-inverse-studio-2026-11-11/`.

## Complete case medians

| Case | Tape at 192 kHz, ms/s | Current at 192 kHz, ms/s | Ratio |
| --- | ---: | ---: | ---: |
| `bad_fours/period2^16` | 1.009 | 15.080 | 14.95x |
| `bad_fours/period2^16.1` | 1.045 | 14.296 | 13.67x |
| `bad_fours/period2^17.3` | 1.079 | 6.938 | 6.43x |
| `bad_fours/period2^18` | 1.038 | 4.602 | 4.43x |
| `bad_fours/period2^19.7` | 1.133 | 2.140 | 1.89x |
| `bad_fours/period2^20` | 1.086 | 1.790 | 1.65x |
| `bad_fours/period2^23.9` | 1.239 | 0.782 | 0.63x |
| `bad_fours/period2^24` | 1.076 | 0.757 | 0.70x |
| `blend/period2^16` | 0.403 | 2.711 | 6.72x |
| `blend/period2^16.1` | 0.419 | 2.582 | 6.16x |
| `blend/period2^17.3` | 0.410 | 1.607 | 3.92x |
| `blend/period2^18` | 0.397 | 1.271 | 3.20x |
| `blend/period2^19.7` | 0.390 | 0.913 | 2.34x |
| `blend/period2^20` | 0.388 | 0.876 | 2.26x |
| `blend/period2^23.9` | 0.390 | 0.595 | 1.52x |
| `blend/period2^24` | 0.365 | 0.595 | 1.63x |
| `default_full_mult/period2^16` | 1.764 | 29.010 | 16.45x |
| `default_full_mult/period2^16.1` | 1.814 | 27.130 | 14.96x |
| `default_full_mult/period2^17.3` | 1.818 | 12.552 | 6.90x |
| `default_full_mult/period2^18` | 1.769 | 8.270 | 4.67x |
| `default_full_mult/period2^19.7` | 1.850 | 3.157 | 1.71x |
| `default_full_mult/period2^20` | 1.795 | 2.701 | 1.50x |
| `default_full_mult/period2^23.9` | 1.831 | 0.894 | 0.49x |
| `default_full_mult/period2^24` | 1.805 | 0.879 | 0.49x |
| `gentle/period2^16` | 0.340 | 1.844 | 5.42x |
| `gentle/period2^16.1` | 0.358 | 1.854 | 5.18x |
| `gentle/period2^17.3` | 0.360 | 1.330 | 3.69x |
| `gentle/period2^18` | 0.364 | 1.124 | 3.09x |
| `gentle/period2^19.7` | 0.353 | 0.792 | 2.24x |
| `gentle/period2^20` | 0.368 | 0.738 | 2.00x |
| `gentle/period2^23.9` | 0.354 | 0.590 | 1.67x |
| `gentle/period2^24` | 0.363 | 0.588 | 1.62x |
| `global_full_mult/period2^16` | 1.863 | 30.024 | 16.11x |
| `global_full_mult/period2^16.1` | 1.971 | 28.428 | 14.42x |
| `global_full_mult/period2^17.3` | 1.958 | 13.016 | 6.65x |
| `global_full_mult/period2^18` | 1.877 | 8.338 | 4.44x |
| `global_full_mult/period2^19.7` | 1.940 | 3.115 | 1.61x |
| `global_full_mult/period2^20` | 1.927 | 2.695 | 1.40x |
| `global_full_mult/period2^23.9` | 1.979 | 0.784 | 0.40x |
| `global_full_mult/period2^24` | 1.972 | 0.778 | 0.39x |
| `global_full_mult_reverse_skew/period2^16` | 1.846 | 29.798 | 16.14x |
| `global_full_mult_reverse_skew/period2^16.1` | 1.888 | 27.816 | 14.73x |
| `global_full_mult_reverse_skew/period2^17.3` | 1.900 | 12.758 | 6.72x |
| `global_full_mult_reverse_skew/period2^18` | 1.871 | 8.305 | 4.44x |
| `global_full_mult_reverse_skew/period2^19.7` | 1.912 | 3.024 | 1.58x |
| `global_full_mult_reverse_skew/period2^20` | 1.901 | 2.629 | 1.38x |
| `global_full_mult_reverse_skew/period2^23.9` | 1.930 | 0.763 | 0.40x |
| `global_full_mult_reverse_skew/period2^24` | 1.925 | 0.752 | 0.39x |
| `global_mid_mult/period2^16` | 0.685 | 8.703 | 12.71x |
| `global_mid_mult/period2^18` | 0.742 | 3.057 | 4.12x |
| `global_mid_mult/period2^20` | 0.773 | 1.290 | 1.67x |
| `global_mid_mult/period2^24` | 0.759 | 0.667 | 0.88x |
| `global_mid_mult_reverse_skew/period2^16` | 0.688 | 8.895 | 12.92x |
| `global_mid_mult_reverse_skew/period2^18` | 0.706 | 2.956 | 4.19x |
| `global_mid_mult_reverse_skew/period2^20` | 0.725 | 1.241 | 1.71x |
| `global_mid_mult_reverse_skew/period2^24` | 0.722 | 0.646 | 0.89x |
| `hard_blend/period2^16` | 2.261 | 24.502 | 10.84x |
| `hard_blend/period2^18` | 2.476 | 8.023 | 3.24x |
| `hard_blend/period2^20` | 2.402 | 2.963 | 1.23x |
| `hard_blend/period2^24` | 2.062 | 0.931 | 0.45x |
| `hard_fives/period2^16` | 0.741 | 10.103 | 13.63x |
| `hard_fives/period2^16.1` | 0.909 | 11.841 | 13.02x |
| `hard_fives/period2^17.3` | 1.801 | 12.560 | 6.98x |
| `hard_fives/period2^18` | 1.813 | 8.123 | 4.48x |
| `hard_fives/period2^19.7` | 1.929 | 3.152 | 1.63x |
| `hard_fives/period2^20` | 1.955 | 2.683 | 1.37x |
| `hard_fives/period2^23.9` | 1.961 | 0.814 | 0.41x |
| `hard_fives/period2^24` | 1.957 | 0.803 | 0.41x |
| `hard_fives_reverse_skew/period2^16` | 0.763 | 10.351 | 13.56x |
| `hard_fives_reverse_skew/period2^18` | 1.783 | 7.539 | 4.23x |
| `hard_fives_reverse_skew/period2^20` | 1.920 | 2.497 | 1.30x |
| `hard_fives_reverse_skew/period2^24` | 1.920 | 0.768 | 0.40x |
| `hard_fours/period2^16` | 1.774 | 27.645 | 15.58x |
| `hard_fours/period2^18` | 1.878 | 7.865 | 4.19x |
| `hard_fours/period2^20` | 1.906 | 2.557 | 1.34x |
| `hard_fours/period2^24` | 1.885 | 0.776 | 0.41x |
| `hard_threes/period2^16` | 1.930 | 28.328 | 14.68x |
| `hard_threes/period2^18` | 1.977 | 7.995 | 4.04x |
| `hard_threes/period2^20` | 1.938 | 2.668 | 1.38x |
| `hard_threes/period2^24` | 1.975 | 0.802 | 0.41x |
| `hard_twos/period2^16` | 1.874 | 29.358 | 15.66x |
| `hard_twos/period2^18` | 1.857 | 8.137 | 4.38x |
| `hard_twos/period2^20` | 1.837 | 2.661 | 1.45x |
| `hard_twos/period2^24` | 1.918 | 0.794 | 0.41x |
| `hard_twos_reverse_skew/period2^16` | 1.851 | 28.815 | 15.57x |
| `hard_twos_reverse_skew/period2^18` | 1.867 | 8.002 | 4.29x |
| `hard_twos_reverse_skew/period2^20` | 1.891 | 2.553 | 1.35x |
| `hard_twos_reverse_skew/period2^24` | 1.901 | 0.758 | 0.40x |
| `limited_cluster/period2^16` | 0.366 | 1.902 | 5.20x |
| `limited_cluster/period2^18` | 0.348 | 1.001 | 2.87x |
| `limited_cluster/period2^20` | 0.341 | 0.731 | 2.14x |
| `limited_cluster/period2^24` | 0.330 | 0.591 | 1.79x |
| `limited_extreme/period2^16` | 0.405 | 2.684 | 6.62x |
| `limited_extreme/period2^18` | 0.513 | 1.971 | 3.84x |
| `limited_extreme/period2^20` | 0.520 | 1.050 | 2.02x |
| `limited_extreme/period2^24` | 0.550 | 0.654 | 1.19x |
| `limited_fast/period2^16` | 0.377 | 2.485 | 6.59x |
| `limited_fast/period2^18` | 0.372 | 1.151 | 3.09x |
| `limited_fast/period2^20` | 0.352 | 0.834 | 2.37x |
| `limited_fast/period2^24` | 0.334 | 0.596 | 1.78x |
| `limited_gentle/period2^16` | 0.372 | 1.958 | 5.26x |
| `limited_gentle/period2^18` | 0.374 | 1.204 | 3.22x |
| `limited_gentle/period2^20` | 0.378 | 0.754 | 1.99x |
| `limited_gentle/period2^24` | 0.361 | 0.589 | 1.63x |
| `limited_reversing/period2^16` | 0.330 | 2.639 | 8.00x |
| `limited_reversing/period2^18` | 0.328 | 1.253 | 3.82x |
| `limited_reversing/period2^20` | 0.337 | 0.876 | 2.60x |
| `limited_reversing/period2^24` | 0.368 | 0.597 | 1.62x |
| `linear/period2^16` | 0.321 | 1.845 | 5.75x |
| `linear/period2^16.1` | 0.320 | 1.812 | 5.66x |
| `linear/period2^17.3` | 0.327 | 1.313 | 4.02x |
| `linear/period2^18` | 0.337 | 1.260 | 3.74x |
| `linear/period2^19.7` | 0.347 | 0.759 | 2.19x |
| `linear/period2^20` | 0.320 | 0.888 | 2.78x |
| `linear/period2^23.9` | 0.324 | 0.595 | 1.84x |
| `linear/period2^24` | 0.342 | 0.592 | 1.73x |
| `slow_reversal/period2^16` | 0.463 | 3.848 | 8.31x |
| `slow_reversal/period2^18` | 0.442 | 1.628 | 3.68x |
| `slow_reversal/period2^20` | 0.435 | 0.883 | 2.03x |
| `slow_reversal/period2^24` | 0.448 | 0.615 | 1.37x |
