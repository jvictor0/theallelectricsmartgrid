## Purpose

Defines the maintained build entry points for the repository so obsolete wrappers and retired compile modes do not remain discoverable as supported targets.

## Requirements

### Requirement: Supported Build Entry Points
The repository SHALL define the maintained build entry points as the SmartGridOne
JUCE app builds, including iOS, the standalone CMake test executable, and the
macOS JUCE visualizer and MIDI integration tests. The
repository root and `JUCE/SmartGridOne` Makefiles SHALL expose `ios-build`,
`ios-install`, and `ios-deploy`, with `deploy-ios` retained as a deployment alias.

#### Scenario: Maintained app builds are discoverable
- **WHEN** a developer inspects project build files
- **THEN** the SmartGridOne JUCE app project remains available
- **AND** the SmartGridOne iOS exporter under `JUCE/SmartGridOne/Builds/iOS/` remains available
- **AND** obsolete top-level iOS wrapper projects are not presented as supported targets

#### Scenario: Maintained tests are discoverable
- **WHEN** a developer inspects test build files
- **THEN** the standalone CMake target under `private/test` is available through `make core-test`
- **AND** `make visualizers-test` and `make midi-test` run the macOS JUCE suites

#### Scenario: Run an iOS goal from the root
- **WHEN** the user runs an iOS Make goal from the repository root
- **THEN** that goal resolves to the SmartGridOne iOS tooling

#### Scenario: Default root build is the JUCE app
- **WHEN** the user runs plain `make` from the repository root
- **THEN** the root `all` target forwards to the SmartGridOne JUCE Makefile's `all` target
- **AND** root `build`, `clean`, and `run` forward to the equivalent JUCE app targets

### Requirement: Incremental output belongs to the invoking worktree
Make targets SHALL resolve paths from their checkout rather than the caller's
working directory. Default outputs SHALL use stable ignored directories under
`build/core/<configuration>`, `build/visualizers/<configuration>`,
`build/midi/<configuration>`, `build/xcode/macos/<configuration>`, and
`build/xcode/ios/<configuration>`. Repeated builds SHALL reuse those directories.
Core tests SHALL retain the RelWithDebInfo default with assertions enabled.

#### Scenario: Invoke Make from another directory
- **WHEN** Make is invoked with `-C` or an absolute `-f` path to either Makefile
- **THEN** its build, run, and install commands use outputs belonging to that Makefile's checkout
- **AND** Xcode DerivedData, intermediates, precompiled headers, and products stay under that checkout's canonical Xcode build directory

#### Scenario: Repeat a build after validation
- **WHEN** a target is built again with the same configuration
- **THEN** it uses the same cache, intermediates, and product paths incrementally
- **AND** no new temporary build directory is allocated

### Requirement: Scoped cleanup and worktree completion
`make clean` SHALL remove only the canonical disposable trees documented in
`docs/building.md`, across all configurations, without invoking Xcode. It SHALL
preserve unique results stored under `build/results/`, sources, dependencies
outside those trees, and unrelated outputs. Worktree completion SHALL preserve
needed ignored results after validation and landing and before removal through
the owning worktree tool; a preceding clean SHALL NOT be required.

#### Scenario: Retain a worktree and reclaim its build output
- **WHEN** `make clean` runs without active builds or tests
- **THEN** it removes the documented canonical output trees
- **AND** it leaves `build/results/`, dependencies, legacy output, and other worktrees alone

#### Scenario: A canonical directory is a symlink
- **WHEN** a cleanup target or its ancestor below the checkout root is a symlink
- **THEN** cleanup fails before deleting any target

### Requirement: Root Makefile has no Rack SDK integration
The root Makefile SHALL NOT contain Rack SDK includes, `RACK_DIR` configuration,
Rack compiler/linker settings, plugin source/distribution lists, or Rack plugin
build/package targets. All maintained root Make targets SHALL operate independently
of the Rack SDK, with no conditional or optional Rack build path.

#### Scenario: Inspect the root build configuration
- **WHEN** a developer inspects the root Makefile
- **THEN** `RACK_DIR`, the `plugin.mk` include, and Rack-specific flags and source/distribution settings are absent
- **AND** obsolete Rack plugin build/packaging instructions and comments are absent

#### Scenario: Rack SDK is not installed
- **WHEN** the user invokes a maintained root Make target on a machine without the Rack SDK
- **THEN** Make does not try to locate or load Rack build files
- **AND** the retired Rack plugin packaging targets are not offered by the root Makefile

