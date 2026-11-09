# Encoder Parameter System Specification

## Purpose
The encoder parameter system (`private/src/Encoder.hpp`, `private/src/EncoderBank.hpp`, `private/src/EncoderBankBank.hpp`, `private/src/EncoderMatrix.hpp`) is the software-defined knob layer for Smart Grid One. Every synthesis parameter is a `BankedEncoderCell` owned by a global `EncoderBankBank` and placed into 4×4 encoder bank grids. Voice parameters have distinct Water, Fire, and Earth cells; Quad and Global parameters have one cell. Each cell stores a normalized base value per scene, accepts up to 15 routable modulation slots whose depths are themselves full encoder cells, supports 16 gesture (macro) targets, morphs between two active scenes (see scene-state-management), and publishes per-voice outputs that the DSP engine reads every sample through `GetValue` and the UI reads through `EncoderBankUIState`. Modulation sources such as the PolyXFader LFOs (see polyxfader-lfos) and AHD envelopes (see ahd-envelopes) are external DSP components that publish globally indexed values to `ModulatorManager`. Fixed instrument-defined groups gather those sources into 15-slot `ModulatorValues`; parameter trees bind directly to their assigned group, independently of bank mode. Group assignments are constant across patches, and patch storage and loading retain their existing local-slot meaning.
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
Every cell SHALL store eight normalized scene targets (`m_targets[scene]`), eight current scene values (`m_values[scene]`) that ramp toward those targets, one banked base value, and one scene activation flag per gesture (`m_isActive[scene]`). Voice parameters SHALL have distinct Water, Fire, and Earth cells and independent child trees. All voices of a cell SHALL share its base and gesture target values; modulation SHALL compute independent outputs per local voice.

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

### Requirement: Normalized Parameter, Page, and Slot Catalogues
The system SHALL define parameters in `ForEachSmartGridOneParam.hpp` as `F(name, shortName, mode, defaultValue, description, color, switchValues, bipolar)`, page memberships in `ForEachSmartGridOnePage.hpp` as `F(page, param, x, y)`, and slot routes in `ForEachSmartGridOneSlot.hpp` as `F(slot, enumtype, enumval, page, color)`. Parameter rows SHALL own value and persistence metadata without machine masks or unique page coordinates. Page membership SHALL permit a parameter to appear on multiple alternative pages without creating another parameter cell. Typed source and singleton route rows SHALL select the page and color for each logical slot.

`SmartGridOneLayout::BuildPageAndSlotTables` SHALL validate catalogue references, page coordinates, route coverage and machine types, and one logical owner slot per parameter at compile time. It SHALL derive page and slot instances, Voice trio replication, slot modes, direct machine-value routes, affected-slot lists, and parameter owner slots. `SmartGridOneEncoders::Bank` numeric values SHALL preserve the established 20-bank layout through derived slot-instance offsets.

#### Scenario: Shared source parameter appears on alternative pages
- **WHEN** `FilterDrive` is a member of several source pages and the selected source machine changes
- **THEN** the routed Source grid shows the membership and coordinates of the selected page
- **AND** all alternatives refer to the same named parameter cell for that trio

#### Scenario: Machine change binds only affected slots
- **WHEN** a source machine changes for one trio
- **THEN** the typed route selects its source page and color without scanning the full parameter catalogue
- **AND** the other trios and unrelated singleton slots retain their bound pages and cell identities
- **AND** a changed page closes the previous modulation selection before replacing its sixteen grid positions

### Requirement: Blend-Proportional Encoder Increments
When a physical encoder is turned without Shift at an intermediate blend factor t (0 < t < 1), the system SHALL distribute the increment across both active scenes' stored values for the current track, adding `delta × (1 − t)` to the scene-1 value and `delta × t` to the scene-2 value, so, without clamping, the blended output changes by `delta × ((1 − t)² + t²)`.
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
Source skins SHALL be authored in `ForEachSmartGridOneModulator.hpp` as `F(enumType, enumValue, mode, color, glyph)` using explicit Voice, Quad, and Global enums with preserved numeric indices. Voice skins SHALL be shared across all trios. Missing skins SHALL be Off/None. Global SHALL define only random sources at indices 2 and 3 and noise at 14; removing skins at 6, 7, and 11 SHALL preserve stored depths and the fifteen-position allocation, connection, and processing contract.

