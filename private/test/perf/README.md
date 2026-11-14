# Reusable performance benchmarks

## Isolated benchmarks on iPad

`SmartGrid Perf` is one development app for named C++ benchmarks. It does not
link JUCE or initialize audio. The selected case runs on one worker thread;
the main thread displays a static label and records thermal transitions.
The app saves a report and exits to the home screen after success, failure,
or its timeout, even if the Mac disconnects. Keep the iPad unlocked for launch.

```sh
# Incrementally build, install, and run the current inverter (linear, fast tempo).
make perf-ipad PERF_ARGS='--notes "Initial iPad inverse baseline"'

# Original plain-double tape versus current, both written at 192 kHz.
make perf-ipad PERF_ARGS='--notes "Compare tape and phase-grid cost" -- tape-performance 3 16 5'

# Run the already installed build, without rebuilding or installing.
scripts/.venv-ios/bin/python scripts/ipad_perf.py --skip-install \
  --case inverter --notes 'Heavy reversal case' -- current-performance 16 16 5
```

The first registered case is `inverter`. Its arguments are
`current-performance|tape-performance|write-performance SETTING PERIOD_BITS [REPEATS] [CYCLES]`,
or `self-test`. Defaults are `current-performance 0 16 3`. Settings are the table
in `density.cpp`; period bits range from 16 to 24. `write-performance` compares
the pre-optimization phase inverse against current. This is the same workload
and timing code as desktop `perf_density`, including initialization outside
timing and the half-second warmup. The device entry point excludes the
multi-threaded contention test. Slow cases retain large precomputed traces;
the historical tape alone uses 512 MiB, and implementations run sequentially.

The Mac writes one row per invocation to `~/Documents/SmartGridOne/perf.sqlite3`.
`--db PATH` overrides this. The `runs` table stores the selected case, arguments,
embedded build commit and dirty flag, built-in case description, supplied notes,
status/error, arbitrary case result objects, and device/build telemetry.
The app reports its own compiled commit, including with `--skip-install`.
Thermal state is Apple's enum: 0 nominal, 1 fair, 2 serious, 3 critical. Telemetry
includes start/end samples, thermal transitions, battery/power state, and loss
of foreground status. These offline timings do not establish audio deadlines.

```sh
sqlite3 ~/Documents/SmartGridOne/perf.sqlite3 \
  'select started_at, case_name, substr(git_commit,1,8), status, notes from runs order by started_at desc limit 10;'
```

`--timeout SECONDS` defaults to 600. Failed launches/transfers remain in the
database as failures. Device JSON reports remain under the benchmark app's
`Documents/perf/` for inspection if collection fails. No recording/patch sync
is performed. Device selection uses the existing `--udid` / `--host` options.

To add a case, implement a function taking `const std::vector<std::string>&`
and `std::ostream&`, add its name/description/function to `device/Cases.cpp`,
and add its source to `device/CMakeLists.txt`. Emit one JSON object per line;
choose any metrics or units, and throw on failure. The case owns setup, warmup,
repetitions, and timed regions. Do not spawn workload threads. Reporting belongs
outside measured regions. No host-script or database-schema change is needed.

## Desktop runner

These are maintained versions of the component and encoder experiments from
October/November 2026. They build against **this checkout's production headers**;
there is no alternate DSP implementation or source editing in the runner.
The explicit inverse-density comparison below additionally compiles a frozen
historical inverse for a controlled before/after comparison.
Requires the same compiler/CMake/Python/pkg-config/libFLAC dependencies as the
core tests. No app, JUCE build, audio device, or iPad is required.

```sh
make perf-build
make perf-test
make perf-run PERF_ARGS='--suite components --seconds 12 --repeats 5'
make perf-run PERF_ARGS='--suite encoders --seconds 6 --repeats 3'
```

Builds reuse `build/tools/perf/Release/` (Release, NDEBUG, LTO, loop unrolling).
Core correctness tests retain their separate configuration and assertions.
`perf-test` smoke-runs all workload/configuration combinations, exercises
bad arguments/replay files, and checks deterministic capture/output. It has **no
timing thresholds**. This validates the harness, not DSP equivalence or quality.

