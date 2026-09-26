## REMOVED Requirements

### Requirement: Global Phase Tracking on the Circle
**Reason**: Absolute phase is canonical; winding reconstruction discards information at the input.
**Migration**: Use absolute unmodulated/modulated global phase and wrap only local outputs.

## ADDED Requirements

### Requirement: Absolute Global Coordinates and Consistent Queries
The system SHALL store absolute unmodulated and modulated global phases as doubles without wrapping or winding counters. Phase SHALL mean absolute cycles, position SHALL mean signed 64-bit common-lattice ticks, and cycle ratio SHALL mean loop cycles per global cycle. The public API SHALL use global, unmodulated, and modulated terminology, explicit loop/sample coordinates, and one phase query accepting a PhaseDomain enum. It SHALL remove direct/indirect and unwound aliases, Boolean phase-domain selectors, recursive MonodromyNumber, and duplicate multiplier conventions. Existing persisted parameter keys SHALL remain unchanged; the rhythm pages SHALL add only their documented StateSaver registrations.

#### Scenario: Input preserves whole cycles
- **WHEN** unmodulated phase is 12.75 and the offset is -0.5
- **THEN** modulated global phase is 12.25 and both absolute values remain available

#### Scenario: Negative phase remains signed
- **WHEN** modulated global phase is -0.25
- **THEN** the stored phase remains -0.25 and a normalized output adapter returns 0.75

### Requirement: Configurable Whole-Cycle Loop Rhythms
The system SHALL provide one rhythm per time loop with 16 engine slots, active size 1-16, and optional reset index -1 or 0-5. The default SHALL be size 2, gate slot 0 true, all remaining slots false, and reset -1. Each step SHALL occupy a complete loop cycle. A reset that ceases to be an ancestor SHALL remain stored but be ignored until accepted topology restores that ancestry.

#### Scenario: Default alternates full cycles
- **WHEN** a loop starts at cycle zero with its default rhythm and proceeds through cycles 0, 1, and 2
- **THEN** its gate is true, false, and true respectively, holding each value for a full cycle

#### Scenario: Stored reset survives reparenting
- **WHEN** an accepted reparent makes the stored reset a non-ancestor
- **THEN** subsequent loop ticks use the absolute step without changing the stored reset
- **AND** the next tick after ancestry is restored uses that reset again

### Requirement: Theory of Time Rhythm Grid and Persistence
The system SHALL expose a six-column/eight-row rhythm page and an ancestor-reset page. A normal rhythm-pad press SHALL toggle its State value; Shift-press on row j SHALL set size to j+1. Only rows below the active size SHALL be lit. The current step SHALL be bright Purple/Pink for on/off, and other active steps dim Purple/Grey. The reset page SHALL enable only strict ancestors in accepted topology; self and non-ancestor cells SHALL be dark and inert. Pressing the selected reset SHALL clear it to -1. The selected ancestor SHALL be Blue, other eligible ancestors dim Blue, and row 7 SHALL display live gates.
The pages SHALL share StateSaver entries TheoryOfTimeRhythm(loop, step) for slots 0-7, TheoryOfTimeRhythmSize(loop), and TheoryOfTimeRhythmReset(loop). The engine's additional slots 8-15 SHALL have no grid or persistence entries. Loading a patch that omits any of these keys SHALL retain the current registered value under normal StateSaver policy. A fresh instance SHALL start from the documented default rhythm.

#### Scenario: Toggle and length use shared state
- **WHEN** a performer toggles row 3 and then Shift-presses row 5 for loop 2
- **THEN** the registered gate at slot 3 changes, the size becomes 6, and save/load restores both values
- **AND** the sounding loop gate waits for loop 2's next modulated tick

#### Scenario: Reset toggle and disabled cells
- **WHEN** a performer presses a valid ancestor twice
- **THEN** it is first selected and then cleared to -1
- **AND** pressing self or a non-ancestor does not change the reset

#### Scenario: Invalid reset is hidden without deletion
- **WHEN** the stored reset loses ancestry after a topology edit
- **THEN** its pad becomes dark and disabled while the stored value is retained

#### Scenario: Legacy patch preserves current state
- **WHEN** a patch has no rhythm keys and the current rhythm differs from its construction default
- **THEN** loading that patch preserves the current rhythm
- **AND** loading it into a fresh instance leaves the fresh default unchanged

## MODIFIED Requirements

