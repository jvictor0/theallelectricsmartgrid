## REMOVED Requirements

### Requirement: Global Phase Tracking on the Circle
**Reason**: Absolute phase is canonical; winding reconstruction discards information at the input.
**Migration**: Use absolute unmodulated/modulated global phase and wrap only local outputs.

## ADDED Requirements

### Requirement: Absolute Global Coordinates and Consistent Queries
The system SHALL store absolute unmodulated and modulated global phases as doubles without wrapping or winding counters. Phase SHALL mean absolute cycles, position SHALL mean signed 64-bit common-lattice ticks, and cycle ratio SHALL mean loop cycles per global cycle. The public API SHALL use global, unmodulated, and modulated terminology, explicit loop/sample coordinates, and one phase query accepting a PhaseDomain enum. It SHALL remove direct/indirect and unwound aliases, Boolean phase-domain selectors, recursive MonodromyNumber, and duplicate multiplier conventions. Persisted parameter keys SHALL remain unchanged.

#### Scenario: Input preserves whole cycles
- **WHEN** unmodulated phase is 12.75 and the offset is -0.5
- **THEN** modulated global phase is 12.25 and both absolute values remain available

#### Scenario: Negative phase remains signed
- **WHEN** modulated global phase is -0.25
- **THEN** the stored phase remains -0.25 and a normalized output adapter returns 0.75

## MODIFIED Requirements

### Requirement: Internal-Only Clock Source
The system SHALL keep ClockMode and Input::m_clockMode absent. It SHALL advance the absolute unmodulated phase by the supplied effective frequency without wrapping. External synchronization SHALL remain outside Theory of Time and change the supplied frequency rather than introduce a selectable tick/PLL/phasor input mode.

#### Scenario: Supplied frequency crosses a cycle
- **WHEN** unmodulated phase is 0.99 and the running phase increment is 0.02
- **THEN** the new unmodulated phase is 1.01
- **AND** no product clock-mode API is introduced

### Requirement: True Phase and Phase-Modulated Phase
The system SHALL maintain absolute unmodulated and modulated phases with modulated = unmodulated - 2 * modIndex * lfoRawOutput. The phase-modulation PolyXFader SHALL read unmodulated loop phases and retain its existing loop-period/global-period amplitude weights. Modulated time SHALL drive gates and musical position; unmodulated time SHALL drive external synchronization, recording timestamps, outgoing clock, and existing unmodulated consumers.

#### Scenario: Zero modulation is identity
- **WHEN** modulation index is zero at global phase 7.25
- **THEN** both domains report 7.25

#### Scenario: LFO retains its source and weighting
- **WHEN** phase modulation is evaluated
- **THEN** its LFO reads unmodulated phases and scales contributions by loop period divided by global period

### Requirement: Six Hierarchical Time Loops
The system SHALL expose six time loops with the global root at index 5 and integer positive parent multipliers. Each loop phase SHALL equal the chosen global phase times its product of parent multipliers. Every loop SHALL observe the same absolute common-lattice position through its own period; child coordinates SHALL NOT be reduced modulo their periods during propagation.

#### Scenario: Child retains whole cycles
- **WHEN** the global phase is 3.25 and a child's global cycle ratio is 4
- **THEN** its phase is 13 and its full-cycle index is 13

#### Scenario: Shared absolute position
- **WHEN** the common-lattice position is 13 and a child's period is 8 ticks
- **THEN** that child observes absolute position 13 and derives local output position 5 only when needed

### Requirement: Topology Changes Only at Parent Zero
While running, the system SHALL accept a requested parent/multiplier pair only on a sample where both the old and requested parents cross their modulated cycle boundaries. Eligibility SHALL be evaluated from one pre-edit topology snapshot; deferred pairs SHALL remain unchanged. Multiplier-only edits SHALL require the unchanged parent's boundary. Accepted edits SHALL recompute the LCM periods and direct phase mappings together. Independent/modulated boundary timing SHALL otherwise retain existing behavior.

#### Scenario: New parent is not aligned
- **WHEN** the old parent crosses zero but the requested parent does not
- **THEN** neither the requested parent nor its accompanying multiplier is accepted

#### Scenario: Simultaneous boundaries accept the pair
- **WHEN** old and requested parents cross zero on the same sample
- **THEN** the requested parent/multiplier pair is accepted together
- **AND** all derived coordinates use the resulting topology

#### Scenario: Multiplier-only edit waits
- **WHEN** a multiplier change is requested mid-parent-cycle
- **THEN** it remains pending until that parent's next cycle boundary

### Requirement: Integer Position and Gate
The system SHALL derive absolute signed 64-bit position P by flooring global phase times the current global lattice period. A loop with even period L SHALL have gate FloorMod(P,L) < L/2, gate-step index FloorDiv(P,L/2), and cycle index FloorDiv(P,L), using Euclidean arithmetic. Crossing flags SHALL compare indices from consecutive absolute phases in consistent lattice units, excluding changes caused solely by a topology coordinate remap. A boundary-crossing flag SHALL represent at least one crossing and SHALL NOT require the final gate Boolean to differ. Actual previous coordinates SHALL NOT be rewritten to adjacent positions.

