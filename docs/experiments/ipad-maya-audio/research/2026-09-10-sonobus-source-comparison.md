# SonoBus / SmartGrid iOS source comparison

September 10, 2026. Source investigation performed while the user-selected one-hour SonoBus recording continues. A fresh-context subagent audited the native/session/adapter path; the parent independently checked the compiled SmartGrid dependency and callback/workers. This report is not the result of the unfinished hour-long recording.

## Main finding

The inspected SonoBus version does not have a substantially different RemoteIO backend or an identifiable USB-dropout repair missing from the upgraded SmartGrid build. This narrows the comparison toward runtime configuration, application workload and the way those interact with the common backend. It does not prove that the defect is wholly inside SmartGrid.

Installed SonoBus metadata is1.7.3/build90. Its matching public `ios_1.7.3` tag declares a JUCE8.0.12 fork, but the relevant native source is nearly identical to upstream8.0.15. SmartGrid's retained build really compiled8.0.15 with the accepted device-default512 adjustment. Matching SonoBus public labels is not a verified App Store binary/source hash equivalence.

## Differences and exclusions

| Area | Evidence | Consequence for the investigation |
| --- | --- | --- |
| RemoteIO rendering and setup | Same activation sequence, float/noninterleaved hardware-channel format, render callback, native try-lock/silence branch, maximum-frame allocation and restart machinery | No source evidence that SonoBus has a special USB recovery path we should copy |
| JUCE callback adapters | The tagged AudioProcessorPlayer and AudioSourcePlayer source files are byte-identical to upstream8.0.15. SonoBus selects the processor adapter; SmartGrid selects the source adapter | A framework-version fix in those files is excluded. Different adapter usage still exists, but both ultimately render synchronously through the same native callback |
| Block-size protection | SonoBus has an additional standalone maximum-size wrapper; our upgraded AudioDeviceManager already contains equivalent protection | Its extra wrapper is not evidence that our current app lacks oversized-callback protection |
| Actual session configuration | SmartGrid requests48k/512 with zero inputs and gets four outputs. SonoBus source defaults to48k/256 with stereo input/output, but saved settings override defaults | Runtime SonoBus rate, IOBufferDuration, channels/category/mode/options must be established before treating the settings as matched |
| Fork-specific options | SonoBus disables Bluetooth microphone/HFP by default, exposes input gain and classifies USB as headphones for feedback muting. The sample-rate workaround cutoff differs but takes the same branch on this iPadOS version | Genuine source differences; weak explanations with Bluetooth off, USB routed and SmartGrid's zero-input condition. They are not demonstrated fixes for either symptom |
| Worker behavior | Our MIDI/I/O workers use100microsecond polling. SonoBus workers generally wait for notification/socket readiness; it checks realtime startup and attempts fallback | A concrete scheduling contrast worth measuring. Actual wakeup rates and successful realtime policy are unknown; do not claim that configured priorities prove starvation |
| Controller traffic | Our audio path sends WB LED SysEx directly; the loop in SonoBus does not generate that traffic | Same attached WB is not the same USB workload. However no-WB and tone failures prevent using this as the sole explanation |
| Diagnostic/UI/DSP work | SmartGrid has controller state production, scopes, per-callback log formatting and message-thread file flushes; SonoBus's loop has a different workload and uses an explicit denormal guard | Candidate contributors to timing/power sensitivity. Logging was added after the original bug, and pure-tone periodic failures weigh against a simple DSP-only explanation |
| File read-ahead / workgroups | SonoBus buffers file reads, and AudioProcessorPlayer forwards a workgroup, but no custom worker join/protection was found | File buffering does not fill an output-driver outage; the workgroup plumbing is not evidence of a special scheduler fix |

Do not infer native512-frame cadence solely from a settings label or an app callback: JUCE can report the requested size and split a larger native callback. Likewise stereo active outputs do not prove that USB bandwidth halves; the native stream format uses hardware channel counts. Preserve these distinctions in the post-run comparison.

## Follow-up order

1. Finish the untouched SonoBus hour, analyze both waveform symptoms and collect the historical iPad logs. Establish actual session settings as far as the evidence allows. Preserve uncertainty where the logs cannot answer it.
2. If SonoBus is clean, return to the unchanged SmartGrid build/topology to establish a positive comparison. If SonoBus's settings were different, explicitly match it to48k/512 in a separately marked exposure. SmartGrid's48k/512 baseline remains a permanent requirement.
3. Measure complete native callback entry-to-return timing and output contents. This distinguishes already-damaged samples from intact buffers corrupted farther downstream, and includes the diagnostic tail omitted by the current DSP timer. Use normal processing to retain the common sporadic symptom, and a deterministic tone where needed to locate periodic holes precisely. A clean tone run is not a pass for normal processing. Measure MIDI-thread startup/policy and wakeups alongside this with bounded overhead.
4. Choose one demonstrated difference for the next change. Event-driven worker waiting is more specific than disabling all workers; matching a verified session option is more specific than copying an entire backend. Keep periodic holes and sporadic restarts as independent outcomes.

Input-enabled SmartGrid has also failed, no-WB runs still had dropouts, and tone bypass has reproduced periodic corruption. None of input enablement, WB traffic or heavy DSP is established as necessary for all failures. A contributing change may improve one symptom without addressing the other.

## Evidence

- [Independent fresh-context source audit](2026-09-10-sonobus-fresh-review.md), including exact references and ranked alternatives.
- [Callback, logger, worker and compiled-source audit](2026-09-10-smartgrid-callback-audit.md).
- `sonobus-fresh-native-vs-smartgrid.diff`: complete native backend difference.
- `sonobus-fresh-extra-sources.json`: pinned URLs and hashes for additional public source.
- `sonobus-adapter-equality-check.json`: independently verified adapter equality.
- `smartgrid-source-comparison-provenance.json`: retained SmartGrid source hashes and build-reference checks.

No application source, experimental settings, hardware or power connections were changed by this investigation.
