# 2026-09-28 — Recurring silent MAYA44 USB+ and WRLD.BLDR OUT stall; failed-state sysdiagnose and controlled recovery

Status: both Maya audio and bidirectional WRLD.BLDR MIDI working at the end, confirmed by the user. Root cause and permanent prevention remain unresolved. No app, firmware, logging profile, or other deployment was performed. The user performed the recovery actions; the agent collected diagnostics over the existing Wi-Fi connection.

Setup: iPad Air 13-inch (M3, Wi-Fi), iPadOS 26.6.1 / build 23G83, MAYA44 USB+ and WRLD.BLDR through the externally powered USB hub. The exact running app binary/hash was not established by this capture; the observed runtime configuration is recorded below. No external analog recording was made.

All event times below are PDT (UTC−07:00). Capture-directory names use UTC and therefore begin `20260929` for this September 28 evening session.

## Starting observations and recurring pattern

Maya was silent although SmartGrid appeared to play. WRLD.BLDR MIDI feedback initially worked. The user has already established that audio callbacks produce sound: meters animate and unplugging the iPad makes sound emerge from its internal speakers. This investigation did not require re-establishing non-silent sample generation. The user also reports that restarting the app has not fixed either recurring failure.

The same silent-Maya/empty-USB-input-transfer pattern was captured September 24 and September 27. It is not automatically the same cause as the older popping/clicking experiments in this journal.

- **September 24:** Maya enumerated at 22:49:00.669 while the iPad reported battery power. External power arrived at 22:51:12.947 and charging was explicitly reported at 22:51:12.954, about 132 seconds after enumeration. The charging sound started Maya streaming at 44.1 kHz at 22:51:13.033; the first excessive-empty-packet warning was at 22:51:13.722, before the 22:51:37.969 SmartGrid launch. This fits the user's account of adding hub/PD power after attaching the iPad/peripherals, but does not prove the power transition caused the fault: there was no confirmed working playback before that first retained stream start. Hub voltage and an explicit PD contract were not measured. The iPad USB reconnect restored Maya; WRLD.BLDR's OUT endpoint then stalled despite its input working, and its own power cycle restored feedback.
- **September 27:** the user reported that the system had remained connected for days, previously worked, and was found silent. The retained failure appeared after wake at 19:33:40.794, with charger state Connected → Connected, system audio starting at 19:33:41.709, and the first empty-packet warning at 19:33:41.771. This was before the 19:34:07.142 SmartGrid launch. A later power-only transition did not end the errors; its exact physical cause was not established. The user confirmed a full-hub reconnect followed by a separate WRLD reconnect as recovery. Initial power-connection ordering is therefore insufficient as a complete explanation. Sleep/wake or audio startup after idle is a candidate trigger, but could merely expose an already-bad device state.

Prior reports: [September 24 findings](/Users/joyo/Documents/SmartGridOne/diagnostics/20260925-maya-wrldbldr-investigation/findings.md), [charging timeline](/Users/joyo/Documents/SmartGridOne/diagnostics/20260925-maya-wrldbldr-investigation/power-timeline.md), and [September 27 recurrence](/Users/joyo/Documents/SmartGridOne/diagnostics/20260928-maya-wrldbldr-recurrence/findings.md). September 27's two-day archive has uneven retention: USB-audio records begin September 27 around 16:16, so it does not establish continuous two-day USB health.

## September 28 failed-state evidence

The first system archive retains actual events from **17:29:32.509 through 17:57:35.998**; its earlier synthetic log-class marker is not an event. Maya remains selected at 48 kHz, 512 frames, four inputs/four outputs. Thermal state is 0 and low-power mode is off. Basic Maya and USB2-hub registry properties match the previous recovered baseline. A route, open endpoint, or enumerated USB device is therefore not a sufficient health check.

| Time | Observation |
| --- | --- |
| 17:50:38.411 | Latest retained pre-wake charging report says Connected → Connected. |
| 17:51:41.300 | SpringBoard records wake source Touch. |
| 17:51:42.065 | SpringBoard selects SystemSoundsAndHaptics. |
| 17:51:42.086 | Maya StartIO begins. |
| 17:51:42.357 | First excessive-zero-length-packet warning. |
| 17:51:42.720 | First zero-length input-transfer batch. |
| 17:53:37.327 | Existing SmartGrid process 4409 exits under installcoordinationd. This diagnostic session did not perform that installation. |
| 17:53:39.305 | Current SmartGrid process 4761 launches; persisted app log starts at 17:53:46. |
| 17:53:41.349–17:57:35.996 | USB input underflows, continuing to capture end. |

Counts: **14** excessive-empty-packet warnings, **29,026** empty input-transfer messages, **21,533** USB underflows, and **zero** pipe-stalled messages in the initial archive. Driver creation records associate these objects with Maya. The explicit errors are on its **INPUT** transfer path; they corroborate USB malfunction but are not measurements of analog OUTPUT or missing output payloads. Output silence is user-observed.

