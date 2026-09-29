# Thermal policy hypotheses for iPad USB audio — 10 September 2026

**The strongest documented explanation for improvement is that thermal policy removes competing work or reduces total device demand. Apple also documents thermal-dependent CPU-cluster control, but no public source found here establishes a Serious-specific USB-audio priority, buffer, or core-placement policy.** A cool-device induced-state comparison is the most informative first separation; it can establish that physical warming is unnecessary, but cannot by itself distinguish scheduling from changed power demand.

Research only: no device queries, condition activation, app changes, profile installation, or experiment was performed. The investigation follows the systematic-debugging skill’s evidence-first approach. All external sources below are primary Apple material, checked on 10 September 2026. Rankings indicate investigation priority, not probabilities.

## Evidence and scope

The local device evidence identifies **iPad Air 13-inch M3, iPad15,5, iPadOS 26.6.1 build 23G83**. This is a locally recorded device fact; it was not inferred from current product availability. See [device evidence](2026-09-09-evidence.md).

The [completed cooled repeat](../2026-09-10-cooled-repeat-result.md) establishes two separate outcomes:

- **A, long interruptions:** 21 approximately 224–231 ms analog losses, each matching a MAYA endpoint transaction error, driver restart, and callback stall. The largest instrumented processing duration within 100 ms before any long stall was 5.461 ms, below the 10.667 ms budget. Ordinary app DSP overload does not explain this observed error→restart sequence.
- **B, periodic holes:** 29 approximately 10 ms holes separated by 21.323 ms match 29 alternate callbacks taking 11.001–11.241 ms elapsed versus a 10.667 ms budget. That interval contains no matching transaction errors or restarts. Elapsed time includes possible blocking/descheduling; it is not CPU time. Earlier low-DSP pure-tone periodic corruption prevents generalizing this one burst into an explanation of every earlier periodic event.
- Nominal: 18 long interruptions in 495.75 s. Serious: 3 in 452.49 s. Observed rates are approximately 2.18/min and 0.40/min. The 16:57:27 transition is associated with improvement, **not immunity**. Time, startup, actual heat, workload, and power remain confounded. Earlier hot startup also had two failures.
- Same binary, 48 kHz/512 settled frames, low-power flag 0. The source audit found thermal-state logging but no app thermal-dependent DSP/UI/MIDI behavior.

## What Apple actually documents

