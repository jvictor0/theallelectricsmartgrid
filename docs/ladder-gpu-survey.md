# Resumable Metal ladder survey

This is the production-style successor to the paused double/real-tanh survey.
It uses the validated [GPU benchmark kernel](ladder-gpu-benchmark.md), with
resumable output and all measurements from the [spectral survey](ladder-energy-survey.md).
It does not change app audio, generated filters or Lean proofs.

## Experiment

- 100 logarithmic input frequencies and 100 logarithmic cutoffs, each 20 Hz–20 kHz.
- 100 equally spaced positions each for drive and resonance.
- Amplitude 0.25, engine sample rate 192 kHz, exactly 192,000 samples per trajectory.
- 100 independent randomized four-state histories per configuration, fixed input phase.
- Keep only k*(g/R(g))³ > 3.5, with feedback after the rational model's kSafe cap.
- Detector disabled; four Euler stages with one sample of feedback delay.
- Binary32 recurrence with explicit FMAs and the production clamped rational
  R(x) = clamp(x*(x*x+27)/fma(x*9,x,27), -1, 1).
- Normalization uses R(g), including the dependent feedback cap and initial bounds.
- Alpha uses the existing survey's trigonometric formula rounded to float.
  This does not substitute the app's Math4096 lookup into the coefficient setup.

The selection contains **36,290,000 configurations**, or **3,629,000,000
trajectories** and **696,768,000,000,000 scalar sample updates**. An independent
NumPy enumeration confirmed the selection, including that no selected four-cutoff
record mixes included and excluded lanes. The real-tanh selection contained
37.08 million configurations; the new normalization changes the selection.

Drive is g=0.25*20^drive, requested feedback is 3.9*(49^resonance-1)/48.
The seed remains 20261119. Initial histories use the same deterministic SplitMix64
point/start/stage keys, with bounds ±(A+k/R(g)) for stage 1 and ±1/R(g) for stages
2–4, then round to float. No warm starts or early convergence exits reduce the
sample count. The threshold is evaluated in double from the actual float
coefficients; this convention is part of the run identity.

## Measurements and format

Format 3 uses the same 216-float point layout and 3,472-byte four-point record
as format 2. Its magic is 0x334752454444414c. The dataset contains about
**31.50 GB** of records, independent of the start count because individual starts
are summarized rather than stored.

All spectral summaries are preserved: raw total power, period-aligned
quarter/half/full-prefix and final-half total/fundamental powers, complex
fundamental, DC, residual, temporal variation, coherence, peak, crest, prefix
changes and chronological window means/deviations. Main measurements retain
mean, population SD, minimum and maximum across all 100 starts. Fundamental
power is computed for each trajectory before aggregating starts.

The GPU uses compensated float sums in each segment; the CPU combines segments
and computes statistics in double. Float projection roundoff tolerance is 2e-6
relative to total power. Definitions and record packing are shared with the CPU
survey. The existing report_ladder_energy_grid.py reads formats 1–3; its real-tanh
scalar replay explicitly refuses format 3.

## Execution

    make ladder-gpu-grid-build
    build/tools/ladder-gpu-grid/ladder-gpu-grid \
      --grid 100 --starts 100 --min-effective-resonance 3.5 --plan
    build/tools/ladder-gpu-grid/ladder-gpu-grid \
      --grid 100 --starts 100 --min-effective-resonance 3.5 \
      --threads 4 --batch-jobs 160 \
      --output build/results/grid100-starts100-kappa-gt3p5-rational-metal-v3-seed20261119

Studio's bundle is /Users/joyo/benchmarks/ladder-energy-grid. The output path
above is relative to that bundle. Build outputs stay in build/tools/ladder-gpu-grid.
Metal source compiles once at startup; GPU buffers are reused between batches.
Each batch has up to 160 four-point records × 100 starts = 64,000 GPU trajectories.
The four CPU workers postprocess completed trajectories.