Results go to a new `build/results/perf-YYYYMMDD-HHMMSS/` directory. Each run keeps
raw JSONL records, medians/min/max in `summary.json`, compiler/platform/Git
metadata, binary/fixture/replay SHA-256 hashes, and exact commands. Timings run
serially in a seeded shuffled order. Do not run builds, tests, profilers or another
benchmark concurrently when collecting reference numbers. Reuse the same
machine, power state, compiler, fixture, and capture for before/after comparisons;
compare matching case IDs in the two `summary.json` files. Dirty revisions are
identified but not archived: commit the source checkpoint you intend to preserve.

```sh
python3 private/test/perf/run.py --list
make perf-run PERF_ARGS='--case "ladder/*/*" --case "svf/*/*" --case "physical/*/*" --seconds 12 --repeats 5 --output build/results/voices-before'
make perf-run PERF_ARGS='--case "matrix/*/sweep" --seconds 6 --repeats 3 --output build/results/matrix-before'
# After a code change, reuse the exact recorded modulation input:
make perf-run PERF_ARGS='--case "matrix/*/sweep" --seconds 6 --repeats 3 --replay build/results/matrix-before/replay.bin --output build/results/matrix-after'
```

An explicit `--output` reuses that directory and replaces its result files. Keep
before/after output directories distinct. `--patch PATH` substitutes another
patch; the built-in fixture has randomized scene values and connected modulation.

## Sampled transfer responses

```sh
make perf-responses PERF_ARGS='5 0.025'
make perf-ipad PERF_ARGS='--case responses --notes "Response grid and SIMD comparison" -- 5 0.025'
```

`perf_responses [REPEATS] [SECONDS_PER_CASE]` evaluates production snapshot providers
with the latest 32 deterministic, varied coefficient captures at 321, 641 and
1024 frequency columns. These correspond to the previous 320/640-pixel grids
and the restored fixed grid. It reports CPU and wall milliseconds per **whole
plot**, with the number of output lanes in each row. Trio/quad results include
all lanes together; they must not be multiplied by lane count. Crossover results
include its LP and HP pair. Mastering has one shared-control response despite
stereo audio; FIR is an available 31-tap response, currently without a production
analyzer. The nonlinear SVF shares the linear SVF snapshot and response kernel.
PartialMachine retains its old interface and is outside this benchmark.

Both the production magnitude override and a forced `abs(complex response)` path
use the same snapshots, frequency grid and percentile implementation. The latter
isolates the shortcut's cost; it is not the historical implementation. Cases run
serially in seeded shuffled order after a half-second warmup. Snapshot construction
and coefficient design are outside timing. Sampling/copying, vector response
math, percentile selection, writing all output columns and checksum consumption
are inside timing. Frequency preparation is cached as in production. JUCE paths,
rasterization, FFTs, audio DSP, concurrent history publication and thermal/deadline
interference are excluded. A 60 Hz frame allows 16.67 ms for **all** UI work; these
numbers only establish the response-evaluation portion of that budget.

The desktop `perf-responses-build` target builds this case alone in the normal
stable performance build directory. Device results use the existing named-case
runner and SQLite report schema. Preserve JSONL output when comparing revisions.

## Units and measurement boundaries

The main unit is **milliseconds of calling-thread CPU time per second of audio**.
1000 ms/sec is one completely occupied CPU core; 10 ms/sec is 1% of that budget.
This is an offline throughput measurement, not an iPad callback deadline or xrun
measurement. Full SynthRig timing excludes its background I/O thread's CPU time.

Prioritize the heaviest DSP workloads and callback maxima when judging an
optimization. A substantial improvement near the real-time limit can justify a
small regression in a case with comfortable headroom. Three to five short-run
medians do not establish stable single-digit ms/sec audio differences. Treat
such differences as inconclusive unless stronger measurements resolve them.
Neither these whole-render medians nor their maximum measures the worst audio
callback; callback maxima require separate measurement at the callback boundary.

