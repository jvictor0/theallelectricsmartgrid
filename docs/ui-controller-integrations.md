# Controller Integrations

This page documents how controller-specific integrations are wired around the shared sequencing/audio core.

## Integration roots

### `TheNonagonSquiggleBoyWrldBldr`

Defined in `private/src/TheNonagonSquiggleBoyWrldBldr.hpp`.

- Exposes three grid routes:
  - left grid,
  - right grid,
  - aux grid.
- Exposes encoder and analog routes.
- Maintains:
  - `GridsMode` (ComuteAndTheory, Matrix, Intervals, SubSequencer, Config),
  - `DisplayMode` (Controller/Visualizer),
  - active trio linkage for trio-dependent grids.

Route IDs:

- Encoder: `4`
- Analog: `5`
- LeftGrid: `6`
- RightGrid: `7`
- AuxGrid: `8`

Message flow:

- inbound MIDI -> `MessageInBus` route conversion,
- `Apply(msg)` routes message to encoder/analog or one of the three grids,
- `ProcessFrame()` publishes `m_leftGrid/m_rightGrid/m_auxGrid` colors into `UIState::m_colorBus[3]`.

### `TheNonagonSquiggleBoyQuadLaunchpadTwister`

Defined in `private/src/TheNonagonSquiggleBoyQuadLaunchpadTwister.hpp`.

- Provides four Launchpad grid routes plus encoder/param routes.
- Supports top-grid multiplexing modes:
  - Matrix,
  - Water,
  - Earth,
  - Fire.
- Handles route dispatch for top-left/top-right/bottom-left/bottom-right sections.
- Publishes LED state through `UIState::m_colorBus[4]`.

## MIDI protocol adapters

### Wrld.Bldr path (`WrldBLDRMidi`)

Defined in `private/src/WrldBLDRMidi.hpp`.

`WrldBLDRMidi::FromMidi()` maps channels to routed `MessageIn`:

- channels 0/1 -> encoder route
- channel 2/14 -> analog parameter routes
- channels 3/4 -> left/right grid note routes
- channel 5 -> aux grid route

`WrldBLDRMidiWriter` emits:

- Yaeltex SysEx color packets,
- indicator messages,
- with internal cooldown/budget handling.

### Launchpad/Twister path

In `JUCE/SmartGridOne/Source/NonagonWrapper.hpp`:

- `MidiLaunchpadOutputHandler` uses `LPSysexWriter` for Launchpad LEDs.
- `MidiEncoderOutputHandler` uses `EncoderMidiWriter` for encoder ring/value output.
- Input handlers forward raw `BasicMidi` to integration owners.

## `NonagonWrapper` as top-level aggregator

`NonagonWrapper` owns:

- `NonagonWrapperQuadLaunchpadTwister`
- `NonagonWrapperWrldBldr`
- shared `MidiSender`
- `TheNonagonSquiggleBoyInternal` core

Per-sample/frame orchestration:

- sample: both integrations process inbound events; core audio sample is generated.
- frame: integrations process frame logic and push MIDI output updates.

## Related

- [Message Routing and Smart Buses](ui-routing-buses.md)
- [MIDI I/O Scheduling](ui-midi-scheduling.md)
- [UI Components and Layout](ui-components-layout.md)
