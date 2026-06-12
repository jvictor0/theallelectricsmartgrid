# Smart Grid Runtime Specification

## Purpose
The Smart Grid runtime (`private/src/SmartGrid.hpp`, `SmartBus.hpp`, `PadUI.hpp`) is the stateless intermediary that translates synthesizer state into a controller-agnostic grid of pads. It defines the `Cell`/`Grid` abstractions that route press, release, and pressure events to interactive cells, and the lock-free `SmartBusGeneric` transport that publishes per-route LED color state with epoch-based change tracking. Device-specific MIDI encoding and decoding for Launchpad-family and Twister controllers is specified in controller-midi-io; on-screen pad and encoder surfaces that consume this state are specified in ui-shell-layout and encoder-parameter-system.

## Requirements

### Requirement: Cell Press and Release Semantics
The system SHALL deliver pad interaction to `Cell` objects through `OnPressStatic(velocity)` and `OnReleaseStatic()`, tracking pressed state in `Cell::m_velocity` (nonzero means pressed). A press on an unpressed cell stores the velocity and invokes the virtual `OnPress(velocity)`; a release on a pressed cell zeroes `m_velocity` and invokes `OnRelease()`; a release on an unpressed cell invokes nothing.

#### Scenario: First press fires OnPress exactly once
- **WHEN** `OnPressStatic(100)` is called on a cell whose `m_velocity` is 0
- **THEN** `m_velocity` becomes 100 and the virtual `OnPress(100)` is invoked
- **AND** `IsPressed()` subsequently returns true

#### Scenario: Redundant release is ignored
- **WHEN** `OnReleaseStatic()` is called on a cell whose `m_velocity` is already 0
- **THEN** the virtual `OnRelease()` is not invoked and `m_velocity` stays 0

### Requirement: Pressure-Sensitive Cells
The system SHALL treat cells marked via `SetPressureSensitive()` as continuous-pressure consumers: while such a cell is held, further press or pressure events update `m_velocity` and invoke the virtual `OnPressureChange(velocity)` instead of `OnPress`. Cells that are not pressure sensitive ignore velocity updates while held, and `OnPressureChangeStatic` is a no-op for them.

#### Scenario: Held pressure-sensitive cell receives pressure updates
- **WHEN** `OnPressStatic(80)` is called on a pressure-sensitive cell that is already pressed
- **THEN** `m_velocity` becomes 80 and `OnPressureChange(80)` is invoked, while `OnPress` is not invoked again

#### Scenario: Pressure event on an unpressed pressure-sensitive cell acts as a press
- **WHEN** `OnPressureChangeStatic(60)` is called on a pressure-sensitive cell whose `m_velocity` is 0
- **THEN** `m_velocity` becomes 60 and the virtual `OnPress(60)` is invoked

#### Scenario: Non-sensitive cell ignores pressure
- **WHEN** `OnPressureChangeStatic(60)` is called on a cell that never called `SetPressureSensitive()`
- **THEN** neither `OnPress` nor `OnPressureChange` is invoked and `m_velocity` is unchanged

### Requirement: Grid Message Routing with Bounds Checking
The system SHALL route grid messages to cells through `Grid`, a 2D array of owned `Cell` pointers. `Apply(Message)` with mode `Note` maps velocity 0 to a release and nonzero velocity to a press at `(m_x, m_y)`; `Apply(MessageIn)` maps `PadPress`/`PadPressure` with amount 0 to release, nonzero amount to press, and `PadRelease` to release. `Grid::Get(i, j)` returns null for coordinates outside the configured grid window, and empty or out-of-range positions report `Color::Off` and absorb presses without effect.

#### Scenario: Note message with zero velocity releases the cell
- **WHEN** a `Message` with mode `Note`, velocity 0 is applied at coordinates holding a pressed cell
- **THEN** the cell's `OnRelease()` is invoked and its `m_velocity` becomes 0

#### Scenario: Out-of-range coordinates are ignored
- **WHEN** a press message targets coordinates outside `[x_gridXMin, x_gridXMax) x [y_gridYMin, y_gridYMax)`
- **THEN** `Grid::Get` returns null, no cell handler runs, and `GetColor` for those coordinates returns `Color::Off`

### Requirement: Grid Coordinate Window
The system SHALL address grid cells in a signed logical coordinate window defined at compile time: by default x in [-1, 9) and y in [-1, 10) with `x_gridMaxSize` 11, so a Launchpad's 8x8 pad field plus its edge buttons fit one grid; under the `SMART_BOX` build the window is x in [0, 20), y in [0, 8) with `x_gridMaxSize` 20. Bus storage and iteration translate logical coordinates to physical array indices by subtracting `x_gridXMin`/`x_gridYMin`.

#### Scenario: Logical edge coordinate maps to physical index zero
- **WHEN** a default-build component stores a value at logical coordinates (-1, -1)
- **THEN** it occupies physical index (0, 0) in the backing 11x11 array and reads back from logical (-1, -1)

#### Scenario: Iterator reports physical coordinates
- **WHEN** a `SmartBusGeneric` iterator positioned at logical (-1, -1) is queried with `GetXPhysical()`/`GetYPhysical()`
- **THEN** both return 0

### Requirement: Epoch-Gated Color Bus Publication
The system SHALL transport LED state through `SmartBusColor` (`SmartBusGeneric<Color>`), an `x_gridMaxSize` x `x_gridMaxSize` array of `std::atomic<Color>` with an atomic `m_epoch` counter. `AbstractGrid::OutputToBus(SmartBusColor*)` writes every logical cell color into the bus via atomic exchange and increments the bus epoch exactly once, only if at least one cell value actually changed. `Get(x, y)` reads a single cell lock-free at any time.

