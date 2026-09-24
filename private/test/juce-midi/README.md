# CoreMIDI reconnect regression tests

Run on macOS with Xcode command-line tools, CMake, pkg-config, and libFLAC installed. Initialize the pinned JUCE submodule first.

```sh
cmake -S private/test/juce-midi -B /tmp/smartgrid-midi-tests
cmake --build /tmp/smartgrid-midi-tests -j 6
ctest --test-dir /tmp/smartgrid-midi-tests --output-on-failure
```

The test process creates and disposes only its own CoreMIDI virtual endpoints. It exercises the real application handlers, wrapper, sender, and generated JUCE overlay; no physical controller or audio device is needed. A desktop sandbox must permit CoreMIDI access.

Coverage includes the original stale output failure when an endpoint returns with the same unique identifier, matching input recovery, startup without a configured device present, preservation of healthy routes, audio-producer writer refresh, and WRLD.BLDR input-only and output-only recovery with handshake plus feedback on all five color channels. The target also runs the core MIDI scheduling, SysEx queue, realtime, and clock-sync tests.
