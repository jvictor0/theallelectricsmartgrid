# Longer fundamental measurements

This follows the [variance characterization](ladder-survey-sql.md). It is a
deliberately stratified follow-up, not an estimate of proportions in the full
36.29-million-point grid. No audio-app or Lean code changes are involved.

## Experiment

Thirty configurations: four hash-selected cases each from strong-fundamental
bass, low-mid, mid and upper-cutoff groups; four weak fundamentals; four onset
cases; two stable controls; and four previously identified outliers. The original
100 random histories are reproduced exactly, with a reset-state control at
start index 100. Amplitude remains 0.25, sample rate 192 kHz, detector disabled,
and the model remains the production-style float/FMA rational saturator.

Runs cover 1, 120 and 600 simulated seconds. Checkpoints at 1, 2, 5, 10, 30, 60,
120 and 600 seconds compare:

- The final half of the elapsed run, so the measurement window grows with time.
- Fixed trailing windows of 0.5, 2 and 10 seconds.
- The full prefix from reset/random initialization.
- Temporal variation in the original whole-period, approximately 50 ms windows.

Intervals are rounded inward to whole input periods. All per-start interval
sums are retained. A long fundamental measurement sums complex coefficients
over its entire interval and then squares, separately for each trajectory.
It does not average short-window powers or average phases across starts.

## Findings

Of 27 selected variable-fundamental cases, 20 have less than 1% power SD/mean
over the final five minutes. Seven retain distinct response levels. The remaining
three configurations are two stable controls and a decaying-ringing control.

| Group | Cases | Initial fundamental CV | Final five-minute CV |
| --- | ---: | ---: | ---: |
| Strong bass/low-mid/mid | 12 | 5.3–73.2% | 0.0011–0.0903% |
| Weak tail | 4 | 44.3–109.1% | 0.0675–0.7285% |
| Onset | 4 | 36.1–101.3% | approximately 0–0.3465% |
| Upper-cutoff and named persistent outliers | 7 | 7.4–416.8% | 7.4–945.2% |

These fractions are conditional on the selected cases. The upper-cutoff group
deliberately sampled the uncommon points already known to have high fundamental
variance; the result does not mean upper cutoffs are usually inconsistent.

**Averaging and settling differ.** All 12 strong bass/mid cases retain substantial
short-window temporal variation after ten minutes. Their mean within-run temporal
CVs remain approximately 8.5–71.6%. Yet their long coherent projections agree
across initial histories: all are below 0.3% trial CV after two minutes and below
0.1% after ten minutes. Their ensemble mean fundamental gains differ by at most
0.00101 dB between the final-minute and final-five-minute estimates. This supports
a stable long-window measurement even though short-window wobble persists.

**Some cases really settle.** Three of the four onset cases, plus the decaying
control, lose their initial variability even in a fixed trailing half-second
window. The fourth onset case continues wobbling, with its long-window trial CV
falling from 101.3% at one second to 0.3465% at ten minutes.

**Weak-tail means can change materially.** The four selected weak fundamentals
change by −24.05, −0.52, −8.69 and −19.39 dB between the original half-second and
final-five-minute projections. This is consistent with finite-window leakage
from other output frequencies contributing strongly to the short measurement.
Longer projection affects the estimated curve, not just its error bars.

**Seven configurations retain separated levels.** Actual individual trials now
show clusters, rather than an inference from means/SD/min/max alone. Splitting at
the largest gap in final fundamental gain gives:

| Point ID | Lower / upper count | Lower gain | Upper gain |
| --- | ---: | ---: | ---: |
| 82905776 | 98 / 2 | 10.132 dB | 18.056 dB |
| 82985375 | 91 / 9 | 8.274 dB | 17.696 dB |
| 75983886 | 52 / 48 | 14.433 dB | 15.079 dB |
| 76973888 | 49 / 51 | 15.048–15.056 dB | 16.256 dB |
| 61887140 | 99 / 1 | −53.30 to −53.07 dB | −20.343 dB |
| 75993866 | 99 / 1 | −0.234 dB | 15.820 dB |
| 89931773 | 78 / 22 | 3.057 dB | 8.049 dB |

