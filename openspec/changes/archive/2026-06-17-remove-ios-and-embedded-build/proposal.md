## Why

The repository still carries obsolete top-level iOS wrapper projects and an `EMBEDDED_BUILD` compile fork, while the real supported app path is SmartGridOne through the JUCE project, including its iOS exporter. Keeping the old scaffolding makes build ownership ambiguous, and letting `private/src` include JUCE or compile an embedded logger branch keeps tests from exercising the same core logging code as the app.

## What Changes

- **BREAKING**: Remove obsolete top-level iOS app wrapper projects, including `TheNonagon/` and any `OctaFade` project artifact if one still exists.
- Keep the SmartGridOne JUCE app build path, including `JUCE/SmartGridOne/Builds/iOS/`, as a legitimate supported target.
- Remove `EMBEDDED_BUILD` as a supported compile definition and delete production code guarded solely for that build mode.
- Enforce the architectural rule that code under `private/src` never includes JUCE headers.
- Remove or replace stale `ios_stubs.hpp` scaffolding with accurately named non-JUCE test/host stubs only where still needed.
- Change async logging so the core logger is JUCE-free and writes session logs to a configured filesystem directory.
- Have the app create/configure a `logs` directory beside `recordings` and `samples`, then pass that directory to the logging drain path at startup.
- Have tests configure a test log directory and verify the full async queue/drain/file behavior without `EMBEDDED_BUILD`.
- Store each app run in its own log file named from the creation timestamp; logs last for the whole session with no in-session rotation.
- Update `scripts/sync_ipad.py` to sync SmartGridOne logs from the iPad app container.

## Capabilities

### New Capabilities
- `supported-build-topology`: Defines the supported SmartGridOne build entry points and excludes obsolete top-level app wrappers and embedded targets.

### Modified Capabilities
- `thread-aware-async-logging`: Tests and app builds use a JUCE-free async logger core that writes one configured file log per session.

## Impact

- Affected project artifacts include `TheNonagon/`, any stale `OctaFade` artifact, docs/specs that describe supported targets, and searches for obsolete top-level iOS wrapper paths.
- SmartGridOne JUCE project artifacts, including `JUCE/SmartGridOne/Builds/iOS/`, are explicitly preserved.
- Affected C++ files include `private/src/AsyncLogger.hpp`, `private/src/ios_stubs.hpp` or its replacement, and tests under `private/test/`, especially `private/test/CMakeLists.txt`, `private/test/support/GlobalEnv.hpp`, and `private/test/unit/infra_async_logger.cpp`.
- Affected app integration points include `JUCE/SmartGridOne/Source/IOUtils.*`, app startup/log drain setup, and `scripts/sync_ipad.py`.
- No new third-party dependencies are expected.
