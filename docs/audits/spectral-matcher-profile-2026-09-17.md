# Spectral matcher profile — 17 September 2026

## Scope and conclusion

Measured `AtomMatcher::Match` alone, including its sorting, workspace clearing, assignment, and result construction. Extraction, merging, decay, synthesis, and the audio callback are excluded. This follows the [spectral model branch review](spectral-model-review-2026-09-17.md).

The expensive part is solving the noncrossing assignment: over 99% of the measured large stationary cases. Most promising is to retain the existing score, tie rules, and Hirschberg reconstruction while replacing full rectangular row scans with candidate-aware row evaluation. A further fast path can prove that independent per-atom choices already form an optimal assignment.

**All matcher optimizations below are temporary prototypes, not production changes.** The 17 September runtime corrections are Deep Vocoder's explicit one-semitone density radius and the user-confirmed `MergeAtom` correction to evaluate density with `atom.m_index`. On 18 September, the user chose a 256-partial limit for Partial Machine; that limit is now configured in production while the matching algorithm remains unchanged. The 1,023-by-1,023 timings below describe the earlier budget.

## Method

- Local arm64 Mac, Apple clang 17.0.0 (`clang-1700.0.13.5`), C++17, `-O2 -g`.
- `SpectralModelGeneric<12, FrequencyDependentParameter>`, existing production atom and result types.
- Five warmup calls, then 15 batches of eight `Match` calls; report the median batch time per call. Baseline and prototypes use the same harness and compiler options, run sequentially.
- Timers surround `Match`; population, candidate counting, result checksums, and printing are outside timing. Repeated calls use the same atoms and peaks, so this measures warm stationary workloads, not streaming audio or cache-cold callback latency.
- Stationary: equal magnitudes and matching frequencies, evenly spaced in linear frequency. Changing: deterministic frequency jitter and differing magnitudes. Both use equal old/new counts. A separate deliberately artificial dense case puts all 1,023 peaks inside 0.4 octave; it is a computation stress test, not a claim about extractable FFT peaks.
- Density is a radius: one cent is `1/1200` octave, one semitone is `1/12` octave, one octave is `1`.

These are synthetic microbenchmarks, not iPad measurements. The hybrid retains a quadratic worst case; these timings do not establish an audio deadline guarantee.

## Where time goes

At 1,023 old atoms and 1,023 analysis peaks, the existing stationary one-cent assignment visits **2,091,021 recurrence cells** across 2,044 row computations. Only **1,023 pairs** are eligible. Hirschberg's forward/backward reconstruction revisits subproblems, explaining the cell count being approximately twice the full matrix size.

Instrumented mean stage times for that case:

| Stage | Time |
| --- | ---: |
| Clear workspace/results | 5.38 µs |
| Sort and prepare densities | 7.43 µs |
| Create analysis results | 0.41 µs |
| Solve assignment | 6,003.49 µs |
| Create synthesis results / find dominators | 1.57 µs |

The stage timers add overhead and their means should not be added to reconstruct the separately measured uninstrumented median. They identify the bottleneck: clearing and sorting are not responsible for the approximately 6 ms runtime.

Eligible stationary pairs at the same count:

| Density radius | Eligible pairs | Existing recurrence cells |
| --- | ---: | ---: |
| One cent | 1,023 | 2,091,021 |
| One semitone | 58,801 | 2,091,021 |
| One octave | 524,127 | 2,091,021 |

## Measured prototypes

Times below are milliseconds per `Match`, with 1,023 atoms on each side. All measured variants produced identical match/dominator result digests on these workloads.

| Workload | Current | Exact hybrid | Hybrid + guarded independent choices |
| --- | ---: | ---: | ---: |
| Stationary, one cent | 6.00 | 0.214 | 0.088 |
| Stationary, one semitone | 6.31 | 1.415 | 0.258 |
| Stationary, one octave | 6.77 | 3.866 | 1.680 |
| Changing, one cent | 4.56 | 0.264 | 0.270 |
| Changing, one semitone | 6.86 | 2.069 | 2.079 |
| Changing, one octave | 7.87 | 5.330 | 5.318 |
| Artificial all-pairs stress | 6.90 | 5.403 | 3.258 |

