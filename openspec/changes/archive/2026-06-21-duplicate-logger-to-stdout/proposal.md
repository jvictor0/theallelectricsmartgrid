## Why

SmartGridOne logs are currently durable in session log files, but they are not visible in the process console while the app or tests are running. Mirroring drained log lines to stdout makes live debugging and harness output easier without changing producer-side logging calls or the existing file log contract.

## What Changes

- Duplicate every drained async logger line to stdout using the same rendered line that is written to the session log file.
- Duplicate missed-message reports to stdout as well as the session log file.
- Preserve the current non-blocking producer path, per-thread queues, round-robin draining, and one-session-file behavior.
- Keep stdout mirroring in the logger drain path so realtime audio and MIDI threads do not perform console IO.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `thread-aware-async-logging`: The async logger's drained output must be mirrored to stdout in addition to the configured session log file.

## Impact

- Affected code: `private/src/AsyncLogger.hpp`, where formatted drained log lines are emitted.
- Affected tests: `private/test/unit/infra_async_logger.cpp` should cover stdout mirroring for normal log lines and missed-message reports.
- No API, dependency, or logging macro changes are expected.
