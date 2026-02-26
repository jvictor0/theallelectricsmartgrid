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

`WrldBuildrComponent` (`JUCE/SmartGridOne/Source/WrldBuildrComponent.hpp`) is the main composition root for the controller/visualizer page.

It places controls on a 24x18 logical grid with holder types:

- `BasicPadGridHolder`
- `EncoderGridHolder`
- `FaderHolder`
- `JoyStickHolder`
- `ScopeComponentHolder`

### Display modes

`WrldBuildrComponent::SetDisplayMode()` switches between:

- **Controller** mode:
  - shows pad grids, encoder grids, faders, joysticks.
- **Visualizer** mode:
  - hides controller widgets,
  - shows scoped/FFT/metering components based on `VisualDisplayMode`.

### Responsive layout

- `CalculateGridLayout()` preserves a fixed 24:18 aspect ratio.
- `UpdatePadGridPosition()` propagates computed cell size and offsets to all holders.
- This keeps the whole surface proportional as window size changes.

## Related

- [Message Routing and Smart Buses](ui-routing-buses.md)
- [Controller Integrations](ui-controller-integrations.md)
- [Visualization Pipeline](ui-visualization-pipeline.md)