The extra shortcut benefits conflict-free tracking, which includes the stationary pad case. When independently preferred peaks collide or cross, it falls back; the changing workloads show why stationary speedups should not be treated as universal.

### 1. Cache each old atom's candidate interval

The analysis array is frequency sorted. Each old atom has one contiguous density interval, even when density varies between atoms. Find its first/end analysis indices once per `Match`, then clip those bounds to each recursive subproblem. Continue applying the original magnitude-ratio gate and theta arithmetic inside the interval.

Boundary searches must use the original subtraction-and-comparison semantics. Searching against precomputed `center ± density` can round differently and lose or add a boundary candidate.

The measured prototypes add two `size_t[8192]` member arrays: **131,072 bytes per model** on this host. They do not allocate an edge matrix, store per-edge backpointers, or allocate heap memory during `Match`. Smaller index types could reduce this, but that was not implemented or measured.

### 2. Use two exact ways to compute a score row

**Banded row:** evaluate the existing recurrence only within the candidate interval. Before the interval, the row is unchanged. After it, propagate the carried best score only until the untouched monotone suffix catches up; binary search locates that endpoint. This preserves the original score and match-count comparisons. Long suffix fills can still be quadratic.

**Sparse row:** use the existing score-row storage as a Fenwick prefix-maximum tree. For each eligible pair, query the best earlier analysis column, add this pair's score/count, and update the tree. Process a single old atom's candidates in descending column order so that its updates cannot reuse the same atom. At the end, recover the ordinary prefix-score row with a linear maximum pass. The existing Hirschberg reconstruction then works unchanged, including its tie policy.

Sparse rows are excellent when there are very few eligible pairs, but the tree traversal costs more than a simple row loop for broad windows. With cached intervals, sparse-only measured 0.211 ms at one cent but 9.706 ms at one octave; banded-only measured 0.636 ms and 3.881 ms respectively.

The hybrid chooses sparse evaluation when the candidate-interval pair count is less than 1/32 of the subproblem rectangle; otherwise it chooses banded evaluation. This is a measured heuristic, not a thoroughly tuned threshold. Candidate intervals include pairs later rejected by the magnitude gate, so the selection count is an upper bound.

### 3. Prove when no assignment solver is needed

For each old atom, choose its highest-scoring eligible peak, taking the first peak on an exact local tie. If those choices are strictly increasing in frequency order, they neither share peaks nor cross. They simultaneously attain every row's individual score upper bound, so no other assignment can improve the total. They also match every eligible row, preserving the secondary match-count objective, including zero-score boundary matches.

On a collision or crossing, discard the tentative choices and invoke the exact hybrid solver. This is a checked optimality shortcut, not greedy matching used despite conflicts. Its scan costs `O(n + E)` after candidate bounds are available; finding arbitrary density bounds costs `O(n log m)` in this prototype.

There is a floating-point wrinkle: distinct local theta values can become equal after summation with a much larger score. An unguarded shortcut then changes identities even though its rounded total agrees with the baseline. A concrete regression has first-row scores `4.7683718085e-10` and `4.7683723636e-10`; adding a second-row score of `1` makes the double totals equal. The existing solver chooses the earlier peak, while unguarded independent choices choose the later peak.

The guarded prototype tracks the smallest positive loss from a row's best score to its next lower distinct score, including leaving the row unmatched as score zero. It accepts the shortcut only when the total is finite and:

```text
minimumPositiveLoss > 8 * epsilon(double) * numberOfOldAtoms * totalBestTheta
```

Loss differences are computed in double from the original float theta values. For finite nonnegative scores and the 8,192-atom cap, this conservatively exceeds accumulated rounding uncertainty. Ambiguous cases use the solver. The concrete rounded-tie regression correctly falls back and preserves the earlier peak.

## Validation and recommendation

### Follow-up: bounding matching at 256 partials

The existing `m_numAtoms` setting limits both analysis peaks before tracking and retained synthesis atoms after tracking. A fixed 256 limit therefore bounds subsequent matching to 256 by 256, about 1/16 of the current maximum rectangle. It does not cap extraction work, which happens before analysis pruning.

