## MODIFIED Requirements

### Requirement: Change-Driven Evaluation Every Host Frame
The system SHALL run `EncoderBankBank::Process` once per engine sample, before encoder values are applied to DSP inputs. Every call SHALL advance the global ModulatorManager's per-source second-order Butterworth filters at ratio SampleTimer::x_oversample (passthrough at ratio 1), clamping filtered source values to [0, 1]. Amplitudes SHALL NOT be decimated. On host frames, the manager SHALL gather source values and amplitudes into each fixed group's ModulatorValues before change detection. Encoder roots SHALL bind directly to their assigned group's values independently of bank mode; nested encoders SHALL inherit that binding.

On host frames only, `ProcessHostFrame` SHALL detect changed source values, amplitudes and gestures, handle the scene-manager flags, and evaluate all named roots, including hidden ones, through `EncoderMatrix`. Outputs SHALL reflect the current relevant inputs and advancing target ramps at that host boundary. The evaluator MAY reuse an output when its relevant inputs and stored values are unchanged; it need not traverse every editable cell or invoke recursive `Compute` on every named root.

The current evaluator SHALL preserve the same modulation and gesture arithmetic in its recursive and prepared paths. Settled simple routes cache scalar depths, while nested routes and gestures use a child-before-parent graph. An active force-update countdown, a revision mismatch, or a root awaiting preparation SHALL take the recursive path. Before editing or exposing deferred child values, `SynchronizePreparedState` SHALL materialize the last evaluated state without advancing a ramp or changing the root's output history. DSP cache reads SHALL NOT evaluate the graph.

An interior blend change (`m_changed` without `m_changedScene`) SHALL refresh every root's evaluation for that pass without requiring a topology rebuild. Scene-index changes and transitions to or from blend endpoints SHALL refresh the scene-dependent values and affecting masks. A settled tree needs one fresh host evaluation; trees with moving values SHALL continue advancing those values through completion.

Changes outside the normal source-input path, including reset, gesture deletion, absolute set and JSON load, SHALL invalidate affected outputs even if all their dependency masks become empty. Target changes set a conservative `TickDistributor::x_maxSamplesToDistribute` progress budget (15001 host frames), and edit/reset callers propagate invalidation to ancestors. Advancing recursive computation updates only moving scenes and refreshes dependency masks at the start and completion. A cell SHALL retire early after its own controls and relevant descendants settle, computing and publishing the final value before returning to prepared evaluation. `SetStateRecursive` uses a one-frame countdown for a settled scene refresh and the full window otherwise. Correctness requires timely reevaluation and ramp completion, rather than waiting out a progress budget after settlement.

#### Scenario: Unaffected parameter reuses its output
- **WHEN** a parameter's relevant source, amplitude and gesture inputs, scene blend, stored values and configuration are unchanged, and no target ramp remains active
- **THEN** the evaluator may reuse its latest output
- **AND** the current host frame's output caches still contain the correct published values

#### Scenario: Source change recomputes routed parameters
- **WHEN** a PolyXFader LFO routed to a parameter produces a changed decimated value on a host frame
- **THEN** that slot's bit is set in the group's changed-modulator bitmask
- **AND** every affected named root evaluates the changed value in that matrix pass, using either the prepared or recursive path

#### Scenario: Settled scene refresh takes effect immediately
- **WHEN** scene assignment or endpoint activation changes while a parameter tree has no moving targets
- **THEN** the next host evaluation uses the new scene values and dependency masks
- **AND** the tree may return to prepared evaluation after that refresh without waiting out the conservative convergence budget

#### Scenario: Clearing modulation invalidates an empty dependency mask
- **WHEN** a modulated parameter is reset and its modulation targets become neutral
- **THEN** the affected tree continues advancing and evaluating until those targets are reached even if sources remain unchanged
- **AND** once the reset has settled, its output is the configured default and the next UI publication reports the corrected values and masks

### Requirement: Hidden Machine-Dependent Parameters Remain Semantically Live
Machine-dependent parameters SHALL remain wired to their bank-mode context and assigned modulation group and participate in semantic parameter operations even when the selected track's machine topology hides them from the visible 4x4 bank. Hiding a parameter from the current bank view SHALL affect only controller/UI placement; it MUST NOT prevent the parameter from recomputing DSP outputs, receiving gesture/modulator change edges, clearing gestures, reverting defaults, copying scenes, or publishing correct values when read by parameter index.

#### Scenario: Hidden parameter catches gesture weight changes
- **WHEN** a Voice-bank parameter is exposed for Water's source machine, hidden when Earth's source machine is selected, and has gesture 2 controlling its Spread modulation depth
- **AND** gesture 2's live weight changes while Earth is selected
- **THEN** the hidden Water parameter recomputes on the next host-frame matrix pass
- **AND** reading the Water parameter by parameter index returns the value corresponding to the new gesture weight when Water is selected again
- **AND** Earth's visible bank view may still publish that grid position as disconnected or zero because Earth does not expose the parameter

#### Scenario: Hidden parameter catches modulation source changes
- **WHEN** a Voice-bank parameter is hidden by the selected track's machine topology but has an active modulation slot whose source changes on an engine sample
- **THEN** the hidden parameter consumes the current decimated source on the next host-frame matrix pass
- **AND** DSP reads by parameter index observe the updated per-voice output even though the parameter is absent from the visible bank view

#### Scenario: Clearing a gesture reaches hidden parameters
- **WHEN** a gesture is active on a machine-dependent parameter that is hidden by the currently selected track's machine topology
- **AND** the performer clears that gesture through the gesture selector
- **THEN** the hidden parameter deactivates that gesture for the affected scene and track scope
- **AND** subsequent sweeps of that gesture's live weight no longer affect the hidden parameter's output

#### Scenario: Full patch default reset reaches hidden parameters
- **WHEN** a machine-dependent parameter has a non-default base value, active modulation depth, or active gesture while hidden by the selected track's machine topology
- **AND** an operation reverts the full patch scope to defaults
- **THEN** the hidden parameter's base value, modulation depth subtree, gesture state, value distribution, and computed output are reset according to the same rules as visible parameters in that scope
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
