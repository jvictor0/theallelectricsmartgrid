# UI Shell Layout Specification

## Purpose
The UI shell (`JUCE/SmartGridOne/Source/WrldBuildrComponent.hpp` and the widget headers it composes) renders the on-screen control surface for Smart Grid One: pad grids, encoder grids, faders, joysticks, and visualizers arranged on a fixed 24×18 logical cell grid. Every widget is a thin presentation adapter over the UIState layer — pads read colors from `SmartBusColor` via `PadUI`, encoders read `EncoderBankUIState`, and faders/joysticks exchange normalized values through `AnalogUI` — so the on-screen surface and physical controllers (see controller-midi-io) share identical state and message paths. Parameter semantics belong to encoder-parameter-system; visualizer content belongs to grid-visualizers.

## Requirements

### Requirement: Fixed-Aspect 24×18 Logical Grid Layout
The system SHALL lay out the control surface on a fixed 24-column by 18-row logical cell grid where the cell size is the largest integer fitting the window (height/18 when the window is wider than 24:18, otherwise width/24), and the grid is centered along the slack axis.
On every resize, `CalculateGridLayout()` recomputes the cell size and origin and `UpdatePadGridPosition()` repositions every holder (pad grids, encoder grids, faders, joysticks, voice config, visualizer) from its logical (gridX, gridY) coordinates.

#### Scenario: Wide window letterboxes horizontally
- **WHEN** the component is resized to 2400×900 pixels
- **THEN** the cell size becomes 900 / 18 = 50 pixels
- **AND** the 1200-pixel-wide grid is centered horizontally with a 600-pixel margin on each side

#### Scenario: Resize repositions all holders
- **WHEN** the window is resized
- **THEN** every holder's `SetBounds(cellSize, gridX, gridY)` is called so each widget's pixel bounds equal its logical position times the new cell size

### Requirement: Pad Subgrid Composition over Shared Routes
The system SHALL compose multiple independent `BasicPadGrid` instances, each managing a rectangular subrange (startX, startY, endX, endY) of pad cells and creating one `PadComponent` per cell via `PadUIGrid::MkPadUI(x, y)`, so that several on-screen grids can present disjoint regions of the same controller route.
The shell builds 8×8 left and right grids on the LeftGrid and RightGrid routes, and splits the AuxGrid route across bottom-center (rows 0–1), below-encoders (rows 2–3), left-top (rows 4–5 columns 0–3), timer (rows 4–5 columns 4–7), and two arcade-button rows (row 6), with per-holder pad width/height multipliers.

#### Scenario: Subgrid creates only its range
- **WHEN** a `BasicPadGrid` is constructed with range (0, 2, 8, 4)
- **THEN** it owns exactly 16 `PadComponent`s whose `PadUI` coordinates cover x 0–7, y 2–3 on the grid's route

#### Scenario: Press forwarding uses grid-local coordinates
- **WHEN** `OnPress(3, 1, t)` is called on a subgrid starting at (0, 2)
- **THEN** the pad at local index 1 × 8 + 3 receives the press and pushes a `PadPress` for its absolute `PadUI` coordinates into the MessageInBus

### Requirement: Pad Rendering from SmartBusColor
The system SHALL paint each pad from the UIState layer: `PadComponent::paint` calls `PadUI::GetColor()`, which atomically reads the cell's `SmartGrid::Color` from the route's `SmartBusColor`, converts it to a JUCE colour, and draws a rounded rectangle with a radial-gradient LED effect; the computed LED brightness (max RGB component / 255) is floored at 20% so unlit pads remain visible.
The arcade-button skin instead draws a circular white button with a small LED in the upper-left corner that shows the bus color when it is not `Color::Off`.

#### Scenario: Lit pad shows its bus color
- **WHEN** the SmartBusColor cell backing a pad holds RGB (0, 180, 90)
- **THEN** the pad paints a rounded rectangle filled with a radial gradient of colour (0, 180, 90)

#### Scenario: Brightness floor for dim colors
- **WHEN** the bus color's largest channel is 25 (brightness 0.098)
- **THEN** the effective brightness used for the LED effect is clamped to 0.2

### Requirement: Pad Mouse Interaction Produces Timestamped Messages
The system SHALL translate pad mouse input into the same message path used by physical controllers: mouse-down calls `PadUI::OnPress(timestamp)` and mouse-up calls `PadUI::OnRelease(timestamp)` with the event time in microseconds (milliseconds × 1000), pushing `PadPress`/`PadRelease` `MessageIn` events with the pad's route ID and coordinates into the MessageInBus.

#### Scenario: Mouse click on a pad
- **WHEN** the user presses the mouse on the pad at (5, 2) of route 6 at event time 1234 ms
- **THEN** a `PadPress` with timestamp 1,234,000 µs, route 6, x = 5, y = 2 is pushed to the bus
- **AND** releasing the mouse pushes the matching `PadRelease`

