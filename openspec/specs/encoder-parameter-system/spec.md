# Encoder Parameter System Specification

## Purpose
The encoder parameter system (`private/src/Encoder.hpp`, `private/src/EncoderBank.hpp`, `private/src/EncoderBankBank.hpp`) is the software-defined knob layer for Smart Grid One. Every synthesis parameter is a `BankedEncoderCell` owned by a global `EncoderBankBank` and placed into 4×4 encoder bank grids. Voice parameters have distinct Water, Fire, and Earth cells; Quad and Global parameters have one cell. Each cell stores a normalized base value per scene, accepts up to 15 routable modulation slots whose depths are themselves full encoder cells, supports 16 gesture (macro) targets, morphs between two active scenes (see scene-state-management), and publishes per-voice outputs to the DSP engine through a parameter slew filter and to the UI through `EncoderBankUIState`. Modulation sources such as the PolyXFader LFOs (see polyxfader-lfos) and AHD envelopes (see ahd-envelopes) are external DSP components that write into shared per-bank-mode `ModulatorValues`; the encoder cells consume those values but do not own the sources.
## Requirements
### Requirement: Constructor-Bound Scene Manager
`EncoderBankBank` SHALL receive `SmartGridOneContext*` in its constructor and use that context for every `CreateEncoder` call. `SmartGridOneEncoders` SHALL receive the context in its constructor and use its fixed three-trio, three-voices-per-trio layout and initialize its modes, banks, and named parameters before construction completes. `SquiggleBoyWithEncoderBank` SHALL pass its constructor-supplied context through this chain. The context SHALL supply the shared scene manager and parameter event logger and outlive its encoder consumers.

#### Scenario: Constructed encoder system has initialized parameters
- **WHEN** `SmartGridOneEncoders(context)` finishes construction
- **THEN** its named parameters and banks are initialized using the supplied context
- **AND** callers can process and query the encoder system without first calling `Init(sceneManager, ...)`

#### Scenario: New parameter uses the bank owner's shared context
- **WHEN** an encoder is created through `EncoderBankBank::CreateEncoder`
- **THEN** it receives the context supplied to the bank owner at construction, without a separate per-call context argument

### Requirement: Independent Trio Cells with Per-Scene Base Storage
Every cell SHALL store eight normalized scene values (`m_values[scene]`), one banked base value, and one scene activation flag per gesture (`m_isActive[scene]`). Voice parameters SHALL have distinct Water, Fire, and Earth cells and independent child trees. All voices of a cell SHALL share its base and gesture target values; modulation SHALL compute independent outputs per local voice.

The owner index SHALL be `EncoderIndex(param, trio) = param * 3 + trio`. Voice parameters SHALL occupy all three slots. Quad and Global parameters SHALL occupy only the trio-zero slot. Voice modes SHALL have three channels each, Quad four, and Global one.

#### Scenario: Trio base values are independent
- **WHEN** Water, Fire, and Earth cells for a Voice parameter hold 0.2, 0.6, and 0.8 with no modulation
- **THEN** parameter reads for voices 0–2, 3–5, and 6–8 return 0.2, 0.6, and 0.8 respectively
- **AND** editing Water leaves Fire and Earth scene values and child trees unchanged

#### Scenario: Scene blend reads stored state rather than computed output
- **WHEN** a cell stores 0.2 and 0.8 in the active scenes and the blend factor is 0.25
- **THEN** its banked base becomes 0.35 even if its previous modulated output differs
- **AND** setting, loading, or copying scene values SHALL NOT substitute the derived cell's post-modulation UI value for the stored base blend

### Requirement: Bank Families Route to the Selected Trio
The encoder owner SHALL contain twelve Voice banks (four families with Water, Fire, and Earth instances), four Quad banks, and four Global banks, using five modes. `SetTrack(trio)` SHALL select that trio's instance of the active Voice-bank family; on a Quad or Global bank it SHALL only remember the trio for future Voice-bank selection. Controller bank selectors SHALL continue to represent four Voice families. Visualizers SHALL recognize every trio instance of a family.

#### Scenario: Track change preserves the Voice-bank family
- **WHEN** Filter and Amp is selected for Water and the performer selects Fire
- **THEN** the selected bank is FilterAndAmpFire and its UI channels 0–2 represent voices 3–5
- **AND** the Filter and Amp visualizer and modulation controls remain available

#### Scenario: Global selection preserves a pending trio choice
- **WHEN** Theory of Time is selected and the performer selects Earth
- **THEN** Theory of Time remains selected
- **AND** subsequently selecting Source opens SourceEarth

