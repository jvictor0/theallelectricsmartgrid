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

### Requirement: Internal-Only Clock Source
The system SHALL keep `TheoryOfTime::ClockMode` and `TheoryOfTime::Input::m_clockMode` absent.
The timebase SHALL advance from the frequency value supplied in `TheoryOfTime::Input::m_freq`; it SHALL NOT expose PLL, external phasor input, tick-to-phasor input, or MIDI clock routing as selectable product clock modes inside Theory of Time. External MIDI synchronization, when enabled, SHALL occur outside Theory of Time by changing the supplied effective frequency.

#### Scenario: Clock mode surface is absent
- **WHEN** product code constructs a `TheoryOfTime::Input`
- **THEN** it has no `m_clockMode` member
- **AND** no `ClockMode` enum is available

#### Scenario: Supplied frequency advances the phasor
- **WHEN** the timebase processes a running sample with supplied frequency `m_freq`
- **THEN** the true phasor advances by `m_freq`
- **AND** the phasor wraps into [0, 1)

#### Scenario: External sync remains outside Theory of Time
- **WHEN** external MIDI clock mode is enabled by the surrounding integration
- **THEN** Theory of Time still advances from `Input::m_freq`
- **AND** it does not inspect MIDI clock ticks directly

### Requirement: True Phase and Phase-Modulated Phase
The system SHALL maintain two phase signals: the true (unmodulated) phase from the internal clock, and a phase-modulated phase equal to the true phase plus an LFO-derived phase offset, wrapped into [0, 1).
The phase-modulation LFO is a PolyXFader driven by the unmodulated (independent) phasors of the six time loops (`m_useIndirectPhasor` is false), so the LFO stays synchronized with the timebase it modulates. The applied offset is `-2 × modIndex × lfoRawOutput`. The master loop's position and gates follow the modulated phase; the independent phase remains available per loop for continuity-sensitive consumers (display, LFO drive, note-writer positions).

#### Scenario: Zero modulation index passes true phase through
- **WHEN** the phase-modulation index is 0
- **THEN** the phase offset is 0 and the modulated phase equals the true phase on every control sample

#### Scenario: LFO is driven by unmodulated phasors
- **WHEN** the phase-modulation LFO computes its output for the current sample
- **THEN** it reads each time loop's independent (unmodulated) phasor, not the phase-modulated phasor
- **AND** the per-loop LFO weights are scaled by `loopSize / masterLoopSize`

### Requirement: Outgoing Phasor-To-Tick Clock Generation
The system SHALL keep outgoing MIDI clock generation as phasor-to-tick behavior, owned by a dedicated `Phasor2Tick` helper in `private/src/Phasor2Tick.hpp`.
When the timebase starts running, the helper SHALL update its divisions from the current internal frequency; while running, a division crossing in the independent master phasor SHALL emit a clock message through the configured message-out buffer.

#### Scenario: Start updates outgoing clock divisions
- **WHEN** the timebase transitions from stopped to running
- **THEN** `Phasor2Tick` updates its divisions from the internal frequency
- **AND** a transport start message is emitted when a message-out buffer is configured

#### Scenario: Running phasor emits clock message
- **WHEN** the independent master phasor crosses a `Phasor2Tick` division while the timebase is running
- **THEN** the message-out buffer receives one MIDI clock message

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
The system SHALL initialize deterministic state on transport start and maintain a topology-aware stopped state whenever the timebase is not running.
On the first running sample, requested topology is applied, loop sizes are computed, the global CircleTracker is reset to phase 0, and every loop's position is set to `loopSize - 1` with gate false, so the first advance lands on position 0 as a top. Whenever the timebase receives a non-running input, `ProcessNotRunning` SHALL inspect requested loop multipliers, accept changed multiplier values, call `SetLoopSizes` only when at least one multiplier changed, clear phasors and winding, set every loop position and previous position to 0, set every loop gate false, and keep independent positions at 0. The any-change flag SHALL be raised on the stop transition or when stopped multiplier maintenance changes observable loop state; otherwise it SHALL remain false while already stopped.