#### Scenario: Negative half-cycle
- **WHEN** P is -1 and L is 8
- **THEN** the gate-step index is -1, the local output position is 7, and the gate is false

#### Scenario: Multiple crossed steps produce one event
- **WHEN** P advances from 1 to 17 with L equal to 8
- **THEN** the gate-step and cycle-crossing flags are true even though both endpoint gates are true
- **AND** there is at most one event per consumer per sample and the previous coordinate remains 1

### Requirement: Monodromy Numbers
The system SHALL expose GetGateStepIndex instead of recursive monodromy reconstruction. With clock period Lc it SHALL return FloorDiv(P,Lc/2) without reset. If reset is the clock or an ancestor of period Lr, it SHALL return FloorMod(FloorDiv(P,Lc/2), Lr/(Lc/2)); non-ancestor and absent resets SHALL return the absolute index. The arp SHALL sample this coordinate on gate-step crossing events. No winding or accumulated event counter SHALL be maintained to answer the query.

#### Scenario: Absolute gate-step index
- **WHEN** global phase is 3.75 and the global loop is selected without reset
- **THEN** the returned index is 7

#### Scenario: Ancestor reset
- **WHEN** P is 13, the clock period is 8, and the ancestor period is 24
- **THEN** the returned reset-relative index is 3

#### Scenario: Reverse time and self reset
- **WHEN** P is -1 and clock period is 8
- **THEN** the absolute index is -1 and selecting the clock itself as reset returns 1

#### Scenario: Unrelated reset is ignored
- **WHEN** a reset loop is neither the selected clock nor its ancestor
- **THEN** the returned index equals the no-reset absolute index

### Requirement: Microblock Buffer with Lookahead Sample
The system SHALL retain nine sample slots for an eight-sample microblock and copy slot 8 to slot 0 on rollover. GetPhase SHALL interpolate absolute global phase before applying the interval's accepted loop ratio. For fractional positions in [j,j+1), it SHALL use j's topology; at j+1 it SHALL use j+1's topology. Slot 8 SHALL be queryable, and interpolation in [7,8] SHALL use both endpoints. No wrapped or differently mapped child coordinates SHALL be interpolated across a cycle or topology boundary.

#### Scenario: Ordinary wrap interpolation
- **WHEN** global endpoint phases are 0.99 and 1.01
- **THEN** the midpoint phase is 1.00 and a wrapped midpoint output is 0

#### Scenario: Topology edit does not sweep through discarded cycles
- **WHEN** the loop ratio changes from 2 to 3 at global phase 10 at sample j+1
- **THEN** queries before j+1 use ratio 2 and queries at j+1 use ratio 3
- **AND** interpolation does not traverse child phases 20 through 30

#### Scenario: Lookahead rollover
- **WHEN** the microblock rolls over
- **THEN** slot 8's absolute phases, coordinates, topology, and event state become slot 0's state

### Requirement: Transport Start and Stop
The system SHALL initialize deterministic absolute coordinates and explicit startup events when transport starts. Startup SHALL accept requested valid topology and emit the initial gate-step/cycle events without inventing a wrapped previous position. On stop, phase, current and previous positions, gates, and motion outputs SHALL clear to zero/false while topology-derived periods remain available. Stopped multiplier edits SHALL recompute periods only when accepted topology changes; startup SHALL accept requested parent changes without waiting for motion. The any-change signal SHALL be raised for the stop transition or an observable stopped topology edit, and remain false for unchanged stopped frames.

#### Scenario: First run
- **WHEN** transport starts
- **THEN** requested topology is active, absolute phase advances from zero, and explicit startup events are emitted

#### Scenario: Stop preserves period configuration
- **WHEN** a running timebase stops
- **THEN** coordinates and gates clear, configured periods remain, and the any-change signal is true

#### Scenario: Stopped editing
- **WHEN** an already stopped multiplier changes
- **THEN** the accepted period configuration updates without motion
- **AND** subsequent unchanged stopped samples do not recompute periods or raise any-change

### Requirement: Deterministic State from Phasor and Topology
The system SHALL derive phase, absolute position, gate value, and gate-step index solely from current absolute global phase and accepted topology. Crossing events SHALL depend only on consecutive samples and the defined topology/startup transition rules. Parameter acceptance and existing DSP filters SHALL retain their explicit state; there SHALL be no separate winding reconstruction or historical child cycle count.

#### Scenario: Same coordinates after different histories
- **WHEN** two timebases reach the same global phases and accepted topology through different earlier phase paths
- **THEN** their loop phases, positions, gate values, and reset-relative indices match

#### Scenario: Coordinate remap is immediate
- **WHEN** an accepted topology edit changes a child's ratio at nonzero absolute time
- **THEN** its absolute phase and index immediately equal the new pure mapping, without historical compensation offsets
