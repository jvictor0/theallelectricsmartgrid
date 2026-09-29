# Passive cooling and same-build repeat — September10,2026

Status: **complete; both symptoms reproduced in the cooled same-build repeat.** The latest user instruction was five additional minutes, then cool and repeat the same build if clean. The earlier proposed hour is canceled. User confirmed WRLD.BLDR LEDs work after the16:13 restart.

## Completed hot continuation

K-Mix3/4 recorded16:34:52.525–16:39:52.301, actual299.776seconds. The original live recorder had a3600second limit; a companion stop controller verified wrapper30136/SoX30143 and sent SIGINT at16:39:52.544 per the user's revised300second request. Return0, stderr0; original raw metadata retained.

There are no long near-silence intervals, short flat candidates or dense periodic episodes. System logs contain zero MAYA transaction errors/restarts, zero-length-transfer reports or ioDrift messages in the recording window.28,104 callbacks stayed48k/512, serious thermal2, low-power0, unmuted, xr2unchanged, with no gaps above20ms, sequence holes or logger misses. DSP median/p99/max4.426/5.150/5.224ms, max arrival gap10.696ms. The MIDI worker was enabled/running in299samples;12,478 additional SysEx handler submissions, sampled depth0–1, no full/invalid/discard counts. Actual LED operation was confirmed by the user for this build. [Full result](summaries/hot-continuation-five-minutes-20260910-result.json).

## Cooling protocol and current state

Preserved system archive and complete app trace before stopping. Config and patch hashes remained unchanged. SmartGridPID19797 received SIGTERM at16:43:33, and the device API confirmed no running process for its bundle at16:43:35. Earliest repeat attempt is16:48:35 after five minutes passive cooling. No hub/interface/controller/power connection, wireless setting, thermal profile, GPU profile, app source or saved configuration was changed. A first local helper call used an unsupported Python enum argument and failed before sending any signal; correcting it to the API's integer argument completed the authorized stop. Raw stop evidence is /private/tmp/smartgrid-ipad-passive-cooldown-r1-20260910.retry.json.

## Authorized repeat

Start K-Mix recording BEFORE relaunch, so startup failures are captured. Use the same installed binary and normalDSP/UI/autoplay/MIDI-worker-enabled environment, internal clock, actual48k/512, input0/output4 and same patch. Avoid screenshots. Inspect the initial app trace to verify nominal thermal0 and settled200callbacks at48k/512. If still elevated, preserve that attempt as a thermal probe, stop the app again and allow more passive cooling; do not call an elevated restart a cooled test. No induced thermal condition may fake a nominal state.

Default repeat capture is960seconds including the planned prelaunch lead-in. Preserve and label expected initial silence before playback,470frame/initialization callbacks, and genuine startup USB/callback/analog failures separately. Do not discard startup faults: the earlier hot restart had two failures before its analog recording. During subsequent measured playback, no device queries, screenshots, builds or state changes. Afterward collect archive/app trace and score both sporadic interruptions and periodic corruption. This is one conditional hot/cooled comparison, not proof of a thermal cause from a single pair.

## Cooled repeat launched and verified

K-Mix recording began16:49:00.456 before launch. SmartGrid began playback about16:49:12, after over five minutes stopped. PID20023, log2026-09-10T16-49-06-780.log; no reinstall or source/config changes. Verification at16:49:18 confirms normalDSP/UI/autoplay, JUCE8.0.15, MIDI worker enabled, increasing SysEx submissions without full/invalid/discard counts, input0/output4, and200consecutive48k/512callbacks with **thermal0**. No screenshot was taken. All device queries ended with startup verification.

Recorder exec30407, wrapper30495, SoX30502. Expected finish17:05:00.456 for960seconds including prelaunch lead-in. The first46.876seconds contain planned silence0–11.620seconds, then ordinary audio around-30/-32dBFS with no additional long-gap candidate. The first initialization callback was470frames and18.535ms DSP; xr0→1 atcb3 during470-frame startup, with11.948ms arrival gap. There is no gap above20ms in the saved startup trace. These initialization events must remain visible separately from settled playback; do not label the xr change a matched USB dropout without system/analog evidence.

[Repeat start metadata](summaries/cooled-repeat-r1-20260910-start.json). Current raw prefix:/private/tmp/smartgrid-ipad-cooled-r1-20260910. Final analog and system-log comparison pending.

## Interim cooled result: long interruptions returned

The local-only scan through499.711seconds (16:57:21 checkpoint) found18 long near-silence intervals after playback began, excluding the expected0–11.620second lead-in. Near-silent cores span117–227ms. The first core starts at WAV87.438seconds, approximately16:50:27.894. Visual inspection of three representative stereo waveforms confirms abrupt loss, a flat near-zero interval, and resumed music; analog settling can make the full interruption longer than the thresholded core. This differs from the measured hot intervals, which had no long gaps.

The run continues unchanged through17:05. No iPad queries have occurred since initial nominal-state verification. USB/callback correlation, thermal evolution and periodic corruption remain pending full post-run analysis. Cooling/restart state is associated with recurrence in this pair; it is not yet a demonstrated thermal mechanism. [Interim evidence](summaries/cooled-repeat-r1-20260910-interim-1657.json).

## Completed result

The recording finished17:05:00 with960seconds retained, return0 and empty stderr. All21 actual long waveform gaps match21MAYA errors,21driver restarts and21callback stalls. A separate0.608second burst contains29periodic holes alongside29alternate callbacks exceeding the10.667ms budget, without xr increment or a nearby USB failure.18long failures occurred while nominal and3after serious state, so serious is not a guarantee of clean playback. Same config/patch and MIDI worker remained active without queue faults. Full evidence and limits: [completed comparison](2026-09-10-cooled-repeat-result.md). The authorized continuation/cooldown/repeat is complete; no further experimental dimension was activated.