The scheduler keeps one input frequency per GPU batch, cycles through a
permutation of all frequency bins, and permutes selected drive/resonance/cutoff
records. Partial results span frequencies early, but are not a statistically
random sample of parameter configurations.

The same command resumes. The --max-jobs N option limits additional four-point
records for a pilot; those records are reused by the full run. Batch size and
postprocessing worker count can change on restart. Numerical settings, seed,
execution model and source fingerprint cannot. The --cpu-reference mode exists
for bounded validation and has a separate identity, preventing mixed records.

The run uses nohup and a caffeinate -i -w PID companion. Its PID is in run.pid;
status, throughput and ETA are in progress.json and run.log. SIGTERM or SIGINT
finishes the current GPU batch, writes and fsyncs its summaries, then stops.
A directory lock prevents concurrent writers.

Each batch appends to worker-000.bin and fsyncs before progress advances. On
restart, every complete record is checked for checksum, identity and duplicate
job ID. An incomplete final record is truncated and recomputed; invalid complete
records are rejected. Startup requires space for all remaining output plus a
10 GiB reserve, which is also checked during execution.

## Validation and retained data

The original CPU survey's eight regression jobs remained bit-for-bit identical
after sharing measurement and packing functions. Its native tests still pass.
Small complete Metal and scalar-reference grids use 191,999 samples to exercise
raw intervals that cut across period boundaries. Validation covers:

- All saved fields at 64 stable zero-feedback points: 13,568 finite fields,
  maximum scaled CPU/GPU difference 1.63e-7.
- Resume from five records with a different batch size and worker count,
  producing bit-for-bit identical records to the uninterrupted run.
- Recovery of an injected incomplete trailing write.
- Rejection of incompatible settings, mixed execution models, wrong formats,
  corrupt complete records and concurrent writers.
- Filtered selection completion and bit-for-bit agreement with the same points
  in an unfiltered run.

Evidence is under Studio's build/results/ladder-gpu-grid-validation-final.
The 16,000-record pilot covered all 100 frequency bins, including setup,
measurements, checksums and durable writes. It completed in 36.49 seconds at
33.68 billion scalar updates/second, projecting about 5.75 hours for the full
selected grid. Allow roughly six hours, with runtime dependent on contention.
Pilot records remain in the full-run directory.

The CPU spectral-v2 run remains paused at 588,172 configurations and 5,881,720
trajectories. Its source snapshot and results remain intact, as do the older
filtered total-power-only partial results. They are not merged with this dataset.

## Fine resonance continuation at −18 dBFS

This separate experiment follows one response from zero feedback instead of
aggregating random starting states. It does not replace or modify the original
100-start dataset. The GPU recurrence and coefficient conventions above remain
unchanged; the input peak amplitude is now 10^(−18/20), approximately 0.125893.

For each of the 100 input frequencies × 100 cutoffs × 100 drive settings:

1. Reset the four histories at the target drive and cutoff, and hold zero
   feedback for 0.25 seconds.
2. Sweep 10,000 equally spaced resonance-knob positions from zero to one,
   spending one second at each position and carrying the complete state forward.
3. Record at the original 100 resonance positions, exactly every 101st fine
   position, only when the actual float coefficients satisfy k*(g/R(g))³ > 3.5.
   All intermediate positions and positions below that threshold still run.

There are one million continuation paths, 36,290,000 saved configurations, and
1,920,048,000,000,000 main recurrence updates. No random kicks or multiple starts
are used. Fine stepping makes the control path reproducible; it does not prove
that a response branch remains stable or that the resulting solution is unique.

### Measurements

Measurements use the latter half of each recorded one-second dwell, trimmed
inward to complete periods of the digital input buffer. They retain:

- Fundamental power, complex fundamental projection, total power, DC, residual
  power, and peak amplitude.
- Fundamental and total power in each of two measurement halves, plus the change
  in complex fundamental relative to input amplitude.