| System behavior | What is established and what is not |
| --- | --- |
| Discretionary work | Apple’s iOS thermal presentation says Fair begins pausing discretionary background work such as photo analysis; Serious affects system performance, lowers ARKit/FaceTime frame rate, and pauses iCloud restoration. It does not say every background thread stops. [WWDC19](https://developer.apple.com/videos/play/wwdc2019/422/) |
| CPU clusters and scheduling | Apple says cluster activation and clock frequency can depend on workload, the cluster’s thermal pressure, and other factors. P-core availability can be withdrawn under **Critical** thermal conditions. QoS influences scheduling and access to other resources, and CPU priority is one input to core choice. This is general Apple-silicon/iOS/macOS guidance, not an M3/iPadOS 26.6.1 policy table. [CPU scheduling Tech Talk](https://developer.apple.com/videos/play/tech-talks/110147/) |
| Audio performance control | Audio workgroups give the OS information about real-time threads sharing a deadline, allowing power/performance balancing. Framework-provided real-time audio threads join automatically; the API is available since iOS 14. Apple’s audio presentation describes a performance controller that can affect CPU assignment and clocks. Neither source specifies thermal-state thresholds or promises a USB reliability improvement. [Audio workgroups](https://developer.apple.com/documentation/audiotoolbox/understanding-audio-workgroups), [WWDC20](https://developer.apple.com/videos/play/wwdc2020/10224/) |
| Display and processing | When devices exceed their normal internal temperature range, charging may slow/stop, displays may dim, and some workloads show lower frame rates or longer processing times. These are possible protections, not proof they occurred here. [Apple temperature guidance](https://support.apple.com/en-us/118431) |
| App adaptation | Apple recommends apps reduce screen updates, animations, networking, and other work at elevated states. These instructions require app behavior; they are not evidence the OS rewrites SmartGrid’s DSP or MIDI workload. Low Power Mode has its own notification/state. [Responding to power notifications](https://developer.apple.com/documentation/xcode/responding-to-power-notifications) |

The private `Moderate … disengaging Thermal mxCoreSession` message shows an audio-related policy event occurred. Its public semantics were not found. Do not map `Moderate` to Foundation Serious, treat “disengaging” as a performance boost, or infer that a particular audio thread changed priority. The observed 48 kHz/512 configuration is positive evidence against a visible sample-rate/buffer-size switch; it does not expose all internal buffering.

The parent investigation’s new [power/thermal audit](/private/tmp/smartgrid-ipad-cooled-r1-20260910.power-thermal-research.json) further limits that interpretation: no `powerd` PerfMode switch was found at the Serious transition. Instead, 42 Restricted/Unrestricted records pair around the 21 USB failures as audio assertions are removed and recreated, making those records downstream correlates rather than evidence of a preceding thermal cause. A repeated missing `cpu-avg-limiter-input-w2r` diagnostic throughout the run has no established causal meaning. Private charger Notcharging codes 100/110 also appear in a quiet hot run and are not independently identified as fault signals. These are local log observations, not publicly decoded Apple policies.

## Ranked mechanisms and falsifiable predictions

### 1. Reduced competing OS/background work improves timing margins

**Documented foundation:** discretionary work reduction above. **Causal inference:** fewer runnable tasks, memory/cache competitors, or I/O bursts could reduce interruptions to app audio or system USB-audio work. This has a direct possible route to B. Explaining A additionally requires demonstrating a host-side scheduling/queueing connection to the transaction error; lower app CPU load alone does not provide it.

**Prediction:** induced Fair/Serious improves behavior while a cool iPad stays in the same audio session; system traces show the relevant competing work declining and shorter audio runnable-but-not-running or blocked intervals. Fair improvement would be especially informative because documented discretionary-work reduction starts there.

**Weakening/refutation:** improvement with no corresponding change in contention weakens this specific mechanism. Persistent A during adequate system/audio service disproves the simple claim that app callback starvation is the initiating event. Equal symptoms through repeated induced state changes, despite verified policy and background-load changes, lowers this hypothesis considerably. Absence of visible third-party background apps does not rule out OS daemons.

### 2. Graphics/display reduction removes a periodic competitor or lowers system demand

**Documented foundation:** thermal conditions can lower graphics/frame performance. CADisplayLink’s actual cadence can differ from the requested cadence; its documentation specifically includes Critical thermal policy among reasons availability can change. **Inference:** reduced app/compositor/GPU activity could shorten interference with audio or reduce instantaneous electrical demand. Neither establishes an automatic SmartGrid frame-rate change at Serious. [CADisplayLink](https://developer.apple.com/documentation/QuartzCore/CADisplayLink)

**Prediction:** actual render/display-link cadence, GPU work, or CPU cost of UI work drops when audio improves. Holding UI rendering reduced at nominal should reproduce some improvement in a continuous session. If the mechanism is shared-resource blocking, B’s excessive wall time should align with an identifiable UI/worker lock owner or render phase.

**Weakening/refutation:** unchanged measured UI/GPU activity across a reproducible improvement weakens the graphics-contention version. UI reduction that helps only when input power also falls leaves electrical demand as an alternative. A GPU-minimum condition is not equivalent to reducing rendering: it can increase GPU execution time. Display dimming is not proof of lower render cadence. Do not assume a 120→60 Hz ProMotion transition on this Air. The observed 21.323 ms is approximately two audio blocks, not direct evidence for a display frequency.

### 3. Thermal-dependent CPU/audio performance control changes a timing-sensitive system path

**Documented foundation:** Apple exposes thermal-dependent cluster control and deadline-aware audio performance management. **Inference:** different clocks, cluster use, migration, or audio scheduling could change jitter or avoid a timing-sensitive USB-audio/host-controller defect. An uncharacterized `audiomxd` policy might participate, but its name does not establish this mechanism.

**Prediction:** improvement follows induced state transitions quickly and reversibly, including when background/UI workload and actual power demand are similar. A trace shows a repeatable change in audio-server/client/driver thread scheduling or per-core activity. The private policy event may be a useful transition marker if it consistently co-occurs with independently measured consequences.

**Weakening/refutation:** thermal throttling with otherwise identical CPU-bound work ordinarily reduces compute margin; it does not explain faster DSP by itself. If CPU time rises while wall-time tails shrink, fewer interruptions could outweigh slower execution. If both CPU and wall time rise, claiming “throttling fixed missed deadlines” is unsupported. Unchanged scheduling and no induced benefit weakens this explanation. Do not infer a Serious-specific E-core-only mode from Apple’s Critical statement, or substitute ordinary QoS for real-time audio scheduling. Do not change thread priority as a diagnostic without first finding the delayed thread and dependency.

### 4. Thermal protection reduces charging or system power demand, easing the USB power path

**Documented foundation:** high temperature can slow/stop charging. **Inference:** reduced battery charging or SoC/display demand could alter electrical stress through the PD hub and reduce A without improving CPU scheduling.

**Prediction:** failures correlate better with measured iPad/hub input demand or peripheral supply disturbance than with Foundation state. Improvement may occur from induced policy if it reduces demand, even though no hardware warms. Independently reducing demand or changing the supply arrangement can reproduce improvement at nominal.

**Constraint:** the existing [power snapshot](../2026-09-10-hub-power.md) already had battery charging false and battery current zero at 100%, yet substantial external system input. This weakens “battery charging stopped” as a universal explanation. It does not reconstruct current during the latest transition or exclude changing system draw. A charging-disabled battery is not an isolated USB-C supply. PD-specific testing is covered by the separate power investigation.

## What induced thermal conditions can establish

Apple’s Xcode thermal conditions raise reported state over a few seconds without physically heating the device and invoke behavior associated with that state. The induced state is a floor; hotter actual conditions can exceed it, and removing the condition does not physically cool hardware. [WWDC19 demonstration](https://developer.apple.com/videos/play/wwdc2019/422/)

Local read-only enumeration already found Fair, Serious, Critical, and separate GPU performance profiles; none had been active. That verifies available controls, not their exact consequences on this build. See [existing thermal controls report](2026-09-10-thermal-controls.md).

Interpret an induced-state test carefully:

- **Benefit while cool:** actual warming of the iPad/hub/MAYA is unnecessary for that benefit. Some consequence of induced policy or changed demand is implicated. This does not isolate the scheduler.
- **Natural heat helps but induced state does not:** investigate sensor-specific policy, electrical behavior, actual component temperature/clock drift, startup, and elapsed-time confounders. This is not proof that a particular component needs heat.
- **Condition off restores failures while temperatures stay stable:** strong evidence for reversible policy/demand mediation, especially when reproduced in alternating order.
- **Same Foundation state in two runs:** insufficient to claim identical CPU clocks, battery temperature, cluster thermal pressure, display behavior, or hub temperature. The public state is a coarse observation, not a complete hardware state vector.

Do not use a synthetic CPU/GPU heat load to mimic “just temperature”; it adds the very scheduling and power competitors being investigated. A GPU minimum profile tests only that profile, not a complete thermal state. No proposed test requires inducing Critical.

## Highest-information observables and test order

1. **Repeat state changes within one uninterrupted session.** Starting physically cool and nominal, compare no condition, induced Fair, induced Serious, then condition off; use repeated/counterbalanced intervals rather than one warming ramp. Keep app binary, MIDI delivery, UI workload, audio route, 48 kHz/512, developer connection, monitoring load, power cables, and peripheral placement matched. Exclude transition/setup periods explicitly. Verify state before scoring. Repeat enough exposure to assess bursty A and B separately; a single short clean segment is not a fix.
2. **Pair outer audio-callback wall time with thread execution state.** The current timer omits outer JUCE/native work and trailing diagnostics. Future instrumentation should bound complete callback entry/exit and the relevant DSP/adapter sections, then distinguish running, runnable/preempted, and blocked time. A per-thread CPU-time measurement can help only after confirming its overhead, granularity, and target behavior; aggregate process CPU percentage cannot resolve 11 ms alternating events. CPU time itself changes with frequency, so unchanged instruction/work counts with longer CPU time can mean slower execution rather than more DSP. [MetricKit CPU discussion](https://developer.apple.com/videos/play/wwdc2020/10081/)
3. **Use a system/audio trace for causality.** Apple’s Audio System Trace documents thermal state, system load, scheduler decisions, client I/O timing, server jitter/deadlines, VM, and system-call activity. Verify which tracks are exposed for this exact Xcode/iPad target before planning around them. Capture enough pre-event context to inspect A’s USB error, driver service, restart, and callback gap, and B’s alternating elapsed-time spike. Time Profiler alone cannot explain time when the thread is not running. [Audio performance with Instruments](https://developer.apple.com/documentation/audiotoolbox/analyzing-audio-performance-with-instruments), [Thread-state analysis](https://developer.apple.com/videos/play/wwdc2023/10248/)
4. **Record actual mediator changes.** Measure UI/display-link cadence, GPU activity, per-process and per-core activity where exposed, iPad battery/system input draw, and external hub/interface temperature. Keep hardware power transients separate from slow battery telemetry. Record app and OS thermal timestamps, power-state notifications, audio configuration, physical MIDI verification, and private `audiomxd` events as uninterpreted markers. A simultaneous archive can look for process activity, `thermalmonitord`/`powerd`, audio services, and USB diagnostics, but absent private logs do not prove an absent policy.
5. **If escalation is needed, use Apple’s supported capture categories.** Apple currently lists **Audio Glitch Trace**, **Battery Life**, and **Performance Trace** for iOS/iPadOS. Preserve matched sysdiagnoses and the analog capture with exact event times and OS build, following the current profile instructions for any future collection. This research verified the index, not the downloaded instruction PDFs or profile installation. [Apple profiles and logs](https://developer.apple.com/feedback-assistant/profiles-and-logs/)

For USB-specific escalation, Apple’s current TN3190 routes questions through Feedback Assistant under **Developer Technologies & SDKs > USB Audio**. It documents that input clock data can govern output startup for implicit feedback, so input-endpoint failures can matter to output; that possibility requires the MAYA’s actual descriptors and cannot be established from endpoint 0x82 alone. TN3190 does not publish a thermal-specific USB policy. [TN3190](https://developer.apple.com/documentation/technotes/tn3190-usb-audio-device-design-considerations)

**Decision target:** determine whether the improvement is reproducible under an induced policy while physical temperatures remain stable; then identify whether reduced contention, a CPU/audio policy change, or reduced electrical demand mediates it. Preserve A and B as separate outcomes throughout.
