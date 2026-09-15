## MODIFIED Requirements

### Requirement: Supported Build Entry Points
The repository SHALL define the maintained build entry points as the SmartGridOne
JUCE app builds, including iOS, and the standalone CMake test executable. The
repository root and `JUCE/SmartGridOne` Makefiles SHALL expose `ios-build`,
`ios-install`, and `ios-deploy`, with `deploy-ios` retained as a deployment alias.

#### Scenario: Maintained app builds are discoverable
- **WHEN** a developer inspects project build files
- **THEN** the SmartGridOne JUCE app project remains available
- **AND** the SmartGridOne iOS exporter under `JUCE/SmartGridOne/Builds/iOS/` remains available
- **AND** obsolete top-level iOS wrapper projects are not presented as supported targets

#### Scenario: Maintained tests are discoverable
- **WHEN** a developer inspects test build files
- **THEN** the maintained test entry point is the standalone CMake target under `private/test`

#### Scenario: Run an iOS goal from the root
- **WHEN** the user runs an iOS Make goal from the repository root
- **THEN** that goal resolves to the SmartGridOne iOS tooling

#### Scenario: Default root build is the JUCE app
- **WHEN** the user runs plain `make` from the repository root
- **THEN** the root `all` target forwards to the SmartGridOne JUCE Makefile's `all` target
- **AND** root `build`, `clean`, and `run` forward to the equivalent JUCE app targets

## ADDED Requirements

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
