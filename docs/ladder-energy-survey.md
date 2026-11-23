# SIMD ladder energy survey

The CPU spectral-v2 run is paused at 588,172 configurations, with checkpoints
preserved. The current 100-start experiment uses the separately identified
[float/rational Metal survey](ladder-gpu-survey.md). This document describes the
CPU model and the measurement layout shared by both implementations.

This research runner tests the detector-disabled real-tanh ladder at fixed
input amplitude 0.25 and engine rate 192 kHz. It does not change the app,
generated processors, or Lean definitions. Each worker processes four adjacent
cutoff values using `simd_double4`; on Apple Silicon this uses two NEON vectors.
The sample loop contains no loop over lanes. State and energy accumulation use
double precision, Apple SIMD tanh, and disabled compiler reassociation/FMA
contraction. Saved statistical summaries use float32.

## Current requested experiment

- 100 input frequencies, logarithmically spanning 20 Hz to 20 kHz.
- 100 cutoff values, logarithmically spanning 20 Hz to 20 kHz.
- 100 equally spaced drive knob positions, g = 0.25 × 20^drive.
- 100 equally spaced resonance positions, kRequested = 3.9 × (49^res - 1)/48.
- The same kSafe cap and alpha convention as the earlier grid.
- Retain only configurations with k × (g/tanh(g))³ > 3.5, using k after kSafe.
- 10 independent randomized initial states per retained grid point, fixed input phase.
- Exactly 192,000 samples per trajectory, including startup.

The underlying grid has 100 million points. The threshold retains 37.08 million
points (37.08%), or 370.8 million trajectories and 7.11936e13 scalar sample
updates. Format 2 collects total and fundamental power together. The complete
output is 9.27 million records of 3,472 bytes, about 32.19 GB, plus small headers,
manifests and progress logs. A 28-thread spectral pilot on the M3 Ultra sustained
approximately 143 batches/s, estimating about 18 hours. Runtime varies with
contention and the parameter region.

The previous unfiltered 100-start run was cancelled on request, and its remote
data and local data snapshot were deleted. Its 20–23 day estimate does not apply
to this replacement run.

The later filtered, total-power-only run was stopped on November 20 after
5,768,644 points. Its approximately 1.2 GB of partial format-1 data is preserved
in `build/results/grid100-starts10-kappa-gt3p5-seed20261119` on Studio. Those
summaries cannot reconstruct the fundamental. Format 2 starts a new directory,
`build/results/grid100-starts10-kappa-gt3p5-spectral-v2-seed20261119`, with the
same parameter grid and randomized starting states. The reader supports both
formats; the runner refuses to append format-2 records to a format-1 directory.

Initial states are independent uniform draws from the natural bounded box:
the first stage is in ±(A+kC), and stages 2–4 are in ±C, with C=1/tanh(g).
The public SplitMix64-based counter mapping in `ladder_energy_grid.cpp` uses
the global seed, grid-point ID, start index, and stage index. Results are
independent of thread assignment and can be regenerated without saving states.
No warm start, convergence shortcut, early exit, or state cache changes the
sample count. All 10 starts are randomized. Original grid IDs and the seed
mapping are preserved, so these are the first 10 starts from the same deterministic
construction. Filtering happens before sample processing. Each selected batch
has all four cutoffs above the strict threshold. Other threshold values that
split a batch are rejected rather than silently including excluded points.

## Measurements

For an interval of N output samples y[n] with absolute sample index n, record:

- Total power P = sum(y[n]²)/N.
- Complex coherent fundamental C = (2/N) sum(y[n] exp(-i omega n)).
- Fundamental power P1 = |C|²/2.
- DC mean mu = sum(y[n])/N.
- Remaining power P - P1 - mu². This includes harmonics, inharmonic oscillation,
  modulation and transients; it is not a harmonic-only distortion measurement.

These definitions match `coherent` and `meanSquare` in
[`ResponseMeasurement.lean`](../formal/DSP/ResponseMeasurement.lean) and
`Prediction.coherentPower` in
[`Approximation.lean`](../formal/DSP/ResponseMeasurement/Approximation.lean).
The input is A cos(omega n), so divide either power by A²/2 for gain squared.
For amplitude gain use its square root; for dB use 10 log10 of gain squared.
Total power includes DC; mean DC power across starts can be recovered from
the stored DC mean and population variance, E[mu²] = E[mu]² + Var(mu).

All five quantities (with C saved as real and imaginary parts) are measured on
four intervals: prefixes ending near 1/4, 1/2 and 1 second, plus the final half.
Projection intervals contain whole periods of the digital input, making the
DC, cosine and sine projections orthogonal. Prefix endpoints round downward
to a period boundary. The final-half start rounds upward, and its end rounds
downward. Exact sample intervals are in each frequency's manifest entry.
All 192,000 samples are processed, including samples outside these intervals.
The original raw whole-second and last-half-second total powers are also saved.