Group membership is identical at every saved checkpoint from one to 600 seconds
for six cases. In point 61887140 it is identical from two seconds onward; the
one-second ordering differs. This does not rule out transitions between saved
checkpoints or beyond the observed ten minutes. The reset control selects a
particular level, so a reset-defined measurement remains a possible target even
where universality over starting states fails numerically.

For point 89931773, the 78/22 split reverses the earlier total-power ordering:
the 78 high-total-power trajectories have lower fundamental power. The same
trajectories produce different spectral allocations, not just an output scaling.

## Validation and reuse

The new multi-tone Metal kernel calls the existing LadderGPU::Step and Measurement.
Coefficient setup, input-period construction and initial-state RNG come from the
survey helpers. Persistent histories cross dispatch and command-buffer boundaries.
The tone is indexed by absolute sample phase; no growing float phase accumulator
is used. Short compensated GPU sums are combined in double on the host.

- Segmented CPU/GPU comparison covers state and measurements across three
  successive dispatches. Full-period synthetic and baseline checks also exercise
  cyclic tone indexing.
- An analytic synthetic waveform checks total, fundamental, phase and DC
  normalization for every selected frequency.
- All 2,880 original one-second scope statistics match the source survey, with
  maximum scaled discrepancy 5.83e-8 (stored binary32 rounding).
- Every overlapping per-trial checkpoint value in the independent 120- and
  600-second runs matches exactly.

The 120-second simulation took 13.1 seconds on Studio; the 600-second simulation
took 64.9 seconds. There are 3,030 trajectories per run, including reset controls.
Each ten-minute trajectory advances 115,200,000 samples. The full ten-minute run
contains 349,056,000,000 sample updates. These timings exclude compilation,
validation and reporting.

## Artifacts and commands

Studio root: /Users/joyo/benchmarks/ladder-energy-grid. Results are under
build/results/ladder-long-runs-v1, with selection.json and point-ids.txt describing
the cases. The pilot, two-minutes and ten-minutes directories contain:

- metadata.json: exact input frequencies, periods and every sample interval.
- partials.bin: every per-trial interval's total/cosine/sine/DC sums and peak.
- final-states.bin: four float history states for every trajectory.
- validation.json and baseline-validation.json: validation evidence.
- checkpoint-trials.npz: per-trial coherent checkpoint measurements.
- report.json and ensemble-windows.json: compact summaries and time courses.

The binary partial file starts with six little-endian uint64 fields: magic
0x31474e4f4c44414c, cases, starts, slots, sample rate, and partial stride (20).
The body is float32 [slot, case, start, field], with five fields in metadata order.
Cases with fewer intervals have zero-measurement padding at the end. Output
directories are exclusive; an interrupted run is not automatically resumed.

    make ladder-gpu-long-build
    build/tools/ladder-gpu-long/ladder-gpu-long POINT_IDS 600 OUTPUT_DIRECTORY
    build/tools/ladder-sql-env/bin/python scripts/report_ladder_long_runs.py \
      OUTPUT_DIRECTORY SELECTION_JSON

Run reporting only after the simulator exits successfully. Plots and the
independent-prefix comparison are generated by scripts/plot_ladder_long_runs.py
on the ten-minutes directory, alongside the two-minutes directory.

Both longer runs are also in measurements.duckdb in the experiment root:
227,250 per-start/checkpoint/window rows across cases and measurements tables.
The original survey database is unchanged. Example query:

    SELECT point_id, stddev_pop(fundamental)/avg(fundamental) AS trial_cv
    FROM measurements
    WHERE run_seconds=600 AND horizon_seconds=600
      AND mode='late_half' AND NOT is_reset
    GROUP BY point_id;