The failure predates this app launch, but an earlier SmartGrid process existed; do not write that the failure preceded all app execution. No new external-power connection was found around the observed onset. The snapshot reports external power connected, battery 100%, fully charged, and not actively charging. Charger telemetry does not establish voltage delivered to Maya, and missing logs do not rule out an earlier electrical event.

## Diagnostics successfully collected while still failed

A full sysdiagnose was initiated remotely **once** through the existing CoreDevice developer tunnel at **18:01:37 PDT**, using advertised feature `com.apple.coredevice.feature.capturesysdiagnose`. The response was ready at 18:03:41; all **722,934,494 bytes** were downloaded over Wi-Fi. Received size matched the expected size, `gzip -t` passed, and the tar index was read successfully. This verifies that computer-triggered sysdiagnose works for this paired iPad; no button sequence, app deployment, or Audio Glitch Trace profile was required for this general capture.

The bundle contains USB, IOService, power, device-tree and port registry text dumps, retained system logs, power logs and a stacks report. Component limits: the XML-registry command and Type-C-retimer collector failed; `coreaudio reporting` did not meet its collection conditions. These are recorded subsystem limitations inside an otherwise complete, validated archive.

**Audio Glitch Trace was not installed.** This sysdiagnose is not an Audio Glitch Trace capture. The previously inspected Apple Audio Glitch Trace instructions URL redirected to developer sign-in; no profile was downloaded or installed during this investigation.

The host used the repository’s pinned pymobiledevice3 environment and shared `connect_developer()` connector, then `DiagnosticsServiceService.capture_sysdiagnose(False)` and its returned file generator. This is a verified service recipe, not a newly added `ipad_logs.py` command. The advertised feature and capture times are preserved in `capture.json`.

A bounded live system stream also succeeded: **270,986 events / 194,820,459 bytes**, host interval **18:06:11–18:16:12 PDT**, with device timestamps and host-receipt timestamps retained. Kernel records were collected, not just app logs. These are iPad unified/system diagnostics rather than a claim to have obtained a standalone `dmesg` buffer or raw USB bus packets.

## Recovery experiments and results

| Intervention, in order | Result |
| --- | --- |
| Settings → Developer → Reset Media Services, with USB/power unchanged | Audio services and usbaudiod restarted, but Maya did not recover. Audio callbacks then stopped; controls independent of audio remained responsive. |
| Force-quit/reopen SmartGrid after that reset, with USB/power unchanged | UI/meters/callbacks resumed at 48 kHz/512; Maya remained silent and driver empty-transfer/underflow errors continued. |
| Disconnect/reconnect **iPad-to-hub cable** | Maya audio returned. All hub devices re-enumerated. iPad → WRLD.BLDR MIDI feedback broke; WRLD.BLDR → iPad still worked. |
| Disconnect/reconnect **WRLD.BLDR's own USB cable**, leaving iPad/hub/Maya connected | MIDI feedback returned and Maya audio remained working. User confirmed both working. |

**Correction of an intermediate assumption:** an isolated Maya-cable reconnect was requested, but the user subsequently clarified that they had unplugged the **iPad-to-hub cable**. Logs agree: the entire upstream hub terminated and all devices re-enumerated. **Maya-only recovery remains untested.** A capture directory named `20260929T011352.155831Z-after-maya-only-reconnect-working` was created before this clarification; its label is incorrect. It actually contains the state after the iPad-to-hub reconnect, with Maya working and WRLD.BLDR feedback stalled.

The media reset changed usbaudiod PID **438 → 5005** and audiomxd **106 → 5002**, while SmartGrid remained PID 4761. SmartGrid received media-services-lost/reset notifications. The new Maya driver repeatedly attempted StartIO at 44.1 kHz, received empty packets and increased lock delay through 500 ms. At approximately **18:07:58**, SmartGrid logged `AUIOClient_StartIO failed (-66681)`; the installed SDK names -66681 `kAudioQueueErr_CannotStart`. Fewer later errors were caused by stopped I/O, not recovery. A fresh SmartGrid process 5010 launched at 18:09:44.661 and resumed callbacks, but silence and transfer errors persisted. Maya, WRLD and hub targeted registry data remained identical across the software reset, including USB session identities. Thus restarting both the app and the USB audio daemon was insufficient in this incident.

The final archive records the physical sequence:

- **18:12:58.787:** USB2 hub reports hardware connection lost; Maya and WRLD are removed with it. USB3 hub also disconnects.
- **18:12:59.887:** iPad power source switches to battery; **18:13:07.810:** external power returns.
- **18:13:10.063:** WRLD.BLDR re-enumerates; **18:13:10.392:** first WRLD OUT endpoint stall.
- **18:13:14.082:** Maya re-enumerates; last brief empty-input report at **18:13:15.961**. No subsequent Maya empty-transfer/underflow reports through **18:16:33.939**, consistent with the user's audible recovery.
- **18:15:49.234:** WRLD alone disconnects; last OUT stall at **18:15:49.235**.
- **18:15:58.696:** WRLD re-enumerates; app reopens both ports at **18:16:00**. No later pipe stalls through final archive end, approximately 35 seconds of retained observation. The final app snapshot continues through 18:17:58. After hub reconnection it reports `preferred_rate=192000` while actual session/device audio remains 48,000 Hz / 512 frames; the preference value is not evidence of an active 192 kHz route.