### Requirement: Encoder Tile Rendering from EncoderBankUIState
The system SHALL render each encoder tile entirely from `EncoderBankUIState`: when `GetConnected(x, y)` is true, the tile draws one 270-degree ring per voice (arc angle = π × 1.25 + value × π × 1.5, radius shrinking 5% per voice ring), a min-to-max value arc and an indicator dot per voice colored by `GetIndicatorColor(voice)`, switch-gap segmentation when `GetSwitchValues(x, y)` exceeds 1, and a brightness-adjusted color swatch from `GetBrightnessAdjustedColor(x, y)` in the lower gap.
Voices are addressed as `GetNumVoices() × GetCurrentTrack() + i`; non-finite values and voice indices ≥ 16 are skipped. A disconnected encoder draws only the color swatch.

#### Scenario: Value maps to ring angle
- **WHEN** a connected encoder's voice value reads 0.5
- **THEN** its indicator is drawn at arc angle π × 1.25 + 0.5 × π × 1.5 = 2π radians

#### Scenario: Disconnected encoder draws no rings
- **WHEN** `GetConnected(x, y)` returns false for an encoder cell
- **THEN** no arcs, badges, or indicators are painted for that cell

### Requirement: Modulation and Gesture Badges
The system SHALL draw one badge per modulator affecting an encoder (from `GetModulatorsAffecting(x, y)`) above the ring, rendering the glyph image selected by `EncoderBankUIState::GetModulationGlyph(modIndex)` from the seven-glyph set (LFO, ADSR, Sheaf, SmoothRandom, Noise, Quadrature, Spread) over a background tinted with `GetModulationGlyphColor(modIndex)`; gesture badges (from `GetGesturesAffecting`) are drawn below the ring with numeric or arrow symbols.
A glyph value of `None` draws no badge for that modulator slot.

#### Scenario: Active LFO modulation shows a badge
- **WHEN** modulator slot 2 affects encoder (1, 0) and `GetModulationGlyph(2)` returns LFO
- **THEN** the LFO glyph image is drawn in a badge above the encoder ring, tinted by `GetModulationGlyphColor(2)`

#### Scenario: Glyph None suppresses the badge image
- **WHEN** `GetModulationGlyph(i)` returns `None` for an affecting modulator
- **THEN** no glyph image is drawn for that slot

### Requirement: Encoder Mouse Interaction
The system SHALL translate encoder mouse gestures into encoder messages on the encoder route: dragging pushes `EncoderIncDec` events with amount = (deltaX − deltaY) × 0.2 truncated to an integer (skipped when zero), and double-clicking pushes `EncoderPush` with amount 1, all timestamped with the event time in microseconds.

#### Scenario: Diagonal drag increments the encoder
- **WHEN** the mouse drags 30 pixels right and 10 pixels up on encoder (2, 1)
- **THEN** an `EncoderIncDec` with amount int((30 + 10) × 0.2) = 8 is pushed for (2, 1)

#### Scenario: Double-click acts as a push
- **WHEN** the user double-clicks an encoder tile
- **THEN** an `EncoderPush` message with amount 1 is pushed for that encoder's coordinates

### Requirement: Analog Widgets Send Normalized Values
The system SHALL send fader and joystick input through `AnalogUI::SendValue(timestamp, value)`, which pushes a `ParamSet14` message carrying value × 16383 on the Analog route for the widget's channel index; fader positions and joystick dots are painted from `AnalogUI::GetValue()`, the atomically published current value.
The shell provides 8 vertical faders (channels 1–8), one horizontal crossfader (channel 0), and two joysticks built from four `AnalogUI` channels each (x⁺, x⁻, y⁺, y⁻; channels 9–12 and 13–16); the return-style joystick sends 0.0 on all four channels at mouse-up.

#### Scenario: Fader drag sends a 14-bit value
- **WHEN** a vertical fader is dragged to its midpoint at event time T
- **THEN** a `ParamSet14` with amount ≈ 0.5 × 16383 is pushed for that fader's analog channel with timestamp T µs

#### Scenario: Return joystick recenters on release
- **WHEN** the mouse is released on the return-style joystick
- **THEN** `SendValue(timestamp, 0.0)` is called on all four of its analog channels

### Requirement: Controller and Visualizer Display Modes
The system SHALL switch the surface between two display modes from the integration's stored `DisplayMode`: Controller mode shows the pad grids, faders, crossfader, and joysticks and hides the visualizer component; Visualizer mode hides those controller widgets and shows the full-bounds visualizer, which excludes the four encoder-grid regions — logical rectangles 8×2 at (8, 0), (16, 0), (8, 3), and (16, 3) — from both its paint clip and its mouse handling so the encoder tiles stay visible and interactive.
Independently, when the grids mode is Config the four encoder grids are hidden and the voice-config component is shown in their place.

#### Scenario: Switching to Visualizer mode
- **WHEN** the stored display mode is Visualizer and `SetDisplayMode()` runs
- **THEN** pad grids, faders, and joysticks become invisible and the visualizer component becomes visible over the full bounds

#### Scenario: Encoder regions excluded from visualizer
- **WHEN** the visualizer paints or receives a click at a point inside the logical rectangle starting at (16, 3) spanning 8×2 cells
- **THEN** the visualizer neither paints into nor handles input for that region

#### Scenario: Config mode swaps encoders for voice config
- **WHEN** the grids mode is Config
- **THEN** all four encoder grids are hidden and the voice-config component is visible at the encoder area
