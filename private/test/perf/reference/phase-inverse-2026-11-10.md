# Phase inverse performance — 2026-11-10

Measured source: `d76d55f` (the nominal-indicator commit, before rebase).
The inverse implementation itself is from `f9e5ee0`. This completes the timing
sweep previously paused because another agent was running.

The compact inverse has a material CPU tradeoff: its highest case-median mean
cost is **161.996 ms CPU per audio second**, versus **4.217 ms** for the legacy
sample grid and **10.624 ms** for the previous branch-unaware phase grid. These
are maxima over different cases, not a matched-case ratio. The largest matched
mean regression is **51.32×** at `hard_twos_reverse_skew/period2^16`.
The largest observed 1024-host-frame CPU block is **10.937 ms** for current,
**5.518 ms** for legacy, and **0.657 ms** for the prior phase grid. Extreme block
maxima include isolated outliers; the table also retains per-case p99 values.
No claim of performance equivalence or iPad deadline safety follows.

Fast phase travel crosses many grid cells. For example, `hard_twos/period2^16`
visits about 25.218 million current-grid edges per audio second, versus 6.301
million legacy-grid edges. Each current cell crossing also performs several
sequentially consistent payload and revision stores. This explains increased
work, but this benchmark does not isolate the individual cost of each change.
Slow-tempo cases often improve on legacy, and gentle cases remain low in
absolute CPU cost. Correctness results are separate from these timings.

The raw 120-case/five-repeat sweep, source snapshots, binary hash and generated
summary are retained under `build/results/time-warp-review/landing-inverse/`;
the main checkout preserves these at
`build/results/warped-scope-history-2026-11-10/time-warp-review/landing-inverse/`
before worktree cleanup.
No concurrent build/test/benchmark was observed during the serial sweep.
Ordinary desktop applications remained running.

120 cases, five repetitions per implementation, serial isolated desktop timings. This measures inverse writes and four reads per PV hop, not whole-app or iPad cost. Each block represents 1024 host frames (21.333 ms of audio at 48 kHz). Trajectory generation and initialization are excluded. Ordinary desktop processes remained active; no competing build or benchmark was observed.

Sources and binary hashes are in `performance-metadata.json`; all raw runs are retained. The legacy and preceding phase-grid reference headers are frozen in the benchmark.

