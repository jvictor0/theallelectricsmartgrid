## ADDED Requirements

### Requirement: External Clock Boundary
The Nonagon sequencer SHALL remain ignorant of MIDI realtime routing and clock synchronization internals. The surrounding SquiggleBoy integration SHALL convert visible realtime clock events into per-sample clock tick status, update the owned clock synchronizer, choose the effective Theory of Time tempo, and then provide normal Nonagon inputs before processing the sample.

#### Scenario: Clock signal stays outside Nonagon logic
- **WHEN** a MIDI clock event becomes visible on an audio sample
- **THEN** the SquiggleBoy integration updates its owned synchronizer before setting Nonagon inputs
- **AND** `TheNonagonInternal::Process` runs with normal timebase inputs and no direct MIDI clock dependency

### Requirement: Per-Sample Tick Status
The system SHALL clear the sample clock tick status after it has been consumed for the current sample, so a single MIDI clock event cannot be processed as multiple ticks on subsequent samples.

#### Scenario: Tick is consumed once
- **WHEN** one visible MIDI clock event is consumed on sample N
- **THEN** the synchronizer receives true for sample N
- **AND** it receives false on sample N + 1 unless another clock event is visible
