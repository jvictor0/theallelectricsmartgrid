# Modulator Manager Specification

## Purpose
Define global published modulation sources and fixed instrument-level groups while preserving local encoder slot evaluation and patch semantics. DSP generators retain their current ownership.

## Requirements

### Requirement: Global source snapshots and fixed groups
The instrument SHALL define globally unique modulator indices, fixed enum-addressed groups with 15 slots, and parameter-to-group assignments through ForEach metadata. The initial groups SHALL be Water, Fire, Earth, Quad, and Global with their existing source-slot mappings. All source values and amplitudes SHALL use QuadFloat. Groups and assignments SHALL be instrument metadata, constant across patches. Child encoders SHALL inherit their parent's group, independently of bank mode.

#### Scenario: Reusing a source across groups
- **WHEN** two group slots bind the same global source
- **THEN** both receive the same filtered source value and amplitude
- **AND** their parameters read their own group's local slot without a per-route global lookup

#### Scenario: Existing patch loading
- **WHEN** an existing patch with nested depth slot 8 is loaded
- **THEN** slot 8 refers to slot 8 of the parameter's statically assigned group
- **AND** patch loading and serialization remain unchanged

#### Scenario: Unassigned legacy slots
- **WHEN** an existing patch uses a depth in an unassigned group slot
- **THEN** loading preserves that depth and its existing connection behavior
- **AND** the instrument supplies the legacy zero source value and unit amplitude for that slot

### Requirement: Source filtering and group publication
The manager SHALL filter each source on every engine sample using the existing second-order Butterworth decimator and clamp its filtered value to [0, 1]. Amplitudes SHALL remain unfiltered. On each matrix sample the manager SHALL gather raw values, filtered values, and amplitudes into group snapshots before change detection. DSP generators SHALL retain their current owners and processing order.

#### Scenario: Shared filtered source
- **WHEN** a source changes between host samples and is assigned to several groups
- **THEN** its filter advances once per engine sample regardless of group count
- **AND** each group receives its current snapshot at the next matrix sample

#### Scenario: Gesture and blend propagation
- **WHEN** gesture inputs, selection, or the scene blend change
- **THEN** every group receives the change and affected parameters retain existing evaluation behavior
