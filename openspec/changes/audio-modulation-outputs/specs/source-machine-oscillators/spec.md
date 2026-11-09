## ADDED Requirements

### Requirement: Paired source modulation outputs
The generic source interface SHALL own two per-voice modulation outputs and pass them to source processing. Dual waveshaping SHALL publish the individual VPS outputs before crossfade/bit reduction, mapped from bipolar audio into the modulation domain without clipping. Physical modeling SHALL publish filtered noise before its envelope and sample-rate reducer with the same mapping, and the internal AHD output as a unipolar second output. Sample and Thru SHALL write zero placeholders on every call. Exposing these outputs SHALL not change source audio processing.

#### Scenario: Dual source taps
- **WHEN** the dual source processes a sample
- **THEN** the two modulation outputs match its individual VPS outputs independently of oscillator mix and bit reduction

#### Scenario: Physical source taps
- **WHEN** the physical source processes a sample
- **THEN** output one represents pre-envelope SVF noise and output two represents its internal AHD output
- **AND** the AHD advances only once per engine sample

#### Scenario: Placeholder source switch
- **WHEN** a voice switches to Sample or Thru
- **THEN** both source modulation outputs are zero and cannot retain the preceding machine's values
