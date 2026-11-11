# Original inverse versus current on Studio — 2026-11-11

This report uses the historical 48 kHz configuration. For the requested
192 kHz tape-versus-current comparison, use
[tape192-inverse-studio-2026-11-11.md](tape192-inverse-studio-2026-11-11.md).

The actual pre-refactor inverse is considerably cheaper than the current inverse.
At the most expensive current setting, measured inverse CPU is **0.481 → 30.142
ms per audio second (62.7×)** on the same M3 Ultra Studio. The median matched-case
ratio across 120 cases is **10.3×**. Current is more expensive in all 120 cases.
The recent 57% optimization remains real; it compared two versions of the new
shared inverse, not the original delay-local implementation.

## Which “before”

The earlier benchmark's `legacy` reference was already from the initial shared
inverse refactor. This comparison separates the actual original from that
intermediate version:

| Implementation | Source | Write cadence | Logical grid | Inverse payload |
| --- | --- | --- | --- | ---: |
| Original | `5c3cb419952ff0dee4cfa9c18cb0e1b18874dbd2`, `DelayLineMovableWriterGeneric` | 48 kHz host frames | one point per warped host sample | 128 MiB |
| Earlier legacy | `a8a6ca1d04c5ac4b82fc7af09d92372f16fffb2d` | 192 kHz engine samples | one point per warped engine sample | 512 MiB |
| Current | `d157dd5902286f5b0e2b8ef153f998b6c2aef77b` | 192 kHz engine samples | 2^18 cells per master cycle | 32 MiB for G |

`5c3cb41` is the rebased parent of the first shared-inverse change. Its
`DelayLine.hpp`, `QuadDelay.hpp`, `TapeHead.hpp`, and `SampleTimer.hpp` are byte-for-byte
identical to the original branch parent `e7afa9761bb2238b5affc3c61d92be27aec88476`.

The original reference extracts only inverse computation from the delay line.
Scatter, index wrapping, and scalar lookup bodies are verbatim. The write body
removes audio/envelope writes and logging, retaining its inverse updates, original
17,700,716 clock origin, unit clock increment, array-index initialization, plain
double storage and reversal handling. Initialization is excluded from timing.
The benchmark calls it at engine observations 3, 7, 11, ... with coordinates in
host samples, matching the original QuadDelay's decimated write cadence. Current
and earlier legacy still receive every engine observation.

This measures G writes and four reads per analysis hop. It excludes waveform
generation, audio-buffer writes, resynthesis, scope rendering, and the separate
unmodulated inverse H. It is not a whole-app or iPad measurement, nor a claim
that the historical implementations have equivalent output behavior.

## Matched Studio results

All values are milliseconds of writer-thread CPU per audio second, median of
five repeats. The current column includes the landed eight-edge batching change.

| Case | Original ms/s | Earlier legacy ms/s | Current ms/s | Current/original |
| --- | ---: | ---: | ---: | ---: |
| `linear/period2^16` | 0.088 | 0.710 | 1.983 | 22.48x |
| `gentle/period2^16` | 0.091 | 0.700 | 1.859 | 20.35x |
| `blend/period2^18` | 0.106 | 0.723 | 1.309 | 12.31x |
| `bad_fours/period2^16` | 0.233 | 1.294 | 15.120 | 64.91x |
| `default_full_mult/period2^16` | 0.440 | 2.530 | 28.922 | 65.73x |
| `global_full_mult/period2^16` | 0.481 | 2.547 | 30.142 | 62.68x |
| `hard_fours/period2^16` | 0.104 | 2.314 | 27.658 | 265.89x |
| `linear/period2^24` | 0.087 | 0.710 | 0.592 | 6.80x |
| `default_full_mult/period2^24` | 0.457 | 2.553 | 0.884 | 1.93x |

At the heaviest current case the absolute increase over original is 29.661 ms
CPU per audio second, about 2.97 percentage points of one core. The highest
case-median mean cost anywhere in the matrix is 0.642 ms/s for original, 4.846
ms/s for earlier legacy, and 30.142 ms/s for current. Those maxima occur at
different settings and must not be treated as a matched-case ratio.

Against the earlier legacy reference, current's median matched ratio is 1.40×,
the largest matched ratio is 11.95×, and current is cheaper in 32 of 120 cases.
Those are the directly comparable Studio versions of the earlier legacy tables.

For 1,024 host-frame blocks, the largest observed CPU duration across all cases
and repeats is 0.318 ms for original, 4.570 ms for earlier legacy, and 1.029 ms
for current. These are individual observed maxima, not latency guarantees or
matched cases. Raw per-case p99, CPU maxima, wall maxima, and repeat data are
retained alongside the summary.

