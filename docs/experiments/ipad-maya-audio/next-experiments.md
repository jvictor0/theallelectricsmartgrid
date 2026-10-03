# Next experiments: iPad / MAYA44 audio

## October 3 update: MIDI resolved; Maya sleep/wake comparison

[WRLD one-way MIDI is fixed in firmware](2026-10-03-wrld-usb-reset-firmware-defect.md#resolution-update), deployed and reported working by the user. Remove it from the open root-cause queue; its reset defect does not establish a common cause with Maya. Earlier dated entries below retain their historical observations.

The user clarified that the suspected idle state is screen locked/asleep, not full shutdown. The [dedicated-iPad note](2026-10-03-dedicated-ipad-and-sleep-wake.md) compares Apple's guidance with the September 28 and October 3 wake/startup failures. Proposed startup comparison: actual audio stop/start while awake versus stop/sleep/wake/start, holding USB and power fixed; continued real playback with the screen locked is an additional control if supported. Confirm driver I/O transitions and audible output. No settings or tests have been activated. The periodic-glitch investigation below remains the priority when that symptom is present.

## October 2 priority: periodic corruption during a performance

The user identifies periodic corruption as the primary problem because it can begin during a set. Persistent silence and one-way MIDI failure have been startup/between-session problems in their observations, not mid-set reproductions. Investigate these separately; do not make completion of the startup-reset tree a prerequisite for the periodic investigation. The original periodic symptom can be absent for weeks, then recur every few minutes within one episode. Brief clean substitutions cannot establish prevention.

**Next practical periodic experiment, proposed only:** use an active recurring episode during rehearsal as a temporary reproduction window. Record Maya's physical analog output through an independent recording path, retain the existing 48 kHz/512-frame settings, and time at least two bursts before changing anything. The external recording measures burst timing and waveform; it does not repeat the established clean-DSP check. Mark collection activity and keep heavy sysdiagnose generation outside the measured window.

Force-quit/reopen SmartGrid once, keeping all USB cables and power unchanged. Verify actual audio-driver StopIO/StartIO and unchanged USB device session identities; an app transport pause is not necessarily a driver-stream reset. If the driver never stops, label the intervention an app restart rather than a verified stream restart. Observe several previous inter-burst intervals afterward (roughly 15–20 minutes for the October 1 cadence), extending if recurrence timing warrants it. That window tests an ongoing cluster, not weeks-long prevention.

- If bursts continue across the verified stream restart, test Maya's own USB reconnect next, while retaining the other connections. This tests whether resetting its USB connection achieves something stream reinitialization did not.
- If bursts stop, or recur with a new onset delay after the stream restart, retain the timing/driver evidence and repeat the same intervention on another active cluster before attributing recovery to it. Spontaneous recovery is already established. A repeatable response would prioritize stream initialization, timeline and buffering state; stream restart may also reprogram the device, so it does not uniquely blame app or host software.
- If the restart produces persistent silence, record a separate startup failure; do not count that as a clean periodic trial.

If these comparisons remain ambiguous, the missing boundary measurement is synchronized USB packet contents/timing and analog output. Ordinary Wi-Fi logs and existing kernel traces are not raw USB packet captures. A protocol analyzer would require suitable external hardware and capture/storage setup; none is installed or started by this proposal. Neither more DSP checks nor recurring disruptive resets constitute a performance fix.

**Secondary startup tree, after the Maya-only reconnect test:**

- If Maya-only reconnect succeeds, the next narrow distinction is data reconnection versus loss of Maya power: reset/reconnect only its data connection while maintaining and verifying its supply. Success would show a power cycle is unnecessary; failure followed by ordinary cable-reconnect success would implicate state cleared by the fuller reset, without uniquely identifying its owner. This requires controlled USB test hardware or a verified equivalent. No such per-device power-preserving reset has been demonstrated through the current iPad tools, so do not promise it as an available Wi-Fi command.
- If Maya-only reconnect fails but the upstream reconnect succeeds, prioritize a different-hub comparison under matched startup/sleep/wake conditions. Require positive failures with the original hub around the comparison; a clean replacement session with no reproducing original baseline is inconclusive. A failure on the replacement means the original physical hub is not necessary for that symptom. Repeated failure only with the original hub would support a hub/topology interaction, not automatically a defective unit.

All interventions above remain proposals. No deployment, hardware change, capture or background monitor was started by this update.

## September 28 update: silent USB audio and one-way MIDI recovery

[Completed investigation and evidence](2026-09-28-silent-maya-usb-reset-recovery.md). This adds a separate silent/enumerated-device failure to the older dropout/periodic-blanking queue below. Both Maya audio and WRLD.BLDR feedback were restored by the end. No new experiment is active; no app, firmware or logging profile was deployed.

**Completed:** failed-state app/system logs, USB/power snapshots, and a full remote sysdiagnose; Reset Media Services restarted both audio services and usbaudiod but audio startup failed; a fresh app launch restored callbacks yet left Maya silent. The iPad-to-hub reconnect restored Maya and was followed by WRLD OUT endpoint 0x01 stalls despite app port reconnection and zero MIDI API errors. WRLD's own USB reconnect restored feedback without changing Maya/hub USB identities. Do not list media-service reset as an untested or successful cure.

**Proposed next tests for this symptom, in order:**

1. During the next preserved silent state, reconnect **Maya's own USB cable only**, keeping iPad, hub power and WRLD connected. This was requested but **not performed** on September 28: the user clarified that the successful reconnect was the iPad-to-hub cable. Confirm the physical action and check hub/device session identities before calling the test isolated.
2. After a confirmed working baseline, compare audio stop/start without sleep against stop, sleep, wake and playback with external power held constant. September 27/28 errors appear with system audio after wake; this is association, not established causation.
3. Compare external-hub-power-first versus iPad/peripherals-first connection order. For the latter, establish working audio before adding power. September 24's first retained playback was after charging began, so that record cannot prove power attachment caused a new fault. The earlier no-PD dropout trial below concerns a different symptom and does not settle this silent-state question.
4. Separately investigate app recovery after media-service reset: callbacks stopped after `AUIOClient_StartIO failed (-66681)`, then resumed on app restart without resolving the USB silence. In-app MIDI acknowledgement/round-trip checks and `MIDIRestart()` remain unimplemented/untested candidates, not validated delivery detection or recovery.

Preserve failed-state logs before recovery and record user-observed sound and both MIDI directions. Existing meters/callbacks already establish app audio generation for this case; route enumeration and successful MIDI submissions do not establish hardware delivery. Audio Glitch Trace is not installed. General sysdiagnose over the existing Wi-Fi developer connection is now verified; its component limitations and capture recipe are in the entry. All future tests remain proposals, and this journal update does not authorize a deployment.

## Historical September 10–13 queue

The original recommendations and completed results below retain their historical context; do not read their old active-status headings as newly started experiments.

Updated September 10, 2026. The authorized JUCE 8.0.15 comparison is complete: both symptoms persist at nominal iPad thermal state. Source audits are complete. The one-hour external JUCE app control is captured and archived. Both the worker-disabled and subsequent worker-routing exposures are complete. Routing kept23 matched sporadic failures, with no dense periodic episode in that run and a late nominal-to-serious thermal change; PD power/heat controls and native output-boundary tracing remain queued afterward. Preserve both outcome measures: sporadic USB-restart dropouts and periodic sub-buffer blanking. Improvement in one does not count as fixing both.

## Current recommendation — no new experiment activated

Instrument the complete native RemoteIO callback and outgoing buffer to distinguish DSP timing from later framework/native work and sample corruption. Keep normal workload,48k/512 and MIDI worker routing. The late thermal-policy transition is an observed lead to test deliberately, not a causal explanation. Read-only15:46 follow-up now confirms about24½minutes without a new native xrun in the same still-serious session, agreeing with the user's quieter listening report. Preserve this distinction from the earlier failing16-minute interval and from unrecorded periodic output. Match and report thermal conditions in repeated exposures. Hub power/heat and SonoBus at512 remain queued controls.

## Permanent baseline requirement — user clarification

The 48 kHz request and enforcement, and the 512-frame buffer request are intended for main regardless of the eventual audio-failure fix. Keep them in every experiment, including any old-JUCE positive control; do not vary or remove them as a debugging dimension. When extracting lasting production changes, carry these settings into main and keep the implementation appropriate to the selected JUCE backend. Their presence has already been demonstrated alongside both symptoms.

## Completed: audio-frame MIDI/SysEx submitted by worker — user selected

The user requested moving SysEx off the audio callback as a lasting correction. WB/Launchpad frame-driven packets will enter a bounded owned-byte queue; the worker constructs juce::MidiMessage and submits them. Twister/K-Mix short-message bypasses also move to the existing basic queue. Preserve48k/512, normalUI/DSP and other settings. Control-thread handshake/final clearing remains separate; orderly worker shutdown prevents stale queued updates after clearing. [Implementation plan](historical-context/docs/superpowers/plans/2026-09-10-midi-sysex-worker.md). Source/tests and task review passed; signed build deployed, startup verified48k/512 and increasing worker SysEx submissions with no queue faults. [Completed trial](2026-09-10-midi-sysex-worker.md) ran15:09:41–15:25:41 PDT with the worker enabled and no direct normal-frame CoreMIDI sends. There were23 one-to-one USB/restart/callback/analog failures and no dense periodic episodes. SysEx enqueues/submissions advanced39,919 with no queue faults. iPad thermal state became serious at15:21:47 after all23 failures; no failures in the remaining234seconds is not evidence of a thermal cure. Full trace stayed48k/512 with no measured DSP overruns. This change is retained as an improvement, not a demonstrated cure.

## Completed: MIDI send worker disabled — user selected

The user selected `SMARTGRID_MIDI_SEND_THREAD=0`: do not create the MIDI send thread, and discard/count enqueue requests before route assignment or queue push. Preserve normal UI/DSP, direct WB LED/SysEx sends, the I/O worker, logging, zero app inputs/four outputs, JUCE8.0.15 and permanent48k/512. This removes both worker wakeups and worker-sent MIDI traffic, so a changed outcome cannot distinguish them yet. Verify the default enabled worker actually starts, then verify disabled `enabled=0 running=0 queued=0` with increasing discards before a separate measured exposure. [Implementation plan](historical-context/docs/superpowers/plans/2026-09-10-midi-sender-off-experiment.md). Source task review, build/signature/install and both mode runtime gates passed. The16minute recording completed14:36:12–14:52:12 PDT:39 matched long failures plus periodic holes; worker stopped/queue0 throughout. [Active protocol and evidence](2026-09-10-midi-worker-off.md).

## Completed: one-hour standalone SonoBus control

The user explicitly chose another open-source JUCE iPad app as the next comparison. [SonoBus control protocol and source provenance](2026-09-10-sonobus-control.md). Its published iOS1.7.3 source uses a JUCE8.0.12 fork and RemoteIO; local file looping avoids another audio host or incoming network audio. Match actual48k/512, channel selection where possible, UI visible and unchanged hub/PD/MAYA/WB. Capture3,600seconds and retrieve logs afterward. If clean, return to SmartGrid on unchanged connections to establish a positive comparison. Record both failure symptoms independently. This takes priority over the proposed hardware and tracing steps below. Capture completed13:25:49–14:25:49 PDT, exact3600seconds, recorder return0; the first45seconds include preflight/SmartGrid shutdown. Final logs establish MAYA48k/256; application channel selection and native callback frame distribution remain unobserved. No detected long interruptions/periodic holes or USB transaction failures in the full hour. See the protocol for live tracking and evidence.

## Source comparison completed during the SonoBus hour

[Combined comparison](research/2026-09-10-sonobus-source-comparison.md), [fresh-context review](research/2026-09-10-sonobus-fresh-review.md), and [callback/worker audit](research/2026-09-10-smartgrid-callback-audit.md). The tagged SonoBus native path is nearly identical to the actual compiled8.0.15 module; no distinct RemoteIO render/retry, maximum-frame allocation, Measurement mode or workgroup remedy emerged. Both current managers already limit app callback size. The two player source files are byte-identical across versions, although the apps use different adapters. Avoid spending another test on a presumed fork recovery fix or an already-present maximum-size guard.

After this hour, establish SonoBus's actual session state and compare the waveform and driver logs. If its settings differ, match SonoBus to48k/512 in a separate exposure while leaving SmartGrid's accepted baseline fixed. A channel selection is not proof of USB packet bandwidth, and a stored buffer-size label is not proof of native callback cadence. The newer user-selected MIDI-worker-off test takes precedence over that unchanged-SmartGrid positive comparison; retain native-boundary tracing in the queue. Retain normal workload for sporadic failures and deterministic output for precise periodic-hole localization.

A newly grounded scheduling comparison is event/deadline waiting versus our100microsecond idle polling. Our MIDI realtime-start return is ignored, so first verify that the worker is actually running and measure effective policy/wakeups. On Apple, the integer priority10 versus1 is not the relevant setting; JUCE uses time-constraint fields. Direct WB LED sends and logger formatting/flushes remain workload differences, but no-WB/tone/pre-logging failures prevent treating any of them as a proven universal explanation. No app or device changes were made during this source investigation.

## 1. Upgrade JUCE with normal app workload — completed, both symptoms persist

**Hypothesis.** JUCE 8.0.2's iOS configuration and hardware-probing sequence can leave this device/driver combination in a state that newer JUCE avoids. We have observed actual 44.1 kHz rejected despite a 48 kHz request, and a startup reporting 606 frames despite subsequent 512-frame callbacks. Subsequent JUCE work changes asynchronous rate/duration handling, redundant probing and configuration transitions. This supports testing the version; it does not prove those startup differences cause late corruption.

**Version choice.** Start with tagged JUCE **8.0.15**, the newest 8.x release listed upstream on September 10. Latest overall is **9.0.2**. Testing within 8.x first limits migration scope while bringing in the intervening iOS work. Before building, verify the exact tag/commit and inspect its iOS backend and breaking changes; record which relevant fixes it contains. A 9.x comparison remains an option if its iOS changes justify it, or after a straightforward 8.x failure. Do not label 8.0.15 the latest overall version. [Official releases](https://github.com/juce-framework/JUCE/releases), [8.0.15](https://github.com/juce-framework/JUCE/releases/tag/8.0.15), [9.0.2](https://github.com/juce-framework/JUCE/releases/tag/9.0.2).

**Why this has priority.** The iOS backend has real changes in the area that has behaved inconsistently. UI rendering has now been suppressed without removing either failure. The dependency comparison tests a remaining difference in the app-to-system audio path while retaining the app workload.

**Change.** Use a separate pinned dependency copy for the experimental target; preserve the existing shared `/Users/joyo/JUCE` 8.0.2 installation and the old signed app package for rollback. Build all JUCE modules consistently from the selected release; do not mix old headers with new module implementations. Review or remove the two existing app-local patch substitutions because they match the 8.0.2 source literally. Retain the app's 48 kHz / 512-frame request and rate guard. If native code still needs a small adjustment to preserve the effective buffer request, record that exact delta rather than describing the dependency as pristine upstream.

**Held constant.** Same iPad/iPadOS, hub, MAYA, WB, patch/configuration, faders, internal clock, Wi-Fi/Bluetooth state, normal DSP and MIDI workers, zero application inputs/four active outputs, autoplay, existing tracing and K-Mix 3/4 capture. First compare normal UI-on, then normal UI-off as a separate condition. Do not simultaneously suppress workers, change AVAudioSession category/mixing, alter logging, change the test signal or rewrite RemoteIO.

**Procedure and gates.**

- [x] Preserve baseline source snapshots, binary/package hashes, config/patch hashes and all build flags. Pin upstream tag and commit; review migration scope and iOS changes.
- [x] Build and verify signature. Confirm actual compiled JUCE version and module include provenance. Record necessary compatibility edits and startup negotiation sequence.
- [x] Install in place over Wi-Fi and launch normal/UI-on/autoplay. Verify real MAYA route, sample rate, delivered frame sizes, input/output counts and sustained music before starting the measured interval. A refused rate or silent transport is a failed startup, not a clean audio trial.
- [x] Record a planned 16-minute normal/UI-on interval. Retain exact exposure if stopped early after a clear failure. No device archive/file/screenshot traffic during the measured window.
- [x] Retrieve logs afterward, then repeat normal/UI-off for 16 minutes with the same new binary to expose sustained periodic corruption that frequent restarts might conceal.
- [x] Analyze long interruptions, dense periodic holes, zero-length input-transfer reports, drift, restart chains and both measured DSP timing and native timestamp continuity. Record startup errors separately; they remain an outcome even when excluded from the steady-state window.
- [x] Conditional branch assessed: not triggered because both symptoms reproduced. If the upgraded build is clean, re-run the preserved old build as a positive control, then repeat/extend the new version to an hour. A single clean short run is encouraging but inconclusive. If the old build is also clean, label that comparison inconclusive and retain the exposures.

**Interpretation.** Both symptoms disappearing reproducibly across version changes supports a JUCE-dependent setup/scheduling path; it does not identify the responsible commit. Startup negotiation improving while late corruption remains separates those issues. Fewer restart gaps with continued periodic damage is partial improvement only. If either symptom persists, move to boundary tracing instead of stacking additional fixes into this build.

**Sources.** The previously inspected upstream patches address [buffer detection/changes](https://github.com/juce-framework/JUCE/commit/4cbbf203f834b617607711f2cc4f75dd875c990c), [rate-change glitches](https://github.com/juce-framework/JUCE/commit/5008d349e9a4a7af4a361efd5f8393516703673a), and [rate caching/redundant requests](https://github.com/juce-framework/JUCE/commit/462c1c857ed817e7cf5dc858e6dcf97133681fdb). Their raw JSON diffs are preserved under `/private/tmp/smartgrid-research-20260909`. JUCE 8.0.14 also explicitly lists an [iOS sample-rate testing fix](https://github.com/juce-framework/JUCE/releases/tag/8.0.14). These are configuration-related evidence, not published confirmation of our exact failure.

## Hardware control: PD supply and hub heat — proposed after user hypothesis

The user identifies a Satechi 4-in-1 USB-C PD hub (ST-H4CPDM) and a 90 W charger. [Power/identity evidence and source links](2026-09-10-hub-power.md). Current battery charging is already off at 100%, with external power connected. An 80% battery limit does not isolate external power.

**Power test.** Keep the same normal/UI-on/autoplay build, patch, cables, MAYA/WB, 48 kHz/512 and radio state. Disconnect only the charger cable from the hub's PD port, leaving the iPad and peripherals connected. Check that the hub/peripherals operate from iPad bus power; verify MAYA route/rate/frames and sustained music before recording. Record the initial disconnect/re-enumeration as a transition, not spontaneous failure. Repeat PD-on/off exposures and score both symptoms. If the peripherals cannot run without external power, this condition is unavailable; do not treat silence or disappearance as clean audio.

**Heat test.** Keep the PD/data configuration unchanged and cool only the hub with external airflow, with a separately measured enclosure temperature if available. Avoid unplugging/restarting the audio session during this comparison. Repeated changes aligned to hub cooling would support temperature sensitivity; no current USB hub thermometer is exposed. This test separates heat more directly than removing PD, which also changes power direction, supply, grounding and possibly USB state.

The no-PD condition has now reproduced seven matched USB/restart/callback failures at nominal state; see [completed ad hoc result](2026-09-10-no-pd-adhoc.md). PD was absent before launch. The hub-only heat test remains unperformed. Charger wattage, individual USB port limits and actual PD input telemetry are distinct. Software-side charge limiting is not equivalent to removing the hub's power path. Keep the completed comparison build unchanged for this control.

## UI causality — proposed, source audits complete

Both sequential version pairs have fewer measured long interruptions with UI off, but retain startup failures and periodic corruption. Alternate UI state repeatedly within one continuous audio session to avoid relaunch/driver-state confounding. Time actual painting and complete native callbacks; relate them to the first USB transaction error, not only recovery. If needed, separate frozen-data drawing from live FFT/scope calculations without disabling DSP production. The audit found substantial rendering work but no normal paint-held audio lock; do not present message-thread recovery as the observed driver restart path.

## 2. Locate where periodic holes first appear — if the upgrade does not resolve them

**Hypothesis.** Either JUCE returns damaged/missing output to RemoteIO, or it returns intact audio and corruption occurs farther downstream. The existing app callback trace cannot distinguish these cases completely.

**Change.** Add bounded, preallocated native-boundary diagnostics: native callback sample/host timestamps and validity flags, frame count, entry/exit times, lock outcome, render status/action flags, app-invocation identity and the samples just before returning to RemoteIO. Include the manager's render-overrun counter separately from the device's sample-timestamp discontinuity counter. Use a small in-memory sample history or within-buffer zero-run metrics; a whole-buffer peak/checksum alone cannot rule out partial holes. Drain diagnostic records outside the render thread and measure the added overhead.

**Controls.** Keep the reproducing version, normal workload, topology and measured configuration. Apply identical instrumentation to any compared variants. Do not rely on the old pure-tone mode as the only test of sporadic failures.

**Decision.** Native output already contains matching holes: inspect the exact JUCE/app branch that introduced them. Intact native samples with matched analog holes: focus on timestamp/presentation/session/driver/device behavior. Late native completion despite short app DSP duration: identify the unmeasured work or scheduling delay. Continuous callbacks alone do not prove intact delivered output.

## 3. Narrow audio-session configuration or bypass JUCE's device setup

**Hypothesis.** Session/category/probing choices explain why SmartGrid differs from clean comparison apps, even when nominal rate and block size agree.

**Procedure.** Choose one difference justified by the upgraded source and native trace. Candidates include mixing options or a minimal RemoteIO setup with a controlled activation/preferences sequence. Source inspection confirms both 8.0.2 and 8.0.15 already select Playback for zero inputs; changing to Playback is therefore not an untried isolation. A category-sequence test would need to specifically address temporary PlayAndRecord during discovery, with its own stated hypothesis. Change one at a time. Capture actual route, rate, frame size, session category/mode/options and driver input-endpoint activity. Zero requested app inputs has already failed to stop input-endpoint failures, so do not repeat that as a new hypothesis.

**Decision.** A reproducible change in both symptoms narrows the responsible setup path. A bare test-tone app working alone cannot exclude normal-workload interactions; a backend replacement comparison should retain the rest of SmartGrid's workload where practical.

## 4. Remaining workers and logger — lower priority, guided by tracing

**Hypothesis.** Remaining polling/scheduling or logging work changes USB fault probability without appearing in the measured DSP region. UI-off retained message timer work, state interchange, logger draining and worker threads.

**Procedure.** First verify actual real-time MIDI-thread startup success and measure relevant work. Suppress one worker or logger behavior at a time only when tracing makes it a plausible contributor. Keep normal processing and comparable MIDI/state production where the test permits. The idle 100-microsecond polling code is a candidate, not a demonstrated cause. Disabling drawing again adds little information.

**Decision.** Reproducible workload-dependent failure with corresponding timing evidence supports a scheduling trigger; a one-off reduction in dropout count does not. Watch periodic damage independently because fewer restarts may let it accumulate.

## Optional short mechanism check

If periodic damage is already active, one deliberate stop/restart can test whether the recovery seen at the spontaneous USB restart repeats. Mark the induced interruption explicitly and compare drift/transfer-manager state before and after. This would strengthen the masking/reset hypothesis, but it has lower priority than finding a configuration that fixes both symptoms. Periodically restarting the device would itself create a performance interruption and is not an acceptable final cure.

## Record for every future run

Keep run ID; hypothesis and one intended change; source/dependency commit and local diff; app/package hashes; environment flags; device/OS/topology; saved patch/config hashes; requested and actual session/route/rate/frame/channel configuration; launch verification; recording start and exact duration; user observations; raw artifact paths; detector methods; USB/restart/drift and waveform results separately; exclusions and their reasons; measured timing limits; interpretation and next decision. Record failed starts and inconclusive clean trials as first-class outcomes. Update status only after data supports it.

## Latest user steering: hot restart and physical MIDI output

The requested same-binary redeployment is complete and a16-minute hot-start recording is running from16:14:25. Before restarting, the user reported MIDI input working but no WRLD.BLDR LED updates. Establish physical output before interpreting the trial as MIDI-on; prior quiet-tail MIDI counters only proved handler calls. The iPad remained serious at startup. ThermalSerious and GPU performance device-condition profiles are available remotely; none is active. After resolving physical MIDI, a nominal versus induced-serious comparison with matched connection traffic is now a concrete candidate. [Current protocol](2026-09-10-hot-restart-midi.md).

## Completed September 10 cooldown comparison

The five-minute hot extension and same-build nominal restart are complete. See [result](2026-09-10-cooled-repeat-result.md). Both symptoms remain. New evidence to investigate before choosing another test:29alternate callbacks exceed10.667ms exactly during29periodic waveform holes, with stable xr; distinguish processing CPU time from descheduling/blocking and outer native deadlines. The21sporadic failures still matchMAYA transaction errors/restarts with prior processing below budget. Three failures persist after serious state, weakening a binary thermal-gate explanation. No additional run, code change, or thermal/GPU override has been started.

## September 10 thermal/power research: proposed queue supersedes broad experiments

[Research synthesis and decision table](2026-09-10-thermal-power-hypotheses.md). Three independent agents completed thermal-policy research, exact-hub power research, and archived-control audit; parent decoded the latest charging/thermal timeline. No new experiment is running.

1. Cool-device induced Serious/off crossover within one continuous audio session, with matched observation traffic and actual return to nominal verified. Tests whether a policy consequence can reproduce benefit without physical warming; does not alone distinguish scheduling from reduced power demand.
2. Repeat matched conditions with PD present versus absent, leaving the same hub/MAYA/WB data path. This forms a two-factor comparison; source handoff/reset gets a separate exclusion window, peripherals must remain supported, and power direction/grounding remain confounds.
3. Verify SonoBus at48k/512 and measured nominal conditions on the same power topology. Prior clean hour was48k/256 and had a cooler battery, with no continuous Foundation thermal-state record.
4. Only if the above points to a physical hub effect, targeted hub airflow with temperature measured and continuous session. Do not treat an unsupported cascaded hub or a replacement controller as pure power isolation.

All conditions retain SmartGrid48k/512, current MIDI-worker build, patch and UI. Score sporadic USB/restart losses and periodic waveform blanking independently. Add meaningful bounded native-boundary/thread-state measurements for the29alternate overrun burst before calling another build diagnostic; no source change was made by this research. Battery filling had stopped~7min before the latest serious transition, and all21failures occurred during sampled charging-off state. External system power still matters (~12W in an earlier charging-off snapshot).

## No-PD follow-up: September 10, 17:51–17:54 — completed

[Logged result](2026-09-10-no-pd-adhoc.md): seven USB/restart/callback-gap episodes during approximately 170 seconds of normal playback, nominal throughout, external power and charging off in all nine OS snapshots. PD was removed before launch. No analog recording; periodic symptom unassessed. This rules out PD as a necessary condition for the sporadic chain. Do not repeat a no-PD test merely to ask that same question. The induced thermal-policy crossover remains proposed; a matched policy-by-power-source comparison could still test an interaction. No new experiment is active.

## Active: authorized induced-thermal crossover

The user approved unattended execution with PD connected. [Protocol and current evidence](2026-09-10-induced-thermal-crossover.md). Up to three A/B/A/B/A replicates, three-minute verified state blocks with transitions excluded, passive cooldown between replicates, constant device sampling and K-Mix capture. Preserve both symptom measures and mark unavailable nominal reversals inconclusive. No other experiment is queued for automatic execution.

## Latest authorization and outcome: September 10, 19:24 PDT

This section supersedes earlier active/queued statuses. Thermal comparison and full correlation are complete:22nominal versus6Serious matched failures; third run8versus5 weakens the original effect. User accepts missing reverse-order/nominal return and approves moving directly to a focused native system trace. [Final results and90second-per-state capture protocol](2026-09-10-induced-thermal-crossover.md). No further reversed-order experiment is a prerequisite. Same build, normalDSP/UI/MIDI,48k/512 and PD topology; no other experimental dimension changes. Remaining queue is conditional on trace findings.

## Latest result supersedes prior active statuses: native tracing complete

[September10 native report](2026-09-10-native-system-trace.md). The captured sporadic fault follows completed audio processing and a normal native wait; observed audio/USB threads were not starved for milliseconds. UI kept executing during recovery. Serious did not substantially change measured polling/UICPUactivity. A separate periodic episode still occurred without measured DSP overruns. The next proposed measurement is the complete RemoteIO output boundary described in step2 above, retaining normal workload,48k/512,MIDI routing and topology. This is not yet implemented. Do not repeat thermal crossover, MIDI-off or UI-off merely as an ungrounded fix. Existing SonoBus48k/512 and session-configuration controls remain conditional options. No capture, thermal override or recurring experiment monitor remains active after final reporting.

## September 11 preparation supersedes earlier pending status

Native metadata/USB-detail tooling is built and signed, not deployed. [Preparation and next protocol](2026-09-11-native-probe-preparation.md). Default PCM capture is off: no positive evidence of damaged app samples explains the sporadic outages, and callback timestamp gaps do not prove sample corruption. Normal DSP/UI/MIDI, zero app inputs and48k/512 remain. On-device overhead/readback preflight remains before the measured iPad session. USB private completion/length fields remain unavailable where undocumented. Desktop revalidation completed four hours; final screening is documented separately.

## Latest September 11 additions — prepared, no iPad run started

User-approved valid clock scalar, explicit lifecycle events, and combined USB/audio/scheduler capture helper are complete. [Current protocol/build](2026-09-11-clock-lifecycle-capture.md). This supersedes the earlier preparation package. Metadata-only default remains; ten-second combined kernel bursts have gaps and no rolling trigger. Hardware preflight/deployment remains the next action when the iPad session resumes.


## September 11 user priority: desktop-first initialization comparison

This supersedes the proposed hub replacement and repeated SonoBus control. User considers the same SmartGrid workload on desktop the stronger working comparison; SonoBus is simpler and its clean run should not veto shared-backend/workload interactions. [Verified comparison and proposed narrow tests](2026-09-11-desktop-first-initialization.md). First proposed change: bypass active sample-rate enumeration with JUCE_IOS_AUDIO_EXPLICIT_SAMPLERATES=48000, preserving normal workload and permanent48k/512. The desktop reads supported rates; the iPad probes them by setting preferences while active. SonoBus also probes, as archived startup confirms. Subsequent session/temporary-unit simplification is conditional and separate. No new build/deployment/test activated by this discussion.


## Active: explicit48 enumeration test, September11 19:47PDT

User-approved setting built, installed, and verified: only48000 preference requests remain; normalDSP/UI/MIDI at48k/512. [Protocol and live evidence](2026-09-11-explicit48-enumeration-trial.md). Fifteen-minute measurement endsabout20:02:36; controller stops recording and collects logs. Starts thermalSerious, so a quiet result cannot be directly attributed against the earlier nominal baseline. Thirty-minute completion monitor active; no further configuration change queued automatically.


## Current user steering: cool before explicit48 trial

Hot pilot intentionally stopped19:49:55; appPID0 verified19:50:12. Wait passively, then verify nominal before a fresh same-build fifteen-minute test. The30minute monitor was updated accordingly. No hot test is currently running.


## Active cooled explicit48 test

Nominal verified20:08:42; fresh same-build900second trial starts2026-09-11T20:09:13.144833-07:00, endsapproximately2026-09-11T20:24:13.144833-07:00. Follow /private/tmp/smartgrid-explicit48-cooled-trial-20260911-r1. Do not restart or make another cooled run while it is measuring.


## Completed: explicit48 probing bypass, both symptoms persist

[Cooled result](2026-09-11-explicit48-cooled-result.md):900secondsnominal,30matchedUSB/restart/native/analogdropouts andbriefperiodicblanking, no exploratoryrate requests and no measuredcallbackoverrun. Active rate enumeration is not necessary for either symptom. No reversal is needed to prove this change is not a cure; a small effect remains unestimated. Desktop-first initialization comparison remains userpriority. TemporaryRemoteIO/sessionactivationcycling is proposednext; no further build/deployment activated. Completion monitor removed.


## Single-start result, September11 21:10

Completed authorized single-start lifecycle trial:22errors/20long dropouts in15nominal minutes despite one activation/initialize/start and no measured lifecycle transitions.24isolated short plateaus remain. Temporary startup unit/session cycling is not necessary for sporadic failures; no further experiment started. See2026-09-11-single-start-result.md.
