# External JUCE iPad control: SonoBus — September 10, 2026

Status: **completed and analyzed**. Exact3,600-second capture13:25:49–14:25:49 PDT, recorder exit0 and empty stderr. No long near-silence candidates, short/long flat candidates or dense periodic episodes, and no USB-audio transaction failures, restart, zero-length-transfer or ioDrift messages in the measured iPad logs. Settings evidence establishes MAYA48k/256, rather than the intended512; app channel selection and native callback frame distribution remain unobserved. [Result and configuration evidence](summaries/sonobus-hour-20260910-result.json). This supports a working JUCE control under its measured configuration; it does not rule out every subtle click or establish a hardware-buffer match. The newer user-selected MIDI-worker-off SmartGrid test is next.


## Why SonoBus

[SonoBus for iPad](https://apps.apple.com/us/app/sonobus/id1523236365) is free, standalone and open source. The App Store listing observed September 10 shows 1.7.3; the developer's general website still advertises 1.7.2, so use the iOS-specific source tag and record the actual installed version. The [published ios_1.7.3 tag](https://github.com/sonosaurus/sonobus/releases/tag/ios_1.7.3) is commit 46864f98e0b7b01093737565c78945016a6f4a28. This improves on our earlier main-branch survey, which inspected 1.7.2/JUCE7.0.8.

The matching [iOS project](https://github.com/sonosaurus/sonobus/blob/46864f98e0b7b01093737565c78945016a6f4a28/mobile/SonoBusMobile.jucer) says version 1.7.3, build 90, enables microphone/background audio and selects ../deps/juce/modules. That tree's [version header](https://github.com/sonosaurus/sonobus/blob/46864f98e0b7b01093737565c78945016a6f4a28/deps/juce/modules/juce_core/system/juce_StandardHeader.h#L42-L44) declares JUCE 8.0.12. Its subrepo metadata points to essej/JUCE, sono8good, b267386e7cec8d333289aea9f8fb800a330d9fd7. This is a maintained fork, not a claim of pristine upstream or exact identity with our 8.0.15. Matching public version labels is not a verified hash correspondence to an installed App Store binary.

The [native backend](https://github.com/sonosaurus/sonobus/blob/46864f98e0b7b01093737565c78945016a6f4a28/deps/juce/modules/juce_audio_devices/native/juce_Audio_ios.cpp) selects RemoteIO, uses AVAudioSession/JUCE's device manager, supports mixing, and selects Playback for zero requested inputs or PlayAndRecord otherwise. Modern iOS uses exact requested duration with zero extra-frame offset. Its AudioUnit creation is at line 1237; the earlier RemoteIO creation at 274 is part of temporary activation setup. This shares the relevant backend family with SmartGrid.

The [standalone setup](https://github.com/sonosaurus/sonobus/blob/46864f98e0b7b01093737565c78945016a6f4a28/Source/SonoStandaloneFilterApp.cpp#L182-L197) prefers 48 kHz and 256 frames on iOS; saved settings can override it. **Set the comparison to 48 kHz/512**, rather than relying on defaults. The published processor starts with stereo inputs/outputs but supports disabled input and multichannel layouts; its custom audio-settings view exposes the device selector. Actual channel selection still needs to be observed on this iPad.

The [user guide](https://sonobus.net/sonobus_userguide.html) documents sample-rate/hardware-buffer selection, local audio-file playback and looping. Source inspection confirms file samples are mixed to the local output-monitor path without requiring a connected server/group (SonobusPluginProcessor.cpp lines 7618–7625 and 8226–8235). Thus a local-file loop can produce continuous music without a second audio-host app or incoming network audio. It includes its own UI/meters and processing infrastructure, but is not an equal-workload replica of SmartGrid's synth and visualizer.

## One-hour procedure

1. Close SmartGrid so its background audio session is not active. Open SonoBus directly as a standalone app. Running its AUv3 inside Drambo/AUM would test the host's audio setup instead.
2. Keep the iPad, MAYA, WB, hub, PD charger, USB/audio cables and radio state unchanged from the reproducible SmartGrid condition. Do not combine this comparison with PD removal, cooling or controller removal.
3. In the Audio settings select MAYA44 USB+, 48,000 Hz and a hardware Audio Buffer Size of 512 samples. This is distinct from SonoBus's network jitter buffer. Choose zero inputs and all four outputs if available to match our current app; otherwise retain and record the actual input/output selection. Keep the output pair wired to K-Mix audible. Muting input monitoring alone does not close hardware inputs.
4. Open a known-clean local music WAV and enable looping and local playback monitoring. Keep network groups disconnected. Keep its UI visible/screen awake for the run. Confirm sustained output before starting the measured hour; record obvious loop-seam artifacts separately.
5. Capture K-Mix channels 3/4 for 3,600 seconds and collect the iPad system archive afterward. Record app/build version, actual route/rate/buffer/channels, source file identity, power state and exact exposure. No background device collection during the measured run.
6. Score both sporadic long interruptions and periodic partial-buffer holes, and count MAYA transaction failures, restarts and drift independently. Continuous music makes both symptoms observable. Do not infer no kernel errors solely from clean listening.
7. If SonoBus is clean, return to the unchanged SmartGrid build on the same connections and reproduce again. A clean alternate app bracketed by SmartGrid positives is stronger than an isolated clean hour. If either side fails to reproduce, preserve that exposure and call the comparison inconclusive where appropriate.

A clean hour would demonstrate that this hardware/iPad can run at least one JUCE-based application under those tested conditions. It would strengthen a SmartGrid-specific setup/workload interaction. It would not by itself locate the defect inside SmartGrid, or rule out an Apple-driver/hub fault triggered by SmartGrid's particular behavior. SonoBus's JUCE fork and workload remain comparison dimensions.

## Alternative considered

[BYOD](https://apps.apple.com/us/app/byod/id1595313287), by Jatin Chowdhury, is also free/open source with an iPad standalone build and substantial circuit-modelled DSP plus scopes. Its inspected JUCE7.0.10 native backend is very close to our original8.0.2. However, its normal input processor expects an external audio signal; no integrated file source was established in this review. The App Store lists2.1.0 while public desktop source uses a different version series, requiring care with provenance. SonoBus is the more convenient first sustained-output control. The earlier [source comparison](research/2026-09-09-juce-ios-projects.md) retains detailed BYOD configuration findings.

## Evidence retained

Downloaded public source and manuals: /private/tmp/smartgrid-research-20260910-juce-control. File URLs/hashes and version distinctions are in summaries/sonobus-control-source-provenance-20260910.json. The main-branch SonoBus1.7.2 findings are historical and are superseded for this proposed iOS1.7.3 test. No iPad settings or application code was changed during candidate research.

## Capture startup record — 13:25:49 PDT

Raw base: `/private/tmp/smartgrid-ipad-sonobus-hour-20260910`. Capture: K-Mix3/4, 48kHz/stereo/24bit, 3,600-second target. Local wrapper PID20624, SoX20631, exec session50336. The heartbeat `track-sonobus-ipad-hour` checks local recorder health every five minutes and will collect/analyze the iPad archive after completion. Detailed startup evidence is in [the start manifest](summaries/sonobus-hour-20260910-start.json). No result is claimed while the run is in progress.

Read-only iPad preflight verified SonoBus1.7.3/build90, bundle com.Sonosaurus.SonoBus, PID18546. Screenshot shows its UI visible, local playback and loop enabled, an apparently nine-second source display with a shorter selected region, and no network group joined. The file label appears to read “all in dojo”; exact source identity and loop endpoints are not established. Account for repeated source rests and loop seams when scoring waveform candidates.

SmartGrid still had a process, PID18488; that alone did not establish whether it was actively rendering. To exclude a competing audio session, it was deliberately terminated at13:26:31–33; re-query returned SmartGrid PID0 and unchanged SonoBus PID18546. **Exclude the first45seconds from steady-state fault attribution.** The nominal capture remains one hour, with about59m15s after this preflight transition. No further agent device queries are planned during the capture.

At startup the high-level battery service reported100%, external power connected and BatteryIsCharging=true. That differs from the earlier12:54 power snapshot; no battery-current measurement was made in this preflight, so do not infer actual charging watts. Topology is unchanged per the user.

The first153.372seconds of analog capture have sustained music, about−31dBFS RMS and−18.2dBFS peak in the final checked interval, no both-channel near-silence candidates at the existing20ms threshold, and an empty capture stderr. This is recorder validation, not a test result or an assessment of periodic short holes. Hardware48k/512 and channel configuration still need confirmation; the Mac recorder's48kHz setting cannot establish them.

## Read-only investigation while recording

The user reported continued clean listening and requested a fresh-context subagent comparison. That review is complete and independently checked: [combined findings](research/2026-09-10-sonobus-source-comparison.md). No distinctive SonoBus native USB recovery or scheduler remedy was found. Actual session choices and workload remain comparison dimensions; this source finding does not certify the still-running recording as clean. The source investigation made no iPad queries or app/recording/settings changes.

## Completed analysis

The waveform scanners processed all3,600seconds. The20ms both-channel near-silence detector found0 candidates. The same8-sample local-flatness detector used on SmartGrid found0 short candidates(0.417–20ms),0 long candidates, and consequently0 dense periodic episodes. Levels remained around−31dBFS RMS and−18.2dBFS peak. These detectors cover the substantial blanking/dropouts previously measured; they are not a guarantee against all click types.

The iPad archive was collected14:26:59–14:27:36 over authenticated Wi-Fi, after capture. It contains976files/about230MB. The measured wall-clock interval has38,370selected log rows, including1,735USB-audio-related rows; no transaction-failure, restart, zero-length transfer or ioDrift messages matched. After the first45seconds' preflight exclusion, the same result holds over3,555seconds. The archive carries continuous usbaudiod timestamp-calculation diagnostics, so missing USB failures is not merely an absent daemon log. `log show` reported wall-clock adjustment; analysis explicitly filtered JSON timestamps and uses nominal wall-clock alignment, not sample-accurate synchronization.

Configuration logs at13:17:55 tie SonoBusPID18546 to48k and5.333ms preferences, show MAYA input/output stream formats48k/four hardware channels, and actual aggregate-device256-frame size. A HAL report at14:25:56 also reports256frames. No configuration-change evidence appears during the capture. Application active-channel masks are not established by hardware-channel counts; neither the UI buffer setting nor this HAL size proves every native callback's frame count.

One HAL overload report appears at14:25:56, about7seconds after the nominal recording end, with a35-sample safety gap(0.729ms) and client-I/O-budget flags. Preserve it as a post-capture observation, not an in-window analog failure or proof that SonoBus never overloads. The archive contains earlier setup and previous audio state; those are not counted as spontaneous SonoBus capture faults.

Raw analysis: `/private/tmp/smartgrid-ipad-sonobus-hour-20260910.analysis.json`; raw archive and waveform share that base. The waveform itself is retained. The control heartbeat was paused after completion. SonoBus was closed after archive preservation at14:31:09, with subsequent PID0 verification before SmartGrid installation. The enabled SmartGrid worker preflight and selected disabled test are separate runs.
