## ADDED Requirements

### Requirement: Streaming Vector FIR Decimation
The system SHALL provide FIRDownsamplerGeneric in ButterworthFilter.hpp, parameterized by number type, vector width, odd tap count, and integer decimation ratio. It SHALL filter with symmetric normalized coefficients, emit one vector every ratio inputs, expose the input-sample group delay, and reset its state without allocation. Ratio one SHALL be a passthrough.

#### Scenario: Channel isolation and delay
- **WHEN** distinct signals pass through a multichannel decimator
- **THEN** each output equals filtering and decimating that channel independently, with the declared common delay

#### Scenario: Anti-alias filtering
- **WHEN** input includes frequencies above the reduced sample rate's Nyquist limit
- **THEN** the FIR attenuates those frequencies before decimation

#### Scenario: Reset
- **WHEN** the decimator resets
- **THEN** its next output follows the original emission cadence with cleared filter history