scripts/import_ladder_long_sql.py DATABASE RUN_DIRECTORY... rebuilds that derived
database from checkpoint exports. Raw interval measurements remain on Studio.
These results are numerical evidence for this float model and finite durations,
not a proof of convergence or permanent attractors.

## Complete spectra of the persistent responses

The seven persistent cases were continued directly from their saved states at
600 seconds. Thirty-three waveforms cover two representatives from each
fundamental-power cluster where available, plus each reset control. Captures last
16.065–16.713 seconds at 192 kHz. Their lengths are multiples of 840 input-buffer
periods, allowing coherent measurement of the input and small-denominator
subharmonics. All parameters and the detector-disabled float/FMA rational model
are unchanged. The input amplitude is 0.25 (peak).

Letters match `spectra/persistent-spectra.png`. Counts describe the original 100
random histories, ordered by lower/higher **input-frequency power**, not total
power. These selected counts do not estimate prevalence across the full grid.

| Case / point | Input Hz | Cutoff Hz | Drive knob | Resonance knob | Lower / higher |
| --- | ---: | ---: | ---: | ---: | ---: |
| A / 82905776 | 6109.091 | 4018.466 | 90.91% | 57.58% | 98 / 2 |
| B / 82985375 | 6109.091 | 3747.635 | 98.99% | 53.54% | 91 / 9 |
| C / 75993866 | 3746.341 | 2000.000 | 100.00% | 38.38% | 99 / 1 |
| D / 89931773 | 9955.556 | 3259.502 | 93.94% | 17.17% | 78 / 22 |
| E / 75983886 | 3746.341 | 8074.035 | 98.99% | 38.38% | 52 / 48 |
| F / 76973888 | 4020.942 | 9283.178 | 97.98% | 38.38% | 49 / 51 |
| G / 61887140 | 1411.765 | 325.950 | 88.89% | 71.72% | 99 / 1 |

### A–C: a competing oscillation and mixing tones

The lower-fundamental responses are dominated by a separate tone at 5080.395,
4786.929 and 2306.889 Hz, respectively. Those lines carry approximately 64.69%,
70.92% and 86.46% of the output power. The input frequency carries only 14.50%,
10.14% and 2.14%. A strong additional line is at twice the oscillation frequency
minus the input: 4051.700, 3464.768 and 867.437 Hz. Higher combinations and
harmonics explain the rich line spectra. This supports a two-frequency mixing
interpretation; it is not a proof of a quasiperiodic invariant torus.

The higher-fundamental responses instead put 95.74%, 93.88% and 94.92% of their
power into the input frequency, with most of the remainder in its odd harmonics.
Total powers differ between levels by only 0.27, 0.24 and 0.42 dB. Thus similar
loudness conceals a substantial change of pitch/spectral content. Reset selects
the lower-fundamental response in all three cases.

### D: input-frequency response versus a one-third subharmonic

The 78 lower-fundamental histories produce a strong 3318.519 Hz tone, exactly
one-third of the 9955.556 Hz input. That line carries 91.40% of the output power;
the input, now its third harmonic, carries 7.78%. Odd harmonics of 3318.519 Hz
account for almost all remaining power.

The 22 higher-fundamental histories put 99.94% of the power into the input. This
response has 6.09 dB less total power, despite more input-frequency power. Reset
selects this higher-fundamental, lower-total-power response.

### E: competing subharmonic families

The 52 lower-fundamental histories have spectral lines on the input/7 family:
535.192 Hz spacing, with large components at 3746.341, 8027.875 and 15520.557 Hz
(input, 15/7 input and 29/7 input). Their power shares are 60.96%, 12.66% and
10.58%. The sampled waveform repeats exactly after seven input-buffer periods.

The 48 higher-fundamental histories instead use the input/3 family, with large
components at 3746.341, 8741.463 and 16234.146 Hz (input, 7/3 input and 13/3 input).
Their power shares are 69.27%, 7.87% and 8.16%. The sampled waveform repeats
exactly after three input-buffer periods. This is a competition between different
commensurate spectral patterns, not merely phase offsets of the same waveform.
The input buffer contains four sinusoid cycles, so these sample-repeat checks
are not assertions of minimal continuous-time periods. Reset selects the /7
family. Total powers differ by only 0.09 dB.

