# Regional response surveys

Supply two mono Lean functions, `prepare : Config → State` and
`step : State → Float32 → State × Float32`. Config is a record of Float32
controls in recipe order. State may contain nested records or fixed vectors
of Float32 values. Preparation runs once after reset. The same arithmetic
evaluator used by DSP codegen emits the sample loop for Metal and an optional
host execution backend. Per-sample host lookup operations currently belong in
preparation. The host prepares the controls and noise, fits the model, and scores the audit.
The default analyzer reads generated waveforms on the host. The two-rate
analyzer processes tiles and accumulates spectra on Metal.

From the repository root, `make response-survey-plan`, `make response-survey`,
and `make response-survey-verify` use the one-pole example. Set `SURVEY_SOURCE`
and `SURVEY_OUT` for another experiment.

Start with [the one-pole example](../../formal/SurveyExamples/OnePole.lean).
Edit a Lean recipe to choose the cost, target, and controls before running.
The Studio Lean queue exports the recipe and processor and runs the certificate
checker. The calling machine measures the response, fits the table, and stores
the results. It needs Python with NumPy and clang++.
Metal uses the system Metal framework on macOS. Use `--device cpu` for a host run.

```sh
python3 scripts/response_survey/run.py plan formal/SurveyExamples/OnePole.lean build/results/survey-onepole
python3 scripts/response_survey/run.py run formal/SurveyExamples/OnePole.lean build/results/survey-onepole
python3 scripts/response_survey/run.py query build/results/survey-onepole --axis alpha=0:1
python3 scripts/response_survey/run.py verify formal/SurveyExamples/OnePole.lean build/results/survey-onepole
```

## The experiment

Each independent audit trial draws a configuration within one audit cell and
measures a group of independent records there. Configuration axes have either
uniform linear/logarithmic within-cell sampling or discrete point masses.
The last cell index selects a named stimulus. Natural-number joint masses in
row-major order can express dependence between controls and histories. Empty
masses give equally weighted control cells times the declared stimulus masses.
An optional noise prelude drives the processor to a reachable state. The
measurement record is Gaussian noise normalized to the specified RMS. Both
prelude and measurement can use a Fourier band limit in Hz. A separate warm-up
prefix is discarded. Each trial pools wet and dry linear powers over records
and nonoverlapping periodic-Hann FFT frames. Band contributions use fractional
bin overlaps between geometric frequency midpoints. The curve is their ratio
in dB with a raw minimum of −240 dB. Interpolate linearly in log Hz and then
apply the declared display floor. The error is RMS over the entire log-frequency
interval between the first and last knots.

Training measures all training vertices. At each vertex it pools over stimuli
using the conditional cell masses. The packed format stores axis endpoints,
spacing, frequency knots, floor and signed 1/64 dB ordinates. The fitter reduces
knot counts to fit the byte limit and resamples the training lattice. The
runtime header `private/src/SurveyResponseTable.hpp` borrows these bytes without expanding the
table. Call `DisplayDb(config,hz)` to query the drawing. An artifact too small
for the corner table fails planning.

By default, neighbouring control settings contribute dB values at the same Hz.
To align features that track a positive control, put
`frequencyRelativeTo? := some "cutoffHz"` in the Lean recipe, using the name of
that control. At each output frequency knot `f`, a vertex with control value
`cᵥ` contributes its own spectrum sampled at `f * cᵥ / c`, where `c` is the
requested control value. Each vertex spectrum is linear in log Hz and extends
its endpoint values beyond the recorded range. The ordinary control weights
blend these aligned dB values. The drawing interpolates the resulting output
knots in log Hz before applying the display floor. The audit scores this same
curve. The packed table records the alignment axis and uses it during lookup.
Compare interpolation rules on development measurements and freeze the chosen
rule before collecting a fresh audit.

## What a report certifies

The audit freezes the packed bytes before drawing independent 128-bit system
seeds, one per cell trial. Given a seed, sampling and measuring a trial are
deterministic for the chosen backend. The statistical interpretation assumes
independent uniform seeds and a fixed measurement implementation. Coverage
refers to the resulting finite digital experiment. Each failed numerical
measurement counts as a miss. A failed batch can be resumed with the original
seeds. Freeze a separate test policy and risk budget before attempting repeated
certifications or selecting among several audited artifacts.

The family consists of each positive-mass cell, each dB threshold, and each
requested coverage fraction. Each test gets an equal share of the total risk.
The Lean checker uses an exact natural-number inequality on the hit count.
Outside a bad event of at most the declared total risk, every accepted lower
coverage holds simultaneously. Nonnegative weighted sums give the conditional
lower coverage of any union of complete cells, including one chosen after
seeing the report. FFT frames within a trial do not supply independent votes.
The planner reports how many trials an all-hit cell needs for each claim.
A lower endpoint of zero denotes insufficient evidence. Queries return the
smallest registered threshold certifying each requested quantile, or null
when none qualifies. Each query verifies the packed table and includes its
SHA-256. Axis index ranges are half-open. Use repeated `--axis`
options for intersections, `--cells` for arbitrary unions, or `--stimulus`
for named histories. Point grids make claims at their supported points.

