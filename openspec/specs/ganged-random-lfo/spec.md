# Ganged Random LFO Specification

## Purpose
The ganged random LFOs provide free-running correlated modulation for voice, quad, global, wavetable, and partial-machine destinations. Each `GangedRandomLFO<VoiceCount>` owns one fixed-size gang and exposes a predictive snapshot for the UI.

## Requirements

### Requirement: One Gang Per Processor
Each `GangedRandomLFO<VoiceCount>` SHALL own exactly `VoiceCount` persistent voices and SHALL NOT contain a runtime gang count or runtime gang size. Callers needing multiple gangs SHALL own multiple processor objects.

#### Scenario: Voice-bank topology
- **WHEN** `SquiggleBoy` constructs its standard random modulators
- **THEN** each of the four modulators owns three `GangedRandomLFO<3>` processors, one for each three-voice track

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

### Requirement: Standard Random Presets
The four Voice-bank random modulators SHALL use waiting means `W = 1, 4, 12, 32` seconds with waiting sigma `0.3W`, waiting internal sigma `0.2/W`, moving mean `W/2`, moving sigma `0.15W`, moving internal sigma `0.4/W`, and target internal sigma `0.1, 0.3, 0.2, 0.1` respectively.

Quad and Global random slots 2 and 3 SHALL use the 8-second and 16-second presets respectively. These values are fixed DSP configuration and SHALL NOT add performer parameters.

### Requirement: Coherent Predictive UI State
Each displayed gang SHALL publish sample rate, elapsed round samples, and each voice's state, progress, source, target, output, shape, waiting increment, and moving increment through an odd/even revision transaction. Readers SHALL retry a bounded number of times and reject snapshots observed during or across a write.

#### Scenario: Predictive rendering
- **WHEN** a coherent snapshot is available
- **THEN** the visualizer draws every voice on one longest-round time axis
- **AND** elapsed motion is solid, predicted motion is dashed, the current position is marked by a dot, and early voices hold their targets to the shared endpoint

#### Scenario: Unstable snapshot
- **WHEN** the visualizer cannot read a coherent snapshot
- **THEN** it draws only its background and center axis

### Requirement: Existing Routing Is Preserved
Voice random outputs SHALL continue to feed modulator slots 0 through 3. Quad and Global random outputs SHALL continue to feed slots 2 and 3. Voice random outputs 2 and 3 SHALL also continue driving the two Dual Wave Shaping VCO wavetable blends and replacement visibility checks.
