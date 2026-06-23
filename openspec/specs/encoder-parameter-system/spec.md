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

Because recompute is change-driven and a cell that is no longer modulated or gestured has an empty affecting bitmask (so no source change can ever re-trigger it), any operation that mutates a cell's modulation configuration, gesture configuration, or base value outside the normal per-frame source path — shift-press encoder reset (`HandleShiftPress`), gesture-button delete (`ClearGesture` / `SetAllModulatorsAffecting`), `RevertToDefault`, absolute set (`EncoderSet`), and JSON load (`FromJSON`) — SHALL set the affected cells' force-update flag (`SetForceUpdateRecursive`) in addition to recomputing the affecting bitmasks (`SetModulatorsAffecting`/`SetModulatorsAffectingRecursive`). Force-update is the only signal that can drive the one corrective recompute after modulation or a gesture is cleared; omitting it leaves the lazy cache serving a stale post-modulation/post-gesture output indefinitely (a frozen value that disagrees with the now-empty affecting bitmask).

#### Scenario: Unaffected parameter skips recompute
- **WHEN** a parameter has no active modulators or gestures and neither its force-update flag nor the scene manager's change flags are set
- **THEN** its Compute pass performs no gesture or modulation mixing and its outputs retain their previous values

#### Scenario: Source change recomputes routed parameters
- **WHEN** a PolyXFader LFO routed to a parameter produces a new value on a control frame
- **THEN** that slot's bit is set in the bank mode's changed-modulator bitmask
- **AND** every cell whose affecting bitmask includes that slot recomputes its outputs on the same control frame

#### Scenario: Clearing modulation forces a corrective recompute
- **WHEN** a parameter is currently modulated (its affecting-modulator bitmask is non-empty and its output differs from its base value) and an operation clears that modulation so the affecting bitmask becomes empty
- **THEN** the operation sets the cell's force-update flag
- **AND** on the next control frame the cell recomputes its output to the unmodulated base value even though no modulation source changed
- **AND** the published `EncoderBankUIState` value for the cell converges to the base value within the parameter-slew bound rather than remaining frozen at the prior modulated value

### Requirement: Hidden Machine-Dependent Parameters Remain Semantically Live
Machine-dependent parameters SHALL remain wired to their bank-mode shared state and participate in semantic parameter operations even when the selected track's machine topology hides them from the visible 4x4 bank. Hiding a parameter from the current bank view SHALL affect only controller/UI placement; it MUST NOT prevent the parameter from recomputing DSP outputs, receiving gesture/modulator change edges, clearing gestures, reverting defaults, copying scenes, or publishing correct values when read by parameter index.

#### Scenario: Hidden parameter catches gesture weight changes
- **WHEN** a Voice-bank parameter is exposed for Water's source machine, hidden when Earth's source machine is selected, and has gesture 2 controlling its Spread modulation depth
- **AND** gesture 2's live weight changes while Earth is selected
- **THEN** the hidden Water parameter recomputes on the next control frame
- **AND** reading the Water parameter by parameter index returns the value corresponding to the new gesture weight when Water is selected again
- **AND** Earth's visible bank view may still publish that grid position as disconnected or zero because Earth does not expose the parameter

#### Scenario: Hidden parameter catches modulation source changes
- **WHEN** a Voice-bank parameter is hidden by the selected track's machine topology but has an active modulation slot whose source changes on a control frame
- **THEN** the hidden parameter recomputes from the changed source on the same control frame
- **AND** DSP reads by parameter index observe the updated per-voice output even though the parameter is absent from the visible bank view

#### Scenario: Clearing a gesture reaches hidden parameters
- **WHEN** a gesture is active on a machine-dependent parameter that is hidden by the currently selected track's machine topology
- **AND** the performer clears that gesture through the gesture selector
- **THEN** the hidden parameter deactivates that gesture for the affected scene and track scope
- **AND** subsequent sweeps of that gesture's live weight no longer affect the hidden parameter's output

#### Scenario: Full patch default reset reaches hidden parameters
- **WHEN** a machine-dependent parameter has a non-default base value, active modulation depth, or active gesture while hidden by the selected track's machine topology
- **AND** an operation reverts the full patch scope to defaults
- **THEN** the hidden parameter's base value, modulation depth subtree, gesture state, slew state, and computed output are reset according to the same rules as visible parameters in that scope
- **AND** the reset value is observed through parameter-index DSP reads without requiring the parameter to become visible first

