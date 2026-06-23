## Why

Smart Grid One can emit MIDI clock, but it cannot reliably follow an external MIDI clock source or align sequencer timing to incoming realtime clock ticks. Reliable external sync now matters because the Theory of Time needs to adjust tempo and phase from MIDI clock input while keeping sample-accurate outgoing tick timing and the existing Nonagon sequencing ownership boundaries.

## What Changes

- Add an internal/external clock toggle on the configuration page, persisted with the existing app configuration.
- Add a MIDI clock synchronizer owned by `TheNonagonSquiggleBoyInternal`, using the existing external-clock synchronization math as the implementation base when that file is available in the worktree.
- Route MIDI realtime clock/start/continue/stop messages into the existing timestamp-gated `MessageInBus` path, including one-byte realtime messages that are currently ignored by MIDI input handling.
- Apply a fixed future scheduling latency of about 20 ms when converting incoming `BasicMidi` messages into queued `MessageIn` events so realtime clock ticks become visible to audio processing at stable sample-derived times.
- Set per-sample clock-tick status from visible realtime MIDI clock messages and feed that status into the clock synchronizer on the sample where the tick becomes visible.
- In internal clock mode, let MIDI start/continue/stop update the same global running state used by the green run/play cell; in external clock mode, let start/continue arm the synchronizer and derive global running from synchronizer state.
- In external clock mode, set the Theory of Time tempo from the clock synchronizer output instead of the tempo parameter, while keeping topology, phase modulation, and outgoing phasor-to-tick behavior otherwise intact.
- Add a Theory of Time page parameter for external sync loop selection/multiple using the same six-position switch pattern as sample source, with the fully counterclockwise value selecting the master loop and the remaining values selecting the other Theory of Time loops.
- Add focused tests for MIDI realtime parsing/routing, delayed output scheduling, clock synchronizer ownership/integration, external tempo handoff, internal run-state start/continue/stop, external armed/running transitions, and loop selection.

## Capabilities

### New Capabilities
- `midi-clock-sync`: Defines external MIDI clock synchronization behavior, clock tick sampling, synchronizer ownership, tempo correction, and MIDI start/continue/stop run-state integration.

### Modified Capabilities
- `controller-midi-io`: MIDI input shall preserve realtime messages, route them into the message bus, and schedule inbound `BasicMidi` to `MessageIn` delivery with the fixed future latency.
- `phasor-timebase`: Theory of Time shall accept an externally synchronized tempo source while preserving the internal clock contract and loop topology behavior.
- `nonagon-sequencer`: Nonagon processing shall expose clock-tick status per sample and keep synchronizer logic owned by the surrounding SquiggleBoy integration.
- `juce-device-configuration`: The config page shall expose and persist internal/external clock mode.
- `encoder-parameter-system`: The Theory of Time page shall expose a six-position loop selector parameter with master-loop selection at the fully counterclockwise setting.

## Impact

- Affected core files include `private/src/TheNonagonSquiggleBoy.hpp`, `private/src/TheNonagon.hpp`, `private/src/TheoryOfTime.hpp`, `private/src/MessageIn.hpp`, `private/src/MessageInBus.hpp`, `private/src/MidiToMessageIn.hpp`, `private/src/BasicMidi.hpp`, and likely a new or existing external clock synchronizer header.
- Affected JUCE files include `JUCE/SmartGridOne/Source/MidiHandlers.hpp`, `JUCE/SmartGridOne/Source/NonagonWrapper.hpp`, `JUCE/SmartGridOne/Source/ConfigPage.hpp`, `JUCE/SmartGridOne/Source/Configuration.hpp`, and config JSON save/load in `MainComponent`.
- Tests should be added under `private/test/unit` and `private/test/system`, using existing fixtures where possible and adding small fakes for MIDI sender/handler timing where JUCE-free tests cannot instantiate JUCE components.
- Before implementation, use the user-mentioned uncommitted external clock sync source from the main worktree at `/Users/joyo/theallelectricsmartgrid/private/src/ExternalClockSync.hpp`. If it is not present, stop and ask for the file or the correct worktree path rather than replacing its synchronization math with a clean-room rewrite.
