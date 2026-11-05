# November 5: gravelly popping reported, clearing after app restart

## Report, source baseline and evidence

The user reported hearing gravelly/popping artifacts several minutes before contacting this task, and said restarting the app cleared them. They requested recent logs and asked that analysis use current main. Updated the clean diagnostic worktree to **`f333621830e34c60a38ed4274c070276dc1dec19`**, “Fix 192 kHz DSP precision and preserve encoder state across patch reloads.” This is the source baseline inspected, not an independently extracted hash of the installed executable.

Collected the iPad's historical unified log and four persisted app sessions over the existing Wi-Fi connection. The archive's decoded dated records cover **09:13:25.000–09:43:27.956 PST on November 5**, with 444,605 dated records. No deployment, reset, app launch/quit, playback change, recording or hardware intervention was initiated by this task.

Two questions were sent to distinguish continuous artifacts from bursts and to establish whether the **09:28:42 hub reconnect** was a user action. Their answers were still pending when this initial entry was written. Do not identify the physical cause of that reconnect or assume a particular audible burst interval without that confirmation.

## Timeline: separate earlier USB trouble from the final pre-restart session

| Time, PST | Retained observation |
| --- | --- |
| 08:38:10 | Last app telemetry before a long gap in the morning app log; Maya 48 kHz/512. This predates system-archive coverage. |
| 09:26:11 | Maya streaming starts/retries and excessive zero-length input-transfer messages begin. |
| 09:26:16–18 | Existing SmartGrid process 9207 reactivates its session; app telemetry resumes. |
| 09:26:17.818–09:28:42.554 | 12,785 retained Maya input-underflow messages. There are 17,126 zero-length-transfer/startup-warning records from 09:26:11.261 through 09:28:53.720. These are log counts, not counts of audible glitches. |
| 09:28:19–25 | Another app launch/setup occurs while the earlier USB failure remains present. |
| 09:28:42.619–09:28:51.566 | Whole upstream hub/Maya/WRLD teardown and re-enumeration. USB2 termination is labeled `reset API call`; USB3 says `hardware connection lost`; Maya later says `port is powering off`. These private log labels do not identify the user's physical action. |
| 09:29:32–37 | App process 9325 launches and establishes Maya 48 kHz/512, four inputs/outputs. |
| 09:29:37–09:37:46 | Same Maya device `0x7e7`; driver timing-drift messages grow, but no retained underflows, zero-length warnings, transaction errors or stream restarts during the settled interval. |
| 09:37:46.972–09:37:47.019 | App audio session deactivates, Maya StopIO completes, and process 9325 exits normally. |
| 09:37:49.651–09:37:54.041 | Replacement app process 9328 launches and starts Maya audio again. No hub/Maya re-enumeration accompanies this restart. |
| 09:39:04.375 | Replacement app exits with `user-quit` context. The recovered active-app exposure is therefore only about 70 seconds, rather than continuous playback through collection time. |
| 09:41:13–14 | USB3 hub function reconnects; no matching Maya/USB2 teardown is retained. This is after the app quit, not evidence for the earlier gravelly episode. |

The persistent zero-length and underflow events are explicitly mapped to Maya input-transfer managers, including `0x7cb` and `0x7dd`. These concern **Maya-to-iPad USB input**, whereas the user hears **iPad-to-Maya output**. Empty transfers are distinct from full packets containing silent PCM. They establish abnormal earlier USB-audio behavior but cannot by themselves be assigned as the exact cause of the later audible output artifact. No USB transaction-error message was found in the full retained window.

## Final app restart: before/after comparison

In the session starting at **09:29:37**, the app consistently reports host audio at **48 kHz/512**, four inputs/outputs, nominal thermal state and low-power mode off. Audio CPU load is approximately **0.685–0.690** in the minute snapshots. The first post-start timing snapshot at 09:30:37 has one long callback gap (23,063 microseconds), one processing overrun, zero format mutes and zero JUCE xruns. No subsequent timing-counter change is logged through 09:37:41. Current `AppObserver.hpp` logs changed timing counters at most once per minute; this does not provide a per-callback trace or cover an unreported final change immediately before exit.