Pressing a connected encoder enters selection mode: the 4×4 grid switches to show the selected cell's 15 depth cells plus the selected cell itself at position (3, 3). Depth cells whose normalized values are neutral (0.5) everywhere, with no active sub-modulators or gestures, are garbage-collected when the selection is closed.

#### Scenario: Selecting a parameter exposes its depth cells
- **WHEN** the performer presses a connected base parameter encoder (without shift)
- **THEN** the visible grid is repopulated with that parameter's 15 modulation depth cells and the parameter itself at (3, 3)
- **AND** each depth cell retains the generic connection contract through `EncoderUIState::GetConnected`; an Off/None skin SHALL NOT disconnect its position or gate modulation computation

#### Scenario: Zeroed depth cell is garbage-collected
- **WHEN** a depth cell's normalized value is 0.5 in every scene, it has no active sub-modulators or gestures, and the selection is deselected
- **THEN** the depth cell is released back to the encoder pool and excluded from serialization

### Requirement: Shift-Turn Adjusts Existing Modulation Depths Recursively
Holding the centralized scene-manager Shift flag while turning an encoder SHALL adjust every existing modulation-depth descendant of the addressed cell, leaving that cell's own base and gesture targets unchanged. Both legacy `EncoderIncDec` and exponential-relative `EncoderExponentialDelta` inputs SHALL support this operation. Missing modulation slots SHALL remain absent. Each descendant SHALL be visited once even when dependency refreshes reorder the active-modulator list. The operation SHALL NOT garbage-collect cells referenced by an open modulation page; normal deselection cleanup remains responsible for collecting neutral depths.

For every edited stored value, counterclockwise motion SHALL reduce its magnitude toward zero, and clockwise motion SHALL increase its magnitude while preserving its sign. A step reaching or crossing zero SHALL clamp exactly to zero. Values already at zero SHALL stay zero in either direction. For bipolar depth cells, zero is normalized `0.5`; outward motion SHALL clamp at normalized `0` or `1`. Ordinary turns without Shift SHALL retain signed zero crossing, and absolute encoder-set messages SHALL continue to edit the addressed base even while Shift is held.

The operation SHALL preserve normal gesture selection and live-weight edit distribution at each depth. Selected gestures SHALL be activated on existing descendants using the ordinary inherited target value; weighted edits SHALL then use the magnitude rule independently for each base or gesture target. Only active scenes SHALL change. At blend `t`, scene shares SHALL remain `delta × (1 − t)` and `delta × t`, with each scene clamping independently and no compensation into the other scene. Edited cells and their ancestors SHALL refresh dependency masks and force recomputation so published values reflect the adjustment even when no modulation source changes.

#### Scenario: Both polarities move toward zero
- **WHEN** a parent has signed depth positions `+0.5` and `-0.5`, and Shift-counterclockwise supplies normalized delta `-0.125`
- **THEN** those positions become `+0.25` and `-0.25`
- **AND** the parent's own stored value, other parameter trees, and inactive scenes remain unchanged

#### Scenario: Zero remains neutral and clockwise preserves polarity
- **WHEN** Shift-counterclockwise overshoots zero on positive and negative depths
- **THEN** both clamp to signed zero
- **AND** further shifted turns in either direction leave them zero
- **AND** Shift-clockwise on other nonzero depths increases their magnitudes without changing sign, up to full depth

#### Scenario: A neutralized sibling does not skip later descendants
- **WHEN** the first depth reaches zero during a recursive adjustment and dependency refresh reorders active slots
- **THEN** every remaining sibling and nested descendant receives the edit exactly once
- **AND** missing slots remain absent

#### Scenario: Scene clamping is independent
- **WHEN** a depth stores normalized `0.55` and `0.2` in the active scenes, blend is `0.25`, and normalized delta is `-0.4`
- **THEN** the scene values become `0.5` and `0.3`
- **AND** the first scene's zero clamp does not increase the second scene's adjustment

#### Scenario: Held gesture edits its target
- **WHEN** a depth has a single active gesture held at full weight and the parent receives a shifted turn
- **THEN** the depth's gesture target moves according to its own polarity and zero clamp
- **AND** that depth's stored base stays unchanged, so releasing the gesture can restore its prior base depth

