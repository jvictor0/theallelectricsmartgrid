## MODIFIED Requirements

### Requirement: One Gang Per Processor
Each `GangedRandomLFO<VoiceCount>` SHALL own exactly `VoiceCount` persistent voices and SHALL NOT contain a runtime gang count or runtime gang size. Callers needing multiple gangs SHALL own multiple processor objects.

#### Scenario: Voice-bank topology
- **WHEN** the instrument constructs its random modulators
- **THEN** each of the three tracks owns ten independent three-lane processors, with two assigned to each of its five groups

#### Scenario: Quad-bank topology
- **WHEN** the instrument constructs its random modulators
- **THEN** it owns eight independent four-lane processors, with two assigned to each of its four quad groups

### Requirement: Standard Random Presets
Each Voice and Quad modulation group SHALL receive an independent pair configured with Standard(12, 0.1) and Standard(24, 0.3). Global random slots 2 and 3 SHALL retain their Standard(8, 0.2) and Standard(16, 0.1) presets. These are fixed DSP configuration and SHALL NOT add performer parameters.

#### Scenario: Per-group presets
- **WHEN** a Voice or Quad group is initialized
- **THEN** slot 2 uses the group's 12-second random and slot 3 uses its 24-second random
- **AND** other groups do not share those processors

### Requirement: Random Modulator Routing
Voice and Quad random outputs SHALL feed slots 2 and 3 of their assigned groups, leaving slots 0 and 1 empty. Each random source SHALL have correctly routed history, glyph/color metadata, and an encoder visualizer. Global sources SHALL retain their existing slots. DualWaveShapingVCO SHALL own two additional internal random generators for wavetable blending and replacement visibility, using Standard(6, 0.1) for both, without receiving track random inputs through source parameter wiring.

#### Scenario: Independent modulation and wavetable morphing
- **WHEN** a parameter group uses its random pair
- **THEN** both random sources have their own state and history
- **AND** changing track modulation random state does not change the VCO's internal morph state or wavetable replacement visibility checks