Maya's private `ioDriftNS` field rises from **291,625 at 09:29:37.739** to **4,479,000 at 09:37:46.738**. This supports investigating stream/timestamp state, but is not a calibrated physical-clock error or analog-latency measurement, and has no established audible-failure threshold.

At the final restart, the same Maya device `0x7e7` remains enumerated. Normal app initialization performs five StartIO calls and actual rate settings **44.1 → 48 → 44.1 → 48 kHz**, ending at 48 kHz by 09:37:54.041. Every startup logs a 250 ms lock delay. These are deliberate initialization transitions, not five spontaneous playback failures or permanent output latency. There is no `Audio reset: complete` entry: this intervention was a process restart, not the Reset Audio button.

The new app's 09:38:54 snapshot again reports 48 kHz/512 and nominal thermal state, with one long callback gap (23,526 microseconds), one overrun, zero format mutes/xruns, and CPU **0.710**. Thus the recorded recovery did not coincide with a lower average app audio load. No driver-drift message is retained during its short settled playback interval. Small drift values at 09:39:06 and 09:41:14 belong to later system-sound activity after the app quit; do not count them as recurrence during recovered app playback.

Unlike October 11, there are **zero** retained `IOProc expected ... got ... frames` messages in this archive. That earlier diagnostic is therefore not an observed marker of today's report. The reset/Apple-audio overlay source is unchanged between the October 11 audit baseline and the inspected current main, although the application/DSP has changed substantially. Historical clean internal recordings remain evidence for the earlier episodes; no internal or analog recording of today's failing interval was obtained.

## Interpretation and limits

The logs show real earlier USB input trouble, then a later session with increasing driver timing-drift diagnostics and stable app timing snapshots. The user's reported relief after reopening audio is consistent with a recoverable streaming/timing state. The final restart resets both the audio connection and application state, so it does not isolate the responsible layer as narrowly as the October 11 Reset Audio intervention, which retained engine state. There is no established new DSP-overload event or thermal transition explaining the reported sound, but these logs do not prove every generated sample was correct in the new build.

Do not merge the earlier 09:26–09:28 failure, the 09:28 hub reconnect, the later gravelly report, and post-quit system sounds into one continuous proven mechanism. Exact audible onset, burst shape, physical reconnect attribution and longer-term recovery are not established by this capture. No preventive change was implemented or deployed.

## Local artifacts

All raw evidence remains on the Mac under:

`/Users/joyo/Documents/SmartGridOne/diagnostics/20261105T174300Z-gravel-restart/`

- `system.logarchive/`, `system-all.log`, `system-event-summary.json`, `recent-app-logs.json`.
- `app-morning/2026-11-05T07-49-41-705.log`.
- `app-earlier/2026-11-05T09-28-19-862.log`.
- `app-before/2026-11-05T09-29-32-760.log`.
- `app-after/2026-11-05T09-37-49-754.log`.

## Afternoon recurrence: captured with the app still running

The user reported several more gravelly/popping episodes around 15:50 PST and asked that journal entries not each be pushed. They were unsure whether this was the morning's uninterrupted app run. The system log resolves that: **process 9536 spawned at 15:45:38.219**, with app log `2026-11-05T15-45-40-595.log` and audio setup at 15:45:46. This is a later launch, so it does not measure time to recurrence in the morning's recovered session.

The new historical archive covers **15:20:31.000–15:50:32.996 PST**, with 260,476 dated records. The user confirmed changing USB or power around **15:45:50**; the exact connection/intervention is unspecified. Consequently, the transaction-error cluster accompanying this disconnect is not evidence of a spontaneous hub failure. The user also confirmed short bursts with clean audio between them; exact audible burst times remain unspecified.

### Evidence before the user's USB/power intervention

