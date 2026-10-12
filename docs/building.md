# Building and reclaiming worktree output

Use the existing worktree and reuse its builds across edits, reviews, and retries.
Commands below run from the checkout root. From elsewhere, use
`make -C /absolute/path/to/worktree <target>` or
`make -f /absolute/path/to/worktree/Makefile <target>`; paths are resolved from
the Makefile, not the caller's working directory. The app Makefile also supports
`make -C JUCE/SmartGridOne <app-target>` and an absolute `-f` path.

## Build and test

| Command | Default output directory |
| --- | --- |
| `make build` / `make run` | `build/xcode/macos/Release/` |
| `make build-sanitized` / `make run-sanitized` | `build/xcode/macos/Debug/` |
| `make ios-build` / `make ios-install` / `make ios-deploy` | `build/xcode/ios/Release/` |
| `make core-test` | `build/core/RelWithDebInfo/` |
| `make visualizers-test` | `build/visualizers/Release/` |
| `make midi-test` | `build/midi/Release/` |
| `make clang-tidy` | `build/clang-tidy/report.txt` |

The CMake families also expose `core-configure` / `core-build`,
`visualizers-configure` / `visualizers-build`, and `midi-configure` / `midi-build`.
Each build reconfigures the same cache and builds incrementally; test targets
build first and run CTest with failure output. Use `JOBS=6` to change CMake build
parallelism (default 4), and `CMAKE_ARGS='...'` for additional configure options.
The core requires CMake, a C++17 compiler, Python 3, pkg-config, and libFLAC.
JUCE tests require macOS and the pinned `third_party/JUCE` submodule, initialized
with `git submodule update --init --recursive`.

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