#### Scenario: Open modulation page remains usable after neutralization
- **WHEN** the performer shift-turns the selected parent at `(3, 3)` until all its descendants are neutral
- **THEN** the existing visible depth cells remain connected with valid pointers and report neutral values
- **AND** the parent's output returns to its value without those modulation contributions
- **AND** a subsequent ordinary depth turn can establish a new signed amount
- **AND** neutral cells with no retained scene or gesture state may be collected when the page is deselected

#### Scenario: Ordinary edits retain their previous semantics
- **WHEN** a parameter is turned without Shift or receives an absolute encoder-set message
- **THEN** it edits the addressed value instead of recursively adjusting descendants
- **AND** ordinary scene compensation occurs only when a candidate leaves the normalized range, not when it lands exactly on an endpoint

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

### Requirement: Interpolated Per-Sample Output Caches with Source-Side Smoothing
The matrix SHALL write root outputs through `SetOutput(output, hostSample)`, stamped with `SampleTimer::GetHostSample()`, preserving the output from before the first write of each host frame in `m_outputPrev`. Every host pass SHALL publish `x_oversample` caches. For an output written in the current host frame, slot `s` SHALL contain `ToValue(m_output + (m_outputPrev - m_output) × (x_oversample - 1 - s) / x_oversample)`. Other outputs SHALL publish their latest value in every slot. Thus interpolation reaches the latest output on the last engine sample, and ratio 1 publishes the latest output directly.

`SmartGridOneEncoders::Process` SHALL select the current sub-sample cache before DSP parameter reads. Its `GetValue` overloads SHALL read that cache, mapping Voice parameters to trio `voice / 3` and local channel `voice % 3`, with direct channels for Quad and Global parameters. The cell-level `GetValue(channel)` helper SHALL provide equivalent interpolation for direct callers. Bipolar conversion SHALL occur once at publication or direct read. Matrix calculations and UI publication SHALL use latest values without output interpolation; deferred visible child values SHALL be synchronized before UI publication.

Smoothing SHALL happen where values enter the encoder system:
- Stored edits: `SetTarget` SHALL retain a per-scene target and preserve the ongoing filter state. Moving scene values SHALL advance at host rate through a one-pole with a 20 ms time constant, using `alpha = 1-exp(-1/(0.020*hostSampleRate))`. Internal filter state SHALL use double precision. After a filter step, a normalized residual at most `1e-6` SHALL snap both the internal and published scene value exactly to the target. New edits SHALL accumulate against targets without changing the coefficient or restarting filter state. Patch JSON SHALL save targets. Settled scene values SHALL perform no further pole arithmetic; after the final output/dependency refresh, a tree with no moving descendants SHALL return to the cached evaluation path. Live modulation SHALL bypass this control filter.
- Scene blend and faders: blend and fader parameter-set messages SHALL set `SettlingSlew` targets that `ProcessInputSlews` advances every sample with a 500 Hz one-pole low-pass, before the scene manager and encoders run, stopping once the value settles on its target. Restoring a blend factor or saved fader positions SHALL snap the corresponding slew to the restored value.

#### Scenario: Encoder edit follows a fixed response
- **WHEN** a settled scene changes from 0.25 to 0.35
- **THEN** after 20 ms its value is approximately `0.35 - 0.10/e`
- **AND** it lands exactly on 0.35 once the residual reaches the settling threshold

#### Scenario: Burst edits preserve filter state
- **WHEN** a small correction arrives 1 ms after a larger edit
- **THEN** the target incorporates the correction and the filter keeps its current state and fixed coefficient
- **AND** the correction does not compress unfinished movement into a new linear ramp

#### Scenario: Settled controls stop smoothing
- **WHEN** all moving values in a control tree reach their targets
- **THEN** the final values are published and the tree becomes eligible for cached evaluation
- **AND** subsequent host frames perform no pole arithmetic for those settled values

#### Scenario: Modulation retains its bandwidth
- **WHEN** an audio-rate source modulates a parameter with settled manual controls
- **THEN** the source continues through the existing matrix and output interpolation without the manual-control one-pole

#### Scenario: DSP reads follow the computed output
- **WHEN** a cell's output changes during a matrix pass at ratio 1
- **THEN** a `GetValue` call later in the same engine sample returns the new output, without an exponential approach from the previous output