## Why the largest ratio is 266×

`hard_fours/period2^16` costs 0.104 → 27.658 ms/s, a 265.9× matched ratio. Work
counts explain why this is an especially misleading case to interpret as
identical signal processing:

| Same trace, fastest hard fours | Original | Earlier legacy | Current |
| --- | ---: | ---: | ---: |
| Grid entries visited per audio second | 47,999.5 | 6,239,999.5 | 25,103,998 |
| Forward phase travel per audio second | 2.930 | 95.947 | 95.947 |
| Backward-to-forward turns observed per audio second | 0 | 48,000 | 48,000 |

Here the fastest warp component has 4^5 × 16 cycles per master cycle, or 48 kHz
at this tempo. The original's 48 kHz observation cadence misses the intervening
warp motion; its sampled trajectory looks monotone. The 192 kHz versions observe
the reversals and do much more work. This cheap old result must remain in the
performance matrix, but it is not evidence that the original accurately handled
that waveform.

The substantial regression is not confined to that aliasing case. At fastest
`global_full_mult`, original and current observe essentially the same total
forward phase travel (96.407 cycles/s) and the same 47 forward turns/s. Original
visits 1.580 million grid entries/s; current visits 25.284 million, approximately
16× as many. Current also maintains branch endpoints and coherent publication.
The resulting measured CPU ratio is 62.7×. These structural facts explain the
direction of the cost; this run does not isolate each contribution.

Even fastest linear input has 47,999.5 old entry writes/s versus 959,997.5 current
visits/s. The current count includes re-entering an exact lower grid endpoint,
so it is 20× here, rather than the density ratio of 16× alone.

## Method, validation and provenance

- `studio.local`: Apple M3 Ultra, 28 CPUs, macOS 26.2 arm64. Existing omlx/system
  services remained active; before/after process snapshots are retained.
- Standard root `make perf-build`; stable `build/tools/perf/Release`; AppleClang
  17.0.0.17000604, `-O3 -DNDEBUG -funroll-loops`, LTO.
- Serial 120-case matrix, five repeats for each of three implementations: 1,800
  timed runs. Order rotates each repeat. Same generated traces and case order
  as the earlier density comparison; minimum two seconds / 3.25 master cycles,
  with half a second of warmup excluded. The very slow cases cover much longer
  traces. Four reads per 1,024 host frames are included for all versions.
- Historical extraction bodies checked against Git; native self-test checks
  original initialization, clock, interpolation and reversal behavior. Original
  branch-parent and rebased-parent sources checked identical.
- Current production header is unchanged from landed `d157dd5`, SHA-256
  `c9cbbb93f5cdbf8ff3d845446fc0bb46a8d5e44917dbf00f2ab913c824685bb5`.
- Measured binary SHA-256:
  `6fb12580cb8c960578a99537e4f2fe317010c9cc20b311dfcb6b871eedfe7428`.
  It is retained separately as `measured-perf-density`. The later grid-count
  diagnostic was built separately; timing snapshots preserve the measured source.
- Current values can differ slightly from the earlier Studio run because this
  is a fresh three-way run with a new harness binary. All ratios in this report
  use matched cases from this run, not cross-machine comparisons.

Raw JSONL, source snapshots, full summaries, the historical source files and
extraction audit are in `build/results/original-inverse-studio/`. A durable copy
is in the main checkout at
`build/results/original-inverse-studio-2026-11-11/`. Studio retains its copy at
`/Users/joyo/benchmarks/smartgrid-inverse-writes/repo/build/results/original-inverse-studio/`.

Reproduce serially on Studio:

```sh
export PATH=/opt/homebrew/bin:/usr/local/bin:$PATH
make perf-build JOBS=4
build/tools/perf/Release/perf_density self-test
python3 private/test/perf/run_density.py --mode original-performance --output build/results/original-repeat
build/tools/perf/Release/perf_density original-work 6 16 1 3.25
build/tools/perf/Release/perf_density original-work 11 16 1 3.25
```

## Complete case medians

