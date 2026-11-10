## MODIFIED Requirements

### Requirement: Delay UI State Observability
The system SHALL publish per-channel damping filter alphas and continuous master-phase head positions, and expose a common scope of the magnitude of each actual completed delay-line write. Its periodic axis SHALL have origin zero and period one master cycle, without start/end events. The UI SHALL sample through the shared phase-to-real inverse, convert filtered magnitude to dB, and draw symmetrically from negative to positive level about each lane center. Separate min/max envelope buckets and the positional waveform recorder SHALL be removed. Signed delay-return scopes used for spectra SHALL retain their existing signal.

#### Scenario: Head positions track the tape heads
- **WHEN** the delay publishes its head positions
- **THEN** markers and waveform use the same explicit master-cycle axis

#### Scenario: Stopped recording remains visible across the whole display
- **WHEN** audio is recorded while transport is stopped
- **THEN** continuous phase advances at nominal tempo and the display retains its full period even before the first cycle completes

#### Scenario: Restart preserves the recorded waveform
- **WHEN** transport restarts with fixed read speed, delay and loop selection
- **THEN** a phase offset absorbs the musical reset without moving the write or read coordinates
- **AND** existing waveform coordinates retain their meaning

#### Scenario: Envelope reflects recorded audio
- **WHEN** audio with a distinct amplitude profile is written into the delay
- **THEN** its magnitude passes through the existing LOD pipeline before symmetric dB display conversion
