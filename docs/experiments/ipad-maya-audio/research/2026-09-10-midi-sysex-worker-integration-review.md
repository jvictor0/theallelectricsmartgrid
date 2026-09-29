# Final integration review — MIDI/SysEx worker

Reviewed worktree: `/Users/joyo/.codex/worktrees/e18e/theallelectricsmartgrid`.

Scope: the exact uncommitted task delta in `/private/tmp/smartgrid-midi-sysex-final-review-package.md`, against the task brief, global constraints and parent implementation plan. Earlier audio diagnostics and worker-off code are the baseline. No merge or Git action was requested. File references below are relative to this worktree unless otherwise stated.

### Strengths

- **The actual producer and consumer match the queue contract.** `JUCE/SmartGridOne/Source/MainComponent.h:99` invokes `NonagonWrapper::Process` from the audio callback. `NonagonWrapper.hpp:814` invokes frame processing from that same sample loop; `:616` and `:617` produce both wrappers' frame MIDI. The only SysEx enqueue sites are `:73` and `:301`. The sole consumer is `MidiSender.hpp:71`. Handshake and final clear remain control-thread handler calls, so this change does not accidentally turn the queue into a multiple-producer queue.

- **Queue ownership and publication are sound.** `private/src/MidiSysexQueue.hpp:30` validates payload length before reserving storage, then writes route, length and exactly the requested bytes before publication at `:46`. `private/src/CircularQueue.hpp:28`, `:38`, `:67` and `:78` use sequentially consistent head/tail atomics. Under SPSC ownership, the producer cannot overwrite a held consumer slot. The worker retains that slot through JUCE construction and synchronized submission, then releases it at `JUCE/SmartGridOne/Source/MidiSender.hpp:191`. It never uses the unsafe early-release `PopPtr` interface.

- **Normal audio-side submission is bounded and owns its data.** The production storage is 64 slots of 2048 payload bytes (`MidiSender.hpp:12`). Normal `SendSysex` work is validation, bounded copy and atomic counters; full returns promptly. Both concrete writer maxima are compile-time checked (`NonagonWrapper.hpp:37`, `:277`). Normal frame output handlers no longer construct JUCE messages or invoke `sendMessageNow`. Twister and K-Mix use their previously allocated routes through the basic queue (`:110`, `:361`).

- **Bytes and existing timestamp policy survive integration.** The SysEx queue copies complete F0-through-F7 buffers. The unchanged JUCE 8.0.15 raw-data constructor copies the specified `dataSize` verbatim (`/private/tmp/smartgrid-juce-8.0.15/modules/juce_audio_basics/midi/juce_MidiMessage.cpp:144`). Encoder factories (`private/src/EncoderMidi.hpp:204`, `:211`, `:219`) and K-Mix (`private/src/KMixMidi.hpp:11`) produce timestamp-zero, three-byte CC messages; their bytes survive the move to `BasicMidi::Size()`-based construction. Existing clock/start/stop timestamps and the 10 ms target offset remain unchanged (`MidiSender.hpp:215`, `:227`, `:243`). A future basic timestamp returns from its handler, allowing the next SysEx service in that iteration. Each iteration handles basic first, then at most one SysEx, then the existing 100 microsecond sleep (`:68`).

- **Registered handler lifetimes are stable.** The seven route registrations occur in the wrapper constructors (`NonagonWrapper.hpp:133`, `:380`), before audio production, and there is no route-table removal/replacement call site. Publication of later queue entries carries the constructor-time registration into the consumer's view. Although the sender thread starts before the wrapper constructors finish, it cannot obtain a published packet from these producers until construction is complete. Terminal shutdown joins before the route-owning members are destroyed (`:547`). Reopening an endpoint changes the handler's device, not the registered handler object's lifetime.

- **Rejected LED updates remain retryable.** Launchpad resets its sent-state and epoch only when enqueue rejects (`NonagonWrapper.hpp:73`; `private/src/LaunchPadMidi.hpp:143`). A previously generated nonempty Launchpad packet required an epoch change, so resetting to zero permits a later frame to revisit the bus without requiring another color change. WB resets only the rejected color writer (`NonagonWrapper.hpp:301`); the unchanged reset clears sent-state and cooldown (`private/src/WrldBLDRMidi.hpp:175`), and subsequent grid/encoder scans regenerate current colors. Previously accepted packets remain ordered ahead of retry packets. Disabled mode returns accepted before queue access and counts deliberate discard, avoiding false full conditions and retry amplification (`MidiSender.hpp:112`).

- **Teardown now follows the required order.** `MainComponent.cpp:108` closes audio before clearing LEDs. `CloseAudioDevice` first detaches the source, then removes the callback and closes the device (`:318`). JUCE's `AudioSourcePlayer::setSource` and callback hold the same `readLock`, so detachment waits for an active source callback to finish (`/private/tmp/smartgrid-juce-8.0.15/modules/juce_audio_devices/sources/juce_AudioSourcePlayer.cpp:47`, `:81`). WB clearing joins the worker before direct clear sends (`NonagonWrapper.hpp:329`), and wrapper destruction repeats shutdown before destroying either route wrapper while retaining the existing I/O worker join (`:547`). `MidiSender.hpp:156` signals exit and waits indefinitely for normal completion; JUCE's negative-timeout wait has no force-kill path (`/private/tmp/smartgrid-juce-8.0.15/modules/juce_core/threads/juce_Thread.cpp:230`). An in-flight submission may finish, but cannot follow the final clear after the join.