### Requirement: Theory of Time Topology Grid Control
The system SHALL expose the per-loop clock topology for live editing through the Theory of Time topology grid (`TheNonagonSmartGrid::TheoryOfTimeTopologyPage`, reachable on the BottomRight base grid / route 3). For each editable time loop bit `i`, the column at grid x `i` SHALL provide:
- multiplier pads at grid y `mult - 2` for `mult ∈ {2, 3, 4, 5}`, each a toggle `StateCell<int>` using the registered `State*` for `m_theoryOfTimeInput.m_input[i].m_parentMult`, so pressing the pad for value `mult` sets that loop bit's parent multiplier to `mult`;
- a parent-index pad at grid y 4 (for the inner bits) bound to `m_input[i].m_parentIndex`;
- a show-only `RuntimeStateCell` at grid y 7 (`TimeBitCell`) reflecting the loop's live gate state without registering it as saved state.

Pressing a multiplier pad SHALL change the underlying `m_parentMult` value, the change taking effect on the clock topology at simultaneous modulated cycle crossings of the old and requested parents (see "Topology Changes Only at Parent Zero"), and the pad's published LED color SHALL reflect the active value: the pad whose `mult` equals the current `m_parentMult` shows the on-color (White) and the others the off-color (Fuscia). The topology values are persisted through the `StateSaver` registry under keys `"TheoryOfTimeMult"` and `"TheoryOfTimeParentIx"`, so they SHALL be restored exactly by a patch save/load round-trip.

#### Scenario: Pressing a multiplier pad sets the loop's parent multiplier
- **WHEN** the performer presses the multiplier pad for value 3 in loop-bit column `i` on the topology grid
- **THEN** after the change settles `m_theoryOfTimeInput.m_input[i].m_parentMult` equals 3
- **AND** the multiplier pad for value 3 publishes the on-color while the pads for 2, 4, and 5 publish the off-color

#### Scenario: Topology multiplier survives a patch save/load round-trip
- **WHEN** loop-bit `i`'s parent multiplier is set to 4 via the topology grid, the patch is saved, the system is reset to defaults, and the patch is loaded
- **THEN** `m_input[i].m_parentMult` is 4 again after loading
- **AND** the topology grid republishes the value-4 pad as the on-color

#### Scenario: Loaded topology drives the clock
- **WHEN** a patch with non-default loop multipliers is loaded and the sequencer runs
- **THEN** the absolute global phase advances and the loops gate without producing NaN or out-of-bound output
- **AND** the loop periods derived from the restored multipliers match those produced by setting the same multipliers live

### Requirement: Outgoing Phasor-To-Tick Clock Generation
The system SHALL keep outgoing MIDI clock generation as phasor-to-tick behavior, owned by a dedicated `Phasor2Tick` helper in `private/src/Phasor2Tick.hpp`.
When the timebase starts running, the helper SHALL update its divisions from the current internal frequency; while running, a division crossing in the absolute unmodulated global phase SHALL emit a clock message through the configured message-out buffer.

#### Scenario: Start updates outgoing clock divisions
- **WHEN** the timebase transitions from stopped to running
- **THEN** `Phasor2Tick` updates its divisions from the internal frequency
- **AND** a transport start message is emitted when a message-out buffer is configured

#### Scenario: Running phasor emits clock message
- **WHEN** the absolute unmodulated global phase crosses a `Phasor2Tick` division while the timebase is running
- **THEN** the message-out buffer receives one MIDI clock message

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
While running, the system SHALL accept a requested parent/multiplier pair only on a sample where both the old and requested parents cross their modulated cycle boundaries. Eligibility SHALL be evaluated from one pre-edit topology snapshot; deferred pairs SHALL remain unchanged. Multiplier-only edits SHALL require the unchanged parent's boundary. Accepted edits SHALL recompute the LCM periods and direct phase mappings together. Unmodulated/modulated boundary timing SHALL otherwise retain existing behavior.

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
The system SHALL derive absolute signed 64-bit position P by flooring global phase times the current global lattice period. A loop of period L SHALL have full-cycle gate-step index FloorDiv(P,L), using Euclidean arithmetic. On each modulated cycle crossing, its gate SHALL be read from its configured rhythm at FloorMod(GetGateStepIndex(loop, sample, resetLoop), rhythmSize); between its crossings the gate SHALL hold. Crossing flags SHALL compare consecutive absolute phases in consistent lattice units before accepted topology edits. After accepting edits, the system SHALL remap positions, preserve those crossing flags, and evaluate gates using the accepted topology. A coordinate remap SHALL NOT create additional elapsed-travel events or rewrite previous coordinates.

#### Scenario: Negative whole-cycle index
- **WHEN** P is -1, L is 8, reset is absent, and the default rhythm is evaluated
- **THEN** the gate-step index is -1, its rhythm slot is 1, and its gate is false

