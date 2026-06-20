## MODIFIED Requirements

### Requirement: Fixed Per-Frame Pipeline Order
The system SHALL execute the following order on every control frame: set timebase inputs and roll over the microblock buffer; record a start-of-loop marker if the master loop wrapped; if any time-loop position changed in the microblock, run the index arp then LameJuis; if the timebase is running, run the Multi-Phasor Gate, otherwise reset both the Multi-Phasor Gate and LameJuis; set outputs and record notes; then process the timebase for samples 1 through 8.
Sample 0 of each microblock was computed in the previous frame (see phasor-timebase), so sequencer logic always runs on an already-computed boundary sample. During the final timebase processing loop, non-running samples SHALL use the timebase `ProcessNotRunning` path so stopped loop sizes, indexes, gates, and related variables are maintained even when the user transport is not running.

#### Scenario: Pipeline runs in order on a changing frame
- **WHEN** a control frame begins and at least one time loop's integer position changed in the microblock
- **THEN** the microblock rollover happens before any sequencer logic
- **AND** the index arp runs before LameJuis, LameJuis before the Multi-Phasor Gate, and outputs are set before the timebase computes samples 1 through 8

#### Scenario: Stopped frame maintains timebase state
- **WHEN** the user transport is not running and no voice gate keeps the timebase alive
- **THEN** the Multi-Phasor Gate and LameJuis are reset before outputs are set
- **AND** the timebase processing loop calls its not-running path for samples 1 through 8

### Requirement: Transport Run-Out and Reset
The system SHALL keep the phasor timebase running while either the user transport is on or any voice gate is still high, and SHALL reset the Multi-Phasor Gate and LameJuis lanes whenever the timebase is not running.
This lets sounding notes finish after the performer stops the transport, guarantees that the first selection after a restart registers as a pitch change (lane reset poisons the stored sections), and keeps the timebase's stopped loop sizes, loop indexes, and related stopped variables current through the phasor-timebase not-running path.

#### Scenario: Notes finish after stop
- **WHEN** the performer turns the transport off while a voice gate is high
- **THEN** the timebase keeps running until that gate deactivates
- **AND** once no gate is high the timebase stops and the Multi-Phasor Gate and LameJuis are reset

#### Scenario: Not-running frame updates stopped timebase contract
- **WHEN** the performer leaves the transport stopped
- **THEN** downstream sequencer and gate state is reset
- **AND** the timebase maintains stopped loop sizes, loop indexes, gates, phasors, and related variables according to accepted loop multipliers
