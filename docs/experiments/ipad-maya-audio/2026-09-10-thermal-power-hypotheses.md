# Thermal state, power delivery and USB audio: evidence and discriminating tests

Research synthesis, September 10, 2026. Three fresh-context agents investigated iPadOS thermal policy, the Satechi/MAYA power path, and the archived alternate-app controls. The parent independently decoded power, thermal and symptomsd snapshots from the completed hot/cooled recordings. This pass changed no app, device setting, hardware connection or running experiment.

## Working hypothesis

**SmartGrid's workload exposes a marginal host/USB operating condition. Thermal management changes that operating condition and often reduces failures. The two leading mediators are reduced electrical demand through the hub and changed contention/scheduling inside the iPad.** We can distinguish these with controlled policy and power interventions; another undirected long run would add little.

This statement is a research direction, not an identified root cause. A specific electrical model is: changes in iPad system demand couple into the hub or MAYA supply, provoking USB transfer failures. A specific scheduling model is: competing work or CPU/audio performance policy interferes with timely servicing of the app or system audio path. These are separate, falsifiable models. Showing the latter for a render-budget overrun does not establish it for a USB transaction error.

The latest same-build run had 18 matched long interruptions over 495.75 seconds while nominal and 3 over 452.49 seconds while serious: approximately 2.18 versus 0.40/minute. That roughly fivefold observed difference is worth pursuing, but comes from one ordered, bursty run. Prior serious-state playback had clean 16-minute and five-minute captures; another serious-state startup had two failures. This excludes any claim that serious state guarantees correct output.

## A new charging timeline recovered from the existing archive

The earlier assumption that 100% battery removes power delivery as a variable was incorrect. A saved 12:54:31 snapshot reported battery charging off and zero battery power, but approximately 12 W of external input supplying the iPad itself. Private telemetry units are internally consistent with mV/mA/mW; this was one snapshot, not a calibrated fault-time electrical measurement.

The latest archive contains 49 periodic battery snapshots, normally about 20 seconds apart:

| Time on September 10 | Recorded observation |
| --- | --- |
| 16:49:06 through 16:50:05 | Displayed battery 100%, external power connected, battery-charging flag 1 |
| 16:50:21.683 | Kernel logs private `Not charging:100`; numeric code not decoded |
| 16:50:25.990 onward | Battery-charging flag 0, external power connected, reported brightness unchanged |
| 16:50:27.980 | First MAYA transaction failure |
| 16:55:08.683 | Private `Not charging:110`; also appears in quiet hot comparison |
| 16:57:27.165 | OS thermal pressure changes; app subsequently samples serious state |
| Through 17:04:45 | Battery-charging flag still 0; all 21 USB errors have a preceding charging-off snapshot |

Therefore **battery filling did not stop at the improvement/serious-state transition**; it had stopped roughly seven minutes earlier. The first USB error happens close to the charging-off observation, which is a possible onset clue, not proof of a power transient. The 20-second snapshots cannot resolve millisecond current fluctuations. They report no external V/I during the failures. The private screen-brightness value stays 600000, so there is no positive evidence of dimming in this run; that field does not measure display refresh or GPU work.

