## MODIFIED Requirements

### Requirement: Published UIState Data Sources
Visualizers SHALL read display data through accepted UI state and explicitly shared atomic payloads. Waveform views SHALL sample ScopeWriter histories through accepted cycle markers; bound LFO, envelope, and delay-magnitude views SHALL use the captured inverse view specified in warped-scope-history. Analyzers SHALL use accepted level-zero windows and UIState transfer-function overlays. Sample waveform buckets and spectral atoms SHALL retain their derived snapshot paths. Meters SHALL use published meter readers. The capture and publication semantics are specified in ui-visualization-pipeline.

#### Scenario: Scope views segment by cycle markers
- **WHEN** a ScopeComponent renders a voice's trace
- **THEN** the samples it reads lie between cycle start/end indices recorded by the ScopeWriter and at or before the writer's atomically published index

#### Scenario: Analyzer overlay uses transfer-function values
- **WHEN** an analyzer draws a filter response overlay for a voice
- **THEN** each curve point equals the UIState transfer function's `FrequencyResponse(freq)` value for the sampled frequency, not a value queried from the audio-thread DSP object