#### Scenario: DSP reads interpolate across a host frame
- **WHEN** the oversampling ratio is 4 and a matrix pass moves a cell's output from 0.2 to 1.0
- **THEN** `GetValue` returns 0.4, 0.6, 0.8, and 1.0 on the four engine samples of that host frame

### Requirement: Named JSON Serialization via the Global Owner
The system SHALL serialize all encoder state through `EncoderBankBank::ToJSON`/`FromJSON`, which iterate the flat owner array of named encoder cells; each cell writes eight flat per-scene target values in `values.values`, its non-null modulation depth cells (recursively), its gesture target cells, and—for gesture cells—the eight per-scene activation flags in `active`.
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
Loading SHALL look each parameter up by name, synchronize any deferred prepared state, reuse depth and gesture cells at matching serialized slot paths, create missing paths, remove paths absent from the patch, garbage-collect empty cells, recompute affecting bitmasks, and set force-update. Matching cells SHALL retain current values and pending ramp timing when the saved targets are unchanged; changed targets SHALL use the ordinary ramp from current values. New paths SHALL ramp from their constructor neutral. The loader SHALL compare targets in the saved value domain before decoding, so an identical bipolar float round-trip does not reset ramp timing. Banks deselect any open modulator selection before loading so visible grid pointers never dangle.

