# October 11: periodic recovery reported after Reset Audio

## Observation and current status

The user reported the familiar artifacts around **11:14 PDT**, pressed the newly installed **Reset Audio** button, and heard the artifacts go away. They requested a Mac watch because recurrence remained uncertain. This is one user-observed immediate recovery after a software intervention. The failing analog interval was not recorded by this task. Prior episodes have also cleared spontaneously; temporal association alone does not establish a repeatable remedy.

**Completed result:** the user subsequently reported **about 20 minutes of clean audible playback**, contrasting this with the usual recurrence every few minutes, and then said they had turned it off. Classify this as **one sustained, user-observed recovery after Reset Audio, with no reported recurrence before the user ended the observation**. The longer clean exposure strengthens the evidence beyond immediate relief between bursts. It supports software reinitialization as a practical recovery candidate; it does not establish repeatability or identify the root cause. The analog recorder and live logger have stopped, and the heartbeat is paused. Final verification and limitations are below.

**Initial checkpoint, approximately 11:20:** app/system logs are saved; a bounded analog recording and live iPad logging are running. Recording started at **11:17:14.093**, about three minutes after reset completion, and is scheduled to end around **11:47:14**. Live logging first retains an event at **11:18:21.442** and ends at 11:47:20. Final verification and assessment remain pending. This task performed no deployment, new reset, app restart or hardware intervention.

## Reset evidence

The app snapshot contains `Audio reset: complete` at **11:14:02**. Telemetry at 11:14:03 and through 11:16:04 reports Maya input/output, 48 kHz, 512 frames, four inputs/outputs, nominal thermal state and low-power mode off. Recorded app initialization is at 10:55:56.

The historical archive was requested for the preceding 20 minutes. The decoded audio/USB selection retains events from **10:55:36.952 through 11:15:37.342**. Device timestamps below are PDT; artifact names use UTC.

| Time | Retained observation |
| --- | --- |
| 11:14:00.172–00.180 | SmartGrid session `0x13bc0bd`, process 13507, stops playing and goes inactive. |
| 11:14:00.181 | Maya device `0x634` begins StopIO. |
| 11:14:00.213 | SpringBoard auxiliary session `0x13bc063` becomes output-rate-controlling session. |
| 11:14:00.217 | Maya configured to 44.1 kHz. |
| 11:14:00.242–00.905 | Maya StartIO, with a logged 250 ms startup lock delay. |
| 11:14:01.205–01.208 | SmartGrid session goes active; Maya begins another StopIO. |
| 11:14:01.233 | Maya configured to 48 kHz. |
| 11:14:01.266–01.927 | Maya StartIO, again with a 250 ms startup lock delay. |
| 11:14:02.226–02.261 | SmartGrid play/stop/play transitions during reset completion. |

The same Maya device object persists. No hub/Maya termination or enumeration is retained in this window. This supports **stream/session reinitialization without USB re-enumeration**, not a software power cycle. The 250 ms driver startup delay is not permanent output latency or a delay explicitly set by the new button.

There is one retained input underflow at **10:55:56.037**, argument `-380`, during app startup; none is retained near the reported 11:14 artifact. No transaction-error or excessive-zero-length message is retained. Unknown driver/log suppression means these absences cannot establish clean playback. The private `ioDriftNS` field reaches **6,750,042 at 11:13:55.946**; this is not a calibrated analog-latency or physical-clock measurement, and no causal threshold is established.

## Implementation and interpretation

### Follow-up: concrete log changes around reset

Inspection through approximately **11:28** finds **28,129** `audiomxd` diagnostics saying `VirtualAudioDevice_Default: IOProc expected 1014, got 512 frames`, starting **10:55:56.030** and ending **11:14:00.157**, immediately before SmartGrid stops playing at 00.172 and Maya StopIO at 00.181. They occur throughout the running interval, rather than appearing uniquely at the user's approximate artifact onset. The app's own telemetry reports 512 frames. The diagnostic describes a system expectation mismatch; it does not establish that 502 samples were dropped or locate the audible corruption.

Over the last five pre-reset minutes, the private driver `ioDriftNS` value rises from **2,854,167 at 11:09:00.945** to **6,750,042 at 11:13:55.946**. There are no retained USB underflow, transaction-error, zero-length or StartIO/StopIO messages within that five-minute interval. Thus no distinct transport error marks the reported onset in this evidence. Missing errors remain inconclusive because emission/suppression behavior is unknown.

Neither the frame-count diagnostic nor `ioDriftNS` appears after reset in the historical selection through 11:15:37, or in the live stream from **11:18:21.442 through 11:27:46.375**. There is a gap between those two windows; do not describe this as uninterrupted post-reset log coverage. This is a useful before/after change consistent with reinitialization and the user's audible recovery. Driver counters, internal expectations and logging can themselves reset, so message disappearance is not an independent proof of lasting physical recovery.

The analog screen has examined **622 complete seconds** without a periodic-cut candidate. SoX has now emitted **nine discarded-buffer warnings**, including a cluster near recorded elapsed 10:21. These belong to the Mac capture path and compromise completeness and precise wall-time alignment. Preserve them as recording defects, not Maya output faults. The initial 142-second warning-free observation above remains a historical checkpoint, not the quality classification for the full recording.

