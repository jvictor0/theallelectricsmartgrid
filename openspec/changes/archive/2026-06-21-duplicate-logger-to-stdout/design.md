## Context

`INFO(...)` currently enqueues messages into the async logger and the drain path formats each message once before writing it to the configured session log file. The logger already centralizes normal messages and missed-message reports through `AsyncLogQueue::WriteLine`, so stdout duplication can live at the sink boundary rather than in producer code.

The important constraint is that realtime audio and MIDI producer threads must remain non-blocking. Console IO must only happen when queued messages are drained, matching the existing file IO timing.

## Goals / Non-Goals

**Goals:**

- Mirror every drained log line to stdout using the same text that is persisted to the session log file.
- Keep stdout writes in the async drain path, after producer enqueue has completed.
- Cover normal messages and missed-message reports in unit tests.
- Preserve existing file log creation, flushing, and test-controlled log directory behavior.

**Non-Goals:**

- Change the `INFO(...)` macro or add producer-side stdout writes.
- Add runtime configuration for enabling or disabling stdout mirroring.
- Replace session log files or alter iPad log sync behavior.
- Introduce JUCE dependencies into the core logger.

## Decisions

1. Duplicate from `AsyncLogQueue::WriteLine`.

   `WriteLine` is the narrowest point where both normal messages and missed-message reports pass through as fully rendered text. Mirroring there keeps ordering identical between stdout and the file sink. The alternative was to mirror in `WriteLogMessage` and `WriteMissedMessage`, but that would duplicate sink logic and risk future formatting paths forgetting stdout.

2. Keep console IO drain-side only.

   Producers continue to enqueue into per-thread queues and return. The drain loop already performs file IO, so stdout IO belongs beside file IO rather than in `Log`. The alternative was immediate stdout output from `Log`, but that would violate the non-blocking logging contract for realtime callers.

3. Treat file and stdout as parallel sinks, not a fallback chain.

   A drained line should go to stdout even when the configured log file cannot be opened. File logging can still return early for the file sink, but stdout mirroring should reflect what the drain attempted to emit. The alternative was preserving the current early return before any output, but that would make stdout dependent on filesystem setup and weaken its debugging value.

## Risks / Trade-offs

- Console output may be noisy during tests or local app runs -> The behavior is intentional for debugging visibility and remains drain-side rather than producer-side.
- Capturing stdout in tests can be platform-sensitive -> Implement the test with standard C/C++ stream redirection or a small test helper scoped to the unit test process.
- If stdout write fails, logger behavior should not block or retry -> Use the same simple best-effort model as existing file logging.