#### Scenario: Visible bank reset does not reach hidden parameters
- **WHEN** a machine-dependent parameter is hidden by the selected track's machine topology
- **AND** the performer resets the selected bank grid
- **THEN** only cells currently placed in that visible grid revert to default
- **AND** the hidden parameter keeps its stored value because it is not part of that visible grid projection

#### Scenario: Scene copy reaches hidden parameters
- **WHEN** a machine-dependent parameter has different values or gesture/depth state between scenes while hidden by the selected track's machine topology
- **AND** a scene copy operation copies into one of those scenes for the affected scope
- **THEN** the hidden parameter's scene-stored state is copied consistently with visible parameters in that scope
- **AND** selecting a compatible machine later shows the copied state rather than the stale pre-copy state

#### Scenario: Hidden parameters keep UI topology separate from semantic state
- **WHEN** the selected track's machine topology does not expose a parameter that remains semantically live for another track
- **THEN** the visible `EncoderBankUIState` for the selected topology reports the incompatible grid position as disconnected or zeroed
- **AND** direct parameter-index reads for compatible tracks continue to expose the parameter's computed semantic output

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

### Requirement: Encoder JSON Float Load Accepts All Numeric Spellings
When loading named encoder state from JSON, the system SHALL restore normalized per-track per-scene base values from JSON numeric values regardless of whether the token is represented internally as an integer node or a real node. A saved encoder base value of `1.0` MUST NOT become `0.0` solely because the JSON text spells the value as `1`.
This applies to base parameter cells, modulation depth cells, and gesture target cells because all are serialized through the same `StateEncoderCell` value arrays.

#### Scenario: Whole-number base value restores as one
- **WHEN** a named encoder cell is loaded from JSON whose `values` array contains the token `1`
- **THEN** the corresponding stored base value is restored as `1.0`
- **AND** the cell's computed unslewed output is `1.0` after load processing

#### Scenario: Fractional base value still restores
- **WHEN** a named encoder cell is loaded from JSON whose `values` array contains the token `0.5`
- **THEN** the corresponding stored base value is restored as `0.5`

#### Scenario: Nested modulation depth whole-number restores
- **WHEN** a saved modulation depth cell contains a whole-number normalized value in its `values` array
- **THEN** loading the patch restores that depth value numerically instead of converting it to zero

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

### Requirement: Shift-Press Reset Semantics
When the centralized shift flag is held and a connected encoder is pressed (`EncoderBankInternal::HandlePress` with `SceneManager::m_shift` true → `BankedEncoderCell::HandleShiftPress`), the system SHALL reset that cell's modulation and gesture configuration for the current track and SHALL invalidate the change-driven recompute cache so the reset is reflected in both the published value and the affecting bitmasks.
The reset has two branches: when no gestures are selected it zeroes all modulation depth slots for the current track AND deactivates every gesture linked to that cell for the current track (`ZeroModulators`, which calls `DeactivateGesture` per gesture and garbage-collects the emptied cells), returning the encoder to its fully ungestured, unmodulated base value; when one or more gestures are selected it instead deactivates only each selected gesture for the current scene(s) (`DeactivateGestureCurrentScene`), leaving modulators intact. In both branches the cell SHALL recompute its affecting-modulator and affecting-gesture bitmasks from the root (`SetModulatorsAffectingRecursive`) AND set its force-update flag (`SetForceUpdateRecursive`), so the next control frame recomputes the output and the `EncoderBankUIState` masks and value agree.

#### Scenario: Shift-press reset of a modulated encoder restores the base value and clears the mask
- **WHEN** a base parameter at base value 0.3 has a modulation slot whose depth drives its output away from 0.3, no gestures are selected, and the performer shift-presses that encoder
- **THEN** after a few control frames the cell's published `EncoderBankUIState` modulatorsAffecting bitmask is empty
- **AND** the cell's published value and `GetValue` converge to the base value 0.3 within the parameter-slew bound (the value is NOT left frozen at the prior modulated value)

#### Scenario: Shift-press reset of nested modulation clears the whole subtree
- **WHEN** a base parameter has a modulation depth cell that is itself modulated (nested depth) and the performer shift-presses the base encoder with no gestures selected
- **THEN** the base cell and its depth subtree are zeroed for the current track and garbage-collected
- **AND** the published modulatorsAffecting bitmask is empty and the published value converges to the base value after settle frames