Fetched main and updated the clean diagnostic worktree to **`33613ced7759c464e7e4b094c391c66092fcd1ac`**, “Add user-triggered iOS audio reset while retaining engine state.” `JUCE/SmartGridOne/scripts/SmartGridIOSAudioReset.inc` detaches the callback; stops, uninitializes and disposes the audio unit; deactivates AVAudioSession; reapplies its configuration and preferred format; reactivates it; runs the iOS RemoteIO warm-up; validates the format; recreates/starts the audio unit; and reattaches the callback. Engine state is retained. This source commit is not an independently extracted hash of the installed executable.

The temporary **48 → 44.1 → 48 kHz** transition accompanies reported recovery. SpringBoard briefly regains rate control while SmartGrid is inactive; its exact audible effect was not identified. This does not exclude a rate transition as a trigger under different preceding hardware conditions.

**Interpretation:** software reinitialization is now a recovery candidate for the periodic symptom without an upstream cable reset. Durability, repeatability, intervention versus spontaneous recovery, and fault ownership remain unestablished. The separate startup-silence symptom and resolved WRLD bug are not outcomes of this test.

## Bounded recurrence watch

Mac enumeration confirms K-Mix at 48 kHz. SoX records inputs **3 and 4** to stereo 24-bit PCM using the previous analog setup; physical routing confirmation was requested and remains pending at this checkpoint. Both channels contain signal: approximately −37.4/−36.6 dBFS RMS in the first second. The first **142 complete seconds** have no recorder warnings and no candidates from the exploratory short-cut screen.

The screen looks for simultaneous stereo quiet runs of 1–9 ms, at least eight starts per second, phase coherence above 0.7 at 512 samples, and both-channel RMS above −55 dBFS. It is not a validated health test for arbitrary music: false positives and missed cuts are possible. The recording has no failing pre-reset control, and its header remains unfinished until SoX exits.

The recorder has a 30-minute PCM limit, a wall deadline and a stop below 1 GiB free disk space; initial free space was about 4.8 GB. The five-minute thread heartbeat `watch-maya-after-audio-reset` should pause after final verification of this one observation. No automatic audio reset is authorized.

## Local artifacts

Raw artifacts and helpers remain on the Mac, outside Git:

- Archive: `/Users/joyo/Documents/SmartGridOne/diagnostics/20261011T181536.518980Z-system.logarchive`
- App snapshot: `/Users/joyo/Documents/SmartGridOne/diagnostics/20261011T181640.805099Z-app/2026-10-11T10-55-51-786.log`
- Watch directory: `/Users/joyo/Documents/SmartGridOne/diagnostics/20261011T181700Z-reset-watch/`
- Watch files: `system-before-audio-usb.log`, `kmix-inputs-3-4.wav`, `recording-status.json`, `sox-recorder.log`, `live-audio-usb.jsonl`, `live-status.json`, `analysis-latest.json`, `record-watch.py`, `collect-live.py`, `analyze-watch.py`.

## Completed capture and user-ended exposure

The user's approximately 20-minute listening report is the primary evidence for sustained audible recovery. “Turned it off” ends the observation; the exact physical action is not assumed. No continued clean exposure after that point is claimed.

SoX had already stopped at **11:33:19.925 PDT**, with exit code 0, because free disk space fell below the recorder's **1 GiB** floor. The wall interval was 965.830 seconds, well below the 30-minute deadline. At 11:34:31 the Mac had **779,853,824 bytes** free. No diagnostic files were deleted. The originally planned 30-minute exposure was not completed.

The closed WAV is **277,717,784 bytes**, stereo 48 kHz/24-bit PCM, containing **964.297583 seconds** (16m04.298s). Its finalized data length exactly matches the actual PCM bytes. SoX emitted **52 discarded-buffer warnings**. Missing audio duration is not known precisely; the difference between launch-to-exit time and PCM duration also includes startup/shutdown timing. Do not align later PCM times to wall time as if the recording were lossless.

The final exploratory screen finds no periodic-cut candidate in 964 complete seconds. Both-channel RMS exceeds −55 dBFS in seconds **0–910** (911 seconds); second 911 fades and seconds 912 onward are near the noise floor. This is consistent with the user ending playback, but is not independent proof of which control/device they turned off. The recording therefore supplies roughly 15m11s of active-signal observation, beginning about three minutes after reset, followed by quiet audio. Detector sensitivity to arbitrary music, capture data loss, and the lack of a failed-state analog control still limit inference.

The live logger was deliberately interrupted after the user ended the observation. It saved **1,561 records** from 120,525 received events, with retained device times **11:18:21.442–11:34:30.373**. It contains no matches for the earlier frame-count warning, driver drift, underflow, transaction-error, excessive-zero-length, Maya start/stop or USB-enumeration/termination signatures. Missing messages remain inconclusive. Its raw status defaults to `reason: deadline` even on SIGINT; actual termination was **user-ended observation**, with process exit 130 and completed final metadata at 11:34:32.303. The corrected reason is preserved separately rather than silently rewriting the raw status.

The `watch-maya-after-audio-reset` heartbeat is **PAUSED**. No further recording was started. Given the completed live stream and low free disk space, no additional large historical archive was collected at shutdown.

Final local evidence in the watch directory: `analysis-final.json`, `analysis-final-console.txt`, `completion-summary.json`, `user-ended-observation.json`, `monitor-checkpoint.json`, and `monitor-events.jsonl`, alongside the WAV/log files listed above. These artifacts remain local; this report records the useful result and limits in Git.
