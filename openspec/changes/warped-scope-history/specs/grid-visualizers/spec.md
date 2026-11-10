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
Scope cursor visibility and longest modulation-source selection SHALL use real duration measured through the accepted inverse for bound axes, and real duration directly for unbound axes. Logical cycle width SHALL NOT be interpreted as a sample count.

#### Scenario: Slow phase-domain LFO
- **WHEN** an LFO's mapped display duration exceeds the existing 0.1-second cursor threshold
- **THEN** the cursor is drawn at its published phase position
- **AND** the same logical period with a shorter real duration remains subject to the existing threshold

### Requirement: Unwarped time-warp visualization
The Theory of Time visualizer SHALL use an explicit periodic unmodulated-phase axis with an accepted inverse. It SHALL record bounded warp displacement and add the unmodulated musical coordinate when drawing. Each plotted value SHALL use one inverse lookup; its cursor SHALL reuse its already-read value. Missing history, vertical wraps and the join between current and previous cycles SHALL begin a new path rather than draw a connecting segment.

#### Scenario: Warp reversal
- **WHEN** modulated phase reverses while unmodulated phase advances
- **THEN** the curve shows the reversal on the forward unmodulated axis
- **AND** positive and negative vertical wraps have no connecting line
