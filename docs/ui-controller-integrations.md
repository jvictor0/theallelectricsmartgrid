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
  - `GridsMode` (ComuteAndTheory, TheoryOfTimeRhythm, Matrix, Intervals, SubSequencer, Config),
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

### Whole-cycle rhythm pages

In the normal aux grid view, pad `(1, 1)` selects `TheoryOfTimeRhythm` mode. Route 6 shows the rhythm page and route 7 the reset page. Mode selection also chooses the Controller display. Aux mode ordinals are ComuteAndTheory 0, TheoryOfTimeRhythm 1, Matrix 2, Intervals 3, SubSequencer 4, and Config 5; these ordinals are runtime state, not saved patch values.

On the left, each column selects one of six loops and each of eight rows a rhythm step. Press toggles a gate; Shift-press sets the active length to that row plus one. Pads beyond the active size are dark. The current step is bright Purple when on and Pink when off; other active steps are dim Purple or Grey. The display reflects the configured pattern immediately, while the sounding gate accepts gate/size/reset edits at that loop's next modulated tick.

On the right, column `i`, row `j` selects loop `j` as the reset for loop `i`. Only strict ancestors in the accepted topology are enabled. The selected reset is Blue, other eligible resets dim Blue, and self/non-ancestor pads are dark and inert. Pressing the selected reset clears it to -1. Row 7 displays each loop's live gate. A reset that loses ancestry remains stored, is hidden and ignored while invalid, and works again when the ancestry returns.

Both pages share StateSaver registrations: `TheoryOfTimeRhythm` stores loop/step slots 0-7, `TheoryOfTimeRhythmSize` stores each loop's size, and `TheoryOfTimeRhythmReset` stores each reset. The engine has 16 slots, but this UI exposes and persists eight. Missing keys in older patches preserve current values under the ordinary StateSaver policy; fresh instances default to size 2, `[true, false]`, and no reset. See [Theory of Time](theory-of-time.md#loop-rhythms-and-tick-events).

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
