# Ganged Random LFO Specification

## Purpose
The ganged random LFOs provide free-running correlated modulation for voice, quad, global, and wavetable destinations. Each `GangedRandomLFO<VoiceCount>` owns one fixed-size gang and exposes a predictive snapshot for the UI.

## Requirements

### Requirement: One Gang Per Processor
Each `GangedRandomLFO<VoiceCount>` SHALL own exactly `VoiceCount` persistent voices and SHALL NOT contain a runtime gang count or runtime gang size. Callers needing multiple gangs SHALL own multiple processor objects.

#### Scenario: Voice-bank topology
- **WHEN** the instrument constructs its random modulators
- **THEN** each of the three tracks owns ten independent three-lane processors, with two assigned to each of its five groups

#### Scenario: Quad-bank topology
- **WHEN** the instrument constructs its random modulators
- **THEN** it owns eight independent four-lane processors, with two assigned to each of its four quad groups

### Requirement: Wait Move Done Voice State
Each voice SHALL progress through `Waiting`, `Moving`, and `Done`. Waiting holds the source value, Moving interpolates from source to target with a blend of linear and raised-cosine progress, and Done holds the target.

#### Scenario: Discarded boundary remainder
- **WHEN** a waiting or moving increment reaches or exceeds one
- **THEN** the voice changes state, resets progress at the wait-to-move boundary, and does not carry overshoot into the next state

### Requirement: Correlated Round Sampling
At the start of a round, the processor SHALL independently sample correlated waiting and moving increments. Each timing configuration SHALL sample one normally distributed center duration, convert it to a center rate, and sample each voice rate around that center using the configured internal rate sigma. Increments SHALL be positive and floored to a maximum duration of one hour.

The processor SHALL then sample one shared uniform target center, normally distributed per-voice targets clamped to `[0, 1]`, and one uniform shape per voice.

#### Scenario: Slowest voice gates the next round
- **WHEN** one voice reaches Done before another voice
- **THEN** it holds its target until every voice is Done
- **AND** only then does the processor sample and reset the complete gang for the next round

### Requirement: Randomized Startup
On the first valid processing sample, the processor SHALL independently draw a correlated source state and a correlated target state using the normal target distribution. It SHALL sample the first round's timing and shapes normally, then choose one shared uniform sample position within that round, including waiting, moving, and completed-voice holds through the final 32-sample frame. Subsequent rounds SHALL start from the preceding targets as usual.

This removes the initial zero state and synchronized round start. It approximates stationary startup by randomizing the position within an ordinarily sampled round; it does not length-weight the first round's timing distribution.

#### Scenario: Startup within an existing round
- **WHEN** the initial random position falls inside a wait, move, or completed-voice hold
- **THEN** every voice starts at that same elapsed round time between its sampled source and target
- **AND** audio uses the enclosing 32-sample interpolation frame
- **AND** history and predictive snapshots report the same elapsed time and output without inventing negative sample timestamps

### Requirement: Standard Random Presets
Each Voice and Quad modulation group SHALL receive an independent pair configured with Standard(12, 0.1) and Standard(24, 0.3). Global random slots 2 and 3 SHALL retain their Standard(8, 0.2) and Standard(16, 0.1) presets. These are fixed DSP configuration and SHALL NOT add performer parameters.

#### Scenario: Per-group presets
- **WHEN** a Voice or Quad group is initialized
- **THEN** slot 2 uses the group's 12-second random and slot 3 uses its 24-second random
- **AND** other groups do not share those processors

### Requirement: Coherent Predictive UI State
Each displayed gang SHALL publish sample rate, elapsed round samples, and each voice's state, progress, source, target, output, shape, waiting increment, and moving increment through an odd/even revision transaction. Readers SHALL retry a bounded number of times and reject snapshots observed during or across a write.

#### Scenario: Predictive rendering
- **WHEN** a coherent snapshot is available
- **THEN** the visualizer draws every voice on one longest-round time axis
- **AND** elapsed motion is solid, predicted motion is dashed, the current position is marked by a dot, and early voices hold their targets to the shared endpoint

#### Scenario: Unstable snapshot
- **WHEN** the visualizer cannot read a coherent snapshot
- **THEN** it draws only its background and center axis

### Requirement: Random Modulator Routing
Voice and Quad random outputs SHALL feed slots 2 and 3 of their assigned groups, with other slots assigned by the instrument group metadata. Each random source SHALL have correctly routed history, glyph/color metadata, and an encoder visualizer. Global sources SHALL retain their existing slots. DualWaveShapingVCO SHALL own two additional internal random generators for wavetable blending and replacement visibility, using Standard(6, 0.1) for both, without receiving track random inputs through source parameter wiring.

#### Scenario: Independent modulation and wavetable morphing
- **WHEN** a parameter group uses its random pair
- **THEN** both random sources have their own state and history
- **AND** changing track modulation random state does not change the VCO's internal morph state or wavetable replacement visibility checks
