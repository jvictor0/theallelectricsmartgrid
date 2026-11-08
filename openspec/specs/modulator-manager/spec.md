# Modulator Manager Specification

## Purpose
Define global published modulation sources and fixed instrument-level groups while preserving local encoder slot evaluation and patch semantics. DSP generators retain their current ownership.

## Requirements

### Requirement: Global source snapshots and fixed groups
The instrument SHALL define globally unique modulator indices, fixed enum-addressed groups with 15 slots, and parameter-to-group assignments through ForEach metadata. Each track SHALL have Source, Shape, PanMelody, LFO, and Sub groups with three active lanes, for 15 track groups total. Quad SHALL have Delay, Reverb, PartialMachine, and LFO groups with four active lanes. The Global group SHALL retain its existing bindings. Parameters SHALL follow their pages; voice mixer gain and sends SHALL use Shape. Voice and Quad groups SHALL have empty slots 0/1 and independent ganged random pairs in slots 2/3, preserving other source slots. All source values and amplitudes SHALL use QuadFloat. Groups and assignments SHALL be instrument metadata, constant across patches. Child encoders SHALL inherit their parent's group, independently of bank mode.

#### Scenario: Reusing a source across groups
- **WHEN** two group slots bind the same global source
- **THEN** both receive the same host-frame source value and amplitude
- **AND** their parameters read their own group's local slot without a per-route global lookup

#### Scenario: Compatible source lane counts
- **WHEN** a parameter is assigned a modulation group
- **THEN** every assigned source in that group SHALL have the same active lane count as the parameter
- **AND** a configuration test SHALL reject mismatches, while source storage remains QuadFloat

#### Scenario: Existing patch loading
- **WHEN** an existing patch with nested depth slot 8 is loaded
- **THEN** slot 8 refers to slot 8 of the parameter's statically assigned group
- **AND** patch loading and serialization remain unchanged

#### Scenario: Unassigned legacy slots
- **WHEN** an existing patch uses a depth in an unassigned group slot
- **THEN** loading preserves that depth and its existing connection behavior
- **AND** the instrument supplies the legacy zero source value and unit amplitude for that slot

### Requirement: Source filtering and group publication
Each global source SHALL declare its downsampling requirement in ForEach metadata. The manager SHALL filter sources requiring downsampling on every engine sample using the existing second-order Butterworth decimator and clamp their filtered values to [0, 1]. Envelopes and waveform LFOs SHALL require downsampling. Ganged random, sheaf, spread, and noise SHALL bypass downsampling and publish their current raw value on each host frame. Modulation noise SHALL be generated only on host samples. Amplitudes SHALL remain unfiltered. On each matrix sample the manager SHALL gather raw values, filtered values, and amplitudes into group snapshots before change detection. Published modulation sources SHALL retain their DSP owners and processing order; the manager SHALL own their downsampling and group publication.

#### Scenario: Shared filtered source
- **WHEN** a source requiring downsampling changes between host samples and is assigned to several groups
- **THEN** its filter advances once per engine sample regardless of group count
- **AND** each group receives its current snapshot at the next matrix sample

#### Scenario: Gesture and blend propagation
- **WHEN** gesture inputs, selection, or the scene blend change
- **THEN** every group receives the change and affected parameters retain existing evaluation behavior

#### Scenario: Direct host-frame publication
- **WHEN** a source is declared not to require downsampling
- **THEN** its group values equal the source value sampled on the host frame with no decimator smoothing
- **AND** modulation noise is held unchanged between host samples
