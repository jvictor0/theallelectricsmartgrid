# CoreMIDI reconnect regression tests

Run on macOS with Xcode command-line tools, CMake, pkg-config, and libFLAC installed. Initialize the pinned JUCE submodule first.

```sh
make midi-test JOBS=6
```

The test process creates and disposes only its own CoreMIDI virtual endpoints. It exercises the real application handlers, wrapper, sender, and generated JUCE overlay; no physical controller or audio device is needed. A desktop sandbox must permit CoreMIDI access.

Coverage includes the original stale output failure when an endpoint returns with the same unique identifier, matching input recovery, startup without a configured device present, preservation of healthy routes, audio-producer writer refresh, and WRLD.BLDR input-only and output-only recovery with handshake plus feedback on all five color channels. The target also runs the core MIDI scheduling, SysEx queue, realtime, and clock-sync tests.

The audio reattachment cases exercise the real engine and recorder without opening an audio device. Run just these with `smartgrid_midi_tests --test-case='audio reattachment*'`. They cover repeated release/prepare during recording, retained patch and transport state, one recording file, and MIDI timestamp re-anchoring after a gap. Native iOS reset requires separate on-device validation.
