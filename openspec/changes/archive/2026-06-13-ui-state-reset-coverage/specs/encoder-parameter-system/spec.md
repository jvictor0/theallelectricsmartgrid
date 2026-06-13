## MODIFIED Requirements

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

## ADDED Requirements

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