### F: a strong subharmonic pattern with a small irregular remainder

The lower-fundamental responses retain about 69.5% of their power at the input,
with strong components at 2.5 times input (10052.356 Hz, about 8.8%) and 4.5 times
input (18094.241 Hz, about 6.5%), plus half-integer and integer multiples. Unlike
the other commensurate cases, the two captured representatives also have a
visible broadband-looking remainder. Averaging by input-buffer phase leaves
0.448% and 1.342% of their total power unexplained; only 9.5% and 15.1% of that
remainder is concentrated in its strongest 100 FFT bins. No exact repeat was
found at the tested lags of up to 64 input-buffer periods. This alone does not
establish chaos. The lower cluster has small internal differences, not one
identical spectrum for every history.

The higher-fundamental response is dominated by the input (85.38%) and its odd
harmonics, including 3 times input (9.04%) and 5 times input (2.93%). Reset selects
this response. Total powers differ by about 0.31 dB.

### G: large relative fundamental variance, nearly the same dominant spectrum

Both levels are dominated by 352.941 Hz, exactly one-quarter of the input, with
about 93.2% of output power there and 5.84% at its third harmonic, 1058.824 Hz.
Their total powers differ by only 0.000023 dB. The input component is tiny in both:
about 0.0000036% of total power for the lower level versus 0.00713% for the higher.
The higher level also has small DC and even harmonics of the 352.941 Hz tone,
consistent with a slightly asymmetric waveform. Its input-frequency gain is
about 33 dB higher even though the dominant sound-producing spectrum is almost
unchanged. Reset selects the lower level.

### Spectrum validation and artifacts

The capture tool shares coefficient/tone setup and `LadderGPU::Step` with the
survey. A 64-sample continuation from every selected saved history matches the
original GPU measurement kernel's resulting state bit for bit. CPU/GPU output
comparison has maximum scaled discrepancy 1.26e-5. All captured input-frequency
power gains are within 0.00056 dB of their corresponding prior five-minute
measurements. FFT total energy matches time-domain mean-square energy within
1e-12 relative error.

Raw waveforms remain on Studio under `build/results/ladder-long-runs-v1/spectra`.
The directory contains `selection.json`, `validation.json`, `spectral-report.json`,
`spectral-plots.npz`, `wave-POINT-START.f32`, frozen source copies and
`provenance.sha256`. Compact results and plots are also copied into the local
worktree's matching results directory. Plots show the complete 0–96 kHz engine
spectrum (log plots start at 20 Hz and omit DC), marking 24 kHz host Nyquist;
the app's subsequent downsampling is not included. Tables use integrated Hann
main-lobe powers for off-bin lines and coherent rectangular projection for the
input. Plot heights use Hann coherent-line normalization, not power density.

    make ladder-gpu-capture-build
    python scripts/select_ladder_spectra.py build/results/ladder-long-runs-v1
    build/tools/ladder-gpu-capture/ladder-gpu-capture \
      build/results/ladder-long-runs-v1/point-ids.txt \
      build/results/ladder-long-runs-v1/ten-minutes/final-states.bin \
      build/results/ladder-long-runs-v1/spectrum-requests.txt 115200000 \
      NEW_OUTPUT_DIRECTORY build/results/ladder-long-runs-v1/spectrum-selection.json
    python scripts/analyze_ladder_spectra.py NEW_OUTPUT_DIRECTORY
    python scripts/plot_ladder_spectra.py NEW_OUTPUT_DIRECTORY

These captures distinguish the observed spectral responses. They do not prove
permanent attractors, quasiperiodicity, chaos, or precision independence.

## Basins, periodic solutions, and selecting the input-dominated response