Apple documents both normal full-charge termination and thermally limited charging. It does not document a universal `serious => charging off` rule. An 80% limit controls battery filling and is not a documented way to disconnect the external system-power path. [Apple battery charging](https://support.apple.com/en-us/118418), [Apple temperature behavior](https://support.apple.com/en-us/118431).

Evidence: `smartgrid-parent-charging-correlation-20260910.json`, the retained 49 snapshots and original archive. These distinguish battery charging from the broader external-power hypothesis, which remains viable.

## What changes under iPadOS thermal pressure?

Apple documents pausing discretionary work such as photo analysis at Fair, and additional framework/background reductions at Serious. It also documents temperature-dependent CPU-cluster activation/frequency and possible display/charging changes. These give two ways of improving audio despite a slower or constrained machine: fewer competing tasks may improve scheduling margin, or reduced system demand may ease the electrical load. Merely lowering a CPU clock with unchanged work would reduce processing margin, so it is not a sufficient explanation by itself. [Apple thermal-state discussion](https://developer.apple.com/videos/play/wwdc2019/422/), [Apple CPU scheduling discussion](https://developer.apple.com/videos/play/tech-talks/110147/).

There is no retrieved public policy table proving that Serious moves audio to a specific core, raises USB priority, changes its buffer size or changes a PD contract. Apple's P-core-unavailability example concerns Critical, not Serious. SmartGrid's own code only logs thermal state; it does not reduce its workload in response. Actual audio stayed 48 kHz/512 after initialization.

At the transition, `audiomxd` logs disengaging a Thermal mxCoreSession. Treat this as a marker of an audio-system policy action whose exact effect is unknown. Separately, powerd's Restricted/Unrestricted pairs occur 13.8–19.8 ms after each transaction failure as audio power assertions are removed/recreated. They are downstream events, not evidence that thermal policy initiated the failure. The repeated missing limiter-property message occurs throughout the run and is not an established cause.

## What the power topology establishes

The user-described hub strongly matches Satechi ST-H4CPDM, but that SKU has not been physically read from the unit. MAYA44 USB+ is USB-powered; its manual shows no independent DC inlet. With the iPad receiving approximately 15 V in the earlier snapshot and peripherals using USB 5 V, some power distribution/conversion is necessarily involved. The exact shared rails, regulator grouping, transient response and simultaneous downstream budget are unpublished. A 90 W charger label does not measure actual demand or peripheral voltage stability. [Satechi product](https://satechi.com/products/4-port-usb-c-hub-with-pd), [MAYA44 USB+ manual](https://download.esi-audiotechnik.com/download/ESI/MAYA44_USBp/MAYA44_USBp-English.pdf).

A conceptual balance is:

`charger power = iPad system power + battery charging + peripherals + hub/losses`.

Setting the battery term to zero leaves the others. Thermal policy can change system power without changing the displayed battery percentage or renegotiating a PD contract. The electrical hypothesis must predict a measurable relation to source arrangement, demand or MAYA's 5 V; it must not be rescued by inventing arbitrary regulator behavior after each contradictory result.

## Controls and what remains unmatched

The separate control audit below is authoritative for exact timestamps and temperature records. We did not retain a continuous Foundation thermal-state trace for SonoBus or Drambo. Lack of a thermal transition in their saved archive is not evidence that their absolute state was nominal. Battery-temperature history is a separate, useful measurement; it does not identify SoC thermal pressure or hub temperature.

The archived battery readings actually make the control comparison more informative:

| Run | Battery temperature, raw | Approximate °C if units are hundredths | Public thermal-state coverage |
| --- | --- | --- | --- |
| SonoBus, 60 minutes | 2869–3009; ends cooler than it began | 28.69–30.09 | No continuous absolute classification recovered |
| Drambo, 13m11s | 2969–3019; ends cooler | 29.69–30.19 | No in-run absolute classification recovered |
| Short Drambo, 3m47s | 3259–3379 | 32.59–33.79 | Adjacent nominal observations; private in-run thermal levels not equivalent to Foundation states |
| SmartGrid cooled repeat | 3289–3550; rising | 32.89–35.50 | Explicit nominal → serious callback trace |

These are battery sensor values, with unverified private scaling/calibration. They suggest the long alternate-app controls were physically cooler at the battery, and do not support dismissing their clean results as simply a hotter iPad. They still do not match SoC thermal pressure, rendering, energy use or native audio configuration. The desktop one-hour control had actual nominal state and48k/512, but a different host and power path.

SonoBus's clean one-hour recording used actual 48 kHz/256 frames. Charging snapshots initially show battery charging on, then off for approximately the final 55 minutes, with external power continuously present. Thus the topology can work under both sampled charging states in another app, but the buffer size and workload were not matched to SmartGrid. Long Drambo was approximately 13m11s with listening and USB-log coverage, not an equal-duration analog control; its battery charging was off. The shorter Drambo comparison had analog coverage and charging-state variation. Exact native Drambo buffer cadence is unobserved.

The clean desktop result proves the interface can operate correctly with the desktop host. It changes the OS/USB host stack, scheduling, power role and hardware, so it cannot isolate iPad software from power delivery. Neither alternate-app success nor desktop success implies unconditional defective hardware; neither excludes hardware/driver sensitivity to the workload and operating state.

## Next research questions and experiments

| Priority | Question and intervention | Distinguishing result | Important limit |
| --- | --- | --- | --- |
| 1 | Does a thermal **policy** change help without physical heating? On a physically cool iPad, use the already available induced Serious condition and return it off within the same audio session; repeat with matched developer connection and observation overhead. | Quick, reversible improvement at stable physical temperatures makes actual component warming unnecessary. | Induced policy can reduce electrical demand too; this does not isolate scheduling. The override is a floor: off must actually return to nominal to form the comparison. |
| 2 | Is improvement dependent on the shared PD supply arrangement? Repeat comparable nominal/induced-state conditions with PD present versus absent, keeping the same hub/MAYA/WB data paths. | Reproducible dependence on PD presence supports a charger/pass-through interaction. Similar policy benefit under both power arrangements favors host mechanisms. | Removing PD changes source direction, grounding/regulation and possibly resets USB. Score settled playback separately from the deliberate handoff. If peripherals cannot run on iPad bus power, that test is unavailable, not a positive result. |
| 3 | Can SonoBus stay clean at our actual operating point? Match SonoBus to 48 kHz/512 and verified nominal conditions with the same power topology; preserve its native configuration and temperature evidence. | Clean output strengthens an app-workload/session-dependent trigger. Failure at 512 narrows the audio operating-point question. | Keep SmartGrid's permanent 48 kHz/512 baseline. Other-app channel/session/workload differences remain. Do not infer native cadence solely from a UI label. |
| 4, conditional | If PD dependence or a physical-temperature effect emerges, vary airflow **only at the hub** in a continuous session and measure its temperature. | Reversible dependence on hub temperature with host state comparable supports hub thermal sensitivity. | Case temperature is not chip temperature; airflow on iPad/MAYA would change another variable. A replacement hub changes controller/firmware as well as power. |

The first two are a small factorial study rather than unrelated experiments:

| | PD present | PD absent |
| --- | --- | --- |
| Verified nominal policy / physically cool | A | B |
| Induced Serious / physically cool | C | D |

Keep physical cooling, patch, UI, MIDI, 48 kHz/512 and monitoring matched. Keep the developer connection open in all conditions so connection traffic is not paired only with intervention. If physical temperature rises enough that removing the override cannot return to nominal, stop calling the comparison reversible; cool and repeat with balanced order, or maintain constant targeted iPad airflow throughout all conditions. No Critical condition or artificial CPU/GPU heat load is needed.

Interpretation should be declared in advance: improvement C versus A and D versus B points to an OS-policy consequence; improvement B versus A and D versus C points to the PD/source arrangement; benefit only at C with a measurable demand drop suggests mediation through shared power. These are narrowing results, not final isolation: operating on battery also changes host electrical and policy conditions. Measure the proposed mediator before claiming its cause.

Use repeated, counterbalanced exposures and report counts per valid elapsed time, not just pass/fail. Startup and intentional transitions get separate labels. Prior 30-minute quiet periods mean one short clean interval cannot establish a fix. Preserve the two symptom metrics independently in every cell. A power meter can reveal slow V/I or contract changes; absence of a dip on a slow meter cannot rule out millisecond rail disturbances.

## Instrumentation that makes the next run valuable

The periodic symptom needs its own localization alongside the thermal/power study. Latest data contains 29 alternate app-processing durations over 10.667 ms, aligned with 29 waveform holes; earlier pure-tone periodic corruption occurred with only about 20 microseconds of DSP. There may be different immediate causes producing similar blanking. An xrun counter of zero or unchanged is insufficient.

For a future instrumented build, measure whole native callback entry/exit, returned-output validity, the app-processing sections and callback thread execution versus elapsed time. Determine whether the 11 ms intervals are running computation, runnable-but-unscheduled time, or blocking. Use a bounded preallocated trace and verify overhead; process-wide CPU percentages and Time Profiler alone cannot reconstruct an off-CPU interval. Confirm which scheduler/audio trace tracks are available on this exact iPad/Xcode target before promising them. Independently record UI cadence, actual power, thermal state and both analog symptom classes. No such source change was made in this research pass.

Do not casually add a powered hub underneath the Satechi as a clean isolation: Satechi explicitly does not support cascading hubs for this model. MAYA lacks a DC inlet. A supported separately powered arrangement or replacing the hub may be useful with available hardware, but its changed data-controller path must be acknowledged.

## Deliverables and status

- Thermal-policy research: `research/2026-09-10-thermal-policy-hypotheses.md`.
- Power-path research: `research/2026-09-10-pd-power-hypotheses.md`.
- Control thermal audit: `research/2026-09-10-control-thermal-audit.md` and evidence JSON.
- Parent archived power/charging correlations and original extracts are retained with hashes in the evidence manifest.

This is a proposed experiment queue, not an active one. The previous recorder and heartbeat are stopped. No new build, device query, induced condition, power change or recording was started.

## Subsequent ad hoc no-PD observation — confirmed in logs

[Completed analysis](2026-09-10-no-pd-adhoc.md): the user clarified that PD was disconnected before launch, followed by many glitches with no interaction. Playback from 17:51:33 to 17:54:23 contained **seven matched MAYA transaction errors, driver restarts and 217–229 ms callback gaps**, all at nominal thermal state. Nine OS snapshots confirm external power and charging off. Actual audio was 48 kHz/512, normal DSP/UI and MIDI worker on; no settled measured DSP overruns. No analog capture was made, so periodic distortion was not assessed.

PD pass-through is not necessary for the sporadic failure chain. This supersedes the initial unclassified two-glitch observation. A repeated no-PD necessity test is redundant; the policy crossover and any policy-by-power-source interaction remain untested. General hub/MAYA power integrity and host scheduling/driver behavior remain candidates. Diagnostic collection was excluded from the measured period. No new automated experiment has begun.
