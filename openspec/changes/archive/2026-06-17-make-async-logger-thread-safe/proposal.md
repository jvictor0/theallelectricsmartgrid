## Why

`INFO(...)` currently funnels every producer through one async log queue that was written for a single logging thread. The audio callback, message thread, MIDI sender threads, async IO worker, file writer, sample loader, and logger drain path can all log, so concurrent producers can race inside the queue and corrupt or drop messages unpredictably.

## What Changes

- Add a shared `ThreadId.hpp` header defining a project thread enum for every known project thread and callback execution context, regardless of whether it currently logs.
- Add a thread-local current-thread variable and set it at the start of each explicitly started worker thread.
- Set the current-thread variable at the top of each message, audio, MIDI, and external callback entry point that runs project code.
- Change `AsyncLogQueue` from one shared producer queue to one queue per thread ID.
- Drain log queues in round-robin order so the logger remains fair across active producers.
- Preserve non-blocking logging behavior for realtime audio and MIDI paths.

## Capabilities

### New Capabilities
- `thread-aware-async-logging`: Defines how every project thread identifies itself and how async logging safely routes log messages through the queue for the current thread ID.

### Modified Capabilities

## Impact

- Affected code includes `private/src/AsyncLogger.hpp`, a new `private/src/ThreadId.hpp`, audio and MIDI callback entry points in JUCE and Swift/CoreMIDI-facing targets, and worker-thread entry points in `private/src/IOTaskThread.hpp`, `JUCE/SmartGridOne/Source/MidiSender.hpp`, `TheNonagon/TheNonagon/Cpp/MidiBus.h`, `private/src/FileWriter.hpp`, and `private/src/MxDJ.hpp`.
- The async logger's internal storage and drain loop change, but the public `INFO(...)` call shape should remain source-compatible.
- No new third-party dependencies are expected.