Reloading an identical settled patch under unchanged scene/modulator/gesture inputs SHALL preserve the evaluated output and every published engine-sample cache value. Changes to topology or activation flags in a different patch are outside that guarantee. The [formal patch-load model](../../../formal/Encoder/README.md#patch-load-coverage) retains the original replacement counterexample and proves continuity for the fixed logical transition.

#### Scenario: JSON round-trip restores modulation topology
- **WHEN** a patch is saved with a parameter whose base is 0.7 in scene 3, with modulation slot 6 at depth 0.4 and gesture 2 active on that cell in scene 0
- **THEN** loading that JSON restores the scene-3 base target to 0.7, reuses or creates the slot-6 depth cell with target 0.4 and the gesture-2 target cell, and restores its scene-0 activation flag
- **AND** advancing the required ramps reaches those stored target values

#### Scenario: Identical modulated reload is continuous
- **GIVEN** a settled parameter with base 0.3, a source fixed at 1, and full positive modulation depth
- **WHEN** its saved patch is loaded again without changing the external inputs
- **THEN** its output stays at 1 from the first published engine sample, rather than falling toward 0.3 and recovering
- **AND** matching modulation and gesture cells retain their current values and ramp timing

#### Scenario: Unknown parameters are skipped
- **WHEN** the JSON being loaded lacks an entry for a named encoder
- **THEN** that encoder keeps its current state and loading continues with the remaining parameters

### Requirement: Encoder JSON Float Load Accepts All Numeric Spellings
When loading named encoder state from JSON, the system SHALL restore per-scene knob values, converting bipolar signed values back to normalized storage, from JSON numeric values regardless of whether the token is represented internally as an integer node or a real node. A saved encoder base value of `1.0` MUST NOT become `0.0` solely because the JSON text spells the value as `1`.
This applies to base parameter cells, modulation depth cells, and gesture target cells because all are serialized through the same `StateEncoderCell` value arrays.

#### Scenario: Whole-number base value restores as one
- **WHEN** a named encoder cell is loaded from JSON whose `values` array contains the token `1`
- **THEN** the corresponding stored base value is restored as `1.0`
- **AND** an otherwise unmodulated cell's computed output (`GetValue`) is `1.0` once its load target has settled

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
When Shift is held and a connected encoder is pressed (`EncoderBankInternal::HandlePress` → `BankedEncoderCell::HandleShiftPress`), the system SHALL apply the reset branch selected by the current gesture selection and invalidate affected outputs and masks.

With no gestures selected, `RevertToDefault(false)` SHALL set the pressed cell's active-scene targets to its configured default, recursively neutralize its modulation depth targets, and deactivate linked gestures throughout that subtree for the active scenes. The current scene values SHALL ramp to their reset targets; inactive-scene targets and activations SHALL be preserved. Neutral cells may be garbage-collected when they meet the existing collection conditions. A modulator amount's default is its normalized neutral position, 0.5.

With one or more gestures selected, the system SHALL deactivate only those gestures linked directly to the pressed cell for the active scenes. This branch SHALL preserve the cell's base targets, modulation descendants and unselected gestures. Both branches SHALL refresh affecting masks from the root and force the affected output to be evaluated on subsequent host frames, including when deletion empties its dependency masks.

#### Scenario: Shift-press restores the configured default
- **WHEN** a base parameter has configured default 0.3, current target 0.7 and active modulation, no gestures are selected, and the performer shift-presses it
- **THEN** its active-scene base targets become 0.3 and its modulation depth targets become neutral
- **AND** after the reset ramps settle its modulation mask is empty and its published value is 0.3

#### Scenario: Shift-press clears nested modulation
- **WHEN** a base parameter has nested modulation and is shift-pressed with no gestures selected
- **THEN** all depth descendants are neutralized and linked gestures throughout the subtree are deactivated for the active scenes
- **AND** the root reaches its configured default while inactive-scene targets and activation flags remain unchanged

#### Scenario: Shift-press clears the encoder's linked gesture
- **WHEN** a parameter has an active gesture, no gesture is selected, and the performer shift-presses the encoder
- **THEN** that gesture is deactivated for the active scenes
- **AND** the reset output reaches the configured default regardless of the gesture-weight input
- **AND** subsequently sweeping that gesture weight does not affect the reset scenes

#### Scenario: Shift-press with a selected gesture preserves modulation
- **WHEN** a parameter has an active modulation slot and a selected active gesture and the performer shift-presses it
- **THEN** the selected local gesture is deactivated for the active scenes while the base targets and modulation subtree remain unchanged
- **AND** the next host evaluation and UI publication reflect the remaining modulation and gestures

### Requirement: Gesture Removal Invalidates the Recompute Cache
When a gesture is deleted by shift-pressing its gesture-selector pad (`GestureSelectorCell::OnPress` with `SceneManager::m_shift` true → `ClearGesture`), the system SHALL deactivate that gesture for the current scene(s) across all tracks, recompute the affecting bitmasks (`SetAllModulatorsAffecting`), AND start the force-update countdown on the affected cells so each encoder that had been driven by that gesture is recomputed back to its ungestured value by the following `EncoderBankBank::Process` passes. The corrected output SHALL be independent of the gesture-weight analog-input position held at deletion time: deleting the gesture returns the encoder to its base (post-remaining-modulation) value whether the analog input is down, middle, or up.

#### Scenario: Deleting a gesture restores the ungestured value
- **WHEN** an encoder is driven away from its base value by an active gesture and the performer shift-presses that gesture's selector pad to delete it
- **THEN** the encoder's published gesturesAffecting bit for that gesture clears once the force-update countdown completes
- **AND** the encoder's published value and `GetValue` return to the ungestured base value by then (the value is NOT left frozen at the prior gestured value)

#### Scenario: Gesture deletion is correct at every analog-input position
- **WHEN** the same gesture-delete is performed three times from fresh setups with the gesture-weight analog input held at 0 (down), 0.5 (middle), and 1 (up) respectively
- **THEN** in all three cases the encoder returns to the same ungestured base value once the force-update countdown completes
- **AND** in all three cases a subsequent sweep of the gesture-weight analog input has no effect on the encoder output

#### Scenario: Gesture deletion leaves other modulation intact
- **WHEN** an encoder has both an active gesture and an active modulation slot and the gesture is deleted via shift-press on its selector pad
- **THEN** the gesture's contribution is removed and the encoder's value reflects only the remaining modulation once the force-update countdown completes
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

### Requirement: Final parameter output bounds
Every top-level parameter SHALL clamp its normalized computed output to [0,1] once per host sample before interpolation and bipolar conversion. Child encoder outputs SHALL retain their existing behavior. Source excursions SHALL contribute unchanged whenever the final parameter retains headroom.

#### Scenario: Saturation after evaluation
- **WHEN** a simple, graph, or editing path computes a finite parameter outside [0,1]
- **THEN** its published output and interpolated engine-rate cache remain within the parameter's unipolar or bipolar bounds
- **AND** previous output history contains the preceding clamped host output

#### Scenario: Headroom preserves modulation
- **WHEN** an out-of-range source at a small signed depth produces an in-range parameter
- **THEN** the final output preserves that contribution without source clipping