#### Scenario: Start primes loops one step before zero
- **WHEN** the transport starts running
- **THEN** each loop's position equals its loop size minus 1 and its gate is false
- **AND** the global phase winding count is reset to 0

#### Scenario: Stop clears loop motion but keeps topology-derived sizes
- **WHEN** the running input goes false while the timebase is running
- **THEN** all six loops report position 0, previous position 0, gate false, phasor 0, independent phasor 0, and winding 0
- **AND** loop sizes match the accepted loop multipliers
- **AND** the any-change flag is true on that control sample

#### Scenario: Already stopped multiplier changes update loop sizes before first run
- **WHEN** the timebase has never run and a requested loop multiplier changes while running input remains false
- **THEN** the changed multiplier is accepted
- **AND** `SetLoopSizes` recomputes stopped loop sizes from the accepted multipliers
- **AND** every loop position, previous position, gate, phasor, and winding remains cleared

#### Scenario: Already stopped unchanged multipliers skip loop-size recompute
- **WHEN** the timebase is not running and all requested loop multipliers match the accepted multipliers
- **THEN** `ProcessNotRunning` does not call `SetLoopSizes`
- **AND** every loop position, previous position, gate, phasor, and winding remains cleared

### Requirement: Deterministic State from Phasor and Topology
The system SHALL derive all loop positions, gates, tops, and monodromy numbers as a pure function of the current phasor (with its winding) and the loop topology, with no hidden sequencer state requiring synchronization.
Consequently the timebase handles arbitrary phase jumps: state after a jump is exactly the state that the new phase implies.

#### Scenario: State recovers after a phase jump
- **WHEN** phase modulation jumps the modulated phasor to a new value within the same winding
- **THEN** every loop's position and gate equal the values implied by the new phase and current loop sizes
- **AND** subsequent monodromy queries reflect the new position with no residual state from before the jump

### Requirement: Theory of Time Topology Grid Control
The system SHALL expose the per-loop clock topology for live editing through the Theory of Time topology grid (`TheNonagonSmartGrid::TheoryOfTimeTopologyPage`, reachable on the BottomRight base grid / route 3). For each editable time loop bit `i`, the column at grid x `i` SHALL provide:
- multiplier pads at grid y `mult - 2` for `mult ∈ {2, 3, 4, 5}`, each a toggle `StateCell<int>` bound to `m_theoryOfTimeInput.m_input[i].m_parentMult`, so pressing the pad for value `mult` sets that loop bit's parent multiplier to `mult`;
- a parent-index pad at grid y 4 (for the inner bits) bound to `m_input[i].m_parentIndex`;
- a gate-display cell at grid y 7 (`TimeBitCell`) reflecting the loop's live gate state.

Pressing a multiplier pad SHALL change the underlying `m_parentMult` value, the change taking effect on the clock topology at the next parent-zero boundary (see "Topology Changes Only at Parent Zero"), and the pad's published LED color SHALL reflect the active value: the pad whose `mult` equals the current `m_parentMult` shows the on-color (White) and the others the off-color (Fuscia). The topology values are persisted through the `StateSaver` registry under keys `"TheoryOfTimeMult"` and `"TheoryOfTimeParentIx"`, so they SHALL be restored exactly by a patch save/load round-trip.

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
- **THEN** the master phasor advances and the loops gate without producing NaN or out-of-bound output
- **AND** the loop sizes derived from the restored multipliers match those produced by setting the same multipliers live

### Requirement: Effective Tempo Source
The system SHALL accept `TheoryOfTime::Input::m_freq` as the already-selected effective tempo source. In internal mode this value comes from the tempo parameter; in external mode this value comes from the external clock synchronizer.

#### Scenario: Internal effective tempo
- **WHEN** internal clock mode is selected
- **THEN** the integration writes the tempo-parameter frequency into `TheoryOfTime::Input::m_freq`

#### Scenario: External effective tempo
- **WHEN** external clock mode is selected
- **THEN** the integration writes the clock synchronizer's frequency estimate into `TheoryOfTime::Input::m_freq`