This follow-up focuses on A–C (oscillation/mixing versus input-following), with D
(the one-third subharmonic) as a comparison. The detector remains disabled. No
app audio, generated filter, or Lean implementation changes are involved.

### Forward experiments

For each point, `ladder_gpu_basins.mm` reuses the unchanged `LadderMeasure` shader,
`LadderGPU::Step`, coefficient setup, and input preparation. It runs:

- 4,096 new random histories in the original bounded state box.
- 128 random kick directions at each of seven sizes, around each of the saved
  lower/higher response states: 1e-7, 1e-5, 0.001, 0.01, 0.1, 0.3, and 1.
- The two unperturbed saved states and reset control.
- Three paths into the target: raise feedback, raise drive, or raise both;
  three ramp durations and 35 starting histories per path/duration.

The state scales are `[0.25+k/R(g), 1/R(g), 1/R(g), 1/R(g)]`. Kick size is the
Euclidean length after dividing by these scales. It is not a percentage of the
instantaneous state or a perturbation of the input signal. Random histories use
a new seed, 20261121. Forcing phase is fixed at the saved 600-second phase for
all direct and continuation experiments; it is not the original survey's
phase-zero initialization. Basin fractions depend on this phase and sampling
measure and are not universal probabilities.

Direct trials run for at least 60 seconds; measurements use the latter half.
All 5,891 direct trials per case remain within 0.00070 dB of one of the two
reference fundamental-power levels. The final-state projections show a single
compact loop plus an isolated point in A–C; D shows three phase positions of
its subharmonic cycle plus the input-following point. These observations support
two attracting response families at each selected setting, without proving
completeness or identifying every distinct rounded-float microcycle.

| Case | Random histories: oscillating / input-following | Input-following fraction |
| --- | ---: | ---: |
| A | 3958 / 138 | 3.37% |
| B | 3806 / 290 | 7.08% |
| C | 3977 / 119 | 2.91% |
| D | 3115 / 981 | 23.95% |

The input-following response survives all 128 kicks at every tested radius
through 0.01 in A/B and through 0.1 in C/D. Larger kicks can switch basins.
The competing response also survives small kicks. This is evidence for local
attraction with different basin sizes, not merely an unstable input-following
trajectory waiting for the first tiny disturbance.

For each control path, first hold its starting coefficients for 0.25 seconds.
Then change the chosen control(s) in 64 steps. Requested ramp durations are
0.1, 1 and 10 seconds, rounded upward to whole input-buffer periods per step;
actual durations span approximately 0.135–0.147, 1.025–1.035 and 10.035–10.047
seconds. Hold the final target for at least 30 seconds. The 35 initial histories
are the saved lower response, saved higher response, reset, and 32 fresh random
histories.

**Raise feedback with drive already at its target:** all 420 trials across the
four cases and three ramp speeds reach the input-dominated response. This also
works when starting from the previously oscillating state, because the initial
zero-feedback hold removes that oscillation. The path scales the actual feedback
coefficient k linearly, rather than moving the logarithmic resonance knob at
constant speed.

**Raise drive, or raise both:** all 630 A–C trials reach the oscillating response.
The corresponding 210 D trials reach its input-following response. Thus the
path through parameters matters even though the final coefficients are identical.

### Periodic return-map diagnostics

`ladder_rational_period.cpp` evaluates the same rational recurrence in binary64
with analytic first derivatives. Endpoint coefficients and input samples are
the exact exported binary32 values. This differentiates the real-valued map,
not the rounding operation. The native recurrence is independently checked
against a scalar NumPy implementation; analytic return-map derivatives are
checked by central differences at every reported root.

For each point, damped Newton search solves `P(x)=x`, where P advances one full
repeating input buffer. Of 515 initial guesses per case, the search finds one
root per case, matching the input-dominated response. A/B have 349/268 failed
Newton guesses; C/D have none. This is not an exhaustive root count. The largest
magnitudes of the return-map eigenvalues are:

