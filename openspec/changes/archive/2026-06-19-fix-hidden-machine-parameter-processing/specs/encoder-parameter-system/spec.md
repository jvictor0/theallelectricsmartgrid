## ADDED Requirements

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