#### Scenario: A bank reset is local to its trio
- **WHEN** the performer resets the selected Water Source bank
- **THEN** only placed cells in SourceWater revert for the active scenes
- **AND** Fire, Earth, and inactive scenes retain their stored values

### Requirement: Blend-Proportional Encoder Increments
When a physical encoder is turned at an intermediate blend factor t (0 < t < 1), the system SHALL distribute the increment across both active scenes' stored values for the current track, adding `delta × (1 − t)` to the scene-1 value and `delta × t` to the scene-2 value, so, without clamping, the blended output changes by `delta × ((1 − t)² + t²)`.
If one scene's value would leave [0, 1], that value is clamped and the other scene's value is solved so the blended output still lands on the clamped target. At t == 0 or t == 1 only the single fully-active scene's value changes.

#### Scenario: Mid-blend turn updates both scenes
- **WHEN** the blend factor is 0.5, a track's stored values are 0.4 (scene 1) and 0.6 (scene 2), and the encoder receives an increment of +0.2
- **THEN** the scene-1 value becomes 0.5 and the scene-2 value becomes 0.7
- **AND** the blended output rises from 0.5 to 0.6

#### Scenario: Clamped scene is compensated
- **WHEN** the blend factor is 0.5 and an increment would push the scene-2 value above 1
- **THEN** the scene-2 value is clamped to 1 and the scene-1 value is recomputed so the blended output equals the clamped overall target

### Requirement: Fifteen Modulation Slots with Nested Depth Cells
Every `BankedEncoderCell` SHALL expose up to 15 modulation slots (`x_numModulators == 15`); each slot's depth is itself a full `BankedEncoderCell`, so modulation depths can in turn be modulated and gesture-controlled to arbitrary nesting depth.
Pressing a connected encoder enters selection mode: the 4×4 grid switches to show the selected cell's 15 depth cells plus the selected cell itself at position (3, 3). Depth cells whose normalized values are neutral (0.5) everywhere, with no active sub-modulators or gestures, are garbage-collected when the selection is closed.

#### Scenario: Selecting a parameter exposes its depth cells
- **WHEN** the performer presses a connected base parameter encoder (without shift)
- **THEN** the visible grid is repopulated with that parameter's 15 modulation depth cells and the parameter itself at (3, 3)
- **AND** each depth cell reports through `EncoderUIState::GetConnected` whether a modulation source is wired to its slot in the current bank mode

#### Scenario: Zeroed depth cell is garbage-collected
- **WHEN** a depth cell's normalized value is 0.5 in every scene, it has no active sub-modulators or gestures, and the selection is deselected
- **THEN** the depth cell is released back to the encoder pool and excluded from serialization

### Requirement: Crossfade Modulation Mixing per Voice
Depth cells SHALL be bipolar: normalized position `u` exposes signed knob position `b = 2u − 1`, with effective depth `sign(b) × (9^abs(b) − 1) / 8`. The parent SHALL apply this curve once per route after recursive depth computation. For each local voice, let `d` be curved depth times source amplitude, `W = Σ abs(d)`, `V = Σ d × sourceValue`, and `O = Σ max(0, −d)`. The normalized output SHALL be `base × max(0, 1 − W) + (V + O) / max(1, W)`. Negative depths crossfade toward the inverted source. Changes to source amplitude alone SHALL invalidate the cached output.

Source values and amplitudes SHALL use local channel indexes in the cell's mode. Water, Fire, and Earth SHALL have independent source arrays. The DSP producer SHALL route each trio's envelope, LFO, and sheaf channels to its corresponding mode.

#### Scenario: Full-depth modulation overrides the base
- **WHEN** one slot has source value 0.8, amplitude 1, and signed depth position 1
- **THEN** that channel's output is 0.8 regardless of the base
- **AND** the modulation extent is [0, 1]

#### Scenario: Negative depth inverts the source
- **WHEN** the signed depth position is -1, source value is 0.8, and amplitude is 1
- **THEN** that channel's output is 0.2

#### Scenario: Partial modulation uses the curved depth
- **WHEN** the base is 0.3, signed depth position is 0.5, source value is 0.8, and amplitude is 1
- **THEN** effective depth is 0.25 and output is 0.425
- **AND** the modulation extent is [0.225, 0.475]

#### Scenario: Per-voice polyphonic outputs from a shared base
- **WHEN** a per-voice modulation source (such as a voice AHD envelope) holds different values for the three voices of a track
- **THEN** `EncoderBankUIState::GetValue(i, j, k)` reads a different post-modulation value for each voice channel k of that track while the base value remains shared

