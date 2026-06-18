## Context

The repository has two different kinds of iOS-related code:

- The legitimate SmartGridOne JUCE app under `JUCE/SmartGridOne`, including the checked-in iOS exporter at `JUCE/SmartGridOne/Builds/iOS/`.
- Obsolete top-level app wrapper scaffolding such as `TheNonagon/`, and possibly `OctaFade` if any artifact remains.

Only the obsolete top-level wrappers should be removed. The SmartGridOne iOS app is part of the maintained JUCE build path and must stay.

There is also an `EMBEDDED_BUILD` compile definition used by the standalone CMake tests. That flag now mainly strips JUCE from headers and changes async logging behavior. `AsyncLogger.hpp` compiles one path for JUCE builds and another path for embedded/tests, while `private/test/CMakeLists.txt` defines `EMBEDDED_BUILD=1`. As a result, tests can pass while avoiding the same logging path that production drains from `MainComponent.cpp`.

`private/src/ios_stubs.hpp` is stale scaffolding rather than true iOS support. It currently contains Rack/widget/MIDI placeholder types for standalone builds and logging macros that include JUCE when `EMBEDDED_BUILD` is absent. That conflicts with the desired rule that `private/src` never includes JUCE.

`FileManager::GetSmartGridOneDirectory()` already provides the shared app data root, with `recordings` and `samples` as children. Logs should live beside those directories in `SmartGridOne/logs`.

## Goals / Non-Goals

**Goals:**

- Make the supported build topology explicit: SmartGridOne JUCE app builds, including iOS, plus standalone CMake tests.
- Remove obsolete top-level iOS wrapper projects such as `TheNonagon/` and any remaining `OctaFade` project artifact.
- Preserve `JUCE/SmartGridOne/Builds/iOS/` and the SmartGridOne iOS exporter.
- Remove `EMBEDDED_BUILD` as a supported compile mode.
- Enforce that `private/src` never includes JUCE headers.
- Replace stale `ios_stubs.hpp` scaffolding with accurately named non-JUCE stubs only if the standalone tests still need them.
- Make async logging JUCE-free in `private/src`, with app/test code configuring a log directory at startup.
- Write one log file per app/test session, named from its creation timestamp, with no in-session rotation.
- Sync iPad logs through `scripts/sync_ipad.py`.

**Non-Goals:**

- Do not remove the SmartGridOne iOS app/exporter.
- Do not rename the Nonagon sequencer, `TheNonagonInternal`, or the core C++ headers that define the musical/sequencer domain.
- Do not remove controller integrations such as Wrld.Bldr or QuadLaunchpadTwister.
- Do not add a new embedded, mobile, or firmware target outside the existing SmartGridOne JUCE app path.

## Decisions

### Supported Targets

Keep SmartGridOne JUCE app builds, including iOS, and the standalone CMake test target as supported build entry points. Remove only obsolete top-level wrapper projects.

Rationale: this matches the actual build topology. The maintained iOS app is the SmartGridOne JUCE exporter, not the old top-level Swift wrapper.

Alternative considered: remove all iOS project files. That would delete the legitimate SmartGridOne iOS app and is explicitly out of scope.

### Core Source Boundary

Treat `private/src` as platform-neutral C++ and forbid JUCE includes there.

Rationale: the DSP/core headers are shared by app and tests. If they include JUCE, then non-JUCE tests need broad compile flags or scaffolding to avoid app dependencies. Keeping JUCE at the `JUCE/SmartGridOne/Source` boundary makes ownership clear.

Alternative considered: keep a small JUCE branch in `AsyncLogger.hpp`. That preserves the same hidden split as `EMBEDDED_BUILD` and violates the source boundary.

### Logger File Sink

Refactor async logging into a JUCE-free queue/drain core that owns or writes to a configured session log file.

The app computes the log directory with JUCE in app code, using `FileManager::GetSmartGridOneDirectory().getChildFile("logs")`, creates it beside `recordings` and `samples`, and passes its path to the logger at startup. Tests pass a test log directory, usually a temporary directory controlled by the test fixture.

Each session opens one log file whose name is based on the creation timestamp, for example `YYYY-MM-DDTHH-MM-SS.log`. That file remains the active log for the lifetime of the session. There is no size-based or date-based rotation inside a running session.

Rationale: the logger remains testable and platform-neutral while the app still controls platform-specific filesystem location. A timestamped session file gives predictable log capture without having to coordinate in-session rotation.

Alternative considered: have the core logger call `juce::Logger`. That makes `private/src` depend on JUCE and prevents standalone tests from compiling the same path.

### Test Observability

Tests should verify the real async queue, missed-count accounting, drain ordering, and file output by configuring a test log directory and reading the produced session file.

Implementation can keep narrow `ForTesting` methods if they inspect/reset real queue state, but it should not keep a separate embedded write path or embedded-only output queues.

Rationale: async logger coverage is still valuable, but it should cover production behavior.

Alternative considered: delete async logger tests. That would remove coverage for the real-time logging behavior that recently changed.

### iPad Log Sync

Extend `scripts/sync_ipad.py` to sync `Documents/SmartGridOne/logs` from the app container to `~/Documents/SmartGridOne/logs` on the Mac.

Logs should be copied from iPad to Mac without deleting them from the iPad by default. Recordings can keep their existing copy-and-delete behavior.

Rationale: logs are diagnostic artifacts, not large captured audio sessions. Keeping them on-device until explicitly removed makes repeated syncs safer.

## Risks / Trade-offs

- Removing the wrong iOS path would delete the maintained app. Mitigation: tasks explicitly preserve `JUCE/SmartGridOne/Builds/iOS/` and delete only obsolete top-level wrappers.
- The standalone tests may currently depend on `ios_stubs.hpp` to define Rack/widget/MIDI placeholder types. Mitigation: split those into accurately named test/host stubs if still needed, and keep them JUCE-free.
- The logger file path must be configured before any meaningful drain. Mitigation: app startup and test setup both initialize the log directory before normal logging assertions or app operation.
- Session log filenames need to be unique when multiple sessions start on the same day. Mitigation: include date and time in the filename, not just the calendar date.

## Migration Plan

1. Delete `TheNonagon/` and any remaining top-level `OctaFade` project artifact found by scan.
2. Preserve `JUCE/SmartGridOne/Builds/iOS/`, the SmartGridOne iOS exporter, and SmartGridOne bundle ID usage.
3. Remove `EMBEDDED_BUILD=1` from `private/test/CMakeLists.txt`.
4. Remove active `EMBEDDED_BUILD` branches from supported source/test code.
5. Refactor `AsyncLogger.hpp` so `private/src` stays JUCE-free and the logger writes to a configured session file.
6. Add app-side log directory creation beside `recordings` and `samples`, then configure the logger at startup.
7. Update tests to configure a test log directory and assert against the produced session log file.
8. Remove or replace `ios_stubs.hpp` with accurately named non-JUCE test/host stubs if still needed.
9. Extend `scripts/sync_ipad.py` to sync logs from iPad to Mac.
10. Run the standalone CMake test suite and the maintained SmartGridOne JUCE app builds, including the iOS project if available in the local toolchain.

Rollback is a normal Git revert of the cleanup change. There is no data migration.
