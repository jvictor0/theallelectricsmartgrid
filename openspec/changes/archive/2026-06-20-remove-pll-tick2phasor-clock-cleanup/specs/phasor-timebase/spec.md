## ADDED Requirements

### Requirement: Internal-Only Clock Source
The system SHALL remove `TheoryOfTime::ClockMode` and `TheoryOfTime::Input::m_clockMode`.
The timebase SHALL advance from the internal frequency input directly; it SHALL NOT expose PLL, external phasor input, or tick-to-phasor input as selectable product clock modes.

#### Scenario: Clock mode surface is absent
- **WHEN** product code constructs a `TheoryOfTime::Input`
- **THEN** it has no `m_clockMode` member
- **AND** no `ClockMode` enum is available

#### Scenario: Internal clock advances the phasor
- **WHEN** the timebase processes a running sample with internal frequency `m_freq`
- **THEN** the true phasor advances by `m_freq`
- **AND** the phasor wraps into [0, 1)

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

## MODIFIED Requirements

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

## REMOVED Requirements

### Requirement: PLL Clock Input
**Reason**: PLL clocking is being removed from the product before new MIDI input work so the remaining clock surface is explicit and internal-only.
**Migration**: Use the internal frequency input directly. Future external synchronization will be introduced by a separate change.

#### Scenario: PLL state is absent
- **WHEN** the product is built after this change
- **THEN** no PLL input state, PLL process method, PLL hit forwarding method, or PLL encoder parameter is available

### Requirement: Tick-To-Phasor Clock Input
**Reason**: Tick-to-phasor input is being removed from the product before new MIDI input work so incoming clock behavior can be reintroduced deliberately.
**Migration**: Use the internal frequency input directly. Outgoing phasor-to-tick clock generation remains available through `Phasor2Tick`.

#### Scenario: Tick-to-phasor state is absent
- **WHEN** the product is built after this change
- **THEN** no tick-to-phasor input state or tick-to-phasor process path is available
