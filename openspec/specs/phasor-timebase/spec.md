# Phasor Timebase Specification

## Purpose
The phasor timebase (Theory of Time, `private/src/TheoryOfTime.hpp`) is the global clock for Smart Grid One. It maps wall-clock time to a phase on the circle S¹ and derives six hierarchical time loops whose integer positions, gates, and monodromy numbers drive the Nonagon sequencer (see nonagon-sequencer) and all phase-synchronized DSP. All downstream sequencer state is a pure function of the current phasor and the loop topology, so the system is deterministic and recoverable after phase jumps.

## Requirements

### Requirement: Global Phase Tracking on the Circle
The system SHALL represent global time as a phasor in [0, 1) tracked by a CircleTracker that maintains both the current phase and an integer winding count of full revolutions.
The CircleTracker detects a wrap when the phase moves by more than 0.5 between consecutive control samples, incrementing the winding count for a forward wrap and decrementing it for a backward wrap. The unwound value (phase + winding) provides continuous time for monodromy computation.

#### Scenario: Forward wrap increments winding
- **WHEN** the modulated phasor advances from 0.97 to 0.02 in one control sample
- **THEN** the CircleTracker detects a jump of magnitude greater than 0.5
- **AND** the winding count increases by exactly 1 while the tracked phase becomes 0.02

#### Scenario: Backward wrap decrements winding
- **WHEN** the phasor moves from 0.02 back across zero to 0.97
- **THEN** the winding count decreases by 1

### Requirement: True Phase and Phase-Modulated Phase
The system SHALL maintain two phase signals: the true (unmodulated) phase from the selected clock source, and a phase-modulated phase equal to the true phase plus an LFO-derived phase offset, wrapped into [0, 1).
The phase-modulation LFO is a PolyXFader driven by the unmodulated (independent) phasors of the six time loops (`m_useIndirectPhasor` is false), so the LFO stays synchronized with the timebase it modulates. The applied offset is `-2 × modIndex × lfoRawOutput`. The master loop's position and gates follow the modulated phase; the independent phase remains available per loop for continuity-sensitive consumers (display, LFO drive, note-writer positions).

#### Scenario: Zero modulation index passes true phase through
- **WHEN** the phase-modulation index is 0
- **THEN** the phase offset is 0 and the modulated phase equals the true phase on every control sample

#### Scenario: LFO is driven by unmodulated phasors
- **WHEN** the phase-modulation LFO computes its output for the current sample
- **THEN** it reads each time loop's independent (unmodulated) phasor, not the phase-modulated phasor
- **AND** the per-loop LFO weights are scaled by `loopSize / masterLoopSize`

### Requirement: Six Hierarchical Time Loops
The system SHALL provide exactly six time loops (`x_numLoops == 6`) arranged in a tree, where the master loop is index 5 (`x_numLoops - 1`), has no parent, and is driven directly by the phase-modulated phasor; every other loop has a parent index and an integer winding multiplier mapping the parent's circle onto its own.
Child loops derive their integer position from the parent: `position = parentPosition % loopSize` and `prevPosition = parentPrevPosition % loopSize`, so integer position flows from the master down the tree. Child unmodulated phasors are computed as the parent's independent phasor times the winding multiplier, wrapped mod 1.

#### Scenario: Child position derives from parent
- **WHEN** a child loop with loop size 8 has a parent whose position is 13
- **THEN** the child's position is 13 mod 8 = 5

#### Scenario: Master loop is the tree root
- **WHEN** the timebase processes a control sample
- **THEN** only loop index 5 computes its position directly from the modulated phasor (`floor(phasor × loopSize)`)
- **AND** loops 0 through 4 obtain positions from their parents