A run saves generated sources, recipe, training and audit power arrays, seeds,
interpolator identity, packed bytes, and scores. Verification replays the
derivation of the table and report from those archived measurements and checks
file hashes. The measurement code, arithmetic lowering, randomness source, and
integrity of the execution environment are part of the trust boundary. These
artifacts support reproducible checking without a cryptographic proof of GPU
execution. The theorem asserts confidence in the finite measurement protocol;
an infinite-time response requires a separate dynamics argument.

The optional `plot.py RUN_DIR OUT.png` command uses Matplotlib to draw the
largest measured whole-curve error against the packed table. The plot is
descriptive. Run `make response-survey-test` to check the statistical modules,
storage budget, display-floor crossings, and mass-weighted region queries.

For the [detector-enabled naïve ladder](../../formal/SurveyExamples/NaiveReuppingLadder.lean),
run `make response-survey SURVEY_SOURCE=formal/SurveyExamples/NaiveReuppingLadder.lean`.
The included pilot covers drive and resonance knob coordinates from 0 to 1 and
cutoff from 20 Hz to 20 kHz. It uses a 192 kHz engine, a 24 kHz input band limit,
8,192 warm-up samples, four 4,096-sample measurement frames and two records per
trial. Its 3 × 3 × 7 training mesh and eight equally weighted audit cells are
for testing the pipeline. Set the training and audit budgets in the recipe for
a different run.

## Dense two-rate surveys

The [dense ladder recipe](../../formal/SurveyExamples/NaiveReuppingLadderDense.lean)
selects both the acquisition protocol and the previously chosen 10 MB display
fit. Any mono survey processor can select the same analyzer. Run the following
from the repository root with the Lean toolchain and a Metal GPU available.

```sh
make response-survey-plan SURVEY_SOURCE=formal/SurveyExamples/NaiveReuppingLadderDense.lean
make response-survey SURVEY_SOURCE=formal/SurveyExamples/NaiveReuppingLadderDense.lean
make response-survey-verify SURVEY_SOURCE=formal/SurveyExamples/NaiveReuppingLadderDense.lean
```

For a separate Metal host, run `plan` on the Lean checkout and copy its
`export.json` and four generated files in `work/` to the host. Use the same
runner, build headers, and audit sources. Build the local certificate checker
with `lake build survey_check`, then run
`python3 scripts/response_survey/run.py run-prepared PLAN_DIR OUT_DIR --device metal`.
This checks the exported file hashes before measurement. The ordinary `verify`
command re-exports the original Lean source for comparison.

`dualRate?` declares the band edges, bass crossover and training tile size.
The recipe gives each band its geometric center as a drawing knot. The
processor runs at the engine rate. The stimulus is Gaussian noise with DC and
Nyquist removed, normalized per record. A host-band stimulus is generated at
one quarter of the engine rate and Fourier-interpolated to the engine rate.
A full-band stimulus is generated directly at the engine rate.
A 257-tap FIR decimates output by four.
A second 129-tap FIR decimates the host-rate signal by eight. Each 131,072-sample
epoch contributes fifteen overlapping 4,096-point Hann frames at the host rate
and one at the bass rate. Bands whose upper edge is at or below the crossover
use the bass spectrum. When a recipe includes bands above host Nyquist, those
bands use the native, undecimated signal. Each epoch contributes sixteen
4,096-point Hann frames centered in evenly spaced 8,192-sample intervals.
Dry input follows the same analysis. Declared prelude,
warmup and measurement durations must be whole epochs. The CPU backend computes
the FIRs and transforms independently. The drawing and score extend the
endpoint ordinates to the outer band edges.

Training shuffles the lattice and shares each noise bank across the settings
in a tile. A 256-setting, 16-record tile runs 4,096 wet trajectories in parallel.
The GPU retains the generated state across chunks and returns accumulated bin
powers. Each atomic training tile stores its vertex indices and wet/dry band
powers for every trial. Restarting skips completed tiles. Audit trials each
draw their own configuration and noise and retain individual scores.

`spectralMesh?` specifies the three mesh sizes. The fitter chooses nonuniform
knots on the training lattice, fits the displayed dB curves by least squares,
and computes spectral bases for each slice on the third axis. The log-frequency
error metric determines the basis and the byte budget determines how many
quantized score and refinement planes to retain. The runtime reader interpolates
the packed scores and spectra. The dense ladder recipe fixes a
mesh of 31 drive, 39 resonance and 333 cutoff positions before its independent
audit. It trains on 73 × 73 × 443 settings from 20 Hz to the 96 kHz maximum
ladder cutoff. The log-frequency overlay spans 23.4375 Hz to 96 kHz in 144
semitone bands. The audit draws 512 fresh trials in each of 20 equal-mass
regions. The packed table has a 10,000,000-byte budget.
