## MODIFIED Requirements

### Requirement: Delay UI State Observability
The system SHALL publish per-channel damping filter alphas and head positions, and expose a common scope of the magnitude of each actual completed delay-line write. Forward full display cycles SHALL record scope starts; backward travel SHALL undo scope boundaries using the generic scope history. The UI SHALL draw this envelope through the shared inverse in warped coordinates, converting filtered magnitude to dB. Separate min/max envelope buckets and the positional waveform recorder SHALL be removed. Signed delay-return scopes used for spectra SHALL retain their existing signal.

#### Scenario: Head positions track the tape heads
- **WHEN** the delay publishes its head positions
- **THEN** the markers and waveform use the same display-cycle coordinates

#### Scenario: Stopped recording remains visible across the whole display
- **WHEN** audio is recorded while transport is stopped
- **THEN** the free-running continuous write head records full display-cycle starts and the envelope remains readable on that axis

#### Scenario: Restart preserves the recorded waveform
- **WHEN** transport restarts after the delay has been free-running
- **THEN** the existing display offset and continuous coordinate preserve the recorded waveform and write marker
- **AND** the read markers retain the existing resynchronization behavior

#### Scenario: Envelope reflects recorded audio
- **WHEN** audio with a distinct amplitude profile is written into the delay
- **THEN** its magnitude is recorded in the common scope and filtered by the existing LOD pipeline before dB display conversion
