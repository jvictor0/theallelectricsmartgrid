# UI Components and Layout

This page documents the JUCE-side control widgets and how they are assembled into the live control surface.

## Core JUCE components

### `BasicPadGrid` (`JUCE/SmartGridOne/Source/BasicPadGrid.hpp`)

- Owns a rectangular range of `PadComponent` instances.
- Each pad receives a `PadUI` object from `PadUIGrid::MkPadUI(x, y)`.
- Handles only local presentation/interaction:
  - layout (`SetCellSize`, spacing, bounds),
  - press/release forwarding (`OnPress`, `OnRelease`),
  - skin changes (`SetSkin`).

### `EncoderComponent` and `EncoderGrid`

- `EncoderComponent` draws one encoder tile:
  - value ring and range,
  - modulation glyph badge (`ModulatorGlyphImages`),
  - short label via `FourteenSegmentDisplayComponent`.
- `EncoderGrid` arranges a sub-rectangle of encoders and resizes them with `SetCellSize`.
- Data source is `EncoderBankUI` / `EncoderBankUIState`.

### `FaderComponent`

- Uses `AnalogUI` as the transport API.
- Vertical and horizontal modes supported.
- Mouse interaction sends normalized `[0,1]` values via `AnalogUI::SendValue(timestamp, value)`.

### `JoyStickComponent`

- Built from paired `AnalogUI` channels.
- Used for fixed and return-style joystick mappings in the Wrld.Bldr view.

## `WrldBuildrComponent` composition

`WrldBuildrComponent` (`JUCE/SmartGridOne/Source/WrldBuildrComponent.hpp`) is the controller page shown when World Builder is disconnected. It always shows its pad grids, encoders, faders and joysticks, including in hardware config mode.

It places controls on a 24x18 logical grid with holder types:

- `BasicPadGridHolder`
- `EncoderGridHolder`
- `FaderHolder`
- `JoyStickHolder`

### Connected visualizer page

`MainComponent` switches to `WrldBuildrVisualizerPage` when World Builder connects. The original controller page has no visualizer mode or voice-config overlay.

The connected page uses a 64×48 logical grid. The top three rows hold File, Config, CPU load, and elapsed recording time. CPU load and recording time use 14-segment displays on black. The recording timer counts captured audio in minutes and seconds, with up to three minute digits and no leading zeros. It lights red during recording and holds the final duration in white until the next take. Three 21×21 visualizer slots separated by half-cell gaps sit above the right-aligned 48×4 meters and two rows of eight encoders. Each encoder has an 8×8 dial and an 8×2 badge strip. Hardware voice config overlays the meter region and extends upward while active.

The connected page directly owns the main visualizer catalogue. Each entry specifies a starting slot, slot span and full/half/quadrant region. Dual VCO scopes share the first slot vertically, and the melody roll spans the last two slots. Voice LFOs shows only its two LFO scopes. Mastering uses final output multiband meters, while Inputs and Deep Vocoder use input source meters. Each keeps both frequency plots, so every page fits in the three-slot row; catalogue bounds are checked at compile time. Both pages initialize encoder modulation backgrounds through `InitializeEncoderModulatorVisualizers`; standalone modulation panels are temporarily absent.

### Responsive layout

- `CalculateGridLayout()` preserves a fixed 24:18 aspect ratio.
- `UpdatePadGridPosition()` propagates computed cell size and offsets to all holders.
- This keeps the whole surface proportional as window size changes.

## Related

- [Message Routing and Smart Buses](ui-routing-buses.md)
- [Controller Integrations](ui-controller-integrations.md)
- [Visualization Pipeline](ui-visualization-pipeline.md)