Component cases include input packing and the parameter mapping/setters shown in
the harness. They exclude the encoder tree, live UI, and construction. Inputs
are deterministic precomputed periodic signals. Warmup is 0.5 seconds for voices
and extras, 1 second for masters/polyxfader/meters/AHD, 2 for reverb/partial/random,
and 4 for delay. Timed durations round down to whole 128-host-sample blocks.
Block p50/p99 are wall times including timer-call overhead, for diagnostics.
No timer/empty-loop baseline is subtracted; `empty/3/0` exposes harness overhead.
Sample rates follow `SampleTimer` (currently 48 kHz host, 192 kHz engine).

IFFT cases report **microseconds per transform**, with no assumed audio cadence.
Their SECONDS argument selects `max(1, floor(SECONDS * 1000))` transforms after
32 warmups. End-to-end delay/partial cases measure the actual synthesis cadence
and report ms/sec audio.

## Component cases

Direct executable arguments are `KIND LANES MODE SECONDS [OUTPUT.f32]`.

| Executable | Kinds | LANES |
| --- | --- | --- |
| `perf_voices` | `ladder`, `svf`, `physical`, `vps`, `dual` | 1 = three independent mono instances; 3 = one trio |
| `perf_effects` | `reverb`, `delay`, `partial`, `quad_master` | 4 |
| `perf_effects` | `stereo_master`, `random` | 2 and 3 respectively |
| `perf_effects` | `polyxfader` | 1 = three mono; 3 = one trio; 4 = one quad |
| `perf_extra` | `lissajous`, `sub_osc`, `sub_kernel` | 1 = three mono; 3 = one trio |
| `perf_extra` | `mixer_inputs`, `mixer_full` | 4 |
| `perf_auxiliary` | `meter` | 1 = three mono; 3 = one trio |
| `perf_auxiliary` | `ahd`, `empty` | 3 |
| `perf_auxiliary` | `ifft`, `ifft_upsample` | 4 |

These mono/trio comparisons use the **current generic implementation at each
width**, not the historical main-branch implementation. Physical modeling uses
seeded noise per instance; mono and trio noise streams need not match bitwise.

Modes for voices/effects: **0** omits outer setter calls where possible; **1**
repeats unchanged setters; **2** changes one lane; **3** changes all lanes.
Recomputation occurs through production setter logic; the harness does not force
internal coefficient calculations. Exceptions matter when reading a comparison:

- VPS, dual and polyxfader accept controls in `Process`, so 0 and 1 both pass
  fixed controls; they are not separate no-setter kernels.
- Physical mode 0 skips `SetCombParams`/outer input updates, but `Process` still
  receives its fixed input. Other modes call `SetCombParams` every engine sample.
- Reverb mode 0 uses fixed explicit delay times/damping rather than the mode 1
  knob mapping. Delay mode 0 uses explicit read/write positions; modes 2/3 also
  enable shift/unison processing. These are different DSP workloads, not pure
  setter-overhead comparisons. Modes 2/3 use octave-down shift index 0;
  historical measurements using index 12 are invalid because the interval
  table has ten entries. Delay clock snapshots are prepared outside timing.
- Master crossover/gain controls are shared across channels: mode 2 changes the
  same shared controls as mode 3, rather than one independent channel.
- Random: 0 uses long waits, 1/2 normal random motion, 3 sustained movement.
- Extras support 1 (fixed controls) and 3 (changing controls) only. Lissajous
  includes caller phase calculation; sub_kernel includes drive and slew. Mixer
  uses 17 input slots, 9 mono inputs, and all sends/returns.
- AHD supports 0 (idle), 1 (periodic triggers), 2 (triggers plus early release).
  Meter, empty and IFFT use mode 0 only.

Optional component dumps are the final output ring: four float32 lanes per
frame (unused lanes zero), in ring storage order, not chronological audio.
They are useful for same-harness comparisons. IFFT does not accept a dump.

## Encoder matrix and full synth

```sh
B=build/tools/perf/Release/perf_encoders
P=private/test/perf/fixtures/modulation-stress.json
$B record "$P" sweep 1 build/results/modulation-input.bin
$B matrix "$P" fixed 6 build/results/modulation-input.bin
$B matrix "$P" mid 6 build/results/modulation-input.bin
$B matrix "$P" sweep 6 build/results/modulation-input.bin
$B full "$P" sweep 6
```