- **15:45:33.075:** the display changes from off to active after a touch. An inactive-backlight assertion reports about 4 h 23 min duration. This supports a wake-from-display-idle context, not a measurement of system sleep or the time the fault first developed.
- **15:45:33.857:** Maya device `0x7e7` starts streaming in the context of SpringBoard's `SystemSoundsAndHaptics` auxiliary session `0x6a0cb`.
- **15:45:34.126:** the Maya driver's startup check reports excessive zero-length packets. Repeated stop/start retries increase the logged lock delay from 250 to 600 ms; by **15:45:37.758**, a requested 650 ms exceeds the driver's logged 600 ms maximum. These failures precede SmartGrid process creation at 15:45:38.219. Thus abnormal USB-audio startup can occur before this app's DSP runs. This does not prove that these input-side failures are the mechanism of the later audible output bursts.
- **15:45:41.353–15:45:50.726:** 427 retained Maya input-underflow messages, spanning app initialization and its first seconds of playback. Empty USB transfers are distinct from PCM samples with zero amplitude.
- **15:45:50.726–15:45:58.356:** 584 kernel transaction-error messages: 570 for WRLD.BLDR and 14 for Maya. The whole hub disconnects around 15:45:50.8, then re-enumerates; Maya returns at **15:45:59.417**. The user confirmed a USB/power intervention here. This is not evidence that the previously fixed WRLD firmware bug returned.

### After the reconnect, in the same app process

The replacement Maya driver object is `0x89d`. Startup retries use logged lock delays of 24, 100, 150, 200 and 200 ms; the last StartIO completes at **15:46:01.642**. Across the whole archive, 903 messages mention zero-length packets/transfers, ending at 15:46:01.293. These counts describe retained messages, not individual packets or audible glitches.

Eight further input-underflow messages occur at **15:46:12.291–15:46:14.363**. The private `ioDriftNS` diagnostic then provides an additional stream-timing lead: 162 messages from **15:46:02.347–15:46:43.348**, increasing from **645,833** to **247,440,500**. The field's interpretation and any audible threshold remain unvalidated; do not present it as measured analog delay or physical oscillator error. No later Maya StartIO/StopIO, USB transaction errors, or `IOProc expected ... got ... frames` messages are retained through 15:50:32.996. Missing or potentially throttled diagnostics do not establish clean audio.

The app's 15:46:46 timing snapshot reports one long callback gap (23,090 microseconds), one processing overrun, zero format mutes and two JUCE xruns. No additional timing-counter change is logged through the final snapshot at **15:54:50**. These aggregate counters include startup/reconnection and cannot be assigned to a specific heard burst. The route remains Maya **48 kHz/512**, four inputs/outputs, nominal thermal state, low-power mode off, with reported CPU approximately **0.564–0.584**. There is no newly logged continuing processing-overrun pattern.

One configuration detail merits preserving: after the route change, `AVAudioSession` reports **preferred rate 192 kHz, actual rate 48 kHz**. The system log explicitly records SmartGrid's 192 kHz preference and iOS choosing 48 kHz. In the inspected JUCE backend, available-rate probing requests 192 kHz, while `trySampleRate` returns early if the desired rate already equals the actual rate; this is a plausible way to leave a stale preference after probing. It is not evidence of a 192 kHz Maya hardware stream or a demonstrated cause of the artifacts. It is also distinct from the app's intentional 192 kHz internal DSP rate.

The useful localization is the pre-app USB startup failure, followed by stream-timing anomalies after reconnection with stable app timing counters. Exact alignment to the user's later audible bursts still needs an audible timestamp or analog capture. No reset, app restart, deployment, setting change or hardware action was initiated by this investigation.

Afternoon raw artifacts remain on the Mac under:

`/Users/joyo/Documents/SmartGridOne/diagnostics/20261105T235010Z-gravel-recurrence/`

- `system.logarchive/`, `system-all.log` and `system-event-summary.json`.
- `app/2026-11-05T15-45-40-595.log`, `app-followup/2026-11-05T15-45-40-595.log` and `app-final/2026-11-05T15-45-40-595.log`.
- Bounded live audio/USB logs in `live-audio-usb.jsonl`, with `live-status.json`.

The four-minute live collector exited successfully at its deadline, **15:55:15.827**, with 35,226 messages seen and 1,551 filtered records saved (1,151,615 bytes). Saved device timestamps span **15:51:17.328–15:55:15.627**; the historical archive ends earlier, leaving an approximately 44-second gap between these sources. No matching underflow, zero-length, driver-drift, transaction-error, Maya stream restart or frame-expectation messages were found in this live window. This is log coverage, not verified clean audible exposure; no analog recording was made and exact burst alignment remains unknown. No collector was left running.

This addition is kept locally; no push is authorized by this recurrence report.