#### Scenario: Multiple crossed steps produce one event
- **WHEN** P advances from 1 to 17 with L equal to 8 and default rhythm
- **THEN** a tick is reported even though both endpoint gates are true
- **AND** there is at most one event per consumer per sample and the previous coordinate remains 1

#### Scenario: Equal neighboring values still tick
- **WHEN** a loop with rhythm [true, true] crosses its next modulated cycle boundary
- **THEN** its gate remains true and the loop's tick event is true

#### Scenario: Accepted topology supplies the rhythm index
- **WHEN** simultaneous old/requested parent crossings admit a topology edit
- **THEN** positions are remapped before gate lookup and the gate uses the new period and reset ancestry
- **AND** crossing flags remain those computed before acceptance

### Requirement: Whole-Cycle Gate-Step Indices
The system SHALL expose GetGateStepIndex instead of recursive monodromy reconstruction. With modulated position P and clock period Lc it SHALL return FloorDiv(P,Lc) without reset. If reset is the clock or an ancestor of period Lr, it SHALL return FloorMod(FloorDiv(P,Lc), Lr/Lc); non-ancestor and absent resets SHALL return the absolute index. The result and intermediate rhythm/motive coordinates SHALL retain signed 64-bit range until bounded reduction. The arp SHALL sample this coordinate on modulated tick events, independently of changes to the rhythm gate Boolean or reset-relative index.

#### Scenario: Absolute gate-step index
- **WHEN** global phase is 3.75 and the global loop is selected without reset
- **THEN** the returned index is 3

#### Scenario: Ancestor reset
- **WHEN** P is 29, the clock period is 8, and the ancestor period is 24
- **THEN** the absolute index is 3 and the reset-relative index is 0

#### Scenario: Reverse time and self reset
- **WHEN** P is -1 and clock period is 8
- **THEN** the absolute index is -1, an ancestor reset of period 24 returns 2, and self reset returns 0

#### Scenario: Self reset preserves ticks
- **WHEN** a loop with self reset crosses a modulated boundary
- **THEN** its returned index remains zero and it still reports a tick

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
- **THEN** requested topology is active, absolute phase advances from zero, every loop receives an explicit modulated/unmodulated crossing event, and every loop rhythm is evaluated

#### Scenario: Stop preserves period configuration
- **WHEN** a running timebase stops
- **THEN** coordinates and gates clear, configured periods remain, and the any-change signal is true

#### Scenario: Stopped editing
- **WHEN** an already stopped multiplier changes
- **THEN** the accepted period configuration updates without motion
- **AND** subsequent unchanged stopped samples do not recompute periods or raise any-change

### Requirement: Deterministic State from Phasor and Topology
The system SHALL derive phase, absolute position, and gate-step index from current absolute global phase and accepted topology. For fixed rhythm configuration, each running loop gate SHALL equal the rhythm value at its current step after startup or a loop tick. Live gate, size, and reset edits SHALL be sampled on that loop's next modulated tick; between ticks the previous gate SHALL persist. Crossing events SHALL depend on consecutive samples and the defined topology/startup rules, without winding reconstruction or historical child cycle counts.

#### Scenario: Same coordinates after different histories
- **WHEN** two running timebases reach the same global phases and accepted topology with the same unchanged rhythm configuration
- **THEN** their loop phases, positions, gate values, and reset-relative indices match

#### Scenario: Coordinate remap is immediate
- **WHEN** an accepted topology edit changes a child's ratio at nonzero absolute time
- **THEN** its absolute phase and index immediately equal the new mapping, without historical compensation offsets

#### Scenario: Edit waits for the edited loop
- **WHEN** a loop's gate, rhythm size, or reset selection is edited between its modulated ticks
- **THEN** its gate remains unchanged until its next modulated tick, even if a faster loop ticks first
- **AND** the edit alone does not raise the timebase any-change flag

### Requirement: Loop Size Propagation by LCM
The system SHALL compute each loop's global cycle ratio as the product of its positive parent multipliers and set the global lattice period to the LCM of all cycle ratios, without doubling. A loop period SHALL equal the global period divided by its cycle ratio. Periods of one tick SHALL be valid; one complete loop period SHALL represent one gate step.

#### Scenario: Coprime sibling ratios
- **WHEN** two children of the global loop have cycle ratios 2 and 3 and all other loops have ratio 1
- **THEN** the global lattice period is 6 ticks and the children's periods are 3 and 2 ticks
- **AND** no factor of two is applied

#### Scenario: Fastest loop has a one-tick period
- **WHEN** the global cycle ratios are 1 and 3
- **THEN** the global period is 3 ticks and the faster loop period is 1 tick

## RENAMED Requirements

- FROM: `### Requirement: Monodromy Numbers`
- TO: `### Requirement: Whole-Cycle Gate-Step Indices`