| Case | Legacy ms CPU/s audio | Prior phase grid | Current | Current/legacy | Current p99 block µs (median run) | Current max block µs |
|---|---:|---:|---:|---:|---:|---:|
| bad_fours/period2^16 | 1.670 | 5.953 | 78.127 | 46.80 | 1756.2 | 1846.1 |
| bad_fours/period2^16.1 | 2.008 | 5.799 | 72.827 | 36.27 | 1640.2 | 1746.0 |
| bad_fours/period2^17.3 | 2.092 | 3.158 | 32.172 | 15.38 | 741.6 | 1019.2 |
| bad_fours/period2^18 | 1.947 | 1.995 | 19.800 | 10.17 | 493.3 | 531.2 |
| bad_fours/period2^19.7 | 2.177 | 1.202 | 6.206 | 2.85 | 155.4 | 211.3 |
| bad_fours/period2^20 | 1.980 | 1.014 | 5.140 | 2.60 | 128.4 | 195.0 |
| bad_fours/period2^23.9 | 2.231 | 0.830 | 0.974 | 0.44 | 32.1 | 116.8 |
| bad_fours/period2^24 | 2.165 | 0.829 | 0.938 | 0.43 | 31.7 | 111.6 |
| blend/period2^16 | 0.902 | 1.555 | 10.533 | 11.68 | 460.1 | 1817.8 |
| blend/period2^16.1 | 0.978 | 1.656 | 9.374 | 9.58 | 453.4 | 522.0 |
| blend/period2^17.3 | 0.897 | 1.057 | 4.334 | 4.83 | 262.6 | 290.7 |
| blend/period2^18 | 0.901 | 0.936 | 2.802 | 3.11 | 175.8 | 222.9 |
| blend/period2^19.7 | 0.928 | 0.842 | 1.228 | 1.32 | 58.5 | 119.8 |
| blend/period2^20 | 0.904 | 0.837 | 1.136 | 1.26 | 50.1 | 124.4 |
| blend/period2^23.9 | 0.921 | 0.823 | 0.775 | 0.84 | 27.5 | 112.6 |
| blend/period2^24 | 0.906 | 0.829 | 0.780 | 0.86 | 28.0 | 108.8 |
| default_full_mult/period2^16 | 3.333 | 10.624 | 157.085 | 47.13 | 3471.3 | 10936.8 |
| default_full_mult/period2^16.1 | 3.405 | 9.848 | 146.480 | 43.02 | 3265.2 | 3347.6 |
| default_full_mult/period2^17.3 | 3.438 | 4.965 | 64.645 | 18.81 | 1482.0 | 1556.9 |
| default_full_mult/period2^18 | 3.413 | 3.470 | 39.508 | 11.58 | 909.3 | 1417.7 |
| default_full_mult/period2^19.7 | 3.450 | 1.784 | 12.489 | 3.62 | 342.7 | 403.8 |
| default_full_mult/period2^20 | 3.445 | 1.579 | 10.353 | 3.01 | 250.5 | 558.7 |
| default_full_mult/period2^23.9 | 3.387 | 0.828 | 1.189 | 0.35 | 52.8 | 130.8 |
| default_full_mult/period2^24 | 3.398 | 0.824 | 1.145 | 0.34 | 49.3 | 165.1 |
| gentle/period2^16 | 0.824 | 0.975 | 4.998 | 6.07 | 127.6 | 144.1 |
| gentle/period2^16.1 | 0.853 | 1.137 | 4.647 | 5.45 | 115.9 | 143.0 |
| gentle/period2^17.3 | 0.825 | 0.859 | 2.191 | 2.66 | 59.3 | 67.1 |
| gentle/period2^18 | 0.868 | 0.858 | 1.509 | 1.74 | 46.4 | 188.7 |
| gentle/period2^19.7 | 0.865 | 0.848 | 0.941 | 1.09 | 31.0 | 72.7 |
| gentle/period2^20 | 0.846 | 0.839 | 0.887 | 1.05 | 28.9 | 80.1 |
| gentle/period2^23.9 | 0.875 | 0.830 | 0.834 | 0.95 | 28.5 | 100.7 |
| gentle/period2^24 | 0.864 | 0.832 | 0.827 | 0.96 | 29.0 | 115.8 |
| global_full_mult/period2^16 | 3.290 | 10.472 | 161.849 | 49.20 | 3744.3 | 6934.5 |
| global_full_mult/period2^16.1 | 3.246 | 9.918 | 151.204 | 46.58 | 3591.8 | 3912.5 |
| global_full_mult/period2^17.3 | 3.275 | 4.808 | 66.022 | 20.16 | 3472.1 | 3562.7 |
| global_full_mult/period2^18 | 3.237 | 3.273 | 40.080 | 12.38 | 3391.4 | 9818.0 |
| global_full_mult/period2^19.7 | 3.281 | 1.645 | 12.797 | 3.90 | 2376.0 | 2517.7 |
| global_full_mult/period2^20 | 3.292 | 1.500 | 10.615 | 3.22 | 1715.3 | 1823.1 |
| global_full_mult/period2^23.9 | 3.340 | 0.846 | 1.341 | 0.40 | 164.3 | 754.8 |
| global_full_mult/period2^24 | 3.324 | 0.843 | 1.301 | 0.39 | 152.6 | 246.4 |
| global_full_mult_reverse_skew/period2^16 | 3.273 | 10.356 | 160.414 | 49.01 | 3600.2 | 4541.4 |
| global_full_mult_reverse_skew/period2^16.1 | 3.229 | 9.614 | 148.643 | 46.04 | 3573.8 | 3782.4 |
| global_full_mult_reverse_skew/period2^17.3 | 3.182 | 4.770 | 65.751 | 20.66 | 3492.4 | 3833.0 |
| global_full_mult_reverse_skew/period2^18 | 3.221 | 3.259 | 40.914 | 12.70 | 3195.5 | 3271.5 |
| global_full_mult_reverse_skew/period2^19.7 | 3.294 | 1.583 | 12.779 | 3.88 | 3225.2 | 3362.5 |
| global_full_mult_reverse_skew/period2^20 | 3.397 | 1.457 | 10.617 | 3.13 | 3109.7 | 3226.4 |
| global_full_mult_reverse_skew/period2^23.9 | 4.217 | 0.863 | 1.379 | 0.33 | 399.5 | 569.2 |
| global_full_mult_reverse_skew/period2^24 | 4.205 | 0.855 | 1.332 | 0.32 | 357.6 | 563.5 |
| global_mid_mult/period2^16 | 1.508 | 3.437 | 42.269 | 28.03 | 3440.3 | 3566.4 |
| global_mid_mult/period2^18 | 1.481 | 1.530 | 11.466 | 7.74 | 1738.0 | 1802.0 |
| global_mid_mult/period2^20 | 1.537 | 1.028 | 3.373 | 2.19 | 623.4 | 689.1 |
| global_mid_mult/period2^24 | 1.508 | 0.828 | 0.893 | 0.59 | 42.1 | 123.9 |
| global_mid_mult_reverse_skew/period2^16 | 1.477 | 3.508 | 44.456 | 30.10 | 3271.2 | 3626.3 |
| global_mid_mult_reverse_skew/period2^18 | 1.488 | 1.484 | 11.501 | 7.73 | 3149.0 | 3240.8 |
| global_mid_mult_reverse_skew/period2^20 | 1.453 | 0.997 | 3.338 | 2.30 | 1489.8 | 1758.6 |
| global_mid_mult_reverse_skew/period2^24 | 1.518 | 0.849 | 0.908 | 0.60 | 95.7 | 206.3 |
| hard_blend/period2^16 | 3.445 | 9.112 | 122.233 | 35.49 | 2752.1 | 2812.5 |
| hard_blend/period2^18 | 3.456 | 3.582 | 33.957 | 9.83 | 1171.3 | 1303.5 |
| hard_blend/period2^20 | 3.398 | 1.937 | 9.112 | 2.68 | 465.0 | 537.4 |
| hard_blend/period2^24 | 3.290 | 0.855 | 1.172 | 0.36 | 50.0 | 134.3 |
| hard_fives/period2^16 | 1.529 | 4.085 | 50.712 | 33.16 | 1166.5 | 1362.8 |
| hard_fives/period2^16.1 | 1.706 | 4.841 | 60.108 | 35.24 | 1381.7 | 1447.1 |
| hard_fives/period2^17.3 | 3.515 | 5.376 | 62.972 | 17.92 | 1426.6 | 1512.9 |
| hard_fives/period2^18 | 3.494 | 3.478 | 39.957 | 11.43 | 943.4 | 2858.0 |
| hard_fives/period2^19.7 | 3.196 | 1.597 | 12.237 | 3.83 | 296.8 | 351.5 |
| hard_fives/period2^20 | 3.275 | 1.449 | 10.136 | 3.10 | 248.5 | 321.0 |
| hard_fives/period2^23.9 | 3.280 | 0.855 | 1.324 | 0.40 | 40.3 | 134.7 |
| hard_fives/period2^24 | 3.297 | 0.852 | 1.276 | 0.39 | 39.3 | 114.8 |
| hard_fives_reverse_skew/period2^16 | 1.464 | 4.274 | 50.930 | 34.80 | 1161.2 | 1211.2 |
| hard_fives_reverse_skew/period2^18 | 3.120 | 3.170 | 39.368 | 12.62 | 943.4 | 1151.1 |
| hard_fives_reverse_skew/period2^20 | 3.221 | 1.466 | 10.326 | 3.21 | 250.0 | 304.7 |
| hard_fives_reverse_skew/period2^24 | 3.301 | 0.880 | 1.337 | 0.40 | 39.6 | 147.3 |
| hard_fours/period2^16 | 3.033 | 10.375 | 154.022 | 50.79 | 3405.8 | 3540.7 |
| hard_fours/period2^18 | 3.162 | 3.263 | 38.968 | 12.32 | 910.0 | 1021.0 |
| hard_fours/period2^20 | 3.208 | 1.403 | 10.130 | 3.16 | 246.3 | 382.3 |
| hard_fours/period2^24 | 3.317 | 0.832 | 1.270 | 0.38 | 39.4 | 125.3 |
| hard_threes/period2^16 | 3.231 | 10.280 | 155.672 | 48.18 | 3508.0 | 3689.5 |
| hard_threes/period2^18 | 3.300 | 3.318 | 38.987 | 11.81 | 900.5 | 996.0 |
| hard_threes/period2^20 | 3.283 | 1.518 | 10.213 | 3.11 | 252.9 | 343.8 |
| hard_threes/period2^24 | 3.317 | 0.852 | 1.297 | 0.39 | 40.3 | 112.3 |
| hard_twos/period2^16 | 3.266 | 10.513 | 161.996 | 49.61 | 3634.3 | 3780.4 |
| hard_twos/period2^18 | 3.235 | 3.253 | 40.292 | 12.45 | 930.1 | 979.8 |
| hard_twos/period2^20 | 3.277 | 1.494 | 10.461 | 3.19 | 253.3 | 351.3 |
| hard_twos/period2^24 | 3.324 | 0.847 | 1.306 | 0.39 | 106.5 | 180.9 |
| hard_twos_reverse_skew/period2^16 | 3.105 | 10.166 | 159.343 | 51.32 | 3541.5 | 3794.7 |
| hard_twos_reverse_skew/period2^18 | 3.258 | 3.227 | 40.490 | 12.43 | 950.4 | 1278.7 |
| hard_twos_reverse_skew/period2^20 | 3.228 | 1.443 | 10.542 | 3.27 | 254.3 | 447.9 |
| hard_twos_reverse_skew/period2^24 | 3.344 | 0.863 | 1.343 | 0.40 | 114.7 | 206.0 |
| limited_cluster/period2^16 | 0.864 | 1.248 | 5.581 | 6.46 | 346.5 | 372.6 |
| limited_cluster/period2^18 | 0.840 | 0.866 | 1.706 | 2.03 | 89.7 | 107.2 |
| limited_cluster/period2^20 | 0.866 | 0.852 | 0.942 | 1.09 | 35.0 | 61.2 |
| limited_cluster/period2^24 | 0.871 | 0.832 | 0.807 | 0.93 | 28.2 | 129.1 |
| limited_extreme/period2^16 | 0.887 | 1.354 | 9.839 | 11.09 | 242.1 | 287.2 |
| limited_extreme/period2^18 | 1.005 | 0.998 | 6.638 | 6.61 | 164.8 | 236.6 |
| limited_extreme/period2^20 | 1.144 | 0.849 | 1.999 | 1.75 | 54.1 | 67.3 |
| limited_extreme/period2^24 | 1.322 | 0.836 | 0.800 | 0.61 | 27.8 | 97.4 |
| limited_fast/period2^16 | 0.903 | 1.307 | 9.330 | 10.34 | 220.7 | 263.7 |
| limited_fast/period2^18 | 0.880 | 0.935 | 2.552 | 2.90 | 66.9 | 109.5 |
| limited_fast/period2^20 | 0.897 | 0.855 | 1.072 | 1.19 | 32.3 | 41.0 |
| limited_fast/period2^24 | 0.883 | 0.823 | 0.774 | 0.88 | 27.4 | 99.5 |
| limited_gentle/period2^16 | 0.812 | 0.969 | 4.955 | 6.10 | 121.3 | 169.7 |
| limited_gentle/period2^18 | 0.846 | 0.822 | 1.528 | 1.81 | 37.5 | 48.2 |
| limited_gentle/period2^20 | 0.877 | 0.868 | 0.926 | 1.06 | 29.5 | 91.8 |
| limited_gentle/period2^24 | 0.872 | 0.831 | 0.828 | 0.95 | 29.1 | 101.2 |
| limited_reversing/period2^16 | 0.856 | 1.507 | 9.131 | 10.67 | 229.4 | 282.9 |
| limited_reversing/period2^18 | 0.888 | 0.873 | 2.531 | 2.85 | 65.5 | 84.0 |
| limited_reversing/period2^20 | 0.855 | 0.812 | 1.100 | 1.29 | 35.0 | 117.9 |
| limited_reversing/period2^24 | 0.904 | 0.832 | 0.782 | 0.87 | 27.7 | 99.3 |
| linear/period2^16 | 0.863 | 1.033 | 6.220 | 7.21 | 157.3 | 184.8 |
| linear/period2^16.1 | 0.840 | 0.911 | 4.695 | 5.59 | 116.5 | 124.3 |
| linear/period2^17.3 | 0.856 | 0.891 | 2.083 | 2.43 | 54.8 | 72.3 |
| linear/period2^18 | 0.846 | 0.846 | 2.371 | 2.80 | 64.3 | 75.0 |
| linear/period2^19.7 | 0.810 | 0.816 | 0.913 | 1.13 | 29.3 | 79.9 |
| linear/period2^20 | 0.835 | 0.829 | 1.093 | 1.31 | 34.0 | 46.9 |
| linear/period2^23.9 | 0.852 | 0.829 | 0.825 | 0.97 | 28.1 | 104.1 |
| linear/period2^24 | 0.869 | 0.830 | 0.821 | 0.95 | 28.0 | 103.9 |
| slow_reversal/period2^16 | 1.029 | 1.832 | 15.753 | 15.31 | 1815.2 | 1902.4 |
| slow_reversal/period2^18 | 1.022 | 1.064 | 3.926 | 3.84 | 612.2 | 659.6 |
| slow_reversal/period2^20 | 1.010 | 0.850 | 1.389 | 1.38 | 157.0 | 202.4 |
| slow_reversal/period2^24 | 1.036 | 0.833 | 0.815 | 0.79 | 28.3 | 94.4 |
