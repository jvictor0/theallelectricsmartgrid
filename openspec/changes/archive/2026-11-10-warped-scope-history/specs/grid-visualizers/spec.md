## MODIFIED Requirements

### Requirement: Published UIState Data Sources
Visualizers SHALL read display data through accepted UI state and explicitly shared atomic payloads. Waveform views SHALL sample ScopeWriter histories through accepted periodic axes or event markers; bound LFO, envelope, and delay-magnitude views SHALL use the captured inverse view specified in warped-scope-history. Analyzers SHALL use accepted level-zero windows and UIState transfer-function overlays. Sample waveform buckets and spectral atoms SHALL retain their derived snapshot paths. Meters SHALL use published meter readers. The capture and publication semantics are specified in ui-visualization-pipeline.

#### Scenario: Scope views use accepted axes
- **WHEN** a ScopeComponent renders a voice's trace
- **THEN** it uses the explicit period and origin for periodic streams or recorded start/end indices for event streams, and reads only accepted sample history

#### Scenario: Analyzer overlay uses transfer-function values
- **WHEN** an analyzer draws a filter response overlay for a voice
- **THEN** each curve point equals the UIState transfer function's `FrequencyResponse(freq)` value for the sampled frequency, not a value queried from the audio-thread DSP object

## ADDED Requirements

### Requirement: Coordinate-independent scope cursor duration
Periodic LFO and Theory of Time cursor visibility SHALL use the nominal sweep duration: the published axis period in master cycles divided by the published unwarped tempo in cycles per second. Their indicators SHALL appear only when this duration exceeds 0.1 seconds and the cursor value is available. Event-based scopes SHALL retain their historical-duration rule. Longest modulation-source selection and LOD selection SHALL continue to use mapped real history. Logical cycle width SHALL NOT be interpreted as a sample count.

#### Scenario: Slow phase-domain LFO
- **WHEN** an LFO's nominal sweep duration exceeds the existing 0.1-second cursor threshold
- **THEN** the cursor is drawn at its published phase position
- **AND** changing the inverse history span alone SHALL NOT change indicator eligibility

#### Scenario: Fast nominal sweep with older history
- **WHEN** a periodic scope's nominal sweep is at most 0.1 seconds and a warp branch selects older history
- **THEN** the indicator remains hidden even when the mapped history spans more than 0.1 seconds

### Requirement: Unwarped time-warp visualization
The Theory of Time visualizer SHALL use an explicit periodic unmodulated-phase axis with an accepted inverse. It SHALL record bounded warp displacement and add the unmodulated musical coordinate when drawing. Each plotted value SHALL use one inverse lookup; its cursor SHALL reuse its already-read value. Missing history, vertical wraps and the join between current and previous cycles SHALL begin a new path rather than draw a connecting segment.

#### Scenario: Warp reversal
- **WHEN** modulated phase reverses while unmodulated phase advances
- **THEN** the curve shows the reversal on the forward unmodulated axis
- **AND** positive and negative vertical wraps have no connecting line
