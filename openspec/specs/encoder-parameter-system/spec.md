# Encoder Parameter System Specification

## Purpose
The encoder parameter system (`private/src/Encoder.hpp`, `private/src/EncoderBank.hpp`, `private/src/EncoderBankBank.hpp`) is the software-defined knob layer for Smart Grid One. Every synthesis parameter is a `BankedEncoderCell` owned by a global `EncoderBankBank` and placed into 4×4 encoder bank grids. Each cell stores a normalized base value per track per scene, accepts up to 15 routable modulation slots whose depths are themselves full encoder cells, supports 16 gesture (macro) targets, morphs between two active scenes (see scene-state-management), and publishes per-voice outputs to the DSP engine through a parameter slew filter and to the UI through `EncoderBankUIState`. Modulation sources such as the PolyXFader LFOs (see polyxfader-lfos) and AHD envelopes (see ahd-envelopes) are external DSP components that write into shared per-bank-mode `ModulatorValues`; the encoder cells consume those values but do not own the sources.

## Requirements

### Requirement: Per-Track Per-Scene Base Value Storage
Every parameter SHALL be stored as a normalized base value in [0, 1] indexed by track and by scene (`StateEncoderCell::m_values[track][scene]`, with `SceneManager::x_numScenes == 8` persistent scenes), so that all voices within a track share one base value while modulation and gestures differentiate the voices.
The bank's mode fixes the track/voice topology: Voice banks use 3 tracks × 3 voices, Quad banks 1 track × 4 voices, and Global banks 1 track × 1 voice.

#### Scenario: Voices in a track share the base value
- **WHEN** a Voice-bank parameter's base value for track 1 is 0.6 and no modulation or gesture is active
- **THEN** all three voice channels of track 1 produce output 0.6
- **AND** `EncoderBankUIState::GetValue(i, j, k)` reads 0.6 for each of track 1's voice channels k

#### Scenario: Scene-blended base value
- **WHEN** a track's base value is 0.2 in scene 1 and 0.8 in scene 2, scene 1 and scene 2 are the active pair, and the global blend factor is 0.25
- **THEN** the effective base value is 0.2 × 0.75 + 0.8 × 0.25 = 0.35

### Requirement: Blend-Proportional Encoder Increments
When a physical encoder is turned at an intermediate blend factor t (0 < t < 1), the system SHALL distribute the increment across both active scenes' stored values for the current track, adding `delta × (1 − t)` to the scene-1 value and `delta × t` to the scene-2 value, so the blended output moves by exactly `delta`.
If one scene's value would leave [0, 1], that value is clamped and the other scene's value is solved so the blended output still lands on the clamped target. At t == 0 or t == 1 only the single fully-active scene's value changes.

#### Scenario: Mid-blend turn updates both scenes
- **WHEN** the blend factor is 0.5, a track's stored values are 0.4 (scene 1) and 0.6 (scene 2), and the encoder receives an increment of +0.2
- **THEN** the scene-1 value becomes 0.5 and the scene-2 value becomes 0.7
- **AND** the blended output rises from 0.5 to 0.7

#### Scenario: Clamped scene is compensated
- **WHEN** the blend factor is 0.5 and an increment would push the scene-2 value above 1
- **THEN** the scene-2 value is clamped to 1 and the scene-1 value is recomputed so the blended output equals the clamped overall target

### Requirement: Fifteen Modulation Slots with Nested Depth Cells
Every `BankedEncoderCell` SHALL expose up to 15 modulation slots (`x_numModulators == 15`); each slot's depth is itself a full `BankedEncoderCell`, so modulation depths can in turn be modulated and gesture-controlled to arbitrary nesting depth.
Pressing a connected encoder enters selection mode: the 4×4 grid switches to show the selected cell's 15 depth cells plus the selected cell itself at position (3, 3). Depth cells whose values are zero everywhere, with no active sub-modulators or gestures, are garbage-collected when the selection is closed.

#### Scenario: Selecting a parameter exposes its depth cells
- **WHEN** the performer presses a connected base parameter encoder (without shift)
- **THEN** the visible grid is repopulated with that parameter's 15 modulation depth cells and the parameter itself at (3, 3)
- **AND** each depth cell reports through `EncoderUIState::GetConnected` whether a modulation source is wired to its slot in the current bank mode

