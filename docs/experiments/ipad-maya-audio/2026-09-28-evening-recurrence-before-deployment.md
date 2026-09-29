# 2026-09-28 evening — Maya fault precedes deployment, after idle sleep and USB resume

Status: Maya audio was reported silent again; WRLD.BLDR feedback still worked. The user was unsure whether Maya was audibly working immediately before deployment, then went AFK. Diagnostics were collected without restarting the app or media services, changing USB/power, installing a profile, or deploying anything from this investigation. No recovery was attempted. The fault continues through the last analyzed system event at 19:35:48.995 PDT.

**The newly deployed app did not initiate the first observed fault.** Maya's first excessive-empty-packet warning is at **19:23:01.733**, while SpringBoard starts system audio after wake. The installation begins at **19:26:19.014**, over three minutes later; the new SmartGrid process launches at **19:26:21.585**. Earlier SmartGrid execution and other activity are not excluded as contributors to the device state.

This follows the [earlier September 28 failed-state and recovery experiments](2026-09-28-silent-maya-usb-reset-recovery.md). Those experiments ended with both devices working after the iPad-to-hub reconnect and then WRLD-only reconnect. Do not replace that earlier result with this later recurrence, or describe the earlier hub reconnect as Maya-only.

All event times below are September 28, PDT (UTC−07:00). Capture folder names use UTC, beginning September 29. Setup remains the same iPad Air M3, iPadOS 26.6.1 / 23G83, Maya and WRLD through the powered hub. No analog recording was made.

## Retained sequence

The one-hour system archive contains actual events from **18:35:47.021 through 19:35:48.995**. This is retained coverage, not a guarantee of every event. Synthetic log-class markers were excluded when calculating the bounds.

| Time | Evidence |
| --- | --- |
| 18:43:03.593 / 18:43:06.609 | An earlier deployment terminates SmartGrid PID 5056 and launches PID 5111. |
| 18:43:09.505–19:20:12.011 | Maya I/O has started at 48 kHz and remains started until the stop below. No empty-input-transfer, USB-underflow, pipe-stalled or transaction-error messages were found during this interval. Earlier 18:43 configuration restarts are startup activity. This is a roughly 37-minute stream without the current fault signature, **not proof of audible playback**. |
| 19:20:11.437 | SmartGrid PID 5111 enters the background. |
| 19:20:12.011 | Maya driver StopIO. |
| 19:20:23.131 | iPad enters Idle Sleep. Several short sleep/wake cycles follow. |
| 19:20:37.805, 19:21:26.462, 19:22:26.134 | Kernel reports `device 5 endpoint 0x83: transfer event for _previousTD`. Other USB records identify device 5 as Maya. These are observations around sleep, not a proven causal explanation. |
| 19:22:07.924 | USB 3 host port resume reports `unexpected link state (PORTSC 0x0e4016c1)`. |
| 19:22:08.025 / 19:22:09.151 | USB3.1 Hub is destroyed with reason `simulated or deferred connection lost`, then enumerates again. No full upstream hardware-disconnection event or Maya/WRLD/USB2-hub enumeration appears in this archive. |
| 19:23:01.139 / 19:23:01.360 | Charger report is Connected → Connected; display comes on due to DisplayTap. |
| 19:23:01.676 / 19:23:01.689 | SpringBoard selects SystemSoundsAndHaptics; Maya starts I/O. Driver requests 48 kHz. |
| 19:23:01.733 / 19:23:01.741 | First excessive-zero-length-packet warning and first empty input-transfer batch. Driver object creation explicitly names Maya. |
| 19:23:08.273 | First USB input underflow. Errors then persist through the installation and new app launch. |
| 19:26:19.014 | Mobile installation proxy explicitly begins installation. |
| 19:26:19.381 / 19:26:21.585 | Installer terminates the old PID 5111; new PID 5197 launches. |
| 19:26:27–19:32:30 | Persisted app snapshot shows Maya input/output, actual 48 kHz / 512 frames, four inputs/four outputs, thermal state 0, low-power mode off. MIDI submission counters increase without API errors. |
| 19:35:48.995 | Empty Maya input transfers and USB underflows continue at archive end. |

Archive totals: **19** excessive-empty-packet warnings, **94,112** empty input-transfer messages, **70,547** USB input underflows and **zero pipe-stalled messages**. The explicit USB-audio errors are on Maya's input transfer path; they accompany user-observed output silence but do not measure the analog output. No attempt was made to re-prove that the app generates non-silent samples.

## What the USB and power comparison establishes

