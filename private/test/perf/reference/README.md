# Historical experiments, 2026-11-03

These summaries preserve useful context from the optimization work. They were
produced by earlier ad-hoc harnesses, not the maintained runner. Measurements were
on an Apple M2 Pro, native arm64 Release (`-O3`, LTO, `NDEBUG`), at 48 kHz host /
192 kHz engine. They are not portable regression thresholds. Final versions were
then-uncommitted snapshots, so the baseline commit alone does not reconstruct
both endpoints.

- `components-20261103.json`: five 24-second runs, baseline `b552dac` versus DSP
  optimizations, plus explicitly named experimental variants. `mode=1` means
  fixed controls, `mode=3` changing controls. The old `size` field selected a
  harness dispatch path: **size 3 did not mean trio for reverb, delay, partial,
  masters or mixer**. Those were complete quad/stereo components. The maintained
  harness validates actual channel counts and removes abandoned variants.
- `encoder-evaluator-20261103.json`: three 3-second runs per workload, baseline
  `0a0d206` versus compact evaluator retaining nested modulation/gestures. Contains
  medians/ranges and the associated correctness/equivalence results. Matrix
  replay and full synth are different measurement boundaries.

Selected costs, ms calling-thread CPU per second of audio:

| Workload | Before | After |
| --- | ---: | ---: |
| Reverb, fixed | 14.326 | 9.526 |
| Reverb, modulated | 18.608 | 11.679 |
| Quad master, fixed | 15.864 | 12.213 |
| Quad master, modulated | 20.046 | 17.894 |
| Flat encoder matrix, sweeping | 800.740 | 366.338 |
| Nested matrix, sweeping | 1748.332 | 816.894 |
| Nested + gestures matrix, sweeping | 4669.872 | 1511.802 |
| Full stress synth, sweeping | 1758.578 | 1183.418 |

The evaluator reduced repeated pointer traversal and moved hot evaluation data
into a compact representation while preserving nested modulation and gestures.
Full-synth savings are smaller than matrix savings because DSP reacting to
modulation remains. Component work improved all-pass SIMD, master crossover
calculation and metering; cubic-to-linear reverb interpolation and partial-hop
setter caching were separate experiments, not part of the `final` result.