Each point retains mean, population standard deviation, minimum and maximum
across its 10 starts for every scoped metric, plus:

1. Total and fundamental temporal coefficient of variation, computed within
   each trajectory over complete windows beginning in the final half.
2. Fundamental coherence, |mean_window C|² / mean_window |C|² over those windows.
   Values near one indicate consistent amplitude and phase. Low values reveal
   amplitude variation or phase cancellation that window powers alone conceal.
3. Peak absolute output and crest factor (peak/RMS) over the aligned final half.
4. Absolute changes from the half to full prefix in total power, fundamental
   power, and the complex coefficient. Power changes are normalized by A²/2;
   coefficient changes by A. Each change is computed per start before aggregating.

It also retains mean and standard deviation across starts for each chronological
window's total and fundamental power. Every trajectory's power is computed
before averaging across starts. Averaging complex coefficients across starts
first could cancel real responses with different phases and lose their energy.
Likewise, long-interval fundamental power uses the long coherent coefficient,
not an average of short-window powers.

As in the earlier periodic sweep, the input frequency is rounded to a repeating
digital sinusoid within 0.1% of its target. Both frequencies and the exact
sample period are recorded. An energy window contains the smallest whole number
of these sample periods totaling at least 50 ms. This avoids attributing a
cut-off fraction of an ordinary periodic waveform to energy instability.
Windows can exceed 50 ms, so each input frequency has its own window count and
length in the manifest. Only complete windows contribute to temporal CV;
the raw full and last-half energy measures include every sample in their intervals.
Unused window slots are NaNs. Zero-energy temporal CV and zero-denominator
coherence are defined to be zero. Relative CV becomes misleading when fundamental
power is tiny; the absolute normalized prefix differences remain useful then.
Negative residual power or coherence above one beyond roundoff causes an error;
only differences within the explicit rounding tolerance are clipped.

These are finite one-second observations, not claims of convergence. Low cutoff,
near-threshold growth, long beats, multiple attractors and chaos may require
longer follow-up. Stability of energy also does not imply periodicity of the
waveform. Do not classify a parameter point as mathematically stable merely
because its finite-window variation is small.

Convergence, independence of the starting state, and regularity across parameters
are distinct questions. Numerical agreement is evidence for choosing a conjecture,
not a proof of any of them. To justify fitting a long-run response from finite
data, we would need a definition of the chosen limiting measurement, finite-time
measurement error bounds, bounds on variation between grid points, and a bound
for the fit itself. Almost-everywhere convergence alone gives no uniform
interpolation bound and need not select the same limit in different basins.
The coherent and total measurements let us investigate both prospective curves
without deciding their aesthetic usefulness in advance.

## Build and run

```sh
make ladder-energy-grid-build
build/tools/ladder-energy-grid/ladder-energy-grid --self-test
build/tools/ladder-energy-grid/ladder-energy-grid \
  --grid 100 --starts 10 --min-effective-resonance 3.5 --plan
build/tools/ladder-energy-grid/ladder-energy-grid \
  --grid 100 --starts 10 --min-effective-resonance 3.5 --threads 28 \
  --output build/results/grid100-starts10-kappa-gt3p5-spectral-v2-seed20261119
```

The same command resumes the same directory. `--max-jobs N` limits additional
four-cutoff batches for a timing run without invalidating their results.
The normal command has no job limit. The grid size must be divisible by four.
Thread count can change on restart; numerical settings, selection threshold,
and seed cannot. `--plan` enumerates the selected work without running trajectories.

The Studio execution bundle lives at:

```
studio.local:/Users/joyo/benchmarks/ladder-energy-grid
```

This directory contains the standalone source and root Make target, with the
same `build/tools` and `build/results` layout. The remote location is necessary
because this experiment explicitly runs on Studio. It is not a development
checkout and does not touch any remote main branch. The source of record remains
the existing local exploration worktree. The run directory contains source and
binary hashes and a source snapshot.

The long run is started with `nohup`, records its process ID in `run.pid`, and
has a `caffeinate -i -w PID` companion that prevents idle sleep only for the
lifetime of the process. Check `progress.json` or `run.log`. To stop cleanly,
send SIGTERM to the PID in `run.pid`; workers finish their current four-cutoff
batch and exit. Restart with the same numerical arguments and output directory.

## Checkpoints and format