| Case | Original ms/s | Earlier legacy ms/s | Current ms/s | Current/original | Current/legacy |
| --- | ---: | ---: | ---: | ---: | ---: |
| `bad_fours/period2^16` | 0.233 | 1.294 | 15.120 | 64.91x | 11.69x |
| `bad_fours/period2^16.1` | 0.227 | 1.460 | 14.302 | 62.95x | 9.80x |
| `bad_fours/period2^17.3` | 0.258 | 1.530 | 6.961 | 26.94x | 4.55x |
| `bad_fours/period2^18` | 0.260 | 1.405 | 4.621 | 17.76x | 3.29x |
| `bad_fours/period2^19.7` | 0.278 | 1.600 | 2.181 | 7.85x | 1.36x |
| `bad_fours/period2^20` | 0.269 | 1.453 | 1.829 | 6.79x | 1.26x |
| `bad_fours/period2^23.9` | 0.304 | 1.669 | 0.783 | 2.58x | 0.47x |
| `bad_fours/period2^24` | 0.278 | 1.587 | 0.757 | 2.73x | 0.48x |
| `blend/period2^16` | 0.102 | 0.725 | 2.741 | 27.01x | 3.78x |
| `blend/period2^16.1` | 0.115 | 0.731 | 2.597 | 22.54x | 3.55x |
| `blend/period2^17.3` | 0.115 | 0.730 | 1.631 | 14.18x | 2.23x |
| `blend/period2^18` | 0.106 | 0.723 | 1.309 | 12.31x | 1.81x |
| `blend/period2^19.7` | 0.110 | 0.720 | 0.920 | 8.40x | 1.28x |
| `blend/period2^20` | 0.106 | 0.723 | 0.878 | 8.26x | 1.21x |
| `blend/period2^23.9` | 0.102 | 0.723 | 0.591 | 5.81x | 0.82x |
| `blend/period2^24` | 0.101 | 0.715 | 0.591 | 5.84x | 0.83x |
| `default_full_mult/period2^16` | 0.440 | 2.530 | 28.922 | 65.73x | 11.43x |
| `default_full_mult/period2^16.1` | 0.463 | 2.638 | 27.153 | 58.66x | 10.29x |
| `default_full_mult/period2^17.3` | 0.463 | 2.619 | 12.768 | 27.59x | 4.88x |
| `default_full_mult/period2^18` | 0.457 | 2.609 | 8.296 | 18.16x | 3.18x |
| `default_full_mult/period2^19.7` | 0.469 | 2.634 | 3.159 | 6.74x | 1.20x |
| `default_full_mult/period2^20` | 0.472 | 2.609 | 2.703 | 5.73x | 1.04x |
| `default_full_mult/period2^23.9` | 0.466 | 2.565 | 0.898 | 1.93x | 0.35x |
| `default_full_mult/period2^24` | 0.457 | 2.553 | 0.884 | 1.93x | 0.35x |
| `gentle/period2^16` | 0.091 | 0.700 | 1.859 | 20.35x | 2.66x |
| `gentle/period2^16.1` | 0.095 | 0.699 | 1.909 | 20.17x | 2.73x |
| `gentle/period2^17.3` | 0.095 | 0.702 | 1.343 | 14.21x | 1.91x |
| `gentle/period2^18` | 0.093 | 0.700 | 1.126 | 12.06x | 1.61x |
| `gentle/period2^19.7` | 0.093 | 0.700 | 0.746 | 8.04x | 1.07x |
| `gentle/period2^20` | 0.096 | 0.704 | 0.702 | 7.27x | 1.00x |
| `gentle/period2^23.9` | 0.096 | 0.709 | 0.587 | 6.08x | 0.83x |
| `gentle/period2^24` | 0.096 | 0.708 | 0.588 | 6.10x | 0.83x |
| `global_full_mult/period2^16` | 0.481 | 2.547 | 30.142 | 62.68x | 11.84x |
| `global_full_mult/period2^16.1` | 0.505 | 2.605 | 28.559 | 56.55x | 10.97x |
| `global_full_mult/period2^17.3` | 0.505 | 2.561 | 13.024 | 25.79x | 5.09x |
| `global_full_mult/period2^18` | 0.468 | 2.529 | 8.363 | 17.87x | 3.31x |
| `global_full_mult/period2^19.7` | 0.503 | 2.520 | 3.125 | 6.22x | 1.24x |
| `global_full_mult/period2^20` | 0.493 | 2.538 | 2.701 | 5.48x | 1.06x |
| `global_full_mult/period2^23.9` | 0.491 | 2.599 | 0.792 | 1.61x | 0.30x |
| `global_full_mult/period2^24` | 0.484 | 2.573 | 0.785 | 1.62x | 0.31x |
| `global_full_mult_reverse_skew/period2^16` | 0.477 | 2.505 | 29.873 | 62.64x | 11.93x |
| `global_full_mult_reverse_skew/period2^16.1` | 0.487 | 2.534 | 27.859 | 57.23x | 10.99x |
| `global_full_mult_reverse_skew/period2^17.3` | 0.486 | 2.487 | 12.782 | 26.31x | 5.14x |
| `global_full_mult_reverse_skew/period2^18` | 0.481 | 2.528 | 8.321 | 17.30x | 3.29x |
| `global_full_mult_reverse_skew/period2^19.7` | 0.489 | 2.756 | 3.034 | 6.20x | 1.10x |
| `global_full_mult_reverse_skew/period2^20` | 0.478 | 2.858 | 2.635 | 5.52x | 0.92x |
| `global_full_mult_reverse_skew/period2^23.9` | 0.482 | 4.846 | 0.766 | 1.59x | 0.16x |
| `global_full_mult_reverse_skew/period2^24` | 0.481 | 4.773 | 0.758 | 1.58x | 0.16x |
| `global_mid_mult/period2^16` | 0.209 | 1.176 | 8.805 | 42.21x | 7.49x |
| `global_mid_mult/period2^18` | 0.185 | 1.195 | 3.069 | 16.56x | 2.57x |
| `global_mid_mult/period2^20` | 0.198 | 1.173 | 1.301 | 6.56x | 1.11x |
| `global_mid_mult/period2^24` | 0.194 | 1.156 | 0.670 | 3.46x | 0.58x |
| `global_mid_mult_reverse_skew/period2^16` | 0.179 | 1.180 | 8.996 | 50.35x | 7.62x |
| `global_mid_mult_reverse_skew/period2^18` | 0.180 | 1.183 | 2.966 | 16.48x | 2.51x |
| `global_mid_mult_reverse_skew/period2^20` | 0.192 | 1.169 | 1.247 | 6.51x | 1.07x |
| `global_mid_mult_reverse_skew/period2^24` | 0.188 | 1.152 | 0.650 | 3.46x | 0.56x |
| `hard_blend/period2^16` | 0.374 | 2.420 | 24.429 | 65.39x | 10.09x |
| `hard_blend/period2^18` | 0.622 | 2.604 | 8.044 | 12.94x | 3.09x |
| `hard_blend/period2^20` | 0.642 | 2.625 | 2.977 | 4.64x | 1.13x |
| `hard_blend/period2^24` | 0.550 | 2.522 | 0.930 | 1.69x | 0.37x |
| `hard_fives/period2^16` | 0.133 | 1.073 | 10.077 | 75.95x | 9.39x |
| `hard_fives/period2^16.1` | 0.116 | 1.243 | 11.830 | 102.19x | 9.52x |
| `hard_fives/period2^17.3` | 0.166 | 2.547 | 12.563 | 75.53x | 4.93x |
| `hard_fives/period2^18` | 0.198 | 2.560 | 8.122 | 41.00x | 3.17x |
| `hard_fives/period2^19.7` | 0.454 | 2.430 | 3.147 | 6.93x | 1.30x |
| `hard_fives/period2^20` | 0.458 | 2.500 | 2.682 | 5.86x | 1.07x |
| `hard_fives/period2^23.9` | 0.496 | 2.546 | 0.817 | 1.64x | 0.32x |
| `hard_fives/period2^24` | 0.496 | 2.539 | 0.805 | 1.62x | 0.32x |
| `hard_fives_reverse_skew/period2^16` | 0.109 | 1.093 | 10.351 | 95.22x | 9.47x |
| `hard_fives_reverse_skew/period2^18` | 0.198 | 2.284 | 7.547 | 38.10x | 3.30x |
| `hard_fives_reverse_skew/period2^20` | 0.454 | 2.411 | 2.496 | 5.50x | 1.04x |
| `hard_fives_reverse_skew/period2^24` | 0.488 | 2.521 | 0.764 | 1.57x | 0.30x |
| `hard_fours/period2^16` | 0.104 | 2.314 | 27.658 | 265.89x | 11.95x |
| `hard_fours/period2^18` | 0.449 | 2.416 | 7.871 | 17.53x | 3.26x |
| `hard_fours/period2^20` | 0.496 | 2.478 | 2.567 | 5.18x | 1.04x |
| `hard_fours/period2^24` | 0.476 | 2.509 | 0.779 | 1.64x | 0.31x |
| `hard_threes/period2^16` | 0.453 | 2.450 | 28.426 | 62.74x | 11.60x |
| `hard_threes/period2^18` | 0.474 | 2.513 | 8.021 | 16.93x | 3.19x |
| `hard_threes/period2^20` | 0.493 | 2.580 | 2.680 | 5.44x | 1.04x |
| `hard_threes/period2^24` | 0.491 | 2.558 | 0.812 | 1.65x | 0.32x |
| `hard_twos/period2^16` | 0.477 | 2.500 | 29.493 | 61.87x | 11.80x |
| `hard_twos/period2^18` | 0.470 | 2.513 | 8.116 | 17.27x | 3.23x |
| `hard_twos/period2^20` | 0.467 | 2.522 | 2.668 | 5.72x | 1.06x |
| `hard_twos/period2^24` | 0.476 | 2.540 | 0.800 | 1.68x | 0.32x |
| `hard_twos_reverse_skew/period2^16` | 0.475 | 2.478 | 29.007 | 61.03x | 11.71x |
| `hard_twos_reverse_skew/period2^18` | 0.472 | 2.497 | 7.957 | 16.87x | 3.19x |
| `hard_twos_reverse_skew/period2^20` | 0.478 | 2.495 | 2.565 | 5.37x | 1.03x |
| `hard_twos_reverse_skew/period2^24` | 0.470 | 2.621 | 0.760 | 1.62x | 0.29x |
| `limited_cluster/period2^16` | 0.095 | 0.706 | 1.919 | 20.14x | 2.72x |
| `limited_cluster/period2^18` | 0.096 | 0.704 | 1.017 | 10.65x | 1.45x |
| `limited_cluster/period2^20` | 0.093 | 0.701 | 0.707 | 7.63x | 1.01x |
| `limited_cluster/period2^24` | 0.089 | 0.706 | 0.589 | 6.60x | 0.83x |
| `limited_extreme/period2^16` | 0.107 | 0.688 | 2.717 | 25.31x | 3.95x |
| `limited_extreme/period2^18` | 0.106 | 0.759 | 1.998 | 18.78x | 2.63x |
| `limited_extreme/period2^20` | 0.133 | 0.857 | 1.082 | 8.15x | 1.26x |
| `limited_extreme/period2^24` | 0.135 | 0.961 | 0.641 | 4.75x | 0.67x |
| `limited_fast/period2^16` | 0.084 | 0.679 | 2.510 | 29.79x | 3.70x |
| `limited_fast/period2^18` | 0.097 | 0.691 | 1.182 | 12.25x | 1.71x |
| `limited_fast/period2^20` | 0.097 | 0.693 | 0.834 | 8.56x | 1.20x |
| `limited_fast/period2^24` | 0.091 | 0.696 | 0.593 | 6.53x | 0.85x |
| `limited_gentle/period2^16` | 0.088 | 0.706 | 2.042 | 23.28x | 2.89x |
| `limited_gentle/period2^18` | 0.097 | 0.708 | 1.203 | 12.44x | 1.70x |
| `limited_gentle/period2^20` | 0.100 | 0.706 | 0.722 | 7.21x | 1.02x |
| `limited_gentle/period2^24` | 0.099 | 0.716 | 0.588 | 5.97x | 0.82x |
| `limited_reversing/period2^16` | 0.097 | 0.689 | 2.647 | 27.28x | 3.84x |
| `limited_reversing/period2^18` | 0.089 | 0.692 | 1.286 | 14.52x | 1.86x |
| `limited_reversing/period2^20` | 0.089 | 0.696 | 0.880 | 9.90x | 1.26x |
| `limited_reversing/period2^24` | 0.096 | 0.714 | 0.591 | 6.16x | 0.83x |
| `linear/period2^16` | 0.088 | 0.710 | 1.983 | 22.48x | 2.80x |
| `linear/period2^16.1` | 0.085 | 0.705 | 1.916 | 22.65x | 2.72x |
| `linear/period2^17.3` | 0.085 | 0.706 | 1.296 | 15.34x | 1.84x |
| `linear/period2^18` | 0.095 | 0.706 | 1.343 | 14.11x | 1.90x |
| `linear/period2^19.7` | 0.084 | 0.707 | 0.723 | 8.57x | 1.02x |
| `linear/period2^20` | 0.085 | 0.705 | 0.843 | 9.96x | 1.19x |
| `linear/period2^23.9` | 0.091 | 0.710 | 0.590 | 6.51x | 0.83x |
| `linear/period2^24` | 0.087 | 0.710 | 0.592 | 6.80x | 0.83x |
| `slow_reversal/period2^16` | 0.118 | 0.827 | 3.916 | 33.29x | 4.73x |
| `slow_reversal/period2^18` | 0.117 | 0.811 | 1.637 | 14.00x | 2.02x |
| `slow_reversal/period2^20` | 0.121 | 0.794 | 0.895 | 7.36x | 1.13x |
| `slow_reversal/period2^24` | 0.119 | 0.789 | 0.608 | 5.13x | 0.77x |
