## MODIFIED Requirements

### Requirement: Transport Run-Out and Reset
The system SHALL keep the phasor timebase running while either the user transport is on or any voice gate is still high, and SHALL reset the Multi-Phasor Gate and LameJuis lanes whenever the timebase is not running.
This lets sounding notes finish after the performer stops the transport, guarantees that the first selection after a restart registers as a pitch change (lane reset poisons the stored sections), and keeps accepted topology and periods current while the stopped timebase clears coordinates, gates, and crossing flags.

#### Scenario: Notes finish after stop
- **WHEN** the performer turns the transport off while a voice gate is high
- **THEN** the timebase keeps running until that gate deactivates
- **AND** once no gate is high the timebase stops and the Multi-Phasor Gate and LameJuis are reset

#### Scenario: Not-running frame updates stopped timebase contract
- **WHEN** the performer leaves the transport stopped
- **THEN** downstream sequencer and gate state is reset
- **AND** the timebase maintains accepted loop periods and topology while phases, positions, gates, and crossing flags remain cleared

### Requirement: Note Writer Recording
The system SHALL record note events through the NonagonNoteWriter: a note-on per trigger carrying the voice index, volt-per-octave pitch, start position (wrapped unmodulated global phase), and the three extra timbres; a note-off setting the event's end position; and a start-of-loop marker on an unmodulated global cycle-crossing event at slot 0.
A new note-on for a voice that still has an open note first closes the open note at the new start position. At the start-of-loop marker, voices with open notes are closed at position 1.0 and re-recorded starting at position 0, so events never span the loop boundary.

#### Scenario: Global boundary splits held notes
- **WHEN** the global loop crosses an unmodulated cycle boundary while voice 3 has an open note
- **THEN** the open note's end position is set to 1.0
- **AND** a new note-on with identical pitch and timbres is recorded at start position 0

### Requirement: Fixed Per-Frame Pipeline Order
The system SHALL execute the following order on every control frame: set timebase inputs and roll over the microblock buffer; record a start-of-loop marker if the global loop crossed an unmodulated cycle boundary; if AnyChangeInMicroBlock reports a change, run the index arp then LameJuis; if the timebase is running, run the Multi-Phasor Gate, otherwise reset both the Multi-Phasor Gate and LameJuis; set outputs and record notes; then process the timebase for samples 1 through 8.
Sample 0 of each microblock was computed in the previous frame (see phasor-timebase), so sequencer logic always runs on an already-computed boundary sample. During the final timebase processing loop, non-running samples SHALL use the stopped branch of `TheoryOfTimeBase::Process` to maintain accepted periods and topology while clearing phases, positions, gates, and crossing flags.

#### Scenario: Pipeline runs in order on a changing frame
- **WHEN** a control frame begins and at least one time loop's integer position changed in the microblock
- **THEN** the microblock rollover happens before any sequencer logic
- **AND** the index arp runs before LameJuis, LameJuis before the Multi-Phasor Gate, and outputs are set before the timebase computes samples 1 through 8

#### Scenario: Stopped frame maintains timebase state
- **WHEN** the user transport is not running and no voice gate keeps the timebase alive
- **THEN** the Multi-Phasor Gate and LameJuis are reset before outputs are set
- **AND** the timebase processing loop calls its not-running path for samples 1 through 8

### Requirement: Change-Gated Sequencer Evaluation
The system SHALL run the index arp and LameJuis only on frames where the phasor timebase reports a change in the microblock (position motion, startup, stop, or accepted topology edit); on all other frames the previous arp outputs and LameJuis pitches persist unchanged.

#### Scenario: Quiet frame preserves pitches
- **WHEN** no time loop position changes, no topology edit is accepted, and no transport transition occurs during a microblock
- **THEN** neither the index arp nor LameJuis is processed that frame
- **AND** every lane's selected pitch and trigger flags remain those of the last changing frame

### Requirement: Whole-Cycle Tick Wiring to the Index Arp
The system SHALL use AnyTick(loop) to supply loop clock events and read flags for dimensions selected by each lane's lens. It SHALL also supply each LameJuis input with its gate value and a separate AnyTick(loop) flag so input configuration accepts modulated crossings independently of Boolean gate changes. For a selected trio clock that ticked, it SHALL sample the signed whole-cycle coordinate GetGateStepIndex(clockSelect, 0, resetSelect). A tick SHALL remain observable when loop gate values repeat or self reset keeps the index at zero. With no clock selected (-1), total index SHALL be zero and no clock event supplied; a read without a clock SHALL reset the arp indices. A tick from another loop SHALL leave the selected clock's total index unchanged.

#### Scenario: Clock tick samples the whole-cycle coordinate
- **WHEN** trio 2's selected clock loop ticks during a changing microblock
- **THEN** its total index is GetGateStepIndex(clockSelect[2], 0, resetSelect[2])
- **AND** an unchanged loop gate does not suppress the clock

#### Scenario: Non-selected tick only updates matching reads
- **WHEN** only a loop other than trio 2's selected clock ticks
- **THEN** trio 2's total index keeps its previous value
- **AND** voices of lanes that read the ticked dimension get their read flag set

#### Scenario: Self reset keeps the clock event
- **WHEN** the selected clock ticks with itself as reset
- **THEN** total index is zero and a clock event is still supplied

#### Scenario: Rhythm edit does not clock the sequencer
- **WHEN** a gate, size, or reset edit occurs without a timebase crossing or other timebase change
- **THEN** the edit alone does not cause an arp clock or sequencer evaluation

### Requirement: Output Gate Lifecycle
The system SHALL set `m_output.m_gate[i]` true on the frame the Multi-Phasor Gate emits an AHD trigger for voice i, and SHALL set it false — recording a note-off in the note writer — on the first frame the Multi-Phasor Gate's gate for that voice is no longer high.
The Multi-Phasor Gate deactivates a voice's gate after half a voice period of absolute modulated global phase travel (see multi-phasor-gate); the Nonagon only mirrors that state into its output and the note writer.

#### Scenario: Gate turns off and records note end
- **WHEN** voice 7's Multi-Phasor Gate gate goes low while the Nonagon output gate is high
- **THEN** the note writer records a note-off for voice 7 at the current wrapped unmodulated global phase position
- **AND** `m_output.m_gate[7]` becomes false

## ADDED Requirements

### Requirement: Voice Cycle Ratios Use the Undoubled Clock and Read LCM
The system SHALL calculate each voice's positive signed 64-bit cycle ratio as the LCM of its selected clock loop cycle ratio and every read (non-co-muted) lens loop cycle ratio, starting at one when no clock is selected. Neither contribution SHALL be doubled. Multi-Phasor Gate SHALL capture globalPeriodSamples / voiceCycleRatio as the envelope period, while its independent note-gate cutoff SHALL remain 0.5 voice cycles. These changes SHALL leave clock frequency unchanged. AHD source phase ratio SHALL remain separately supplied from loop 0.

#### Scenario: Clock and read ratios combine without doubling
- **WHEN** the voice clock cycle ratio is 3, the only read loop cycle ratio is 4, and the global period is 480000 samples
- **THEN** the voice ratio is 12 and its captured envelope period is 40000 samples
- **AND** the note gate closes after an absolute modulated global phase distance of 1/24

#### Scenario: No clock and no read dimensions
- **WHEN** no voice clock is selected and all lens dimensions are co-muted
- **THEN** the voice cycle ratio is 1

## RENAMED Requirements

- FROM: `### Requirement: Monodromy Wiring to the Index Arp`
- TO: `### Requirement: Whole-Cycle Tick Wiring to the Index Arp`