#### Scenario: Zeroed depth cell is garbage-collected
- **WHEN** a depth cell's value is 0 in every scene and track, it has no active sub-modulators or gestures, and the selection is deselected
- **THEN** the depth cell is released back to the encoder pool and excluded from serialization

### Requirement: Crossfade Modulation Mixing per Voice
The system SHALL compute each parameter's per-voice output as a crossfade between the post-gesture base value and the modulation sources: with total modulation weight `W = Σ sourceOutput × amplitude` and weighted value `V = Σ sourceOutput × depthValue × amplitude` over active slots, the output is `base × (1 − W) + V` when W ≤ 1, and `V / W` when W > 1.
Source values and amplitudes are read per voice channel from the shared `ModulatorValues::m_value[slot][channel]` and `m_amplitude[slot][channel]` of the cell's bank mode, written by external DSP components (PolyXFader LFOs, AHD envelopes, ganged random LFOs, sheafy modulators). A depth value of 1 with source output 1 therefore yields full takeover of the parameter.

#### Scenario: Full-depth modulation overrides the base
- **WHEN** one modulation slot has source output 1.0, amplitude 1, and depth value 1.0 on a voice channel
- **THEN** that channel's output equals 1.0 regardless of the base value
- **AND** `GetMinValue` reports 0 and `GetMaxValue` reports 1 for that channel

#### Scenario: Partial modulation crossfades
- **WHEN** the post-gesture base value is 0.5 and one slot contributes source output 0.5, amplitude 1, depth 0.8 on a channel
- **THEN** the channel output is 0.5 × (1 − 0.5) + 0.5 × 0.8 = 0.65
- **AND** `GetMinValue` reports 0.25 and `GetMaxValue` reports 0.75 (the modulation extent around the diminished base)

#### Scenario: Per-voice polyphonic outputs from a shared base
- **WHEN** a per-voice modulation source (such as a voice AHD envelope) holds different values for the three voices of a track
- **THEN** `EncoderBankUIState::GetValue(i, j, k)` reads a different post-modulation value for each voice channel k of that track while the base value remains shared

### Requirement: Sixteen Gesture Targets with Constant-Output Editing
Every parameter SHALL support 16 gesture parameters (`x_numGestureParams == 16`), each pairing a hidden target-state `BankedEncoderCell` with a live weight in [0, 1] supplied through `ModulatorValues::m_gestureWeights` from physical analog inputs. For each track the post-gesture value is the weight-normalized blend `Σᵢ wᵢ × (base × (1 − wᵢ) + targetᵢ × wᵢ) / Σᵢ wᵢ` over gestures active for that track and scene; with no active gestures it is the base value.
Gesture activation is stored per scene per track (`m_isActive`), and the effective weight is the scene-blended activation times the live weight. Turning an encoder while gestures are active splits the physical delta between the gesture targets (proportional to w²) and the base (proportional to w(1 − w)), normalized by the weight sum, so the audible output tracks the physical motion. A shift-press while gestures are selected deactivates those gestures for the current scene(s) instead of zeroing modulators.

#### Scenario: Gesture endpoints
- **WHEN** a single gesture with target value 0.9 is active on a parameter whose base is 0.3
- **THEN** the post-gesture value is 0.3 when the gesture weight is 0
- **AND** the post-gesture value is 0.9 when the gesture weight is 1

#### Scenario: Turning the knob mid-gesture keeps output consistent
- **WHEN** a gesture is held at weight 0.5 and the performer turns the encoder
- **THEN** the increment is distributed between the gesture target cell and the base value rather than applied to the base alone
- **AND** subsequent releases of the gesture leave both the base and target reflecting the edit

### Requirement: Change-Driven Recompute on Control Frames
The system SHALL recompute parameter outputs only on control frames (`EncoderBankBank::Process` gates on `SampleTimer::IsControlFrame()`), and only for cells whose force-update flag is set or whose affecting-modulator/affecting-gesture bitmasks intersect the bank mode's changed-modulator/changed-gesture bitmasks computed by `ModulatorValues::ComputeChanged()`.
Scene-manager change flags trigger bulk refreshes: a blend change (`m_changed`) re-derives every cell's banked values from scene storage, and a scene-index change (`m_changedScene`) recomputes the affecting bitmasks (see scene-state-management).