### Requirement: Sixteen Gesture Targets with Constant-Output Editing
Every parameter SHALL support 16 gesture parameters (`x_numGestureParams == 16`), each pairing a hidden target-state `BankedEncoderCell` with a live weight in [0, 1] supplied through `ModulatorValues::m_gestureWeights` from physical analog inputs. For each track the post-gesture value is the weight-normalized blend `Σᵢ wᵢ × (base × (1 − wᵢ) + targetᵢ × wᵢ) / Σᵢ wᵢ` over gestures active for that track and scene; with no active gestures it is the base value.
Gesture activation is stored per scene in each independent cell (`m_isActive[scene]`), and the effective weight is the scene-blended activation times the live weight. Turning an encoder while gestures are active splits the physical delta between the gesture targets (proportional to w²) and the base (proportional to w(1 − w)), normalized by the weight sum, so the audible output tracks the physical motion. A shift-press while gestures are selected deactivates those gestures for the current scene(s) instead of zeroing modulators.

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
The system SHALL serialize all encoder state through `EncoderBankBank::ToJSON`/`FromJSON`, which iterate the flat owner array of named encoder cells; each cell writes eight flat per-scene base values in `values.values`, its non-null modulation depth cells (recursively), its gesture target cells, and—for gesture cells—the eight per-scene activation flags in `active`.
Voice root names SHALL append Water, Fire, or Earth (for example `Harmonics1Fire`); Quad and Global names SHALL remain unsuffixed. Patch JSON SHALL carry top-level integer `version: 1`. Missing or explicit zero
SHALL be treated as legacy version 0 and upgraded in memory on the message
thread before a load is published to audio. Opening SHALL NOT rewrite the disk
file; the converted form SHALL be written on a normal save. Negative, future, noninteger, and
null versions SHALL be rejected. Duplicate top-level `version` or
`squiggleBoy` keys SHALL be rejected. A version 1 object SHALL be passed through.
The version 0 upgrade SHALL split known Voice roots into Water, Fire, and Earth,
selecting track indexes 0, 1, and 2 from every scene row recursively through
modulators and gestures. Gesture activation SHALL select
`active[scene * 16 + trio]`. Shared roots SHALL retain their name and select
track zero. Missing entries SHALL become zero or false; absent fields SHALL
remain absent. Unknown roots and fields SHALL survive. Already suffixed Voice
roots and already scalar shared values or eight-entry shared activation arrays
SHALL be preserved during the transition. Ambiguous collisions
between legacy and already suffixed Voice roots SHALL be rejected. Recording
container version 5 is independent of patch version.
Loading looks each parameter up by name, rebuilds depth and gesture cells from the JSON, garbage-collects empty cells, recomputes affecting bitmasks, and sets force-update. Banks deselect any open modulator selection before loading so visible grid pointers never dangle.

#### Scenario: JSON round-trip restores modulation topology
- **WHEN** a patch is saved with a parameter whose base is 0.7 in scene 3, with modulation slot 6 at depth 0.4 and gesture 2 active on that cell in scene 0
- **THEN** loading that JSON restores the scene-3 base to 0.7, recreates the slot-6 depth cell with value 0.4, recreates the gesture-2 target cell, and restores its scene-0 activation flag

#### Scenario: Unknown parameters are skipped
- **WHEN** the JSON being loaded lacks an entry for a named encoder
- **THEN** that encoder keeps its current state and loading continues with the remaining parameters

### Requirement: Encoder JSON Float Load Accepts All Numeric Spellings
When loading named encoder state from JSON, the system SHALL restore per-scene knob values, converting bipolar signed values back to normalized storage, from JSON numeric values regardless of whether the token is represented internally as an integer node or a real node. A saved encoder base value of `1.0` MUST NOT become `0.0` solely because the JSON text spells the value as `1`.
This applies to base parameter cells, modulation depth cells, and gesture target cells because all are serialized through the same `StateEncoderCell` value arrays.

#### Scenario: Whole-number base value restores as one
- **WHEN** a named encoder cell is loaded from JSON whose `values` array contains the token `1`
- **THEN** the corresponding stored base value is restored as `1.0`
- **AND** the cell's computed unslewed output is `1.0` after load processing

#### Scenario: Fractional base value still restores
- **WHEN** a named encoder cell is loaded from JSON whose `values` array contains the token `0.5`
- **THEN** the corresponding stored base value is restored as `0.5`

#### Scenario: Nested modulation depth whole-number restores
- **WHEN** a saved modulation depth cell contains a whole-number knob value in its `values` array
- **THEN** loading the patch restores that depth value numerically instead of converting it to zero