| Case | Largest multiplier | GPU/root fundamental-power difference |
| --- | ---: | ---: |
| A | 0.279433 | 0.00000027 dB |
| B | 0.220568 | 0.00000061 dB |
| C | 0.937244 | 0.00000033 dB |
| D | 0.978120 | 0.00000007 dB |

All are below one, predicting local asymptotic attraction of the numerical
periodic solutions. Root defects are at most 1.36e-13 in these solves. Eigenvalues
describe asymptotic local behavior, not monotonic Euclidean contraction on every
individual step. These are numerical diagnostics, not interval certificates.

Continuing the periodic root through 129 settings reproduces the path effect:
feedback-only continuation has multipliers below one at every sampled setting.
Drive-only and combined paths enter an unstable region in A–C before returning
to a stable input-following root at the target. Newton can follow that unstable
periodic branch while forward simulation leaves it. The maximum multiplier on
the drive-only path is 3.25, 9.25 and 2.68 for A, B and C. D's sampled paths remain
stable. Between-step stability is not certified.

### An additional unstable solution in D

Solving `P^3(x)=x` from 261 guesses finds seven roots, grouping into three cycles:

- The input-following fixed point (one phase position).
- The stable input/3 cycle (three phase positions), multiplier about 0.92261
  over three input buffers and total power 0.81153.
- A nearby unstable input/3 cycle (three phase positions), multiplier about
  1.08070 and total power 0.80108.

Perturbing one point of the unstable cycle by ±1e-6 along its unstable eigenvector
and iterating for 2.109 seconds sends one sign to the input-following response
and the other to the stable subharmonic cycle. This is direct numerical evidence
of a local dividing solution between these outcomes; it is not a global basin
boundary proof. The seven roots are not seven unrelated sounds: three positions
on one period-three orbit belong to the same cycle.

### A candidate selection rule and its limits

A concrete candidate distinguished response is: fix input amplitude/frequency,
cutoff and target drive; obtain the unique zero-feedback periodic response; then
continue that solution in feedback to the requested value. It is reproducible
and attracting throughout the sampled paths for these four points. This targets
an existing response of the final filter rather than altering its final DSP.
It does not assert that reset at the final parameters selects the same response.

We have not established this rule across the parameter grid, ruled out folds or
instability elsewhere, exhausted all periodic solutions, or proved the drifting
response is an invariant circle. E/F/G are not part of this basin follow-up.
The next mathematical step could certify the selected periodic roots and their
local stability, then examine continuation across the parameter range.

### Reproduction and retained data

Results are in `build/results/ladder-long-runs-v1/basins-v1` on Studio and in the
local worktree. Each case has model JSON, per-trial CSVs with initial/final states
and measurements from both halves, and periodic-root/continuation JSON. Case D
also has `89931773-period-3.json`. `findings.json` summarizes counts and checks;
`basins-and-continuation.png` shows the final-state geometry and local stability
along the paths. Frozen sources and checksums accompany the remote results.

    make ladder-gpu-basins-build
    build/tools/ladder-gpu-basins/ladder-gpu-basins \
      build/results/ladder-long-runs-v1/basin-requests.txt \
      build/results/ladder-long-runs-v1/ten-minutes/final-states.bin NEW_OUTPUT_DIRECTORY
    make ladder-rational-period-build
    python scripts/analyze_ladder_periodic_branches.py NEW_OUTPUT_DIRECTORY
    python scripts/analyze_ladder_periodic_branches.py NEW_OUTPUT_DIRECTORY \
      --point 89931773 --multiple 3
    python scripts/report_ladder_basins.py NEW_OUTPUT_DIRECTORY

Basin requests contain `point_id lower_global_state_index higher_global_state_index`,
referencing the preserved ten-minute state buffer. Existing output directories
are rejected by the GPU runner. The periodic script skips existing per-case
periodic JSON results; remove only those derived files if intentionally
recomputing them. Raw GPU CSVs and saved states are retained.