Scene positions: `fixed` = left endpoint, `right` = right endpoint, `mid` = 0.5,
`sweep` = a one-second triangle. Full synth also supports `scenes`, switching the
selected scene pair each second while sweeping. Full synth drives normal messages
at 1 ms intervals during a sweep and renders through SynthRig, including DSP and
parameter reactions. It warms up for 0.5 seconds before timing.

Matrix replay copies recorded host values, amplitudes and gesture weights into
the bank, processes scene state, advances the sample clock and invokes
`ProcessHostFrame`. Timing includes those copies and change detection. DSP and
modulator generation do not run in the timed loop. Every scene position uses
the same captured source stream, looping it as needed, which isolates matrix
work from changes in the source signals. Capture runs seeded SynthRig after a
0.5-second warmup; each capture position can be selected independently. The runner
defaults to a one-second sweep capture (0.02 seconds in smoke tests).

The runner regenerates two additional fixtures from the committed flat patch:
`nested` adds one modulation layer; `gestures` also adds scene-dependent gestures
on roots and modulation depths. `matrix_gestures` supplies deterministic changing
gesture weights precomputed outside timing. These are synthetic stress cases,
not recordings of a performance. The large replay payload is regenerable and
stays ignored; its versioned header rejects mismatched rates/dimensions, truncated
or trailing data, and nonfinite input. Format v1 uses a 64-byte header and native
little-endian IEEE float32 arrays; captures are not portable across other endian
or float formats. Change the format version if source interpretation changes.

Append `[DUMP]` to full/matrix commands for comparisons. Full dumps contain
SynthRig's chronological seven-float output frames (quad, stereo, sub); matrix
dumps contain four floats per named encoder at 1 ms intervals in enumeration
order. Dump I/O is inside timing, so use dumps for validation, not reference
performance. Seeded capture, full output and nested/gesture matrix output are
checked for repeatability by `perf-test`.

The `full/mixed/*` cases run a mixed fixture through the complete SynthRig:

```sh
make perf-run PERF_ARGS='--case "full/mixed/*" --seconds 6 --repeats 5'
```

The fixture alternates alphabetically sorted parameter names: 165 parameters
receive two gestures each and have their modulation routes removed; the other
164 retain the original modulation routes and have no gestures. Gesture targets
reuse that parameter's values from other scenes, preserving its value range.
The two gestures have different scene activation patterns and are distributed
across all 16 faders. At least one is active in each scene. Fixed gesture weights
start at 0.5.

| Case | Scene blend | Gesture weights |
| --- | --- | --- |
| `full/mixed/fixed` | Left endpoint | Fixed at 0.5 |
| `full/mixed/blend` | One-second triangle | Fixed at 0.5 |
| `full/mixed/gestures` | Left endpoint | Sweeping |
| `full/mixed/both` | One-second triangle | Sweeping |
| `full/mixed/scenes` | Sweeping, scene pair changes each second | Sweeping |

`full_gestures PATCH MODE SECONDS [DUMP]` drives the gesture weights with
phase-offset triangles of two, three and four seconds. It sends `SetFader`
messages every millisecond through the normal input queue and fader slews;
faders also retain their normal mixer effects. Timing includes those input
updates and the full synth. The runner generates `mixed-patch.json`, which can
also be passed directly to `full` or `full_gestures`. The full results report
loaded parameter counts and a gesture-output checksum outside the timed region.
Protocol checks verify the split, deterministic output, and that moving the
faders changes the gesture-controlled encoder outputs.

The `consumer/flat/{fixed,sweep}` and `consumer/mixed/{fixed,sweep}` cases extend
matrix replay with the actual `SquiggleBoy::SetEncoderParameters` call at every
192 kHz engine sample. Each call reads the current oversample cache through the
production getters and updates the DSP parameters. These cases guard the
encoder/consumer boundary against moving expensive work out of the matrix and
into `GetValue` or parameter application. Full SynthRig remains the primary
optimization target and includes that path with live DSP feedback.

