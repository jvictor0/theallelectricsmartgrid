# SmartGrid integration audit alongside the fresh SonoBus review

September 10, 2026. Read-only source investigation during the continuing SonoBus recording. No application, device, route, radio or power changes were made for this audit.

## What was actually compiled

The retained JUCE8.0.15 build dependency file names the generated `SmartGridJuceModules/juce_audio_devices/native/juce_Audio_ios.cpp`. Its entire difference from upstream8.0.15 is the device default buffer constant changing256 to512. SHA256: `36eef099f39b09a68c8ceda47be47de54c6704a8f1d45a1cff7d5b0cbc57b650`. The same dependency file references the8.0.15 module tree and has no old `/Users/joyo/JUCE/modules` reference. The inspected integration files predate the retained MainComponent object. This supports comparing the upgraded source rather than accidentally comparing an old shared module. File hashes and modification checks are retained in `smartgrid-source-comparison-provenance.json`; modification times alone are not a binary/source equivalence proof.

Exact compiled native path: `/private/tmp/smartgrid-juce815-ios-build/Build/Intermediates.noindex/SmartGridOne.build/SmartGridJuceModules/juce_audio_devices/native/juce_Audio_ios.cpp`.

## Application differences that remain worth measuring

### Worker wakeups and unchecked thread startup

SmartGrid's `JUCE/SmartGridOne/Source/MidiSender.hpp:31` requests period0.1ms and processing time0.05ms, ignores `startRealtimeThread`'s result, and unconditionally logs that realtime scheduling started. Its loop handles at most one queued message then sleeps100microseconds (`:47–53`). The I/O worker also polls every100microseconds when idle (`private/src/IOTaskThread.hpp:652–668`). These are requested scheduling/wakeup parameters, not measured wakeup frequencies or a claim that the thread consumes half a CPU.

SonoBus's send worker waits on a notification with a20ms timeout, its receive worker waits for socket readiness with a20ms timeout, and its event worker sleeps20ms (`sonobus173-processor.cpp:414–484`). Send/receive realtime startup is checked and a normal-thread fallback is attempted (`:1021–1042`). On iOS it requests no periodic interval and a10ms maximum processing time. Its Windows-only comment about avoiding realtime process priority does not describe its iOS branch.

The relevant Apple JUCE implementation consumes period/computation/constraint, **not** the integer `withPriority` parameter (`juce_core/native/juce_Threads_mac.mm:87–126`). Thus describing this as SmartGrid priority10 versus SonoBus priority1 would be misleading. Our effective requested time-constraint values are0.1/0.05/0.05ms; SonoBus's are0/10/10ms. Actual startup success and effective policy have not been measured on this iPad. A code-level logging defect is established; its role in audio failures is not.

Useful next measurement: confirm the worker actually enters `run`, record realtime-start return/policy and wakeup/message counts without frequent device polling. If it is waking excessively, replace idle polling with a notification/deadline wait as one separately tested change. Do not equate disabling all workers with this more specific scheduling hypothesis.

### The same attached controller does not mean the same USB workload

SmartGrid's audio callback calls `NonagonWrapper::Process`, which invokes `ProcessFrame` every512 generated samples. `ProcessFrame` sends controller output (`NonagonWrapper.hpp:596–603,777–804`). WB color SysEx goes through `sendMessageNow` on that path (`:281–295`); WB indicator messages go through the MIDI queue (`:298–307`). K-Mix MIDI output also calls `sendMessageNow` if its port is configured (`:343–350`). This code still exists; the MIDI worker has not replaced all direct sends.

SonoBus local-file playback does not reproduce SmartGrid's custom WB LED generation. That makes its control useful for showing that this topology can support another application, but it does not isolate audio-session setup from SmartGrid's USB activity. Previous no-WB SmartGrid runs still had dropouts, and pure-tone bypass still produced periodic holes. Therefore WB traffic cannot be a necessary explanation for every observed failure. Record outgoing message/byte/burst counts and the complete callback duration before choosing a traffic-only control.

### Callback timing does not cover all diagnostic work

`MainComponent.h:64–128` takes its end timestamp before formatting/enqueuing the per-callback diagnostic line. At48k/512 that adds approximately93.75 formatted records per second. The queue drops new messages when full rather than waiting (`AsyncLogger.hpp:97–112`), but formatting happens on the caller. The message-thread timer drains the queue (`MainComponent.cpp:550–555`); each line is written to stdout and the session file, and the file is flushed per line (`AsyncLogger.hpp:247–267`).

This is a real workload difference from the SonoBus local-loop path, but the audio issue predates this diagnostic logging, and tone mode retains it. It is therefore not a demonstrated original cause. The next native-boundary trace should measure callback entry through final output return, including this tail. Any logger experiment should preserve an independent external recording and equivalent low-overhead timing evidence.

### Explicit protection against very small floating-point values

SonoBus enters `ScopedNoDenormals` at every `processBlock` (`sonobus173-processor.cpp:7326–7328`). No equivalent was found in SmartGrid's callback or its AudioSourcePlayer path. The JUCE guard changes FPCR on64-bit ARM (`juce_audio_basics/buffers/juce_FloatVectorOperations.cpp:1544–1563`), so it is not merely an x86 detail.

This is a concrete implementation difference but a weak explanation by itself: inherited floating-point control state was not measured, normal measured DSP often stayed around4–5ms, and a sine-tone bypass reproduced periodic holes. First inspect the callback's floating-point mode and full timing; do not present the missing guard as the root cause or assume that the iPad is currently processing denormals slowly.

### Source read-ahead is different from protection against output-driver corruption

SonoBus's local transport uses a65536-sample read-ahead buffer and a normal-priority disk thread (`sonobus173-processor.cpp:9593–9652`). This protects file delivery from disk-read delays. It does not supply a separate hardware output clock or repair USB output after RemoteIO has received its samples. Its disconnected network group also means the current control is not an incoming network-jitter-buffer comparison.

## Constraints for the combined comparison

- Keep SmartGrid's48kHz/512 request and enforcement as permanent baseline requirements. If SonoBus's actual settings differ, mark this exposure honestly and match SonoBus in a separate future run; do not undo SmartGrid's accepted settings.
- Actual SonoBus input/output channels, AVAudioSession category/mode/options, sample rate and IOBufferDuration have not yet been confirmed. A UI buffer label or a maximum-size-enforced processor callback is not necessarily the native RemoteIO frame count.
- Distinguish control configuration from workload. Both inputs-enabled and zero-input SmartGrid runs have failed; no-WB and pure-tone conditions also retain at least one symptom.
- Score sporadic driver-restart interruptions and periodic partial-buffer corruption separately. Finding a contributor to one is not evidence that both are fixed.
- A clean alternate app strengthens an application-dependent trigger. It does not identify whether the vulnerable component is SmartGrid, JUCE, Apple's driver or hub behavior under that trigger.

The independent fresh-context subagent report will supply the native/session/adapter comparison. No source change is proposed as proven by the material above.
