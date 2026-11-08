## MODIFIED Requirements

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


### Requirement: Change-Driven Evaluation Every Host Frame
The system SHALL run `EncoderBankBank::Process` once per engine sample, before encoder values are applied to DSP inputs. Every call SHALL run `ModulatorValues::ProcessSample()` for every bank mode, filtering every slot's raw value (`m_value`) through a second-order Butterworth `DownsamplerGeneric` at ratio `SampleTimer::x_oversample` (a passthrough at ratio 1) and clamping its matrix input (`m_hostValue`) to [0, 1]. Modulator amplitudes SHALL NOT be decimated.

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
- **THEN** that slot's bit is set in the bank mode's changed-modulator bitmask
- **AND** every affected named root evaluates the changed value in that matrix pass, using either the prepared or recursive path

#### Scenario: Settled scene refresh takes effect immediately
- **WHEN** scene assignment or endpoint activation changes while a parameter tree has no moving targets
- **THEN** the next host evaluation uses the new scene values and dependency masks
- **AND** the tree may return to prepared evaluation after that refresh without waiting out the conservative convergence budget

#### Scenario: Clearing modulation invalidates an empty dependency mask
- **WHEN** a modulated parameter is reset and its modulation targets become neutral
- **THEN** the affected tree continues advancing and evaluating until those targets are reached even if sources remain unchanged
- **AND** once the reset has settled, its output is the configured default and the next UI publication reports the corrected values and masks