### Requirement: Topology Changes Only at Parent Zero
The system SHALL apply requested changes to a loop's parent index or winding multiplier only on a control sample where the loop's parent is at zero (the parent's top flag is set), keeping positions continuous across topology changes.
After any accepted topology change, loop sizes are recomputed for the whole tree.

#### Scenario: Mid-cycle change is deferred
- **WHEN** the performer changes a loop's winding multiplier while the parent loop is mid-cycle
- **THEN** the loop keeps its previous multiplier for every sample until the parent next reaches position zero
- **AND** the new multiplier takes effect on the sample where the parent's top flag is true

### Requirement: Loop Size Propagation by LCM
The system SHALL compute loop sizes from the topology in three passes: an upward child-to-parent pass setting `parentSize = lcm(parentSize, childSize × childMult)`, a downward pass setting `childSize = parentSize / childMult`, and a final doubling of every loop size so each loop has two gate states per cycle.
This guarantees every division is exact and that all gates change on the same integer step when they mathematically coincide.

#### Scenario: Two children with multipliers 2 and 3
- **WHEN** two loops are children of the master with winding multipliers 2 and 3
- **THEN** the master's pre-doubling size is lcm(2, 3) = 6 and the children's are 3 and 2
- **AND** after doubling the master loop size is 12 and the children's are 6 and 4

### Requirement: Integer Position and Gate
The system SHALL track an integer position per loop, `position = floor(phase × loopSize)`, and a gate defined as `gate = (position < loopSize / 2)`, with a gate-changed flag set on any sample where the gate flips.
The six gates form the 6-bit time slice in I⁶ consumed by lamejuis-sequencer. When the master position jumps by more than one step between samples (for example after a seek or phase modulation jump), the previous position is rewritten to the adjacent step so transitions remain one-step and no false edge bursts are reported.

#### Scenario: Gate covers the first half of the cycle
- **WHEN** a loop has loop size 8
- **THEN** its gate is true at positions 0 through 3 and false at positions 4 through 7
- **AND** the gate-changed flag is set exactly on the samples where the position crosses 3→4 and 7→0

#### Scenario: Position jump is corrected
- **WHEN** the master position jumps from 2 to 9 in one sample while ascending
- **THEN** the stored previous position becomes 8 so that exactly one position transition is observed

### Requirement: Monodromy Numbers
The system SHALL expose `MonodromyNumber(clockIx, resetIx)` returning the number of half-cycles (gate state changes) of loop `clockIx` since loop `resetIx` was last at zero, or, when `resetIx` is -1, since the clock started, using the global winding count at the master loop.
The recurrence counts in units of `loopSize / 2`, ascending toward the reset ancestor; at the master loop with no reset it returns `GlobalWinding() × 2`, plus 1 when the master gate is in its second half. The index arp (see lamejuis-sequencer) samples this value only when the selected clock loop's gate changes.

#### Scenario: Total count since startup
- **WHEN** the global phase has wound 3 full revolutions since start and the master loop's gate is currently false (second half)
- **THEN** `MonodromyNumber(masterIndex, -1)` returns 3 × 2 + 1 = 7

#### Scenario: Count relative to a reset ancestor
- **WHEN** a clock loop of size 8 is a direct child of the reset loop and the reset loop's position is 13
- **THEN** `MonodromyNumber(clockIx, resetIx)` returns 13 / 4 = 3 gate flips since the reset loop was at zero

### Requirement: Microblock Buffer with Lookahead Sample
The system SHALL buffer per-loop state in arrays of 9 control samples per microblock (`x_microBlockBufferSize = 9` for an 8-sample control frame), where slot 8 holds the first sample of the next microblock, computed during the current block.
`RolloverMicroblockBuffer()` copies slot 8 into slot 0 at the start of each microblock, so the first sample of every block was computed in the previous block and interpolation anywhere inside a microblock has accurate boundary samples on both ends.

#### Scenario: Rollover carries the lookahead sample
- **WHEN** a new microblock begins
- **THEN** every loop's slot-8 state (position, gate, phasors, winding, flags) is copied into slot 0
- **AND** processing then fills slots 1 through 8, producing this block's remaining samples plus the next block's first sample

### Requirement: Transport Start and Stop
The system SHALL initialize deterministic state on transport start and clear all loop state on stop.
On the first running sample, requested topology is applied, loop sizes are computed, the global CircleTracker is reset to phase 0, and every loop's position is set to `loopSize - 1` with gate false, so the first advance lands on position 0 as a top. On stop, every loop is reset to position 0, loop size 1, gate false, and the any-change flag is raised for that frame so consumers observe the transition.

#### Scenario: Start primes loops one step before zero
- **WHEN** the transport starts running
- **THEN** each loop's position equals its loop size minus 1 and its gate is false
- **AND** the global phase winding count is reset to 0

#### Scenario: Stop clears loop state
- **WHEN** the running input goes false while the timebase is running
- **THEN** all six loops report position 0, gate false, and loop size 1
- **AND** the any-change flag is true on that control sample

### Requirement: Deterministic State from Phasor and Topology
The system SHALL derive all loop positions, gates, tops, and monodromy numbers as a pure function of the current phasor (with its winding) and the loop topology, with no hidden sequencer state requiring synchronization.
Consequently the timebase handles arbitrary phase jumps: state after a jump is exactly the state that the new phase implies.

#### Scenario: State recovers after a phase jump
- **WHEN** phase modulation jumps the modulated phasor to a new value within the same winding
- **THEN** every loop's position and gate equal the values implied by the new phase and current loop sizes
- **AND** subsequent monodromy queries reflect the new position with no residual state from before the jump
