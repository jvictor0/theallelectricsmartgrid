## 1. Preflight and Sync Math

- [x] 1.1 Copy or port the user-provided uncommitted external clock sync source from `/Users/joyo/theallelectricsmartgrid/private/src/ExternalClockSync.hpp`, or stop and ask if that file is unavailable.
- [x] 1.2 Preserve the existing synchronization math as the base for the new clock synchronizer helper instead of replacing it with a fresh implementation.
- [x] 1.3 Add focused unit tests for the clock synchronizer helper covering steady MIDI clock tempo, selected-loop stopped tempo estimation, phase-error correction direction, missing tick recovery, and start/stop transition behavior.

## 2. Realtime MIDI Input and Message Bus

- [x] 2.1 Extend `BasicMidi` helpers/tests so one-byte realtime clock/start/continue/stop messages keep the correct status, timestamp, route, and size.
- [x] 2.2 Update `MidiInputHandler` to forward supported one-byte realtime clock/start/continue/stop messages while preserving existing 3-byte controller behavior.
- [x] 2.3 Add `MessageIn` realtime modes or an equivalent typed representation for MIDI clock, start, continue, and stop.
- [x] 2.4 Add fixed realtime-route conversion in `MidiToMessageIn` and `MessageInBus`, with tests for timestamp-gated clock/start/continue/stop delivery.
- [x] 2.5 Verify unsupported SysEx and unrelated realtime messages are still ignored or produce `NoMessage` as specified.

## 3. Incoming MIDI Scheduling

- [x] 3.1 Change incoming `BasicMidi` to `MessageIn` queueing to stamp converted messages approximately 20,000 microseconds in the future from the MIDI timestamp.
- [x] 3.2 Ensure all incoming `MessageIn` pushes receive the same fixed queued visible timestamp in `MessageInBus`.
- [x] 3.3 Add JUCE-free tests for inbound timestamp latency, queue holdback, and zero-timestamp realtime input delay behavior.

## 4. Synchronizer Integration

- [x] 4.1 Add internal/external clock mode state to `TheNonagonSquiggleBoyInternal`, defaulting to internal.
- [x] 4.2 Add the clock synchronizer member to `TheNonagonSquiggleBoyInternal` and keep all external clock logic inside the integration/synchronizer boundary.
- [x] 4.3 Consume visible realtime `MessageIn` events per sample, set clock tick status true only for the sample where a MIDI clock event is visible, and clear it after consumption.
- [x] 4.4 On visible MIDI start/continue/stop events, update `m_running` exactly as the green run/play cell does in internal clock mode; in external clock mode, arm/stop the synchronizer and derive `m_running` from synchronizer state, treating continue like start.
- [x] 4.5 In `SetNonagonInputs`, use the tempo parameter in internal mode and the clock synchronizer frequency estimate in external mode.
- [x] 4.6 Add system tests showing MIDI clock ticks affect synchronizer input once, internal MIDI start/continue/stop changes global running state, external start/continue arms until clock, external stop derives stopped running state, and external mode overrides the tempo parameter without adding MIDI logic to `TheNonagonInternal` or `TheoryOfTime`.

## 5. Configuration UI and Persistence

- [x] 5.1 Add clock mode storage to runtime configuration and config JSON save/load, treating missing JSON fields as internal mode.
- [x] 5.2 Add an internal/external clock toggle to `ConfigPage` and wire it to the runtime clock mode.
- [x] 5.3 Add tests or manual-verification hooks for config JSON round-trip and older-config default behavior.

## 6. Theory of Time Loop Selector Parameter

- [x] 6.1 Add a six-position switch-valued Theory of Time loop selector parameter using the existing `ForEachSmartGridOneParam.hpp` pattern.
- [x] 6.2 Map switch values exactly like `SampleLoopIndex`: `TheoryOfTimeBase::x_numLoops - switchVal - 1`, so fully counterclockwise selects the master loop and the other values select the remaining loops.
- [x] 6.3 Read the loop selector parameter where the external clock synchronizer integration needs `ExternalClockSync::Input::m_loopIndex`.
- [x] 6.4 Add tests for encoder switch metadata publication, switch-to-loop-index mapping, and patch JSON round-trip of the new parameter.

## 7. Verification

- [x] 7.1 Run the focused unit tests added for synchronizer, MIDI realtime routing, incoming scheduling, and loop selector parameter behavior.
- [x] 7.2 Run the focused system tests for `TheNonagonSquiggleBoyInternal` external clock mode and run-state integration.
- [x] 7.3 Run the full `private/test` CMake test suite.
- [x] 7.4 Run `openspec status --change add-midi-clock-sync` and confirm all artifacts remain apply-ready.