- **Serialization includes the framework's mutable conversion storage.** `MidiHandlers.hpp:181` holds the handler lock around `sendMessageNow`, and the handshake/clear changes use this entry point. JUCE 8.0.15 converts into mutable `mainPackets` and calls the output connection synchronously (`/private/tmp/smartgrid-juce-8.0.15/modules/juce_audio_devices/midi_io/juce_MidiDevices.h:458`). Keeping worker and control-thread submissions under the same lock avoids concurrent use of that storage. The local CoreMIDI implementation consumes those packets inside the call; no application stack-packet lifetime escapes are introduced.

- **The tests exercise real storage and concurrency.** `private/test/unit/midi_sysex_queue.cpp:9`, `:28`, `:67`, `:85` and `:111` cover source mutation, retained-slot/full rejection, FIFO and wraparound, invalid sizes, exact maximum size, and an 8192-packet producer/consumer run with varied payloads and a third diagnostic observer. The concurrent test is bounded, joins its threads and uses independent sequence-derived expectations. The reported behavioral red/green progression and targeted diagnostic-race reproduction support the coverage; this review did not rerun tests.

### Issues

#### Critical (Must Fix)

None identified in the reviewed delta.

#### Important (Should Fix)

None identified in the reviewed delta. The remaining runtime gates below limit validation claims, but do not demonstrate an implementation defect or require interrupting the active capture.

#### Minor (Nice to Have)

None requiring a source change for this task.

### Recommendations

1. **Retain the full-run measurement gate before claiming an audio fix.** The startup evidence shows worker-enabled operation at actual 48 kHz with 200 settled 512-frame callbacks, normal DSP/UI/autoplay, and SysEx enqueued/submitted increasing from 8 to 173 with full/invalid/discard counters at zero. It proves the transport is active during that short startup interval. It does not establish the 960-second audio outcome, explain either symptom, or prove that moving MIDI caused an improvement. Finish the already authorized capture and correlate both interruptions and periodic damage with the preserved full-run diagnostics, as the parent plan requires.

2. **Treat diagnostics according to their actual contract.** `MidiSender.hpp:190` counts calls completed through the output handler, not physical acknowledgments or successful device reception. The handler returns void and may find no open output. `MidiSysexQueue.hpp:63` is a clamped approximate third-observer snapshot; it is not an exact high-water mark. Enqueued, submitted and depth are read independently, so a single log line need not satisfy an accounting identity. Terminal shutdown may leave abandoned packets in the queue, and `sysex_discarded` specifically counts disabled-mode attempts rather than shutdown abandonment. Use trends and the explicit full counter rather than inferring delivery or corruption from one inconsistent snapshot.

3. **Carry the unexercised integration cases into production extraction.** Forced-overflow LED convergence, clock jitter under sustained SysEx load, physical Launchpad/Twister/K-Mix behavior and graceful shutdown have not been demonstrated by the supplied startup evidence. One SysEx per iteration prevents an unbounded application drain, but the synchronous output call and its lock have no hard latency bound; basic messages can therefore still be delayed by a long SysEx call. Timestamp-zero messages also share FIFO ordering with existing future-timestamp traffic. The policy is preserved, not a new timing guarantee.

4. **Keep existing connection-management limitations explicit.** Audio-side reads of `m_midiOutput` and control-thread writer resets remain unsynchronized baseline behavior (`NonagonWrapper.hpp:67`, `:291`; `MidiHandlers.hpp:126`). Stable route ownership does not freeze the selected endpoint: a queued packet can be submitted to the handler's currently selected device after a reconnect, and no endpoint-generation cancellation policy was introduced. Configuration switching under queued traffic remains outside the demonstrated runtime coverage. The unchanged basic queue also silently drops on full (`MidiSender.hpp:104`), so the new SysEx overflow diagnostics do not describe basic-message loss. These are broader existing-policy limits, not newly claimed solutions.

### Assessment

**Ready to merge? Yes — for the reviewed MIDI/SysEx implementation, as a code-readiness verdict only.** No merge is requested or authorized by this review, and the surrounding uncommitted diagnostic work is not covered by that verdict.

**Reasoning:** The delta fulfills the specified transport, thread, retry and lifetime contracts, and no blocking integration defect was found. The signed iOS build and startup evidence advance integration confidence, while the long-run audio result and the stated device/timing edge cases remain unproven.

**Plan status:** Task 1 implementation is approved. Task 2 measurement/reporting remains open at the supplied gate state; this report does not claim the original audio problem is fixed.

### Review evidence and boundaries

- Read the exact task package first, then the task brief/global constraints, parent plan/progress, modified source and tests, and concrete unchanged connecting source for route registration, callback production, writer reset, handler serialization and terminal lifetime. The previous approval was treated as evidence to inspect, not a finding to inherit.
- Independently calculated read-only SHA-256 hashes for all five task source/test files. Every hash matches the build/package source metadata: MidiSender `7f2ed905...`, NonagonWrapper `6cc3aa4a...`, MainComponent `a2cc5b1c...`, MidiSysexQueue `5192632d...`, tests `1453595e...`.
- Read the existing build log and confirmed `** BUILD SUCCEEDED **` at line 942. Strict signing, IPA/binary hashes, in-place Upgrade with identical before/after config and patch hashes, and enabled-worker startup markers are supplied in the package. They were not regenerated or re-queried during review.
- Supplied focused-test evidence is 5 cases / 2105 assertions passing, followed by 50 targeted concurrent repeats after reproducing the approximate-depth race. No test suite, compiler, build, device service, process/capture action, Git mutation or source edit was run by this reviewer. The only write was this report.
