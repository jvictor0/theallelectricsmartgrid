## 1. Thread Identity

- [x] 1.1 Create `private/src/ThreadId.hpp` with `ThreadId`, `thread_threadId` thread-local current ID storage, conversion helpers, and a scoped setter that clears callbacks to `Unknown`.
- [x] 1.2 Add enum values for unknown/default, message, audio, JUCE MIDI input, JUCE MIDI sender, Swift/CoreMIDI input, Swift/CoreMIDI sender, async IO, file writer, sample loader, logger, and count.
- [x] 1.3 Include `ThreadId.hpp` from async logger and affected thread-entry files without introducing circular includes.
- [x] 1.4 Search for project-created threads and callback entry points, and confirm every runtime context has a corresponding `ThreadId` even when it does not call `INFO(...)`.

## 2. Async Logger Queues

- [x] 2.1 Replace the single `AsyncLogQueue` queue and missed counter with per-`ThreadId` queue and missed-counter storage.
- [x] 2.2 Route `AsyncLogQueue::Log(...)` through the queue selected by the current thread-local `ThreadId`.
- [x] 2.3 Preserve non-blocking overflow behavior by incrementing only the selected queue's missed counter when that queue is full.
- [x] 2.4 Implement round-robin draining across all thread queues in `AsyncLogQueue::DoLog()`.
- [x] 2.5 Report missed-message counts per queue after draining pending messages.

## 3. Thread Entry Marking

- [x] 3.1 Mark `MainComponent::getNextAudioBlock(...)` as `ThreadId::Audio` at callback entry.
- [x] 3.2 Mark the JUCE message thread once during app initialization as `ThreadId::Message`.
- [x] 3.3 Keep the timer-driven async logger drain on the message thread unless a dedicated logger thread is introduced.
- [x] 3.4 Mark JUCE MIDI input callback entry as `ThreadId::MidiInput`.
- [x] 3.5 Mark `MidiSender::run()` as `ThreadId::MidiSender`.
- [x] 3.6 Mark `IoTaskThread::Run()` as `ThreadId::AsyncIo`.
- [x] 3.7 Mark `FileWriter`'s write thread as `ThreadId::FileWriter`.
- [x] 3.8 Mark `MxDJInternal::LoadThread()` as `ThreadId::SampleLoader`.
- [x] 3.9 Mark `MidiBus::SendLoop()` in the Swift/CoreMIDI target as `ThreadId::MidiBusSender`.
- [x] 3.10 Mark Swift/CoreMIDI MIDI input callback entry as `ThreadId::MidiBusInput`.
- [x] 3.11 Review external audio/MIDI callback entry points and set a thread ID for each project execution context, whether or not it logs today.

## 4. Verification

- [x] 4.1 Add focused tests or test-only hooks proving logs from two different thread IDs route to different queues.
- [x] 4.2 Add a focused round-robin drain test or debug harness that verifies one noisy queue does not starve another queue.
- [x] 4.3 Add or update a test that covers full-queue missed-message accounting for one selected thread queue.
- [x] 4.4 Build the affected C++ targets or closest available test target that compiles `AsyncLogger.hpp`, `ThreadId.hpp`, and the modified thread-entry headers.
- [x] 4.5 Search for remaining project thread starts and callback entry points and confirm each is covered by a thread ID assignment.
