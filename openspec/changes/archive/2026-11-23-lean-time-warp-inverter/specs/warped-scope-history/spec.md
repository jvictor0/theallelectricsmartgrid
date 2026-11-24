## ADDED Requirements

### Requirement: Executable Lean inverse

The Theory of Time inverse SHALL use generated native operations for the Lean read and write rules. The Lean proof audit SHALL connect the executable rules to the certified-cell phase bound and the bounded coherent batch model. The native implementation SHALL preserve the fixed payloads, revision protocol, and sample-time lookup interface.

#### Scenario: Inverse validation

- **WHEN** the proof and generation checks run
- **THEN** Lean checks the executable inverse contracts and regeneration matches the checked-in header
- **AND** matched native traces return the same timestamps and missing results as the C++ reference

#### Scenario: Concurrent real-time lookup

- **WHEN** the writer crosses several grid boundaries while a reader queries history
- **THEN** the writer commits bounded batches from preallocated storage and a successful lookup uses one coherent revision
