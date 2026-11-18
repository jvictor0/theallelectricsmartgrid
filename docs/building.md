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

The CMake families also expose `core-configure` / `core-build`,
`visualizers-configure` / `visualizers-build`, and `midi-configure` / `midi-build`.
Each build reconfigures the same cache and builds incrementally; test targets
build first and run CTest with failure output. Use `JOBS=6` to change CMake build
parallelism (default 4), and `CMAKE_ARGS='...'` for additional configure options.
The core requires CMake, a C++17 compiler, Python 3, pkg-config, and libFLAC.
JUCE tests require macOS; their Make targets initialize the pinned
`third_party/JUCE` submodule automatically when it is missing.

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