#### Scenario: Unaffected parameter skips recompute
- **WHEN** a parameter has no active modulators or gestures and neither its force-update flag nor the scene manager's change flags are set
- **THEN** its Compute pass performs no gesture or modulation mixing and its outputs retain their previous values

#### Scenario: Source change recomputes routed parameters
- **WHEN** a PolyXFader LFO routed to a parameter produces a new value on a control frame
- **THEN** that slot's bit is set in the bank mode's changed-modulator bitmask
- **AND** every cell whose affecting bitmask includes that slot recomputes its outputs on the same control frame

### Requirement: Parameter Slew on Final Outputs
The system SHALL pass each of a cell's up-to-16 per-channel outputs through a one-pole low-pass slew filter (natural frequency 500 Hz at 48 kHz) when read by the DSP engine via `GetSlewedValue`, smoothing the control-frame staircase; an unslewed read path (`GetValueNoSlew`) is also provided.
`InitSlewState(value)` resets all 16 slew filters to a given value; it is applied when an encoder is created with its default value and on `RevertToDefault`, preventing audible sweeps from stale slew state.

#### Scenario: Revert does not glide from stale state
- **WHEN** a parameter is reverted to its default value of 0.5
- **THEN** every slew filter's state is set to 0.5 immediately
- **AND** the next `GetSlewedValue` call returns 0.5 without an exponential approach from the previous output

### Requirement: Named JSON Serialization via the Global Owner
The system SHALL serialize all encoder state through `EncoderBankBank::ToJSON`/`FromJSON`, which iterate the flat owner array of named encoder cells; each cell writes its per-scene per-track base values, its non-null modulation depth cells (recursively), its gesture target cells, and—for gesture cells—the per-scene per-track activation flags.
Loading looks each parameter up by name, rebuilds depth and gesture cells from the JSON, garbage-collects empty cells, recomputes affecting bitmasks, and sets force-update. Banks deselect any open modulator selection before loading so visible grid pointers never dangle.

#### Scenario: JSON round-trip restores modulation topology
- **WHEN** a patch is saved with a parameter whose base is 0.7 in scene 3, with modulation slot 6 at depth 0.4 and gesture 2 active on track 0 in scene 0
- **THEN** loading that JSON restores the scene-3 base to 0.7, recreates the slot-6 depth cell with value 0.4, recreates the gesture-2 target cell, and restores its scene-0/track-0 activation flag

#### Scenario: Unknown parameters are skipped
- **WHEN** the JSON being loaded lacks an entry for a named encoder
- **THEN** that encoder keeps its current state and loading continues with the remaining parameters

### Requirement: UI State Publication Through Atomics
The system SHALL publish the selected bank's visible 4×4 grid to an `EncoderBankUIState` of lock-free atomics every UI population pass: per cell the post-modulation output per channel (`GetValue(i, j, k)`), the modulation extent (`GetMinValue`/`GetMaxValue`), brightness, connectedness (`GetConnected`), color, short name, switch values, and the per-track affecting-modulator and affecting-gesture bitmasks; plus bank-level track/voice counts and current track.
Brightness encodes modulation takeover (1 minus the current track's modulation weight, clamped to [0, 1]); during the scene blinker's off phase, base parameters with no selected gestures additionally dim by their gesture weight sum so gesture-captured knobs blink. Disconnected cells publish connected == false, brightness 0, and zeroed values.

#### Scenario: Empty grid position reads disconnected
- **WHEN** a grid position holds no connected cell (for example after a machine change placed nullptr there)
- **THEN** `GetConnected(i, j)` returns false and `GetBrightness(i, j)` returns 0
- **AND** `GetValue(i, j, k)` returns 0 for every channel k

#### Scenario: Modulated cell dims and reports extent
- **WHEN** the current track's modulation weight on a visible cell is 0.75
- **THEN** `GetBrightness` for that cell reads 0.25
- **AND** `GetMinValue`/`GetMaxValue` bracket the channel outputs so the UI can draw the modulation arc
