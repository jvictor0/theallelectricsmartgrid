# Reusable performance benchmarks

These are maintained versions of the component and encoder experiments from
October/November 2026. They build against **this checkout's production headers**;
there is no alternate DSP implementation or source editing in the runner.
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
  setter-overhead comparisons. Delay clock snapshots are prepared outside timing.
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

## Historical reference

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
