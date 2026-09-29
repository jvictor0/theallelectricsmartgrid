# Read-only code audit: iPad periodic audio holes

Audit date: 2026-09-09. Repository inspected: `/Users/joyo/.codex/worktrees/e18e/theallelectricsmartgrid`, branch `codex/audio-diagnostics-48khz`, HEAD `ba0fe08cce29b20861545e3779ef08b914f9d452`, including its existing uncommitted diagnostics/tone changes. No source edits, builds, device queries, setting changes, installation, deployment, or device tests were performed. This file is the only audit artifact written.

## Conclusion

I did not find a demonstrated app buffer-writing, oscillator, ownership, or recurring session-reconfiguration defect that explains the late partial holes. The strongest overlooked **app-specific isolation candidate** is the pair of background polling threads that survive the tone substitution, especially the MIDI sender's extremely short requested Mach real-time period. This is a scheduling hypothesis, not evidence that it caused an input-transfer anomaly.

The strongest **diagnostic correction** is that “20 µs render” and a constant native xrun count do not cover all native output behavior. However, already-recorded JUCE manager CPU telemetry substantially weakens sustained expensive logger/AudioSourcePlayer processing as the explanation: during the stable MAYA capture its 528 one-second samples were **0.15% minimum, 0.27% median, 0.33% maximum**, with none above 1%.

The 384-frame hole-start grid, 512-frame hole-end grid, 32 ms combined pattern, phase continuity, and input-transfer/drift associations remain more specific evidence than any residual code hypothesis found here. None identifies the initiating owner.

## 1. Highest-value overlooked candidate: background polling survives tone mode

**Verified behavior.** `NonagonWrapper` still exists as a member of `MainComponent`; replacing its `Process` call does not remove construction of its workers:

- [MainComponent.h:274](../historical-context/JUCE/SmartGridOne/Source/MainComponent.h#L274) declares the wrapper.
- [NonagonWrapper.hpp:509](../historical-context/JUCE/SmartGridOne/Source/NonagonWrapper.hpp#L509) declares `MidiSender m_midiSender`; line 510 declares `IoTaskThread m_ioTaskThread`.
- [MidiSender.hpp:31](../historical-context/JUCE/SmartGridOne/Source/MidiSender.hpp#L31) requests `withPriority(10).withPeriodMs(0.1).withProcessingTimeMs(0.05)`.
- [MidiSender.hpp:49](../historical-context/JUCE/SmartGridOne/Source/MidiSender.hpp#L49) repeatedly calls `HandleMessage()` and sleeps for 100 microseconds. An empty queue returns immediately at lines 90–93; the outer loop still wakes again. The nominal sleep permits up to roughly 10,000 polling iterations/second, reduced by loop work and scheduler delay. This is not a measurement of actual wakeup frequency.
- [IOTaskThread.hpp:568](../historical-context/private/src/IOTaskThread.hpp#L568) unconditionally constructs `std::thread(&IoTaskThread::Run, this)` with `m_running=true`. Successful completion of the app's construction means this thread was created; thread-creation failure would throw. Its idle path also sleeps only 100 microseconds at lines 657–668. No real-time priority request is present for this worker.

**Exact scheduler semantics.** Local [juce_Threads_mac.mm:87](/Users/joyo/JUCE/modules/juce_core/native/juce_Threads_mac.mm:87) converts the requested values to Mach `THREAD_TIME_CONSTRAINT_POLICY`:

- period: 0.1 ms;
- computation: 0.05 ms;
- constraint: 0.05 ms, because maximum processing time defaults to computation;
- preemptible: true.

These are requested scheduling parameters. They do **not** establish that the thread consumes or reserves 50% of a CPU. The Darwin implementation does not use the option's numerical `priority`; therefore “priority 10” must not be reported as a measured or requested Darwin priority class.

**Concrete defect in the current evidence.** [MidiSender.hpp:35](../historical-context/JUCE/SmartGridOne/Source/MidiSender.hpp#L35) discards the Boolean from `startRealtimeThread`, then line 37 unconditionally logs “started with real-time scheduling.” In local [juce_Threads_mac.mm:148](/Users/joyo/JUCE/modules/juce_core/native/juce_Threads_mac.mm:148), failure to obtain the Mach policy exits before the worker's `run()`; [juce_Thread.cpp:175](/Users/joyo/JUCE/modules/juce_core/threads/juce_Thread.cpp:175) propagates failure. The present log proves that the constructor reached its log statement, not that MIDI's real-time loop is running. The ordinary I/O worker does not have this ambiguity.

**Relevance.** The tone bypass removes outgoing MIDI production in `ProcessSample/ProcessFrame`, but it does not remove either worker. This persists with WRLD.BLDR disconnected, so the controller-removal trial did not isolate these threads. A separate high-frequency worker can affect scheduling without appearing in audio render duration or thermal severity. Tone mode also stops `IoTaskThread::Acknowledge()` ([NonagonWrapper.hpp:596](../historical-context/JUCE/SmartGridOne/Source/NonagonWrapper.hpp#L596)); if tasks accumulate, the I/O worker can instead poll a full acknowledgement queue every 100 microseconds at `IOTaskThread.hpp:672`. It cannot grow that queue without bound.

**Limits/falsification.** This code has no seven-minute timer, 384-frame operation, or 97-frame correction. It runs from startup, and the clean desktop run retains the same app code. Those facts weaken a direct periodic-starvation explanation. The observed phase-coherent sub-block holes are not what this code directly generates; a downstream timing/state interaction would be required. Existing logs contain no actual thread scheduling policy, wakeup count, or worker CPU measurement. A minimal later isolation would preserve the current JUCE/session/tone path while suppressing these worker starts; first confirm whether the MIDI real-time request succeeded. That would test this candidate more specifically than changing the audio backend simultaneously.

**Workgroups.** The local native backend obtains an audio workgroup at [juce_Audio_ios.cpp:1064](/Users/joyo/JUCE/modules/juce_audio_devices/native/juce_Audio_ios.cpp:1064). The app does not join workers to it. The tone executes directly on RemoteIO's supplied render thread and does not wait for DSP worker completion, so absence of a user-managed join is not itself a demonstrated tone-path defect. Joining unrelated polling work is not established as a remedy.

## 2. Logging and callback locks: real blind spots, with useful negative evidence

The current path is RemoteIO → JUCE native `process` → `AudioDeviceManager` → `AudioSourcePlayer` → tone → return through all three layers.

[MainComponent.h:90](../historical-context/JUCE/SmartGridOne/Source/MainComponent.h#L90) stops the reported timer **before** `getXRunCount` and `INFO`. The log producer synchronously formats floats/integers with `snprintf` ([AsyncLogger.hpp:40](../historical-context/private/src/AsyncLogger.hpp#L40)), then publishes to the queue. The queue avoids file I/O on the render thread; it does not make formatting asynchronous.

The consumer runs on the message thread ([MainComponent.cpp:529](../historical-context/JUCE/SmartGridOne/Source/MainComponent.cpp#L529)) and flushes the log file after every line ([AsyncLogger.hpp:247](../historical-context/private/src/AsyncLogger.hpp#L247)). It also writes stdout. This is a real remaining I/O workload, but it does not hold the logger queue behind a producer-side file lock.

**Already available broader timing evidence.** [juce_AudioDeviceManager.cpp:1018](/Users/joyo/JUCE/modules/juce_audio_devices/audio_io/juce_AudioDeviceManager.cpp:1018) starts JUCE's load measurement before the player call and ends after it. Consequently the CPU telemetry includes tone generation, callback logging, player locking, buffer wrapping/zeroing, and the gain pass. [juce_AudioProcessLoadMeasurer.cpp:73](/Users/joyo/JUCE/modules/juce_audio_basics/buffers/juce_AudioProcessLoadMeasurer.cpp:73) applies an exponential filter with factor 0.2. The measured 0.27% median corresponds to roughly 29 µs of averaged work per 10.667 ms block, consistent with the tone plus modest overhead. It is not a per-callback maximum: a rare spike can decay before the once-per-second poll.

**Concrete counter selection gap.** The app logs `device->getXRunCount()`, not `m_deviceManager.getXRunCount()`. The latter includes the manager's retained render-budget overrun counter ([juce_AudioDeviceManager.cpp:1311](/Users/joyo/JUCE/modules/juce_audio_devices/audio_io/juce_AudioDeviceManager.cpp:1311)). Thus a rare expensive `INFO` call would not be exposed by either current `dsp_us` or the selected xrun counter, even though the manager already measures it. Recording both counters would close this specific gap without changing audio behavior. The manager timing still excludes waiting to enter its own outer `audioCallbackLock` and native work/copy after it returns.

**Native silence path invisible to app callback logs.** [juce_Audio_ios.cpp:871](/Users/joyo/JUCE/modules/juce_audio_devices/native/juce_Audio_ios.cpp:871) records sample-time continuity before trying the native callback lock. At lines 880–925, failure to get that lock—or a null callback—zeroes the native output without invoking the app. Continuous native timestamps can therefore leave `xr` unchanged while silence is emitted. Sequential app callback numbers alone do not prove that this native branch never ran.

**Why ordinary lock misses are a poor fit here.** Skipping a normally sized 512-frame callback would normally create a roughly 21.33 ms interval between app entries, pause the source's phase progression by a block, and zero a whole 512-frame region. The capture instead has no ≥20 ms app gaps, small/nonuniform holes, continuous surviving phase, and a distinct 384-frame start grid. The 13 isolated stretched intervals are only about 12.8 ms. This sharply weakens repeated ordinary 512-frame lock misses. It does not rule out unobserved differently sized native calls, callback flags, or a downstream deadline miss.

**Targeted missing evidence.** At native callback entry/exit, capture only fixed-size records: native callback count, frames, `AudioTimeStamp` sample/host times and validity flags, callback-lock outcome, render-action flags, return status, end time, and a cheap output checksum/peak before return. That distinguishes silence generated by the JUCE fallback, late completion, and valid tone returned to RemoteIO. App entry time alone is not the scheduled presentation/deadline timestamp.

**Logger integrity check.** Its per-role queues are SPSC ([CircularQueue.hpp:28](../historical-context/private/src/CircularQueue.hpp#L28)); role labels do not guarantee unique producer threads in all uses. But this tone log contains only Audio and Message producers (65,647 and 2,797 lines respectively), no worker-category records or missed records, and callback payloads are at most 119 characters versus 256-byte slots. There is no observed role-collision, truncation, queue-wrap corruption, or queue-pressure explanation of these holes. Atomics use the default sequential consistency, which is adequate for this SPSC publication order.

## 3. Session negotiation remains a startup-state hypothesis, not late polling mutation

The app explicitly requests 48 kHz, 512 frames, zero inputs and seven desired outputs ([MainComponent.cpp:254](../historical-context/JUCE/SmartGridOne/Source/MainComponent.cpp#L254)). The native channel map clips requested channels to available hardware channels ([juce_Audio_ios.cpp:1277](/Users/joyo/JUCE/modules/juce_audio_devices/native/juce_Audio_ios.cpp:1277)); the MAYA delivers four active outputs.

Local JUCE 8.0.2 still:

1. Creates its device under PlayAndRecord, activates, probes hardware, and deactivates in the constructor (native lines 263–279).
2. Activates before switching the opened device to Playback, reconfigures channel state, probes again, then sets the target rate and size (lines 461–501).
3. Probes rate boundaries and intermediate requested rates (377–420), and requests 64 / 4096 / restored buffer sizes (332–350).
4. Uses default MixWithOthers unless `JUCE_DISABLE_AUDIO_MIXING_WITH_OTHER_APPS` is defined (289–307); no app definition was found.
5. Repeats target-setting/recreation on relevant route or stream-format notifications (773–820, 1120–1175).

This active probing is not removed by the two app-local patches. It provides a concrete app/backend difference that could leave different lower-layer startup state than another host. The published JUCE history in `online-research.md` supports treating asynchronous negotiation as real, but does not prove it causes the late periodic failure.

**Polling audit.** The once-per-second app diagnostics read thermal/power state and AVAudioSession properties/routes ([AudioPlatformDiagnostics.mm:10](../historical-context/JUCE/SmartGridOne/Source/AudioPlatformDiagnostics.mm#L10)); they call no session setters or audio restarts. Device rate/size accessors return cached native fields; the latency accessors query AVAudioSession. The UI timer does not request buffer size or rate. Treating “session polling” as repeated negotiation is incorrect for this code.

**Existing negative evidence.** The whole tone app log contains exactly two `prepareToPlay` entries, at 22:46:46 and 22:49:02, and matching callback-sequence resets. There is no prepare/reset near the 22:56:33 onset or subsequent damage. All 528 settled MAYA session records are identical: 48 kHz, 10.667 ms actual/preferred I/O, 0 inputs, 4 outputs, 1.042 ms output latency; the route UID is unchanged. This rules against a recurring app restart, tone phase reset, or observed late duration/rate switch. It cannot rule out every notification that only briefly takes the native lock: JUCE native audio logging is disabled by default, and `AudioSourcePlayer` does not surface the device-error callback into these app records.

## 4. Tone buffer, format, gain, and lifetime review

- [MainComponent.h:68](../historical-context/JUCE/SmartGridOne/Source/MainComponent.h#L68) uses a double phase, increments once per sample, wraps every cycle, and writes every available channel over exactly `startSample .. startSample + numSamples`. There is no callback-phase reset or elapsed-time gate. The rate predicate is true in every captured callback. No accumulating large-angle numerical error or denormal feedback state exists.
- [juce_AudioSourcePlayer.cpp:140](/Users/joyo/JUCE/modules/juce_audio_devices/sources/juce_AudioSourcePlayer.cpp:140) zeroes output-only channels, wraps them in a non-owning AudioBuffer, and passes `startSample=0, numSamples` to the source. The tone then overwrites all those samples. Four channels use the player's fixed pointer arrays, with no additional-input temporary allocation. Its gain/lastGain both start at 1 ([juce_AudioSourcePlayer.h:121](/Users/joyo/JUCE/modules/juce_audio_devices/sources/juce_AudioSourcePlayer.h:121)); no app gain changes or second callback/test sound were found.
- Native [juce_Audio_ios.cpp:1033](/Users/joyo/JUCE/modules/juce_audio_devices/native/juce_Audio_ios.cpp:1033) requests float32, packed, noninterleaved client buffers; lines 913–920 copy every active output into RemoteIO buffers. Hardware 16-bit transport is a separate downstream format, not evidence the app should write int16. Output-only state skips `AudioUnitRender` for input at lines 873–876.
- Native storage only grows if a callback exceeds current capacity (884–885), and maximum frames per slice are queried during creation (1049–1061). The manager also avoids reallocation at constant size. Stale cached 4096-frame reports do not make a 512-frame loop read/write 4096 samples. All captured app calls are 512.
- Native setters/initialization mostly ignore OSStatus, and native copies assume the negotiated AudioBufferList count, channel layout, pointers and byte capacities. No readback of the actual client ASBD or returned ABL geometry is logged. A violated negotiation contract is therefore not exhaustively excluded; it is **not** a demonstrated wrong-format defect. A stable long clean beginning and the later geometry weaken a simple fixed layout error. Native ASBD/status/ABL validation at setup and exceptional callbacks would discriminate it.
- Attaching the player before `setSource(this)` is sound: adding it prepares the player's device fields; setting the new source prepares the source before exposing its pointer under the player lock ([juce_AudioSourcePlayer.cpp:47](/Users/joyo/JUCE/modules/juce_audio_devices/sources/juce_AudioSourcePlayer.cpp:47)). Removal first detaches the source under that lock, then removes the callback and closes the device. The raw current-device lookup in the app callback does not introduce an identified use-after-free in that normal shutdown ordering.
- `NonagonWrapper::PrepareToPlay` is empty (543–545). Normal state processing, direct MIDI sends, recording work, and DSP UI-state updates are bypassed. Message-thread painting and pending-state checks remain, but they do not write the tone's phase or output storage.

**Concrete pre-existing low-relevance defects.** `SampleTimer::Init` allocates a replacement singleton without freeing the previous one ([SampleTimer.hpp:25](../historical-context/private/src/SampleTimer.hpp#L25)); the current run has only two prepares, so this is a tiny startup leak rather than progressive per-callback leakage. Its global non-atomic sample counter has cross-thread readers in normal DSP logging, but tone never increments it; the zero sample-prefix in this log is expected. Configuration stereo flags are plain bools read/written across callback and UI ([MainComponent.h:63](../historical-context/JUCE/SmartGridOne/Source/MainComponent.h#L63), [ConfigPage.hpp:569](../historical-context/JUCE/SmartGridOne/Source/ConfigPage.hpp#L569)); this is a real interactive data race, but those values do not control the all-channel tone. Neither supplies an evidence-backed route to the recorded stable-playback symptom. Teardown MIDI/lifecycle operations occur after the investigated playback and cannot explain its onset.

## 5. Build-copy integration was verified against retained products

The retained Release build log identifies this worktree and `/private/tmp/smartgrid-diagnostics-ios-build`; tone's `MainComponent.cpp` was recompiled. Its compiler dependency file names this worktree's exact header and diagnostics header.

The generated module contains 180 source files. A read-only byte comparison found:

- every file except `native/juce_Audio_ios.cpp` matches `/Users/joyo/JUCE/modules/juce_audio_devices`;
- no extra files in the copy;
- the native file equals precisely the two expected substitutions: exact `newBufferSize / currentSampleRate`, and physical default 256 → 512.

The compiler's retained `include_juce_audio_devices.d` names the generated module's implementation, header, iOS native source, manager and player. The retained `MainComponent.d` also names the generated audio-device header, ruling out an observed split between app header and compiled module. Shared `juce_StandardHeader.h:42–44` identifies 8.0.2.

[prepare_ios_audio_module.py:6](../historical-context/JUCE/SmartGridOne/scripts/prepare_ios_audio_module.py#L6) rejects overlapping source/destination and requires exactly one match per replacement. [project.pbxproj:290](../historical-context/JUCE/SmartGridOne/Builds/iOS/SmartGridOne.xcodeproj/project.pbxproj#L290) runs preparation before Sources; line 371 makes it always run; lines 461–468 and 526–533 put the generated root before shared JUCE, with header maps disabled. The Projucer file retains both prebuild and header overrides at [SmartGridOne.jucer:70](../historical-context/JUCE/SmartGridOne/SmartGridOne.jucer#L70).

The script does not remove files deleted from a future upstream module, but this specific retained copy has no extras. No current build-copy mismatch was found.

## What this audit changes

The app is a tone source inside the existing application, not yet a single-thread minimal native audio program. The narrow next app-specific variable is the surviving 100 µs polling infrastructure, with its real-time-start result currently unknown. The narrow next observability gap is native callback entry/exit and manager overruns, not more detailed normal DSP profiling. Those two points allow stronger exclusion of app scheduling and JUCE's own silence fallback while preserving the observed input-transfer/timestamp chain as the leading measured mechanism.
