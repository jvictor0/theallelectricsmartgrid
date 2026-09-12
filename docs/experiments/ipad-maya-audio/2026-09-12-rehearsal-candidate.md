# iPad MAYA rehearsal candidate — 2026-09-12

This branch packages the audio fixes for rehearsal testing. It has not been deployed to the iPad or run through another listening session.

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
