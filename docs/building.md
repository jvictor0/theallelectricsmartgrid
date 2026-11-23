# Building and reclaiming worktree output

Use the existing worktree and reuse its builds across edits, reviews, and retries.
Commands below run from the checkout root. From elsewhere, use
`make -C /absolute/path/to/worktree <target>` or
`make -f /absolute/path/to/worktree/Makefile <target>`; paths are resolved from
the Makefile, not the caller's working directory. The app Makefile also supports
`make -C JUCE/SmartGridOne <app-target>` and an absolute `-f` path.

## Set up JUCE in a new checkout or worktree

App builds, `ios-deploy` (and `deploy-ios`), `clang-tidy`, and the MIDI and
visualizer test targets automatically initialize JUCE when it is missing from
the checkout or worktree. Setup finishes before compilation starts, including
with parallel Make goals. An initialized JUCE checkout is left untouched.

To explicitly initialize or update JUCE to the repository's pinned revision, run:

```sh
make juce-setup
```

This runs `git submodule update --init --recursive -- third_party/JUCE` from
the checkout root, checking out the JUCE revision pinned by this repository.
It is safe to rerun and may download missing Git objects. Automatic setup uses
the same command; if it fails, the build stops before compiling or deploying.

## Build and test

| Command | Default output directory |
| --- | --- |
| `make build` / `make run` | `build/xcode/macos/Release/` |
| `make build-sanitized` / `make run-sanitized` | `build/xcode/macos/Debug/` |
| `make ios-build` / `make ios-install` / `make ios-deploy` | `build/xcode/ios/Release/` |
| `make core-test` | `build/core/RelWithDebInfo/` |
| `make perf-build` / `make perf-test` / `make perf-run` / `make perf-sgrec` / `make perf-responses` | `build/tools/perf/Release/` (results in `build/results/`) |
| `make perf-ipad-build` / `make perf-ipad` | `build/tools/perf-ipad/Release/` (results in `~/Documents/SmartGridOne/perf.sqlite3`) |
| `make visualizers-test` | `build/visualizers/Release/` |
| `make midi-test` | `build/midi/Release/` |
| `make clang-tidy` | `build/clang-tidy/report.txt` |
| `make lean-test` | `build/lean/` |
| `make lean-dsp-test` | `build/lean/` |
| `make lean-dsp-codegen` / `make lean-dsp-codegen-test` | `build/lean/generated/`, `build/lean/codegen-test/` |
| `make lean-dsp-generate` | Updates the headers in `formal/DSP/generated/` from build-tree generation |
| `make perf-ladder-build` / `make perf-ladder PERF_ARGS="9 4194304"` | Builds/runs the ladder comparison in `build/tools/perf/Release/` |
| `make ladder-periodic-grid` | Research sweep helper in `build/tools/ladder-periodic-grid/`; table in `build/results/ladder-periodic-grid/` |
| `make ladder-energy-grid-build` | macOS SIMD research runner in `build/tools/ladder-energy-grid/` |
| `make ladder-gpu-bench-build` | Standalone macOS Metal research benchmark in `build/tools/ladder-gpu-bench/` |
| `make ladder-gpu-grid-build` | Resumable float/rational Metal survey in `build/tools/ladder-gpu-grid/` |
| `make ladder-gpu-long-build` | Selected long Metal trajectories in `build/tools/ladder-gpu-long/`; see [long-run measurements](ladder-long-runs.md) |
| `make ladder-gpu-capture-build` | Saved-state waveform continuations in `build/tools/ladder-gpu-capture/`; see [persistent spectra](ladder-long-runs.md#complete-spectra-of-the-persistent-responses) |
| `make ladder-gpu-basins-build` | Perturbation and control-path experiments using the survey shader, in `build/tools/ladder-gpu-basins/` |
| `make ladder-rational-period-build` | Binary64 rational return map and derivative diagnostics, in `build/tools/ladder-rational-period/` |
| `make ladder-gpu-continuation-build` | Fine resonance continuation and periodicity measurements in `build/tools/ladder-gpu-continuation/`; see [the continuation protocol](ladder-gpu-survey.md#fine-resonance-continuation-at-18-dbfs) |
| `make ladder-survey-sql-build` | Read-only checksum verifier for the [DuckDB survey importer](ladder-survey-sql.md), in `build/tools/ladder-survey-sql/` |

The CMake families also expose `core-configure` / `core-build`,
`visualizers-configure` / `visualizers-build`, and `midi-configure` / `midi-build`.
Each build reconfigures the same cache and builds incrementally; test targets
build first and run CTest with failure output. Use `JOBS=6` to change CMake build
parallelism (default 4), and `CMAKE_ARGS='...'` for additional configure options.
The core requires CMake, a C++17 compiler, Python 3, pkg-config, and libFLAC.
JUCE tests require macOS; their Make targets initialize the pinned
`third_party/JUCE` submodule automatically when it is missing.

The optional `ladder-periodic-grid` research target requires NumPy and SciPy.
It runs the detector-disabled real-tanh recurrence in binary64 over a 10×10×10×10
parameter grid and records numerical repeat residuals and settling times, without
changing the app. `LADDER_GRID_ARGS` passes probe options; for another experiment,
choose a new `--output build/results/<name>` to retain previous results. Run
`scripts/report_ladder_periodic_grid.py <output-directory>` for the offline HTML
table and CSV response view. These results are numerical observations, not proof
certificates or a performance benchmark of the app.

The separate `ladder-energy-grid-build` target builds a macOS double-precision
SIMD energy survey, with four cutoff values per worker and resumable output.
See [the energy survey guide](ladder-energy-survey.md) for the exact model,
window definitions, binary format, Studio execution, and restart procedure.
The current survey selects k*s³ > 3.5 from a 100×100×100×100 grid and runs 10
one-second initial conditions per retained point. This is a long-running
experiment, not a routine test target.

`ladder-gpu-bench-build` benchmarks a separate float/rational-tanh model using
Metal compute and a matching multithreaded NEON CPU reference. It uses the
installed Metal runtime to compile the shader source at startup. See the
[GPU benchmark notes](ladder-gpu-benchmark.md) for commands, validation and the
distinction between this bounded benchmark and the resumable full survey.
The [Metal survey guide](ladder-gpu-survey.md) documents the 100-start GPU run,
selection, measurements and recovery.

Use `CORE_CONFIG=Debug`, `VISUALIZERS_CONFIG=Debug`, or `MIDI_CONFIG=Debug` for
separate debug test builds. Core tests default to RelWithDebInfo with assertions
enabled; do not disable assertions to speed up validation. Do not use
`CMAKE_ARGS` to override the configuration or binary directory behind Make's back.
For a focused core run after `make core-build`:

```sh
build/core/RelWithDebInfo/smartgrid_tests --test-case='SequencerUI:*'
```

Native app targets use `CONFIG=Release` by default; `CONFIG=Debug` selects a
separate build. Sanitized targets always use the project's Debug configuration,
which enables AddressSanitizer. Each Xcode configuration contains `DerivedData/`,
`Intermediates/` (including precompiled headers), and `Products/SmartGridOne.app`.
Make explicitly supplies these paths, including the product directory that the
generated Xcode projects otherwise override. Use these Make targets for the
contained command-line workflow; opening a project directly in Xcode uses the
IDE's own DerivedData preferences. `XCODE_FLAGS='...'` passes additional Xcode
options. Keep the path settings intact.

`ios-install` uses the selected configuration's existing app and fails if it is
missing. `ios-deploy` builds successfully before installing and launching;
`deploy-ios` is its alias. Direct `scripts/ios_deploy.py` defaults to the canonical
iOS Release product; pass `--app` for another configuration. Device overrides and
setup are described in [ipad-tools.md](ipad-tools.md).

Run only one build/test at a time per output directory, including across separate
Make processes. Use separate configurations or an explicitly chosen build
directory for simultaneous builds with different options. A fresh build or an
output location outside the worktree needs a concrete technical reason; do not
create a new `/tmp` tree for each edit, review, or retry. Historical audit and
experiment documents retain their original commands and evidence paths.

## Performance benchmarks

`make perf-test` builds the separate optimized Release harness and smoke-tests
component, encoder matrix replay, and full SynthRig workloads. `make perf-run`
records serial repeated timings in ms CPU/sec audio, without launching the app.
See [the benchmark guide](../private/test/perf/README.md) for workload selection,
input captures, measurement boundaries, and before/after comparisons. These
targets do not change the core tests' assertion-enabled configuration.

For an SGREC performance investigation, run
`make perf-sgrec SGREC_ARGS='"/path/to/jam.sgrec" --callback-frames 512'`.
This validates and replays the whole control timeline, saving CPU/wall timings
for each callback-sized window and percentile/worst-callback summaries. Supply
the callback size from the app's audio route log; SGREC storage blocks are not
device callbacks. See the [SGREC benchmark guide](../private/test/perf/README.md#sgrec-control-timeline-replay)
for duration limits, serial repeats, output files and interpretation limits.

## Lean specifications

Run `make lean-setup` once, optionally `make lean-cache` to download the pinned
Mathlib proof cache, then `make lean-test` to check the reusable DSP filter proofs,
encoder and phase-time Lean/Veil state machines, contract proofs, proof axioms and
C++ source references.
Toolchains, dependencies and incremental proof artifacts stay in `build/lean/`;
the small Lake configuration cache is ignored at `.lake/`. This target does not
build or launch the audio app. Setup requirements, model coverage and proof
boundaries are documented in [formal/DSP/README.md](../formal/DSP/README.md),
[formal/Encoder/README.md](../formal/Encoder/README.md)
and [formal/PhaseTime/README.md](../formal/PhaseTime/README.md).

`make lean-dsp-test` runs the DSP proofs and axiom audit plus a native executable
smoke test of the generic one-pole, SVF, morph and ladder filters with
`Float` (and `Float32` for the one-poles). The full `lean-test` target also
includes this executable check.
`make lean-dsp-floor-free-probe` separately runs the proof-side ladder response
experiment in binary64, with native tanh and with its zero-input slope. It keeps
the runtime unchanged; its finite-window measurements are not convergence proofs.
Set `FLOOR_FREE_PROBE_ARGS=smoothing` for the slower-envelope sweep,
`FLOOR_FREE_PROBE_ARGS=quadrature` for single/cascaded SVF quadrature detectors,
or `FLOOR_FREE_PROBE_ARGS=quadrature-check` for additional frequencies, gains,
and the small-amplitude native-tanh comparison. These modes report coefficient
variation, period-repeat error, and the fundamental mixing term missed by
replacing the coefficient with its mean. See the
[ladder findings](../formal/DSP/Filters/NaiveReuppingLadder/README.md#detector-and-periodic-response-experiments)
for limitations and theorem inventory.
The filter arithmetic and state updates are shared by the executable cores and
the real-number proofs;
the smoke test does not establish floating-point error bounds.
It also checks the standalone C++ processor generator, compares native Lean/C++
execution, and verifies the saved generated headers are current. Both scalar
headers and `Generic<Number, Size>` headers are checked; the latter use the
app's `MultiChannelNumber` at widths 1, 2, 3 and 4 with `float` and `double`.
Run
`make lean-dsp-generate` to refresh those headers for review. See the
[codegen guide](../formal/DSP/Codegen/README.md) for the supported Lean subset and
the distinction between model proofs and code-generation tests.

The app's delay/reverb damping filters and SVF consumers include the checked-in
generated TPT headers. CMake targets and both Xcode projects include
`formal/DSP/generated` and compile with `-fno-fast-math -ffp-contract=off`, matching
the codegen arithmetic contract. Explicit `FusedMultiplyAdd` calls remain fused.
App builds consume the checked-in C++ headers without invoking Lean.

## Cleanup and completion

When keeping a worktree, `make clean` removes these complete disposable trees,
including every configuration, CMake cache, generated JUCE overlay, Xcode cache,
intermediate, product, and clang-tidy report inside them:

```text
build/core/             build/visualizers/       build/midi/
build/xcode/macos/      build/xcode/ios/         build/clang-tidy/
build/tools/
```

Cleanup does not invoke Xcode, accept output-path overrides, or remove the whole
`build/` directory. It checks all targets before deletion and refuses symlinked
target directories or ancestors. Source files, pinned `third_party/` dependencies,
`scripts/.venv-ios`, legacy build directories, external `/tmp` output, global
DerivedData, and other worktrees are outside its scope. Never run cleanup while
a build/test uses those directories.

Keep unique experiment results, recordings, and logs worth preserving under
`build/results/` (ignored and preserved by `make clean`) or outside disposable
build trees. Move any needed CTest logs out of their build tree before cleaning.

At completion, finish validation and land through the repository Git workflow.
Preserve unique results and other needed ignored files outside the worktree,
then archive/remove that worktree through its owning tool. Codex uses
`archive_worktree`; its snapshot does not preserve ignored output. If this task
initialized submodules, deinitialize those clean dependencies before archiving
because the managed archive tool does not accept initialized submodules. Do not
run a redundant `make clean` immediately before removing the checkout.
