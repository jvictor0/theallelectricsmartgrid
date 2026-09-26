## MODIFIED Requirements

### Requirement: Three Concurrent Grid Routes with Mode Switching
The system SHALL operate three concurrent Wrld.Bldr grid routes — LeftGrid (6), RightGrid (7), and AuxGrid (8) — where `SetGridsMode(mode)` swaps the left and right grid pointers among the `GridsMode` pages (ComuteAndTheory, TheoryOfTimeRhythm, Matrix, Intervals, SubSequencer, Config) while the aux grid remains fixed; `ProcessFrame()` publishes each grid's colors into the integration's `UIState::m_colorBus[3]`.
Grid cell semantics and color computation belong to the grid layer (see smart-grid-runtime); this capability covers only routing and publication. Selecting a grids-mode cell also stores Controller display mode in the UI state.

#### Scenario: Grids mode swap retargets pad routes
- **WHEN** `SetGridsMode(GridsMode::Matrix)` is called
- **THEN** subsequent pad presses on routes 6 and 7 are applied to the Matrix-mode left and right grids
- **AND** the stored grids mode in the UI state reads Matrix

#### Scenario: Per-frame color publication
- **WHEN** `ProcessFrame()` runs
- **THEN** the left, right, and aux grids each write their current colors into `m_colorBus[0]`, `m_colorBus[1]`, and `m_colorBus[2]` respectively

The TheoryOfTimeRhythm mode SHALL be selected by aux pad (1,1) in the normal selector view and map the rhythm page to route 6 and reset page to route 7. GridsMode ordinals SHALL remain runtime-only state and SHALL NOT be serialized into patches.

#### Scenario: Rhythm mode selects both pages
- **WHEN** aux pad (1,1) is pressed in the normal selector view
- **THEN** left/right routes target the rhythm/reset pages and Controller display mode is selected
- **AND** a left-pad rhythm edit and right-pad reset edit reach their shared StateSaver entries