Direct commands are `consumer PATCH MODE SECONDS REPLAY [DUMP]` and
`consumer_gestures PATCH MODE SECONDS REPLAY [DUMP]`. The latter uses the mixed
fixture with the same precomputed synthetic gesture weights as matrix replay;
it does not run full-rig fader slews or audio DSP. Timings include publication,
getters and parameter application together. `parameter_updates` reports the
number of actual setter passes. Protocol checks confirm deterministic output,
the engine-rate call count and matching encoder output with/without the consumer.

## Saved patches through the full synth

```sh
make perf-run PERF_ARGS='--suite saved --patch /path/to/saved-patch.json --seconds 6 --repeats 5'
```

These four cases load the supplied patch without injecting modulation routes or
gestures. Without `--patch`, they use the committed Espace Étale saved fixture.
The input file is read only; normal patch loading/migration applies.

| Case | Blend | Gesture faders |
| --- | --- | --- |
| `saved/hold` | Saved position | Saved positions |
| `saved/blend` | One-second triangle | Saved positions |
| `saved/gestures` | Saved position | Sweeping |
| `saved/both` | One-second triangle | Sweeping |

All four render the complete SynthRig. Gesture runs discover fader IDs linked
to stored gestures active in at least one scene, including gestures on nested
modulators. Only those faders receive the two-, three-, or four-second triangle
messages; unrelated faders retain their saved positions. Their ordinary mixer
effects and input slews remain in the workload. No gesture is activated or
created. A patch without stored active gestures therefore has identical control
motion in `hold`/`gestures` and in `blend`/`both`.

Direct commands use `full PATCH hold SECONDS [DUMP]` to retain the loaded blend,
or `full_saved_gestures PATCH hold|sweep SECONDS [DUMP]` to move only its gesture
faders. The original `full_gestures` command still moves all sixteen faders for
synthetic stress tests. JSON reports the selected fader bitmask and initial/final
blend along with loaded parameter counts. Protocol checks exercise nonzero saved
blend preservation, repeatability, unchanged input files, and the no-gesture case.

## SGREC control-timeline replay

Benchmark a v6 recording with one command. This replays assignments and transport
through the complete SynthRig, with the ordinary encoder and fader smoothing:

```sh
make perf-sgrec SGREC_ARGS='"/path/to/performance.sgrec" --callback-frames 512'
make perf-sgrec SGREC_ARGS='"/path/to/performance.sgrec" --callback-frames 512 --seconds 960 --repeats 3 --output build/results/jam-before'
```

`--callback-frames` is required: obtain it from the app's `Audio route ... frames=`
log for the incident. SGREC **does not store device callback size or boundaries**;
its `block_frames` describes storage/compression blocks and must not be used here.
512 host frames at 48 kHz gives 93.75 measured callbacks per audio second and a
10.667 ms budget. This is offline execution without real-time pacing, not a
reproduction of device scheduling. Windows are aligned to recording frame zero;
the original callback offset and route changes during a recording are unknown.

The default renders the entire recording. `--seconds` limits rendering from frame
zero; it does not skip history. Repeats are independent, serial runs. The runner
creates a new `build/results/sgrec-perf-.../` directory, or uses the explicitly
named **new** directory; it refuses to replace previous results. It saves:

- `callbacks-N.jsonl`: one row per callback-sized window, including exact start
  frame, duration, calling-thread CPU seconds, wall seconds and control event count.
- `summary.json`: per-run CPU and wall p50/p95/p99/p99.9/max, ten worst callbacks
  with frame positions, and counts exceeding the nominal callback budget.
  These are desktop budget comparisons, **not measured device xruns**. Full
  callbacks determine percentiles and exceedance counts; a partial tail is
  reported separately and included in total CPU/wall time and throughput.
- `metadata.json`: source recording timestamp/revision/hash, checkout revision
  and dirty status, compiler/platform, binary/replay hashes and exact commands.
  `source.diff` retains the tracked working diff, not untracked source files.
- Validated `replay.json`, per-run logs and final patches. The latter can be
  compared with `extract_recording.py patch --sample`.