### Requirement: UI State Publication Through Atomics
The system SHALL publish the selected bank's visible 4×4 grid to an `EncoderBankUIState` of lock-free atomics every UI population pass: per cell the post-modulation output per channel (`GetValue(i, j, k)`), the modulation extent (`GetMinValue`/`GetMaxValue`), brightness, connectedness (`GetConnected`), color, short name, switch values, and the cell's affecting-modulator and affecting-gesture bitmasks; plus the selected bank's local voice count. The UI SHALL use local channels from zero with no track offset.
Brightness encodes modulation takeover (1 minus local channel zero's absolute modulation weight, clamped to [0, 1]); during the scene blinker's off phase, base parameters with no selected gestures additionally dim by their gesture weight sum so gesture-captured knobs blink. Disconnected cells publish connected == false, brightness 0, and zeroed values.

#### Scenario: Empty grid position reads disconnected
- **WHEN** a grid position holds no connected cell (for example after a machine change placed nullptr there)
- **THEN** `GetConnected(i, j)` returns false and `GetBrightness(i, j)` returns 0
- **AND** `GetValue(i, j, k)` returns 0 for every channel k

#### Scenario: Modulated cell dims and reports extent
- **WHEN** local channel zero's absolute modulation weight on a visible cell is 0.75
- **THEN** `GetBrightness` for that cell reads 0.25
- **AND** `GetMinValue`/`GetMaxValue` bracket the channel outputs so the UI can draw the modulation arc

### Requirement: Shift-Press Reset Semantics
When the centralized shift flag is held and a connected encoder is pressed (`EncoderBankInternal::HandlePress` with `SceneManager::m_shift` true → `BankedEncoderCell::HandleShiftPress`), the system SHALL reset that cell's modulation and gesture configuration for the current track and SHALL invalidate the change-driven recompute cache so the reset is reflected in both the published value and the affecting bitmasks.
The reset has two branches: when no gestures are selected it neutralizes all modulation depth slots of that cell for the active scenes AND deactivates every gesture linked to that cell for the current track (`ZeroModulators`, which calls `DeactivateGesture` per gesture and garbage-collects the emptied cells), returning the encoder to its fully ungestured, unmodulated base value; when one or more gestures are selected it instead deactivates only each selected gesture for the current scene(s) (`DeactivateGestureCurrentScene`), leaving modulators intact. In both branches the cell SHALL recompute its affecting-modulator and affecting-gesture bitmasks from the root (`SetModulatorsAffectingRecursive`) AND set its force-update flag (`SetForceUpdateRecursive`), so the next control frame recomputes the output and the `EncoderBankUIState` masks and value agree.

#### Scenario: Shift-press reset of a modulated encoder restores the base value and clears the mask
- **WHEN** a base parameter at base value 0.3 has a modulation slot whose depth drives its output away from 0.3, no gestures are selected, and the performer shift-presses that encoder
- **THEN** after a few control frames the cell's published `EncoderBankUIState` modulatorsAffecting bitmask is empty
- **AND** the cell's published value and `GetValue` converge to the base value 0.3 within the parameter-slew bound (the value is NOT left frozen at the prior modulated value)

#### Scenario: Shift-press reset of nested modulation clears the whole subtree
- **WHEN** a base parameter has a modulation depth cell that is itself modulated (nested depth) and the performer shift-presses the base encoder with no gestures selected
- **THEN** the depth subtree is neutralized for the active scenes and cells with no remaining scene state are garbage-collected; the base value and inactive scenes are preserved
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

### Requirement: Stored Encoder Assignments Emit Parameter Events
While recording, encoder value assignments routed through `SetAndRecordValue` SHALL capture the root parameter name (including the trio suffix for Voice parameters), scene, complete tagged child path, and value converted to patch JSON units. Gesture activation through `SetActive` SHALL emit EncoderActivate; when activation inherits a parent value, the copied value SHALL also emit EncoderSet. Event capture SHALL occur before smoothing or modulation and SHALL NOT allocate on the audio thread. Only gesture nodes SHALL emit activation events. Format-v5 encoder events SHALL omit the track byte; the root name supplies trio identity. Replay SHALL address flat scene arrays for new patches.

#### Scenario: Bipolar nested parameter edit
- **WHEN** a bipolar modulator or gesture stores normalized value 0.75 during recording
- **THEN** EncoderSet contains patch value 0.5 and identifies every modulator/gesture hop from the root

#### Scenario: Scene copy traverses a base encoder
- **WHEN** a scene-copy operation visits base and modulator nodes as well as gestures
- **THEN** stored values emit assignments and only gesture nodes emit activation events
- **AND** no activation assertion is triggered for a base or modulator node
