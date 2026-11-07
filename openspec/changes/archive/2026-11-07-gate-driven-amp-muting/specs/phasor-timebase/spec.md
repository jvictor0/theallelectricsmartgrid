## ADDED Requirements

### Requirement: Modulated Global Phase Increment
The timebase SHALL expose the current sample's signed modulated global phase increment. Consecutive running samples SHALL report current modulated global phase minus previous modulated global phase. Startup SHALL report one basic unmodulated clock increment so initialization is not elapsed travel. Stopped samples SHALL report zero modulated movement; the existing basic clock increment SHALL remain independently available. Topology edits SHALL NOT introduce movement into this global increment.

#### Scenario: Modulation changes direction
- **WHEN** consecutive running modulated phases are 1.5 and 1.25
- **THEN** the reported increment is -0.25
- **AND** a following sample at 1.25 reports zero

#### Scenario: Transport resets are not phase travel
- **WHEN** ToT stops from a nonzero phase
- **THEN** its modulated increment is zero
- **AND** restarting at an arbitrary phase offset reports one basic clock increment on the startup sample
