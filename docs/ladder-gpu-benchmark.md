# Ladder GPU measurement benchmark

The double/real-tanh Studio survey was paused cleanly at the user's request on
2026-11-20, preserving 147,043 completed quad jobs (588,172 configurations,
5,881,720 trajectories) in the existing spectral-v2 directory. Its measured
throughput was 1.0645 billion scalar sample updates per second. It remains
resumable with the original command and numerical settings.

This experiment evaluates an alternative model aligned with the production
saturator. It does not modify the audio processor, generated code or proofs.
It does not append to or replace the double-precision survey's records.

## Model and workload

- Same detector-disabled four-stage recurrence and one-sample feedback delay.
- Binary32 histories, coefficients and arithmetic; explicit FMA stage updates.
- Production vector saturation: clamp(x*(x*x+27)/fma(x*9,x,27), -1, 1).
- Normalize each saturator by that same rational function evaluated at drive g.
- Recompute kSafe using the rational normalization's slope. This deliberately
  changes both the nonlinearity and its dependent coefficients from the real-tanh
  model; it is not a precision-only replacement.
- Same 192 kHz, amplitude 0.25, one-second duration, frequency preparation and
  deterministic randomized-history construction as the earlier survey.
- Initial bounds use the new rational normalization. The alpha setup still uses
  the survey's exact trigonometric formula before rounding to float, rather than
  production's Math4096 lookup. Thus this is not a bit-for-bit app emulation.
- Benchmark parameter points sample the original real-tanh k*s³ > 3.5 work list,
  with ten starts per point, permuted across drive/resonance/cutoff. It is not a
  completed grid or a new full-survey selection policy.

One Metal thread owns each trajectory. A dispatch processes one measurement
segment (approximately 50 ms or shorter), preserving four histories between
dispatches. Coefficients and inputs are prepared outside the kernel. No loops
over channels or starting states occur inside a trajectory's sample loop.

Every sample contributes total power, real and imaginary fundamental projection,
DC and absolute peak. Float Kahan sums reduce summation loss; segment results
are combined in double precision on the CPU. The same four projection scopes,
whole-period windows, total/fundamental temporal CV and coherence definitions
are reused from the spectral survey. Float projection roundoff tolerance is
2e-6 relative to total power; only the residual is clipped within tolerance.

Metal uses safe arithmetic optimization and precise math functions. The CPU
comparison uses four independent trajectories per NEON vector, explicit FMAs,
matching compensated sums and all 28 workers. CPU/GPU division rounding may
differ. Agreement of long chaotic trajectories is not a validation requirement;
short-step and stable cases test the implementation, while power distributions
compare behavior in unstable cases.

## Commands

```sh
make ladder-gpu-bench-build
build/tools/ladder-gpu-bench/ladder-gpu-bench \
  --trajectories 4096 --repeats 1 --frequency-index 50 --validate --cpu \
  --output build/results/ladder-gpu-benchmark-20261120/validation
build/tools/ladder-gpu-bench/ladder-gpu-bench \
  --trajectories 65536 --repeats 3 --frequency-index 50 --cpu --compare-double \
  --output build/results/ladder-gpu-benchmark-20261120/f50
python3 scripts/report_ladder_gpu_bench.py build/results/ladder-gpu-benchmark-20261120
```

Frequency index is on the original 100-point log axis from 20 Hz to 20 kHz.
The benchmark uses the Metal runtime's source compilation, with shader files
located next to its C++ source. No offline Metal compiler installation is needed.
Source is shared between the Metal kernel and scalar CPU validation; frequency,
RNG, measurement definitions and double reference reuse the existing survey.

On Studio the standalone bundle is `/Users/joyo/benchmarks/ladder-energy-grid`.
The benchmark's output is under `build/results/ladder-gpu-benchmark-20261120/`.
Compact comparisons, timing and provenance are copied into the local worktree's
corresponding results directory. Raw per-trajectory CSVs remain on Studio.

## Validation and performance

Validation passed on the M3 Ultra's 60-core GPU:

- 4,096 randomized trajectories over eight steps: maximum scaled state error
  versus the scalar float reference 4.77e-7; measurement error 1.16e-6.
- A known signal with DC, complex fundamental and third harmonic: maximum
  spectral metric absolute error 4.67e-8.
- Full-second stable zero-feedback cases: maximum scoped metric difference
  1.47e-6, scaled by max(reference magnitude, input power).
- The original CPU survey's self-test still passes after exposing its shared
  measurement helper to the benchmark.

At 65,536 simultaneous trajectories, measurements at 20, 200, 2,000 and
20,000 Hz sustained 30.3–30.7 billion sample updates/second including CPU
postprocessing. The matching 28-thread float/rational CPU version sustained
2.72–2.74 billion. This is about 28.5–28.9 times the original double/real-tanh
survey, and about 11 times the same arithmetic on the CPU. A 4,096-trajectory
batch reaches only 7.85 billion; large batches are needed to fill the GPU.
Twenty additional repetitions at 655.29 Hz (with period boundaries that do not
divide the observation duration) sustained a median 30.75 billion updates/second.

These measurements time 192,000 samples per trajectory and the spectral
summaries. They exclude one-time shader compilation and allocation, parameter
and input preparation, CSV output, and integration with durable full-survey
checkpoints. Direct extrapolation of the selected 37.08-million-point grid gives
about 39 minutes for ten starts or 6.5 hours for 100 starts, before those costs.
These are throughput projections, not measured full-run completion times.

Per-trajectory CSVs retain late-half total/fundamental power, complex projection,
DC, residual, temporal CVs, coherence and peak. The optional double reference
replays 128 parameter points with ten starts through the original survey.
`comparison.json` reports differences in point means, including input-power
normalized absolute differences and relative differences where output power is
above 1% of input power. Differences between real and rational tanh are deliberate
model changes, and extra randomized starts cannot remove that systematic change.

Across 32,765 complete ten-start parameter groups at the five input frequencies,
GPU and CPU float/rational point means were very close for most cases. Among
responses with power greater than 1% of input power, the largest relative
differences were 2.82% for total power and 1.22% for fundamental power; all were
within 5%. The 99th percentile was below 0.057% for total and below 0.089% for
fundamental in each frequency slice. These observations are not numerical error
bounds or guarantees for other parameters or longer observation times.

The deliberate change from real to rational tanh is more significant: in the
128-point reference subset per frequency, median total-power differences were
roughly 3–3.6%, with much larger outliers (up to 68.3%). The production-aligned
model should therefore be labeled separately rather than mixing its values into
the real-tanh table. Extra trials sample its initial-condition behavior; they do
not make these two nonlinearities equivalent.

The benchmark now shares its Metal host and kernel with the
[resumable GPU survey](ladder-gpu-survey.md). That runner applies the rational
model's selection, collects 100 starts and writes all spectral summaries to a
separately identified format-3 dataset. The old double results remain separate.
