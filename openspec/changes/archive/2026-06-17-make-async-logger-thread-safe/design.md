## Context

`AsyncLogger.hpp` currently owns one `CircularQueue<LogMessage, 16384>` and exposes `INFO(...)` through `AsyncLogQueue::s_instance.Log(...)`. That queue behaves as a single-producer/single-consumer queue, but `INFO(...)` is reachable from several producers: the JUCE audio callback, JUCE message/timer callbacks, the JUCE realtime MIDI sender, `IoTaskThread`, `FileWriter`, `MxDJInternal`'s sample loader, and the Swift/CoreMIDI target's `MidiBus::SendLoop`. The drain path currently runs from `MainComponent::timerCallback()` via `DoLog()`.

The thread identity layer is broader than logging: every project-owned thread or callback execution context should have a `ThreadId`, even if it does not currently call `INFO(...)`. The async logger consumes that identity to choose a safe producer queue.

The fix should preserve the realtime contract: audio and MIDI paths must remain non-blocking and must not take a mutex just to log. The public logging call shape should stay as `INFO(...)`.

## Goals / Non-Goals

**Goals:**

- Give every project-owned thread and callback execution context a stable `ThreadId`.
- Store the active `ThreadId` in a C++ `thread_local` variable named `thread_threadId`.
- Set the thread ID at each explicit worker-thread entry point and each known callback entry point that runs project code.
- Route each `INFO(...)` call to the queue assigned to the current thread ID.
- Drain queues in round-robin order so one noisy producer does not starve others.
- Keep overflow accounting per queue and continue reporting missed messages.

**Non-Goals:**

- Replace `CircularQueue` with a general multi-producer queue.
- Add locks to realtime audio or MIDI logging.
- Change the `INFO(...)` macro signature.
- Redesign direct `juce::Logger::writeToLog(...)` call sites that are outside the async logger.
- Identify arbitrary OS-managed threads that never enter project code.

## Decisions

1. Add `private/src/ThreadId.hpp` as the shared identity contract.

   Define `enum class ThreadId` values for `Unknown`, `Message`, `Audio`, `MidiInput`, `MidiSender`, `MidiBusInput`, `MidiBusSender`, `AsyncIo`, `FileWriter`, `SampleLoader`, `Logger`, and `Count`. Define an inline `thread_local ThreadId thread_threadId = ThreadId::Unknown`, plus small helpers such as `SetCurrentThreadId`, `GetCurrentThreadId`, `ThreadIdToIndex`, and a scoped setter for callback-style entry points. The scoped setter clears `thread_threadId` to `Unknown` in its destructor so callback identities do not leak into later work on the same physical thread.

   Alternative considered: infer thread identity from `std::thread::id`. That would require registration and lookup in every `INFO(...)` call, which is more complex and less friendly to realtime paths than a direct thread-local enum read.

2. Mark callbacks even when they are not explicitly started by project code.

   The audio callback, JUCE MIDI input callback, Swift/CoreMIDI input callback, and Swift audio bridge callback are runtime execution contexts that may not correspond to a project-created `std::thread`. They still receive explicit `ThreadId` values at callback entry because the identity contract is intended to describe every project execution context, not only threads created with `std::thread`. These callback identities are scoped and clear to `Unknown` at callback exit.

   Alternative considered: only tag explicitly started threads. That would miss callback threads, which are central runtime contexts and were part of the original problem statement.

3. Set the JUCE message thread once at application initialization.

   `SmartGridOneApplication::initialise()` marks the JUCE message thread as `ThreadId::Message`. The timer-driven logger drain runs on that same message thread and does not temporarily replace the thread ID with `Logger`; otherwise the callback-style guard would clear the persistent message-thread identity. `ThreadId::Logger` remains part of the enum for a future dedicated logger thread.

   Alternative considered: immediately create a dedicated logger `std::thread`. That would be a larger lifecycle change and is not required to make producers safe, because the existing drain path is already a single consumer.

4. Use one SPSC log queue per `ThreadId`.

   Replace the single `m_queue`/`m_missed` pair with arrays indexed by `ThreadId`. Each enumerated producer writes only to its own queue, and the logger drain path is the only consumer. `Unknown` exists as a diagnostic/default context, but implementation should mark all project-owned threads and callbacks so concurrent production does not depend on the `Unknown` queue.

   Alternative considered: add a mutex around the existing queue. That would make producer writes thread-safe, but it would introduce blocking and priority-inversion risk in the audio and realtime MIDI paths.

5. Drain in round-robin order.

   Keep a `m_nextDrainIndex` member in `AsyncLogQueue`. Each drain pass starts from that index, attempts to write at most one message per queue, advances modulo the number of queues, and repeats until a full cycle finds no messages. Missed counts are reported per queue after message draining.

   Alternative considered: drain each queue fully before moving to the next. That is simpler, but a noisy thread could delay lower-rate logs from other contexts indefinitely.

## Risks / Trade-offs

- Untagged project threads or callbacks could still collide in the `Unknown` queue -> mark every discovered project thread and callback entry point, and add a test or debug assertion for unexpected `Unknown` logging.
- Logs from different threads will no longer be strictly chronological -> include the existing timestamp/sample metadata and use round-robin fairness as the priority over total ordering.
- The logger drain path currently runs on the message thread -> keep the drain on the message thread until a dedicated logger thread exists, otherwise scoped callback cleanup would erase the message-thread identity.
- Array indexing by enum can drift when enum values change -> use `ThreadId::Count` and helper conversion functions, and keep tests that iterate every thread ID.

## Migration Plan

1. Add `ThreadId.hpp` and include it from `AsyncLogger.hpp` and thread-entry source files.
2. Update `AsyncLogger.hpp` to hold per-thread queues, missed counters, and round-robin drain state.
3. Mark thread IDs in app/message-thread initialization, `MainComponent::getNextAudioBlock`, JUCE MIDI input callbacks, `MidiSender::run`, `IoTaskThread::Run`, `FileWriter`'s write thread, `MxDJInternal::LoadThread`, `MidiBus::SendLoop`, Swift audio bridge callback, and Swift/CoreMIDI input callbacks.
4. Build and run focused tests for logger queue routing plus the existing unit/system targets that compile affected headers.
5. If regression appears, the rollback is to revert the logger and thread-ID changes; no persisted data migration is involved.