A fresh registry snapshot at **19:41:09 PDT** has exactly the same targeted Maya, WRLD and USB2-hub dictionaries as the user-confirmed working snapshot at 18:16:42, including their USB session identities. The local registry comparison artifact records the exact values.

The new sysdiagnose's USB tree places Maya and WRLD beneath the USB2.0 Hub. The USB3.1 Hub is a separate branch and has a new session identity. Therefore the USB 3 resume problem is a useful nearby event, **not evidence that Maya itself disconnected or that the USB 3 branch directly carries Maya's stream**. Basic USB enumeration properties still do not distinguish working from failed Maya audio.

All nine retained charger-state-change reports say Connected → Connected, including 19:23:01.139 just before audio starts. The final snapshot reports external power connected, 100%, fully charged, and not actively charging. This recurrence does not require a newly observed PD connection like the September 24 sequence. It does not exclude an unlogged electrical transient or establish the voltage reaching Maya.

## Deployment context and interpretation

The local deployment log from task “Audit Startup State Issues” records a successful Release build, installation **over Wi-Fi**, and app launch. The app log filename matches this capture. At inspection its worktree was based on commit `4d2090a` with local startup/state changes; an exact hash of the running iPad binary was not obtained. Current source inspection and the historical deployment log are supporting provenance, not a readback of the installed executable.

The strongest next hypothesis is a **USB suspend/resume or audio-start-after-idle failure**. This episode gives a narrower sequence than the earlier reports: streaming without the fault signature, background/StopIO, system sleep, USB resume anomalies, then failure at system-sound StartIO before the app upgrade. The first failed StartIO is at 48 kHz, so a switch to 44.1 kHz is not necessary for this onset.

This still does not identify the responsible component: Maya firmware/state, hub behavior, or the iPad USB stack. The USB 3 event was not found in the earlier September 28 initial/reset/reconnect archives, so it is not yet a universal prerequisite. Deployment is when the user noticed the failure; the installation and execution of this new build occurred after the driver fault was already present. A future comparison should distinguish stopping/starting audio while awake from stopping, sleeping and waking, starting from a confirmed working baseline. A Maya-only cable recovery test also remains unperformed. These are proposed tests, not actions taken while the user was AFK.

## Captures and limitations

A new sysdiagnose was requested remotely at **19:36:42.249**, ready at **19:38:46.431**, and completely downloaded by **19:40:35.278**. Expected and received size both equal **725,661,509 bytes**. The gzip integrity test passed and the tar index was read successfully. USB, IOService, power and port text dumps are present. The XML registry output and coreaudio reporting summary are empty; the bundle lists ioreg and Type-C-retimer collection failures. This is a general sysdiagnose; Audio Glitch Trace was last reported uninstalled and was not installed here.

The first concurrent registry request failed with `ConnectionRefusedError` when opening the device diagnostics service. Its empty `20260929T023607.439374Z-post-deployment-maya-silent.partial` folder was retained. A fresh invocation after the system/sysdiagnose transfers finished succeeded; the exact reason for the refused connection was not established. No partial capture was repaired or described as complete.

Durable raw evidence is under `/Users/joyo/Documents/SmartGridOne/diagnostics/`:

| Evidence | Path beneath diagnostic root |
| --- | --- |
| Current app snapshot | `20260929T023233.564084Z-app/2026-09-28T19-26-22-039.log` |
| One-hour iPad system archive | `20260929T023547.188962Z-system.logarchive/` |
| Full failed-state sysdiagnose and transfer metadata | `20260929T023641.701612Z-postdeploy-maya-silent-sysdiagnose/` |
| Successful USB/power registry snapshot | `20260929T024109.250490Z-post-deployment-maya-silent/` |
| Findings and supporting excerpts | `20260928-maya-evening-recurrence/` |

The journal preserves the useful findings, timeline and limitations. Supporting artifacts stay on the Mac, outside Git: [selected system events](/Users/joyo/Documents/SmartGridOne/diagnostics/20260928-maya-evening-recurrence/selected-system-events.log), [app snapshot](/Users/joyo/Documents/SmartGridOne/diagnostics/20260928-maya-evening-recurrence/app-snapshot.log), [counts and bounds](/Users/joyo/Documents/SmartGridOne/diagnostics/20260928-maya-evening-recurrence/summary.json), and [registry comparison](/Users/joyo/Documents/SmartGridOne/diagnostics/20260928-maya-evening-recurrence/registry-comparison.json). The full sysdiagnose and original captures remain in the folders listed above. These are local storage locations, not remote backups. Diagnostics have finished; no monitoring, deployment or recovery action remains active.