Each worker appends to its own `worker-NNN.bin`. Every completed record is
flushed to stable storage. A directory lock prevents concurrent writers.
On restart, record checksums, configuration identity, and job uniqueness are
checked. A partial trailing record is truncated and recomputed. A complete
record with a bad checksum causes an error; it is not silently discarded.
The run stops cleanly if available disk space falls below 10 GiB.

Jobs traverse a deterministic permutation of the eligible configurations, so
early results span the selected parameter ranges. This is not a statistically
randomized sample of grid points. Stored job IDs still refer to the original
unfiltered grid. The flattened point order is frequency, drive, resonance,
cutoff; cutoff is fastest. Point ID = 4 × job ID + lane. The manifest records
both the original grid size and selected count; progress and report completion
use the selected count.

Each file starts with three little-endian uint64 values: magic, configuration
identity hash, and record size. A record has:

- uint64 job ID;
- four 864-byte point records;
- uint64 FNV-1a checksum of the preceding 3,464 bytes.

Format-2 magic is `0x324752454444414c`. Each point is 216 little-endian float32
values. Offsets below are in floats, with exclusive ends:

| Offset | Content |
|---|---|
| 0:4, 4:8 | Raw full and late total power aggregates |
| 8:12, 12:16 | Total and fundamental temporal CV aggregates |
| 16:20 | Fundamental coherence aggregate |
| 20:24, 24:28 | Late peak and crest factor aggregates |
| 28:32, 32:36, 36:40 | Total, fundamental and complex prefix-change aggregates |
| 40:136 | Four scopes × six metrics × four aggregate fields |
| 136:156, 156:176 | Chronological window total means and SDs |
| 176:196, 196:216 | Chronological window fundamental means and SDs |

Scope order is quarter prefix, half prefix, full prefix, late half. Metric order
is total power, fundamental power, C real, C imaginary, DC mean, residual power.
Every aggregate is mean, population SD, minimum, maximum across starts. This
saves per-point summaries, not every trajectory's measurements or waveform.
Individual harmonic bins and full spectra are deferred to reproducible outlier
follow-ups; this run records their combined remainder rather than an FFT.

## Reading and validation

```sh
python3 scripts/report_ladder_energy_grid.py build/results/grid100-starts10-kappa-gt3p5-spectral-v2-seed20261119
```

The NumPy reader can inspect partial output without modifying binary files. It
reports the fraction of points whose across-start and temporal CVs are below
1% and 5%, plus a coarse split at drive/resonance 80%. These are descriptive
thresholds, not stability theorems. It retains the largest across-start
variation cases for follow-up. The report clearly marks an incomplete grid.
For format 2 it also reports fundamental CV, coherence and prefix-difference
counts. All spectral summaries are accumulated in double precision. A short
256-sample SIMD output buffer separates the recurrence from measurement to
avoid keeping the extra accumulators live across transcendental calls; no
samples are skipped and the filter recurrence is unchanged.

Validation performed before the long run:

- 40,000 SIMD tanh arguments against scalar tanh, including tiny arguments.
- 40,000 scalar/SIMD one-sample state and energy comparisons, including low and
  high cutoffs and randomized drive/resonance.
- Synthetic signals with known DC, complex fundamental and third harmonic,
  checking the power decomposition and phase sign. Two opposite-phase windows
  verify that long coherent power cancels while their total power remains.
- Independent one-second scalar replay of eight zero-feedback grid points,
  three starts each, checking all 216 saved fields, including scopes, power,
  coherence, prefix changes and every window aggregate.
- A restart with a different worker count preserved existing records and added
  unique jobs; an injected partial trailing write was recovered.
- An incompatible starts count was rejected before changing the results.
- The threshold selector agreed with an independent scalar enumeration:
  37.08 million eligible points on the requested grid, with no partly eligible
  SIMD batch. A complete smaller grid confirmed every retained record is
  bit-for-bit identical to its unfiltered counterpart with the same starts.
- The filtered run resumed with a different worker count, and its progress
  and report marked completion against the eligible count.

The maximum sampled tanh difference was 1.11e-16, and the maximum one-step
state/energy difference was 1.78e-14. Saved one-second energy summaries agreed
with the independent scalar replay within 5.30e-8 relative error, consistent
with float32 output rounding. This does not certify transcendental rounding
for all inputs or bound long chaotic trajectory differences.

Local validation evidence is under `build/results/ladder-energy-validation/`
and `build/results/ladder-energy-filter-validation/`. Format-2 evidence is under
`build/results/ladder-energy-spectral-validation/` and
`build/results/ladder-energy-spectral-boundaries/`. The latter uses 191,999 samples
to check non-period-aligned raw interval boundaries. The synthetic projection
test's maximum absolute error was 1.14e-14. The independent replay's largest
absolute saved-summary discrepancy was 1.10e-7 (float32 rounding).
