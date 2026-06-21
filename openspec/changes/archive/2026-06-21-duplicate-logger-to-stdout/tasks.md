## 1. Test Coverage

- [x] 1.1 Add a unit-test helper or scoped fixture that captures stdout during async logger drain calls.
- [x] 1.2 Add coverage proving a normal drained `INFO(...)` line appears in both the session log file and captured stdout.
- [x] 1.3 Add coverage proving missed-message reports appear in both the session log file and captured stdout.
- [x] 1.4 Add coverage or assertions showing `INFO(...)` itself does not write to stdout before `DoLog()` drains the queue.

## 2. Logger Implementation

- [x] 2.1 Update `AsyncLogQueue::WriteLine` so each rendered line is emitted to stdout from the drain path.
- [x] 2.2 Preserve session log file writes and flushing for configured log directories.
- [x] 2.3 Ensure stdout mirroring still happens when file setup is unavailable, without adding retries or blocking behavior beyond the drain-side write.

## 3. Validation

- [x] 3.1 Run the async logger unit tests.
- [x] 3.2 Run the broader standalone test target if the local build setup supports it.
- [x] 3.3 Confirm `openspec status --change "duplicate-logger-to-stdout"` reports the change as apply-ready.
