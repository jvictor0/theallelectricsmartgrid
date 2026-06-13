## ADDED Requirements

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