Replay metadata records completion, failure, or interruption. Interrupting a
run stops its native replay process and retains completed repetitions and the
partial callback output for inspection.

After `make perf-build`, the runner can also be invoked directly:

```sh
python3 private/test/perf/run_sgrec.py performance.sgrec --callback-frames 512
```

For low-level use, preparation and native replay remain separate commands:

```sh
python3 scripts/prepare_sgrec_replay.py performance.sgrec build/results/replay.json
build/tools/perf/Release/perf_sgrec build/results/replay.json 512 all build/results/callbacks.jsonl build/results/final-patch.json
```

The native duration is `all` or a positive number of seconds; the final patch
path is optional. Preparation refuses an existing output; the native executable
replaces its explicitly named result files. Use the runner for provenance and
overwrite protection.

Preparation checks every block CRC, frame continuity, event framing and clean
completion. It decodes the first sampled phase block, and skips audio. It
rejects bulk patch load/reset events rather than approximating their semantics.
Native replay rejects unknown controls, invalid paths, and out-of-order events.

Each output row also records current clock phase and the last delay grain's
active PVDR bin count (four channels combined). Control events still split
rendering at their exact host frame inside a measured callback; they do not split
timing rows or shift the next timing boundary. Timing includes timer overhead,
SynthRig output capture, finite checks, control application and the test grid
wrapper. No timer baseline is subtracted. Result rows are buffered in fixed-size
batches and written outside timing. Timing excludes JSON preparation, result
writing, recording compression, device I/O, background I/O-thread CPU and GUI rendering.
The last-grain bin count is a snapshot, not an average for the timing window.

This is **control replay, not deterministic audio reconstruction**. It restores
the initial patch, settles patch-load controls for half a second, initializes
the clock from the first recorded unmodulated phase, then follows recorded event
times at host-frame resolution. It does not force subsequent recorded phases.
Delay/history buffers, oscillators, envelopes, and random state at recording
start are not recoverable from SGREC. Random sources use reproducible harness
seeds. Raw external input and sample assets are not replayed; the default audio
input is silent. UI selection and MIDI output work are also omitted.

Build against the recording's revision when comparing behavior. An embedded
SHA does not establish whether the original app had uncommitted changes.
Compare relative workload changes rather than equating desktop throughput with
the iPad's smoothed callback-budget meter.

## Audio callback diagnostics overhead

`build/tools/perf/Release/perf_audio_diagnostics 1000000` measures the callback
instrumentation without DSP: wall-clock-only timing, paired wall/thread-CPU
timing with aggregation, a synthetic overrun storm, and a stalled reporter with
a full queue. It reports CPU/wall nanoseconds per callback and asserts zero C++
allocations in each measured loop. It exercises publication at approximately
the current 47 callbacks per audio second; consumer queue copying is included,
but formatting and disk I/O are excluded. Run after `make perf-build`, serially
with other benchmarks. This is a host measurement, not an iPad latency guarantee.

On 2026-11-11, five serial runs of one million callbacks per mode on this arm64
Mac measured median CPU costs of 35.7 ns for wall-only timing, 289.5 ns with
metrics, 291.2 ns during an incident storm, and 290.2 ns with a full queue and
stalled reporter. All measured loops reported zero C++ allocations. The added
normal cost was about 0.25 microseconds per callback; total metrics cost was
0.0014% of the current 1024-frame / 48 kHz budget. Raw runs, source snapshots
and build metadata are retained in `build/results/audio-metrics-overhead/`.

## Historical reference

### Inverse density comparison

`perf_density` compares three implementations: the sample-coordinate inverse
frozen from commit `a8a6ca1` (`legacy`), the preceding phase grid with no branch
knots (`phase_grid`), and the current compact branch-aware inverse (`current`).
The frozen headers differ from their source versions only in type name and
provenance comment. All three receive identical engine-sampled trajectories,
isolating inverse storage from other phase slew and transport changes.
Constant-tempo traces make the old coordinate exactly
`phase * engineSamplesPerMasterCycle`.