#### Scenario: Unchanged frame leaves the epoch untouched
- **WHEN** `OutputToBus` publishes a grid whose colors are all identical to the values already stored in the bus
- **THEN** `m_epoch` is not incremented
- **AND** every `SmartBusColor::Get(x, y)` returns the same color as before the call

#### Scenario: Single cell change bumps the epoch once
- **WHEN** exactly one cell's color differs from the stored bus value during `OutputToBus`
- **THEN** `m_epoch` increases by exactly 1 and `SmartBusColor::Get` at that coordinate returns the new color

### Requirement: Epoch-Diff Iteration for Output Handlers
The system SHALL let output handlers detect "nothing changed" without iterating: `SmartBusGeneric::begin(uint64_t* storedEpoch)` compares the caller's stored epoch against the bus epoch and returns `end()` immediately when they are equal; when they differ it updates the caller's stored epoch and returns an iterator over the full logical grid window.

#### Scenario: Stale epoch short-circuits iteration
- **WHEN** a handler calls `begin(&storedEpoch)` and `storedEpoch` equals the bus `m_epoch`
- **THEN** the returned iterator equals `end()` and no cells are visited

#### Scenario: New epoch yields a full sweep
- **WHEN** a handler calls `begin(&storedEpoch)` after the bus epoch has advanced
- **THEN** `storedEpoch` is set to the current bus epoch and the iterator visits every logical coordinate in the grid window, yielding a `Message` per cell

### Requirement: Grid Identity and Per-Route Bus Isolation
The system SHALL identify grids by IDs in [0, `x_numGridIds`) with `x_numGridIds` = 256. `GridIdAllocator::Alloc` claims the lowest free ID with an atomic exchange and returns the sentinel `x_numGridIds` when all 256 are taken; `Free` releases an ID for reuse. `SmartBusHolder` owns one `SmartBus` (input velocity bus plus output color bus) per grid ID, so writes through `PutColor`/`PutVelocity` for one grid ID never alter another route's state or epochs.

#### Scenario: Allocated IDs are unique
- **WHEN** two grids allocate IDs from the same `GridIdAllocator`
- **THEN** they receive distinct values, each less than 256

#### Scenario: Exhaustion returns the sentinel
- **WHEN** `Alloc` is called after all 256 IDs are allocated
- **THEN** it returns `x_numGridIds` (256), the "no grid ID" sentinel

#### Scenario: Routes are isolated
- **WHEN** `SmartBusHolder::PutColor(gridIdA, x, y, c)` stores a changed color
- **THEN** only the buses at `m_buses[gridIdA]` have their epochs incremented, and `GetColor(gridIdB, x, y)` for any other grid ID is unchanged

### Requirement: Periodic Bus I/O Cadence
The system SHALL drive each grid's bus traffic from `AbstractGrid::ProcessStatic(dt)`: incoming bus state is applied via `ApplyFromBus()` on every call, while `OutputToBus()` publication is throttled to at most once per `x_busIOInterval` (0.005 seconds) of accumulated `dt`.

#### Scenario: Output publication is throttled
- **WHEN** `ProcessStatic` is called repeatedly with `dt` values summing to less than 0.005 seconds since the last publication
- **THEN** `OutputToBus()` is not called again, while `ApplyFromBus()` runs on every invocation

### Requirement: PadUI Bridge Between UI and Buses
The system SHALL expose pad state to UI components through `PadUI`, which binds logical coordinates and a route ID to a `SmartBusColor` for readback and a `MessageInBus` for interaction. `PadUI::GetColor()` atomically reads `SmartBusColor::Get(m_x, m_y)`; `OnPress(timestamp)` pushes a `MessageIn` with mode `PadPress`, the route ID, the pad coordinates, and amount 1; `OnRelease(timestamp)` pushes mode `PadRelease` with amount 0. `PadUIGrid` is a factory that mints `PadUI` handles sharing one route's buses.

#### Scenario: UI readback reflects the color bus
- **WHEN** the color bus stores color c at (x, y) and a `PadUI` bound to (x, y) calls `GetColor()`
- **THEN** the call returns c without locking

#### Scenario: UI press enqueues a routed timestamped message
- **WHEN** `PadUI::OnPress(t)` is called on a pad bound to route r at (x, y)
- **THEN** the `MessageInBus` receives a `MessageIn` with timestamp t, route r, mode `PadPress`, coordinates (x, y), and amount 1

### Requirement: Flash Overlay for State Cells
The system SHALL support a flash overlay on state-driven cells: a `Flash<StateClass>` policy reports `IsFlashing()` true exactly when `*m_state == m_myState`, and a flashing `StateCell` reports its flash color variant from `GetColor()` (`m_onFlashColor` when its own state matches, `m_offFlashColor` otherwise) instead of the normal on/off colors. `BoolFlash` and `NoFlash` provide pointer-driven and always-off policies respectively.

#### Scenario: Matching flash state selects the flash color
- **WHEN** a `StateCell` with a `Flash` policy is queried via `GetColor()` while the flash state pointer equals the policy's `m_myState`
- **THEN** the cell returns `m_onFlashColor` if its own state matches `m_myState`, else `m_offFlashColor`

#### Scenario: NoFlash cells never flash
- **WHEN** a `StateCell` constructed with the `NoFlash` policy is queried via `GetColor()`
- **THEN** it returns `m_onColor` or `m_offColor` according to state match, never a flash color
