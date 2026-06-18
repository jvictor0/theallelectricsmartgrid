## 1. Remove Obsolete Project Surfaces

- [x] 1.1 Delete the top-level `TheNonagon/` Xcode/Swift wrapper project and confirm core `private/src/TheNonagon*.hpp` headers remain untouched.
- [x] 1.2 Search for `OctaFade`, `octafade`, and related casing variants; delete any obsolete top-level app/project artifacts found and record if none exist.
- [x] 1.3 Preserve `JUCE/SmartGridOne/Builds/iOS/`, the SmartGridOne iOS exporter, and the SmartGridOne iOS bundle ID references.
- [x] 1.4 Update docs/specs/build notes that described the removed top-level wrappers as supported targets.

## 2. Remove Embedded Build Mode And JUCE From Core

- [x] 2.1 Remove `EMBEDDED_BUILD=1` from `private/test/CMakeLists.txt`.
- [x] 2.2 Remove active `EMBEDDED_BUILD` conditionals from supported source/test code.
- [x] 2.3 Ensure files under `private/src` do not include JUCE headers directly or through project-owned compatibility scaffolding.
- [x] 2.4 Remove or replace `private/src/ios_stubs.hpp` with accurately named non-JUCE test/host stubs if standalone tests still need Rack/widget/MIDI placeholder types.
- [x] 2.5 Keep `DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES` unless a clean test compile proves the project logging macros no longer collide with doctest short macros.

## 3. Implement Session File Logging

- [x] 3.1 Refactor `private/src/AsyncLogger.hpp` so queueing, missed-count accounting, and drain logic are compiled unconditionally and remain JUCE-free.
- [x] 3.2 Add logger startup/configuration that accepts a log directory path and creates one timestamp-named session log file.
- [x] 3.3 Append all drained log messages and missed-message reports to the same session log file until process shutdown.
- [x] 3.4 Remove the JUCE logger sink from `private/src`; keep any JUCE path resolution in `JUCE/SmartGridOne/Source`.
- [x] 3.5 Update async logger tests to configure a test log directory and assert queue selection, drain ordering, missed counts, and file contents through the production logger path.

## 4. Wire App And iPad Sync

- [x] 4.1 Add `FileManager::GetLogsDirectory()` returning `SmartGridOne/logs` beside `recordings` and `samples`.
- [x] 4.2 Configure the async logger from SmartGridOne app startup before normal timer-driven draining.
- [x] 4.3 Update `scripts/sync_ipad.py` with `MAC_LOGS_DIR`, iPad logs path setup, and one-way iPad-to-Mac log copying without deleting iPad logs.
- [x] 4.4 Update user-facing sync output so logs are listed as a synced artifact alongside patches and recordings.

## 5. Verify

- [x] 5.1 Configure and build the standalone CMake test target from a clean build directory.
- [x] 5.2 Run the full standalone test suite and confirm async logger tests pass without `EMBEDDED_BUILD`.
- [x] 5.3 Build the SmartGridOne JUCE desktop target.
- [x] 5.4 Build or validate the SmartGridOne JUCE iOS target when the local Xcode/iOS toolchain is available. Local validation was attempted and blocked by Xcode reporting `iOS 18.5 Platform Not Installed` while compiling `LaunchScreen.storyboard`.
- [x] 5.5 Confirm searches show no active `EMBEDDED_BUILD` branches in supported source/test code, no JUCE includes under `private/src`, no obsolete top-level wrapper artifacts, and `JUCE/SmartGridOne/Builds/iOS/` still present.
