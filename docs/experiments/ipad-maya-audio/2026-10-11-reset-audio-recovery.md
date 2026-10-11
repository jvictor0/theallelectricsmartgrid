# October 11: periodic recovery reported after Reset Audio

## Observation and current status

The user reported the familiar artifacts around **11:14 PDT**, pressed the newly installed **Reset Audio** button, and heard the artifacts go away. They requested a Mac watch because recurrence remained uncertain. This is one user-observed immediate recovery after a software intervention. The failing analog interval was not recorded by this task. Prior episodes have also cleared spontaneously; temporal association alone does not establish a repeatable remedy.

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