```sh
make perf-build
build/tools/perf/Release/perf_density self-test
build/tools/perf/Release/perf_density correctness 3 24
build/tools/perf/Release/perf_density performance 14 16 5
python3 private/test/perf/run_density.py --mode correctness --output build/results/inverse-comparison
python3 private/test/perf/run_density.py --mode performance --output build/results/inverse-comparison
```

The native arguments are `MODE SETTING PERIOD_BITS [REPEATS] [CYCLES]`. The
setting table is in `density.cpp`; period bits may be fractional, within 16–24.
The serial runner covers 120 cases: 17 production settings plus five finite
Fourier warps at four tempos, and eight production settings at four off-grid
tempos. Traces cover 3.25 master cycles and at least two seconds. No production
settings are changed. Fourier cases include gentle, reversing, high-frequency,
and degree-three clustered-turn inputs; they declare a spectral range without
assuming that today's production oscillator satisfies it. Source snapshots,
hashes, compiler/binary metadata, and raw rows are retained separately by mode.
The runner refuses to replace an existing combined result file.

Correctness queries all 16 supported read speeds, five delay fractions and six
loop selections at every PV hop, plus periodic scope queries. It reports raw
and window-constrained timestamps in host frames, including a settled subset
after two master cycles. Missing/spurious mappings and old-accurate/new-inaccurate
intersections are reported separately. Rows prefixed `prior_` compare the preceding
phase grid against the compact inverse; other error rows compare legacy against
compact. Storage rows record payload size, fixed piece capacity, island reassignments,
noncausal returns and reconstructed phase error in cell widths. Latest-forward
timestamp differences are diagnostic: fixed-cell reassignment deliberately permits
an older valid traversal. Availability and phase errors must be evaluated separately. No universal
error or performance acceptance threshold is silently inferred from the results.

The independent oracle retains complete sampled forward runs and clips their
overwritten intervals. It does not grid the inverse. Its self-test compares
against a backwards exhaustive search through randomized samples, and checks
the production trajectory generator against `TheoryOfTime::Process`. Forward
endpoints are included and stationary observations do not advance them; at a
shared endpoint the latest forward visit wins. This corrects the half-open
endpoint convention in the earlier density experiment, so its old result counts
need not match exactly. `inspect SETTING PERIOD_BITS SAMPLE PHASE` also checks
a full-size query by backwards search. It prints the selected one-sided endpoints,
compares them with oracle endpoints only when the result is in that latest run,
and reports whether fallback or replacement was selected. It also measures the
phase difference over paired analysis support. This inspection performs no timing measurements.
Upstream oscillator aliasing is shared by all implementations, not counted as
an inverse regression. These measurements concern the sampled trajectory;
the Lean chord bound separately covers an assumed continuous spectral model.

Performance excludes trajectory generation, initialization, oracle work and
output. It includes every inverse write and four reads per PV hop, with calling-
thread CPU measurements over 1024-host-frame blocks (one PV hop, not necessarily
the device's callback size) after half a second of warmup. Five repetitions rotate
all three implementation orders. JSONL includes CPU ms/audio-second,
block p50/p99/max CPU time, maximum wall time, and separate counts of
visited grid edges and peak edge visits per engine sample/block. Fixed-cell visits
include the upper endpoint and perform entry/completion stores, so these counts
are not atomic-store counts. It includes unsupported
inputs and repeated-reassignment paths; unavailable reads do not exempt a case from timing.
These are isolated desktop inverse costs, not whole-app or iPad deadline
measurements. Run serially without competing builds or tests. `work` computes
only the grid-edge counts.

[reference/README.md](reference/README.md) and its small JSON summaries retain
selected component and evaluator experiments. They are historical evidence, not
thresholds for this harness. The larger raw traces, flamegraph viewers and
one-off experiments remain in ignored `build/results/`; they are not dependencies
of these benchmarks. Do not commit raw profiler exports or inherited environments.

The `matrix/{flat,nested,gestures}/knobs` cases keep the scene blend fixed and
retarget every sixteenth named control at 30 Hz, with a second tick 1 ms later.
They measure moving manual controls alongside the same replayed live sources;
`fixed` measures the corresponding settled controls. Both include input updates
and matrix publication.
