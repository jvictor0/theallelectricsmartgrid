## ADDED Requirements

### Requirement: Shared Pan-Phase Azimuth Offset
The system SHALL set the Partial Machine synthesis azimuth offset from the same shared pan phase that drives voice Lissajous panning. The offset value SHALL be the current `SquiggleBoy::m_panPhase.m_phase` value after the pan phase advances for the audio sample, so Partial Machine atom placement rotates in phase with the Lissajous pan motion.

#### Scenario: Partial Machine offset follows the Lissajous pan phase
- **WHEN** `SquiggleBoy::ProcessSample` advances the shared pan phase for an audio sample
- **THEN** the Partial Machine synthesis context receives that same phase value as `m_azimuthOffset`
- **AND** voice pan inputs for that sample are driven from the same phase value

#### Scenario: Existing frequency azimuth mapping is preserved
- **WHEN** the Partial Machine computes an atom azimuth
- **THEN** the frequency-derived azimuth factor and linear frequency mapping are still added before wrapping to [0, 1)
- **AND** the shared pan-phase offset only rotates the resulting spectral orbit
