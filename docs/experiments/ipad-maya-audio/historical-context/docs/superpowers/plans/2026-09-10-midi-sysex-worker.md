# MIDI SysEx worker routing implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Move normal audio-frame MIDI submission and SysEx message allocation off the audio callback onto the existing MIDI sender worker, as the user requested.

**Architecture:** Add a bounded SPSC queue of owned variable-length byte packets alongside the existing short-message queue. Audio-side writers copy raw bytes into preallocated slots; the existing worker constructs JUCE messages and submits them. Keep LED writer state retryable on queue-full, service short-message timing before each bounded SysEx dispatch, and join the worker before output-handler destruction/LED shutdown clearing.

**Tech Stack:** C++17, existing CircularQueue, JUCE8.0.15, doctest, iOS Release build and authenticated Wi-Fi deployment.

## User authorization and design

The user asked whether sendMessageNow on the audio thread could be problematic and explicitly requested moving SysEx to the MIDI thread as a lasting improvement. We found both WB and Launchpad frame-driven SysEx submissions, plus Twister/K-Mix frame-driven short-message bypasses. Route those short messages through the existing worker too, so normal frame processing makes no direct CoreMIDI call. Connection handshakes and final LED clearing are control-thread operations and do not become additional producers of the SPSC queue. Serialize them through the output handler and stop the worker before final clearing. The current MIDI-worker-off exposure must finish and be archived on its existing binary before heavy builds or deployment. Source/light focused tests can proceed while it records. No repeated approval or new user task is required; no commits/landing of the surrounding dirty diagnostics are requested.

Alternatives considered: a queue of juce::MidiMessage would still allocate/copy heap-backed SysEx on the audio producer; enlarging all16384 basic-message slots wastes tens of MB; the existing CircularByteQueue spins when full and cannot be used on audio. A small separately owned packet queue avoids those costs. This is not evidence that the call caused either observed symptom: the next run must score both.

## Global Constraints

- Preserve JUCE8.0.15, normal UI/DSP, zero app inputs/four outputs, autoplay/internal clock and the permanent48k/512 baseline.
- Keep existing worker realtime options and100microsecond idle sleep; no scheduler, UI, session or logger overhaul in this change.
- Audio-side enqueue must copy owned bytes with no heap allocation, wait, mutex, per-message log or CoreMIDI call. Never queue a pointer to a stack packet.
- Use a bounded SPSC SysEx queue:2048bytes per packet,64slots. Compile-time assert both WB and Launchpad maximum payloads fit. Preserve complete F0…F7 bytes and route identity; do not split messages.
- On full SysEx queue, return promptly and count it; reset the affected LED writer so discarded deltas are retried on a later frame. Disabled-worker diagnostic mode deliberately discards without filling either queue or forcing LED retries.
- The worker retains each queue slot until synchronous message construction/submission finishes, then pops it. Do not use PopPtr, which releases ownership too early.
- Preserve existing basic-message timestamps/latency behavior. Service basic messages before one SysEx packet per worker iteration so a waiting timestamp does not starve SysEx or a large SysEx drain monopolize the loop.
- Stop audio producers before final LED clearing. Join the MIDI worker before its route handlers can be destroyed, and before direct control-thread clearing can be followed by stale queued LED updates.
- Existing experimental flags stay available. Default worker enabled; flag0 discards normal and SysEx enqueue attempts. Low-rate diagnostics distinguish queue-full from deliberate disabled discard and worker SysEx submissions.
- Follow repository braces/member/function/comment style; preserve mixed line endings outside exact edits. No edits on main or to shared JUCE.
- No iPad query, heavy build or deployment until the active MIDI-off recording and archive are preserved. Parent owns device and recording actions.

## Task 1: Implement and test owned SysEx transport and audio-path routing

**Files:** Create private/src/MidiSysexQueue.hpp and private/test/unit/midi_sysex_queue.cpp. Modify JUCE/SmartGridOne/Source/MidiSender.hpp, NonagonWrapper.hpp and MainComponent.cpp only for routing/worker lifetime. Existing private/test/CMakeLists.txt automatically discovers new unit files; no new test framework or project required.

**Queue interface:** Implement `SmartGrid::MidiSysexQueue<MaxMessageBytes, QueueSize>` using `CircularQueue<Packet, QueueSize>` with public Packet fields `int m_routeId`, `size_t m_size`, and fixed `uint8_t m_data[MaxMessageBytes]`. Expose `bool TryPush(const uint8_t* data, size_t size, int routeId)`, `Packet* Peek()`, `void Pop()`, and `size_t Size() const`. TryPush validates non-null data and0<size<=MaxMessageBytes before reserving a producer slot with NextToPush, writes exactly size bytes and route/length, then CompletePush. A failed push leaves queued packets intact. Peek/Pop keep the current consumer slot owned until Pop. Document one producer(audio) and one consumer(MIDI worker). Size is a documented approximate third-thread diagnostic snapshot clamped to QueueSize; it is never used for capacity decisions. Generic CircularQueue remains unchanged.