- Relative RMS output difference one physical input cycle later.
- Relative RMS output difference one exact digital buffer period later, and the
  corresponding four-state difference.
- The actual capped feedback coefficient.

The frequency construction reuses the earlier survey: input frequency is rounded
within 0.1% to p/q times the engine sample rate, with a repeating q-sample buffer.
That buffer can contain multiple input cycles. Testing only q-sample repetition
would therefore miss some subharmonics.

The physical-cycle diagnostic uses a 16-tap Lagrange interpolation of the output
at delay q/p. The manifest records its error on the pure fundamental. Distortion
harmonics, especially near Nyquist, can produce interpolation error even for a
response that follows the input. The exact-buffer diagnostic uses no interpolation.
Both are measurements, not certificates of periodicity.

Diagnostic lookahead uses copies of the current state with the current feedback
held fixed. It never changes the state carried into the next resonance position.
Fundamental and total powers are absolute mean-square output values; SQL views
also provide gains relative to the input power A²/2.

### Build, run, and inspect

    make ladder-gpu-continuation-build
    build/tools/ladder-gpu-continuation/ladder-gpu-continuation \
      --output build/results/grid100-fine10000-minus18db-continuation-v1 --plan
    build/tools/ladder-gpu-continuation/ladder-gpu-continuation \
      --output build/results/grid100-fine10000-minus18db-continuation-v1

The defaults implement the protocol above. The same command resumes a stopped
run. Checkpoints retain active histories and the next fine position, so resuming
does not restart a resonance sweep. Batch size can change on resume; numerical
settings and source identity cannot. SIGTERM/SIGINT finish the current fine step,
checkpoint, and stop.

On Studio, the run directory is:

    /Users/joyo/benchmarks/ladder-energy-grid/build/results/grid100-fine10000-minus18db-continuation-v1

Its frozen source and executable are under source/. The wrapper run.sh, controller
and worker PIDs, manifest, source hashes, validation results, benchmark, and
progress.json are retained alongside the data. The wrapper loads the finished
dataset into DuckDB automatically. A caffeinate companion prevents idle sleep.

Each points.bin record has an original-grid point ID, 16 float measurements, and
a checksum (80 bytes total). The finished binary is approximately 2.90 GB.
checkpoint.bin commits the output length, active states and scheduling position
together. Restart verifies committed records and rolls back any uncommitted tail.

    build/tools/ladder-sql-env/bin/python scripts/report_ladder_continuation.py \
      build/results/grid100-fine10000-minus18db-continuation-v1

The reporter reads committed partial output while the simulation runs. Add
--import-sql only once the run is complete; it validates record checksums and
creates measurements.duckdb, containing measurements, frequencies, and a responses
view with decoded grid coordinates and gains in dB. Partial-summary quantiles use
at most 200,000 sampled records and are not population statistics.

### Validation and timing

scripts/validate_ladder_continuation.py checks CPU/GPU agreement at stable
zero-feedback points, analytically known synthetic powers and periodicity,
bit-identical resume with different batch sizes, filtered/unfiltered agreement,
interrupted-write recovery, incompatible settings, corrupt records/checkpoints,
and concurrent-writer rejection. Final evidence is retained under Studio's
build/results/ladder-continuation-validation-v2 and copied into the run directory.
The maximum scaled zero-feedback CPU/GPU difference was 7.64e-8. Synthetic metric
error was below 5.97e-8; the exact-buffer repeat error was zero.

The M3 Ultra benchmark used 65,536 paths and measured about 50.4 billion main
sample updates/second for unrecorded fine positions. Recorded positions were
slower (18–23 billion/second at medium/high resonance) because of measurement
lookahead, but constitute only 1% of fine positions. The full run is estimated at
about 11 hours, subject to contention. This is numerical exploration of the
detector-disabled float/rational model, not a Lean proof or an app audio change.