#### Scenario: Shift-press reset clears the encoder's linked gesture
- **WHEN** a parameter has an active gesture linked to it (gesture target set, gesture weight positioned anywhere — down, middle, or up), no gesture is selected, and the performer shift-presses the encoder
- **THEN** the linked gesture is deactivated for the current track and the published gesturesAffecting bitmask is empty after settle frames
- **AND** the published value and `GetValue` return to the ungestured base value within the slew bound, regardless of where the gesture-weight analog input sits
- **AND** subsequently sweeping that gesture-weight analog input no longer moves the encoder output

#### Scenario: Shift-press with a gesture selected deactivates the gesture, not the modulators
- **WHEN** a parameter has both an active modulation slot and an active gesture, the gesture is currently selected, and the performer shift-presses the encoder
- **THEN** the selected gesture is deactivated for the current scene(s) while the modulation slot remains active
- **AND** the published gesturesAffecting bitmask drops the gesture's bit and the published value reflects the still-active modulation after settle frames

### Requirement: Gesture Removal Invalidates the Recompute Cache
When a gesture is deleted by shift-pressing its gesture-selector pad (`GestureSelectorCell::OnPress` with `SceneManager::m_shift` true → `ClearGesture`), the system SHALL deactivate that gesture for the current scene(s) across all tracks, recompute the affecting bitmasks (`SetAllModulatorsAffecting`), AND set force-update on the affected cells so each encoder that had been driven by that gesture is recomputed back to its ungestured value on the next control frame. The corrected output SHALL be independent of the gesture-weight analog-input position held at deletion time: deleting the gesture returns the encoder to its base (post-remaining-modulation) value whether the analog input is down, middle, or up.

#### Scenario: Deleting a gesture restores the ungestured value
- **WHEN** an encoder is driven away from its base value by an active gesture and the performer shift-presses that gesture's selector pad to delete it
- **THEN** the encoder's published gesturesAffecting bit for that gesture clears after settle frames
- **AND** the encoder's published value and `GetValue` return to the ungestured base value within the slew bound (the value is NOT left frozen at the prior gestured value)

#### Scenario: Gesture deletion is correct at every analog-input position
- **WHEN** the same gesture-delete is performed three times from fresh setups with the gesture-weight analog input held at 0 (down), 0.5 (middle), and 1 (up) respectively
- **THEN** in all three cases the encoder returns to the same ungestured base value after settle frames
- **AND** in all three cases a subsequent sweep of the gesture-weight analog input has no effect on the encoder output

#### Scenario: Gesture deletion leaves other modulation intact
- **WHEN** an encoder has both an active gesture and an active modulation slot and the gesture is deleted via shift-press on its selector pad
- **THEN** the gesture's contribution is removed and the encoder's value reflects only the remaining modulation after settle frames
- **AND** the published modulatorsAffecting bitmask still includes the surviving modulation slot

### Requirement: Theory Of Time Loop Selector Parameter
The encoder parameter system SHALL expose a new six-position switch-valued parameter on the Theory of Time global bank for selecting the external sync loop. The parameter SHALL use the same switch-valued encoder pattern as `SampleLoopIndex`, SHALL serialize through the named encoder JSON path, and SHALL publish switch metadata through `EncoderBankUIState`. The fully counterclockwise switch position SHALL select the master loop, with switch values mapping to loop indexes as `TheoryOfTimeBase::x_numLoops - switchVal - 1`.

#### Scenario: Fully counterclockwise selects master loop
- **WHEN** the Theory of Time loop selector parameter is at switch value 0
- **THEN** the effective loop selection is `TheoryOfTimeBase::x_masterLoop`

#### Scenario: Other switch values select other loops
- **WHEN** the Theory of Time loop selector parameter is at switch value 3
- **THEN** the effective loop selection is `TheoryOfTimeBase::x_numLoops - 3 - 1`

#### Scenario: Default selects upper-middle switch value
- **WHEN** the encoder parameter system initializes the Theory of Time loop selector parameter from defaults
- **THEN** the selector publishes switch value 3
- **AND** the effective loop selection is `TheoryOfTimeBase::x_numLoops - 3 - 1`

#### Scenario: Switch metadata is published
- **WHEN** the Theory of Time bank is selected
- **THEN** the loop selector encoder cell is connected
- **AND** `EncoderBankUIState` reports switch values for that cell

#### Scenario: Loop selection survives patch round trip
- **WHEN** the loop selector parameter is set away from its default and the patch is saved then loaded
- **THEN** the named encoder JSON path restores that loop selector parameter value