WRLD's final-window totals are **11,479 OUT endpoint 0x01 stalls**, each status `0xe0005000`, zero bytes transferred; one IN 0x82 stall and four control 0x00 stalls occur around its physical removal. Do not confuse those removal-time errors with the sustained one-way output failure. Final registry comparison changes only WRLD's sessionID; Maya and hub properties are unchanged by the successful WRLD-only reconnect.

## Why the app's reconnect logic and counters did not detect recovery failure

The app logged both WRLD ports disconnected at **18:13:01** and reopened both at **18:13:10**. Reconnect logic did run. At 18:13:54 its MIDI worker was running, queues were empty, submission counts had increased, and `errors=0`, while the kernel was continuously reporting stalled WRLD OUT transfers. Those counters measure API submissions rather than delivery acknowledgements. Input continuing to work does not establish output health. Endpoint availability and open handles are insufficient for detecting this state.

No reliable in-app silent-output detector or software recovery method was demonstrated. A hardware acknowledgement/round-trip protocol could potentially detect lost MIDI feedback if WRLD supports one; actual audio-path feedback/loopback would provide stronger output verification than route or callback status. These remain design candidates, not implemented fixes. `MIDIRestart()` was previously identified as an untested MIDI-only experiment; it was not called here. No supported in-app physical USB reset was demonstrated. The Apple-documented media-services reset was tested and failed to clear Maya.

## Interpretation and next experiments

The evidence establishes persistent USB-stream/endpoint failure with apparently healthy app routing/submission, plus recovery after USB device reconnection. It does **not** identify Maya firmware, WRLD firmware, hub/cable/power behavior or the iPad's lower-level USB stack as the initiating cause, nor rule out application activity as a trigger. There is no demonstrated permanent fix.

Next time, preserve the failure before changing anything, then test **Maya's own cable only** while hub power, iPad and WRLD connections stay fixed; document the exact physical action. This isolates the reset more narrowly than the verified iPad-to-hub recovery. Separately compare audio restart with and without sleep after an established working baseline, and compare power-first versus iPad-first connection order with a working-before-power-change baseline. A successful single run does not establish prevention. App callback/meters need not be re-proven as the first step.

The reset-induced stopped-callback state is a separate app recovery-handling issue worth investigating, without assuming that fixing it would cure the original USB silence. Do not deploy an experiment without the user's authorization.

## Durable evidence

Diagnostic root: `/Users/joyo/Documents/SmartGridOne/diagnostics/`.

| Evidence | Path beneath diagnostic root |
| --- | --- |
| September 28 findings, focused excerpts and coverage metadata | `20260928-maya-live-failure/` |
| Initial failed app / system archive / device state | `20260929T005705.147174Z-app/`, `20260929T005733.955585Z-system.logarchive/`, `20260929T005738.775395Z-failed-device-state/` |
| Full failed-state sysdiagnose | `20260929T010136.392593Z-failed-sysdiagnose/sysdiagnose_2026.09.28_18-01-37-0700_iPhone-OS_iPad_23G83.tar.gz` |
| Live recovery stream | `20260929T010611.485739Z-live-recovery/` |
| Media-reset app / system archive | `20260929T010845.994811Z-app/`, `20260929T011002.272752Z-system.logarchive/` |
| Reset + fresh-launch still-silent state / app | `20260929T011057.536667Z-after-media-reset-still-silent/`, `20260929T011101.776881Z-app/` |
| Hub-reconnect state, misleading historical label explained above | `20260929T011352.155831Z-after-maya-only-reconnect-working/` |
| Maya recovered / MIDI stalled app and system archive | `20260929T011434.311593Z-app/`, `20260929T011429.852773Z-system.logarchive/` |
| Final working system / device state / app | `20260929T011632.684223Z-system.logarchive/`, `20260929T011642.264648Z-both-audio-midi-working/`, `20260929T011809.520961Z-app/` |

System archive retained bounds: initial **17:29:32.509–17:57:35.998**; reset **17:55:02.002–18:10:03.996**; reconnect **18:04:29.000–18:14:30.994**; final **18:11:32.004–18:16:33.939**. Coverage describes retained events, not guaranteed completeness. Live-stream and archive subsecond timestamps differ slightly; use the historical archive for the final physical-action timeline.

One final app download failed with an empty error string and remains `20260929T011637.735371Z-app.partial`; it was not repaired or represented as complete. A fresh invocation succeeded after the concurrent system capture completed. The exact failure cause was not established.

References: [Apple media-services reset documentation](https://developer.apple.com/documentation/avfaudio/avaudiosession/mediaserviceswereresetnotification), [CoreMIDI MIDIRestart](https://developer.apple.com/documentation/coremidi/midirestart%28%29), [Audio Glitch Trace instructions URL](https://developer.apple.com/services-account/download?path=/iOS/iOS_Logs/Audio_Glitch_Trace_Logging_Instructions.pdf).
