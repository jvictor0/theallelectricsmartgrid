# Warped Scope History Specification

## Purpose

Share the Theory of Time inverse between cycle-aware scopes, modulation overlays, and the delay magnitude display, with generic reversible boundaries and bounded concurrent reads.

## Requirements

### Requirement: Shared warped scope reads
Bound scopes SHALL construct cycle axes in the continuous Theory of Time write-head coordinate and read real-time scope history through the chosen inverse. Scope origins, decimation, and existing LOD delays SHALL be respected. Unavailable inverse or sample history SHALL produce an unavailable point. Modulation overlays SHALL use the same mapped real timestamp as their scope axis.

#### Scenario: Nonuniform forward time
- **WHEN** two real-time intervals cover different warped distances
- **THEN** scope points and overlays occupy their warped positions while reading the corresponding real samples

### Requirement: Reversible scope boundaries
Scope markers SHALL retain real and warped positions. Backward travel SHALL undo every crossed end and start, reopening paired starts and restoring prior interval state. Backward start callbacks, including envelope triggers, SHALL NOT append a scope start. Existing starts at unchanged coordinates SHALL remain permitted. Transport phase reset SHALL NOT itself be interpreted as rewind.

#### Scenario: Rewind across multiple boundaries
- **WHEN** travel descends through an end, its start, and an earlier end
- **THEN** the corresponding intervals reopen, disappear, and reopen in reverse order

#### Scenario: Backward envelope trigger
- **WHEN** an envelope triggers while the continuous coordinate decreases
- **THEN** its scope records no new start

#### Scenario: Transport restart
- **WHEN** musical phase resets while the continuous coordinate stays unchanged
- **THEN** the existing restart start is accepted without undoing older scope history

### Requirement: Bounded concurrent publication
UI inverse and scope reads SHALL use race-free shared payload access and a published real-time cutoff. Reused inverse coordinates outside the retained span and interpolation endpoints newer than the cutoff SHALL be rejected. Marker acceptance SHALL use a coherent publication while the writer remains nonblocking.

#### Scenario: Writer advances during display
- **WHEN** an inverse endpoint is overwritten with a timestamp after the accepted publication
- **THEN** the UI query is unavailable rather than interpolating that endpoint into accepted history