**MidiSender integration:** Add `SendSysex(const uint8_t*, size_t, int)` returning bool accepted for handling. A true return means queued or deliberately discarded by disabled mode; false means invalid/full and needs a writer retry. Disabled mode increments a separate relaxed discard counter and returns true before queue access. Reject invalid route/null/size with a diagnostic counter and suitable assertion for impossible producer inputs. Enabled successful enqueue increments a relaxed queued counter; full increments a separate overflow counter. In run, HandleMessage then HandleSysex then the existing sleep. HandleSysex reads Peek, constructs juce::MidiMessage only there, submits via the existing handler SendMessage under its lock, counts submission, then Pop. Validate stable route before dereference. Extend existing once-second LogDiagnostics with SysEx queued/enqueued/submitted/full/discarded counters; no per-packet log. Add compile-time size checks at the concrete producer integration if writer types are not available in MidiSender.

**Routing:** WB Process calls SendSysex with buffer.m_buffer/m_size/m_routeId and resets only its failed color writer; Launchpad Process does the equivalent and resets its writer on rejection. Give Launchpad, encoder and K-Mix handlers their owner MidiSender pointer using the existing initialization/constructor flow. Route their normal short messages through SendMessage, preserving routes and full MIDI bytes. Do not construct juce::MidiMessage in normal audio-frame output handlers. Reuse existing route allocations. Handshake stays on control thread but uses handler.SendMessage for serialization.

**Lifetime:** Make shutdown join the MIDI worker on the non-audio owner/control thread. In MainComponent destructor CloseAudioDevice before ClearLEDs; in NonagonWrapper destructor stop the MIDI worker before any route handler destruction, while retaining I/O shutdown. ClearLEDs must stop/join queued MIDI before direct final clearing; use the handler's synchronized send. Do not force-kill a worker while it owns a queue slot or output lock. Repeated shutdown is harmless. No restart after this terminal shutdown is required.

- [x] Preserve exact pre-edit snapshots and implement meaningful queue tests first. Capture expected failure before completing the production implementation.

Example required owned-data test (adapt doctest macro names exactly to repository conventions):

```cpp
SmartGrid::MidiSysexQueue<16, 2> queue;
uint8_t bytes[] = {0xF0, 0x79, 0x01, 0xF7};
DOCTEST_REQUIRE(queue.TryPush(bytes, sizeof(bytes), 3));
bytes[1] = 0;
auto* message = queue.Peek();
DOCTEST_REQUIRE(message != nullptr);
DOCTEST_CHECK(message->m_routeId == 3);
DOCTEST_CHECK(message->m_size == 4);
DOCTEST_CHECK(message->m_data[1] == 0x79);
```

Cover queue-full rejection preserving both existing packets, FIFO route/payload order, producer wraparound, null/zero/oversize rejection, exact maximum payload including its last F7 byte, and the consumer holding a slot while a producer attempts to fill/reuse it. Include a bounded real producer/consumer test with varying route/payload patterns, not a mocked JUCE test. Test expectations must be independent literal/pattern fixtures. A realistic pointer-retention, truncation, early-pop or publication-order bug should fail these tests.

- [x] Implement the bounded queue and run focused tests. A lightweight test executable may be built during the capture with one compiler invocation using real production header plus existing doctest TestMain; no full DSP suite or iOS build until capture ends.

```sh
clang++ -std=c++17 -O2 -pthread -DDOCTEST_CONFIG_NO_SHORT_MACRO_NAMES=1 -Iprivate/test -Iprivate/src private/test/support/TestMain.cpp private/test/unit/midi_sysex_queue.cpp -o /private/tmp/smartgrid-midi-sysex-queue-tests
/private/tmp/smartgrid-midi-sysex-queue-tests
```

- [x] Implement MidiSender, producer routing and orderly shutdown with the above contracts; check exact delta and mixed line endings. Inspect normal output paths to verify no direct sendMessageNow or heap-backed MidiMessage construction remains there.
- [x] Fresh task reviewer checks spec, ownership/publication, overflow retry, ordering and worker/handler lifetime. Parent resolves all blocking findings before deployment. No mirrored source-text tests.

## Task 2: Build, deploy and measure the worker-routed MIDI variant

**Files:** Record protocol/results in docs/experiments/ipad-maya-audio and the existing checkpoint; source changes remain those reviewed above.

- [x] Finish the already-running MIDI-off960second exposure and promptly preserve iPad archive/app log before building or relaunching. Correlate its early interruptions and score periodic damage separately.
- [x] Build iOS Release using existing8.0.15 dependency/cache with a new product directory, verify strict signature, preserve IPA/source/binary hashes and old package. Run focused tests and required relevant integration checks; do not rebuild unrelated targets without justification.
- [x] Install via established Wi-Fi Upgrade preserving config/patch. Launch normal/UI-on/autoplay with MIDI_SEND_THREAD=1; verify actual MAYA48k/512, worker running, SysEx enqueued/submitted increasing, queue bounded and full0 at startup. Confirm music on K-Mix3/4 and preserve startup transients separately.
- [x] Record a separately named960second exposure, no device service traffic during measurement. Collect archive/app log afterward, inspect full-run queue/submit/overflow state and score both symptoms. Do not call a quiet preflight or short trial a fix.
- [x] Preserve source/task/final reviews and artifact evidence. Report whether off-thread submission changed symptoms, with workload/traffic and timing limits. Keep remaining hypotheses queued rather than stacking another change without new evidence/user steering.