Additional measurements use `benchmark_cap.cpp` with the same timing method and include changing magnitudes/frequencies and dense windows:

| 256-by-256 workload | Current | Exact hybrid |
| --- | ---: | ---: |
| Stationary, one semitone | 0.389 ms | 0.128 ms |
| Changing, one semitone | 0.414 ms | 0.150 ms |
| Dense, all pairs eligible | 0.434 ms | 0.362 ms |
| Changing, artificial 16-octave all-pairs window | 0.534 ms | 0.550 ms |

The largest observed batch averages across these 12 cases were 0.569 ms current and 0.584 ms hybrid. These are observations, not worst-case execution-time bounds. The all-pairs result reinforces that the hybrid primarily improves sparse cases; the cap does the more important work of bounding the problem size. CSVs: `baseline/results-cap.csv` and `hybrid/results-cap.csv` in the local experiment directory.

For a strict callback budget, prefer a 256 cap first and regard optimization as additional headroom, subject to device timing and listening. The musical tradeoff requires attention: magnitude pruning can remove quiet tails and newly attacking atoms, and analysis peaks are subtracted from the residual before the count limit, so rejected peaks do not automatically return to the residual.

The MTG reference tracker follows a different contract. [SMS Tools `sineTracking`](https://github.com/MTG/sms-tools/blob/master/smstools/models/sineModel.py) processes incoming peaks by descending magnitude, greedily claims the nearest available old track in Hz within `20 + 0.01 * peakFrequency` by default, and terminates unmatched old tracks. It neither maximizes a total assignment score nor enforces no crossing or our sustained-tail suppression policy. The nearest-track scan is itself potentially quadratic. [Essentia's C++ implementation](https://github.com/MTG/essentia/blob/master/src/algorithms/synthesis/sinemodelanal.cpp) uses this same greedy scheme and truncates the stored tracks before the next frame; its [documented defaults](https://essentia.upf.edu/reference/std_SineModelAnal.html) are 250 candidate peaks and 100 tracks. Those limits and the simpler recurrence-free search explain the different computation budget; no reference timings were measured here.

### Prototype validation

- Each row-evaluation variant passed 20,000 deterministic cases comparing **exact forward/backward score rows and exact selected match/dominator identities** with the original implementation. Cases include empty inputs, changing counts, varied density lanes, magnitude gates, exact/adjacent floating-point boundaries, and occasional larger arrays.
- The guarded variant also passed 20,000 cases spanning a wider magnitude exponent range, plus the explicit erased-advantage counterexample above.
- The guarded 20,000-case validation passed AddressSanitizer and UndefinedBehaviorSanitizer without diagnostics.
- This does not exhaust all inputs or substitute for streaming/device validation. Keep the independent exact-oracle checks when integrating an optimization.

Recommended optimization if needed after setting a suitable count budget: cached intervals plus hybrid row evaluation, retaining the existing reconstruction. It captures large gains without changing the assignment contract or requiring pair storage. The guarded independent-choice shortcut is a promising additional step for sustained pads, but brings a separate numerical proof and regression obligation. Neither optimization requires changing density's musical meaning.

The two requested runtime corrections passed **38 targeted tests / 86,889 assertions** on 17 September. After the 18 September rebase and 256-partial limit, the expanded set passes **39 targeted tests / 87,411 assertions**. Both modified OpenSpec specs pass strict validation; full-suite results and independent review are recorded in the [branch review](spectral-model-review-2026-09-17.md).

Local experiment sources and CSVs are under `/tmp/spectral-matching-profile`: `benchmark.cpp`, `validate.cpp`, `validate_extreme.cpp`, and the `baseline`, `instrumented`, `banded-cached`, `sparse-cached`, `hybrid`, and `guarded` directories. The rounded-tie probe is `/tmp/spectral_fastpath_tie_probe.cpp`. These temporary experiments are not part of the production build.

Example reproduction from the repository root:

```sh
c++ -std=c++17 -O2 -g \
    -I/tmp/spectral-matching-profile/guarded -Iprivate/src \
    /tmp/spectral-matching-profile/benchmark.cpp \
    -o /tmp/spectral-matching-profile/guarded/benchmark
/tmp/spectral-matching-profile/guarded/benchmark
```