### Requirement: Ordered wireless deployment
`make ios-deploy` SHALL build the signed Release app by default, package the
resulting app, upgrade the selected iPad over Wi-Fi, and launch the installed app.
It SHALL preserve application data through an in-place upgrade and SHALL serialize
all stages even when Make is invoked with parallel jobs. Build configuration and
device selection SHALL remain overridable.

#### Scenario: Deploy to an unlocked paired iPad
- **WHEN** the iPad is unlocked and reachable over Wi-Fi and setup/signing prerequisites are satisfied
- **THEN** `make ios-deploy` builds, upgrades, and launches SmartGridOne without a USB connection
- **AND** the installed app retains its saved patches and configuration

#### Scenario: Parallel Make invocation
- **WHEN** `make -j ios-deploy` is invoked
- **THEN** packaging begins only after a successful build
- **AND** installation completes before launch begins

### Requirement: Build and install can be invoked separately
`ios-build` SHALL compile for iOS without requiring a reachable iPad or an active
device tunnel. `ios-install` SHALL upgrade the selected device using an explicitly
identified existing app build and SHALL NOT launch it. The helper SHALL check the
expected app path and bundle ID and propagate Xcode/platform signing errors.

#### Scenario: Compile while the iPad is unavailable
- **WHEN** `make ios-build` is invoked with valid build/signing prerequisites and no reachable iPad
- **THEN** it builds the iOS app without requiring a device connection

#### Scenario: Install target has no app product
- **WHEN** `make ios-install` is invoked without its expected app product
- **THEN** the command fails and explains how to build it
- **AND** it does not choose an unrelated or stale app from another output directory

### Requirement: Deployment outcomes are explicit
Deployment SHALL print the app path, selected device, and current step. A failed
stage SHALL stop the command with a nonzero exit status and its error. A failed
build SHALL NOT trigger installation of a pre-existing product. The user SHALL
be able to rerun deployment from the beginning after fixing the cause. Deployment
SHALL NOT add receipts, artifact hashes, automatic retries, resume, or rollback.

#### Scenario: Build fails with an older app still on disk
- **WHEN** the current build fails
- **THEN** deployment reports the build failure and does not invoke installation

#### Scenario: Device locks before launch
- **WHEN** installation succeeds but iPadOS refuses launch because the device is locked
- **THEN** the command reports installed but not launched and returns nonzero
- **AND** the error tells the user to unlock the iPad and rerun

#### Scenario: Retry after a failed deployment
- **WHEN** the user reruns `make ios-deploy` after fixing a failure
- **THEN** it executes the normal build, upgrade, and launch sequence
- **AND** it does not require restoring or clearing a saved deployment state

### Requirement: Obsolete Top-Level App Wrappers Removed
The repository SHALL NOT retain checked-in obsolete top-level iOS wrapper projects.

#### Scenario: TheNonagon wrapper removed
- **WHEN** the repository tree is scanned for top-level app wrappers
- **THEN** `TheNonagon/` is absent
- **AND** `private/src/TheNonagon.hpp` remains available

#### Scenario: OctaFade cleanup
- **WHEN** the repository tree is scanned for `OctaFade` project artifacts
- **THEN** no top-level `OctaFade` application project remains checked in

#### Scenario: SmartGridOne iOS exporter preserved
- **WHEN** the JUCE project tree is scanned for generated build projects
- **THEN** `JUCE/SmartGridOne/Builds/iOS/` is present

### Requirement: No Embedded Build Mode
The repository SHALL NOT define or document `EMBEDDED_BUILD` as a supported compile mode.

#### Scenario: Test target compile definitions
- **WHEN** the standalone CMake test target is configured
- **THEN** it does not define `EMBEDDED_BUILD`

#### Scenario: Production headers
- **WHEN** production headers are compiled by supported targets
- **THEN** they do not select behavior through `EMBEDDED_BUILD`

### Requirement: Private Source Has No JUCE Dependency
Code under `private/src` SHALL NOT include JUCE headers directly or indirectly through project-owned compatibility scaffolding.

#### Scenario: Core header scan
- **WHEN** files under `private/src` are scanned
- **THEN** they do not contain `#include <JuceHeader.h>`
- **AND** they do not contain `#include "JuceHeader.h"`

#### Scenario: Standalone test compile
- **WHEN** the standalone CMake test target compiles core headers
- **THEN** it does not require JUCE include paths

### Requirement: Documentation Matches Supported Targets
Docs, specs, and helper scripts SHALL describe SmartGridOne JUCE app builds, including iOS, and standalone tests as the maintained build topology.

#### Scenario: Current support docs
- **WHEN** documentation describes current supported platforms or build targets
- **THEN** it preserves the SmartGridOne iOS app as supported
- **AND** it does not claim that obsolete top-level wrappers or embedded targets are supported
