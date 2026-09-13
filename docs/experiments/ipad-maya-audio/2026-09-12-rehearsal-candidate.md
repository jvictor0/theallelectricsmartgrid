# iPad MAYA rehearsal candidate — 2026-09-12

This branch packages the audio fixes for rehearsal testing. The signed Release build was deployed to the iPad on 2026-09-12; extended rehearsal testing remains pending.

## Evidence carried forward

The 2026-09-12 diagnostic build changed the idle I/O wait from 100 µs to 5 ms and made the MIDI sender ordinary-priority. With a hot-to-cool iPad, it ran from 13:09 to 13:50 PDT. Two K-Mix recordings covered 32 minutes, with no long dropouts or short flat artifacts by the same detectors that found 1,377 short artifacts in a known-bad recording. The app reported 48 kHz/512 frames, no callback gaps over 20 ms, no xruns, and no MAYA transaction failures in the inspected log archive. Wakeups fell from about 8,582/s to 840/s, and context switches from about 9,612/s to 1,845/s. The older app had also produced a 35-minute quiet period, so this is provisional evidence, not a cure claim.

That diagnostic build requested **zero audio inputs** and carried a one-off JUCE single-start experiment and native probes. This candidate restores `SourceMixer::x_numPhysicalInputChannels` inputs, uses JUCE 8.0.15's ordinary iOS audio startup, and removes those probes. Its behavior is therefore not identical to the quiet trial.

## Candidate behavior

- Pin JUCE 8.0.15 as `third_party/JUCE`, for both Xcode targets. A build-local `juce_audio_devices` overlay sends explicit CoreMIDI host timestamps on both platforms; its iOS build also sets the initial device buffer default to 512. The overlay checks its source contexts before patching.
- Load saved audio configuration before opening the device. Request 48 kHz/512 frames, MAYA inputs, and seven outputs. Refuse to render if the actual rate or callback size differs; record muted callbacks instead of feeding an unsupported format to the fixed-rate DSP.
- Keep the MIDI sender off the real-time scheduler. On macOS and iOS it wakes every 2 ms, submits bounded batches of short MIDI with 20 ms timestamp lead, drops late/backed-up messages, and sends complete bounded SysEx packets from its worker thread. Output handlers are joined before destruction. MIDI connection refresh is outside this diff.
- Wait 5 ms when the I/O job queue is empty. Work arrives with at most that additional idle polling latency; the queue does not accumulate work during idle.

## Diagnostics kept and removed

`AppObserver` keeps the audio callback counters and periodic reports out of `MainComponent`. The callback only updates counters for arrival gaps, overruns, format mutes, and callbacks. The message thread reports changed timing counters at most once per minute, and once per minute reports the route, actual rate/frames/input/output channels, thermal state, low-power state, audio session preferences, and MIDI queue/native submission counters.

The test tone, UI-off and zero-input test flags, per-callback INFO lines, native RemoteIO probes, single-start patch, and raw recording/log artifacts are excluded from this branch. The diagnostic worktree and `/private/tmp/smartgrid-idle5-midi-normal-20260912` retain the trial material locally.

## Rehearsal interpretation

Confirm the launch log reports 48 kHz, 512 frames, and the expected active inputs. If a glitch occurs, note its wall-clock time and whether it was isolated or periodic, then compare the timing/MIDI counters and iPad transaction errors near that time. A mismatch or persistent format mute indicates a route/configuration failure, not valid 48 kHz processing.

## MIDI output incident after Release deployment

The user moved WRLD.BLDR to another Satechi hub port shortly before its LEDs went dark after the signed Release deployment. The saved configuration and `ms` patch hashes were unchanged. The app reported a running MIDI worker, submitted SysEx, and zero CoreMIDI submission errors, but those counters do not acknowledge physical USB delivery. The iPad kernel logged WRLD.BLDR USB OUT endpoint `0x01` failures with status `0xe0005000` (`pipe stalled`), starting at 17:37:20.940 PDT during a SmartGridOne launch. There were 1,426 such stalls from 17:39:35 to 17:40:05. An earlier known-good iPad trial had zero such stalls in its inspected 13:09–13:11 interval.

At 17:52, the previously working signed build was installed in place with the same config and patch. It also encountered 26,900 WRLD.BLDR pipe stalls from 17:52:19 to 17:54:20 while the controller was in the failed state. The user then unplugged and replugged only WRLD.BLDR's USB cable and confirmed MIDI reception. The older build's following 58 seconds logged zero WRLD.BLDR pipe stalls. The current signed Release build was reinstalled in place, again preserving both hashes, and launched at 17:59:22. Its log showed 48 kHz/512, four active inputs, four active outputs, a running MIDI worker, and zero MIDI submission errors. The iPad archive showed zero WRLD.BLDR pipe stalls from 17:59:21 to 18:00:42, and the user reported that MIDI output seemed to work.

The immediate failure was a stalled USB OUT pipe. Replugging WRLD.BLDR after the port move cleared the failure; the iPad reported USB location `00120000` before and after that replug. The port move or its re-enumeration is a plausible trigger, but the logs do not prove it. Running the older build during the stalled state does not establish which build or event first caused the stall. A clean restart of the Release build did not reproduce it in this short check. If LEDs go dark again, capture the onset time and an iPad archive before resetting the controller, so the first stall can be compared with MIDI traffic and USB events. Play is not required to open MIDI or send the initial handshake; transport can change later LED content.
