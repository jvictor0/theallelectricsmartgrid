# Nonagon Sequencer Specification

## Purpose
The Nonagon (`TheNonagonInternal` in `private/src/TheNonagon.hpp`) is the core sequencer that wires the phasor timebase (see phasor-timebase), the LameJuis pitch engine and index arp (see lamejuis-sequencer), and the Multi-Phasor Gate (see multi-phasor-gate) into a single nine-voice polyphonic sequence generator. It runs the subcomponents in a fixed per-frame order, distributes outputs between them, emits per-voice gates, volt-per-octave pitches, and extra timbre values, and records note events into the note writer for MIDI and UI consumption.

## Requirements

### Requirement: Nine Voices in Three Trios
The system SHALL manage exactly 9 voices arranged in 3 trios — voices 0–2 (Water), 3–5 (Earth), and 6–8 (Fire) — where each trio shares a single LameJuis lane for its pitch logic.
Per-trio settings (clock select, reset select, co-mutes, section choice strategy, trigger modes, octave switches, unison master) apply to all three voices of the trio; per-voice settings (rhythm, range, mute) apply individually.

#### Scenario: Trio voices share one lane
- **WHEN** voice 4 needs a pitch
- **THEN** it reads channel 1 of LameJuis lane 1 (4 / 3 = trio 1, 4 mod 3 = channel 1)

### Requirement: Fixed Per-Frame Pipeline Order
The system SHALL execute the following order on every control frame: set timebase inputs and roll over the microblock buffer; record a start-of-loop marker if the master loop wrapped; if any time-loop position changed in the microblock, run the index arp then LameJuis; if the timebase is running, run the Multi-Phasor Gate, otherwise reset both the Multi-Phasor Gate and LameJuis; set outputs and record notes; then process the timebase for samples 1 through 8.
Sample 0 of each microblock was computed in the previous frame (see phasor-timebase), so sequencer logic always runs on an already-computed boundary sample.

#### Scenario: Pipeline runs in order on a changing frame
- **WHEN** a control frame begins and at least one time loop's integer position changed in the microblock
- **THEN** the microblock rollover happens before any sequencer logic
- **AND** the index arp runs before LameJuis, LameJuis before the Multi-Phasor Gate, and outputs are set before the timebase computes samples 1 through 8

### Requirement: Change-Gated Sequencer Evaluation
The system SHALL run the index arp and LameJuis only on frames where the phasor timebase reports an integer position change in the microblock; on all other frames the previous arp outputs and LameJuis pitches persist unchanged.

#### Scenario: Quiet frame preserves pitches
- **WHEN** no time loop position changes during a microblock
- **THEN** neither the index arp nor LameJuis is processed that frame
- **AND** every lane's selected pitch and trigger flags remain those of the last changing frame

### Requirement: Monodromy Wiring to the Index Arp
The system SHALL update each trio's arp total index from the timebase monodromy only when the trio's selected clock loop's gate changed in the microblock, and SHALL set per-voice read flags for voices whose lane reads (does not co-mute) a dimension whose gate ticked.
When a trio has no clock selected (clock select -1), its total index is set to 0 and its voices receive no clock; a read with no clock resets the arp indices.

#### Scenario: Clock tick samples monodromy
- **WHEN** trio 2's selected clock loop registers a gate change during a changing microblock
- **THEN** trio 2's arp total index is set to `MonodromyNumber(clockSelect[2], resetSelect[2])`

#### Scenario: Non-selected tick does not resample
- **WHEN** only a loop other than trio 2's selected clock ticks
- **THEN** trio 2's total index keeps its previous value
- **AND** voices of lanes that read the ticked dimension get their read flag set

### Requirement: Per-Voice Trigger Emission Rules
The system SHALL request a trigger for voice i when the timebase changed in the microblock and either (a) the trio's trig-on-pitch-changed mode is enabled and the lane channel's pitch changed, or (b) the trio's trig-on-sub-trigger mode is enabled and the voice's index arp triggered; subject to mute, unison, and interrupt rules evaluated by the trigger logic and consumed by the Multi-Phasor Gate (see multi-phasor-gate).
When a trio's unison master is set, all three voices of the trio evaluate the master voice's pitch-changed and sub-trigger flags. When interrupt is enabled from trio A to trio B, a triggering, unmuted lower-index voice in trio B cancels the trigger of a higher-index voice in trio A on the same frame. Muted voices never start a note.

#### Scenario: Pitch-changed trigger
- **WHEN** trig-on-pitch-changed is enabled for a trio and a lane channel selects a different section than before during a changing frame
- **THEN** a trigger is requested for the corresponding voice

#### Scenario: Interrupt cancels a higher voice
- **WHEN** interrupt is enabled for voice i's trio against voice j's trio, j < i, and voice j triggers unmuted on the same frame
- **THEN** voice i's trigger is cancelled for that frame

#### Scenario: Muted voice does not sound
- **WHEN** a voice is muted and its trigger condition is met
- **THEN** no AHD trigger is emitted and the voice's output gate stays false

### Requirement: Output Gate Lifecycle
The system SHALL set `m_output.m_gate[i]` true on the frame the Multi-Phasor Gate emits an AHD trigger for voice i, and SHALL set it false — recording a note-off in the note writer — on the first frame the Multi-Phasor Gate's gate for that voice is no longer high.
The Multi-Phasor Gate deactivates a voice's gate after half a voice period of master-phasor travel (see multi-phasor-gate); the Nonagon only mirrors that state into its output and the note writer.

#### Scenario: Gate turns off and records note end
- **WHEN** voice 7's Multi-Phasor Gate gate goes low while the Nonagon output gate is high
- **THEN** the note writer records a note-off for voice 7 at the current independent master phasor position
- **AND** `m_output.m_gate[7]` becomes false

### Requirement: Pitch Output with Trio Octave Switches
The system SHALL compute each triggered voice's volt-per-octave output by taking the selected lane pitch (using the unison master's channel when set) and applying the trio octave switches: `output = pitch + octave[trio] + SpreadOffset(spread[trio], voiceInTrio)`.
Spread offsets are: Tight adds 0 to all voices; OneUp adds 1 octave to the third voice; TwoUp adds 1 octave to the second and third voices; Loose adds the voice's index within the trio (0, 1, 2 octaves).

#### Scenario: Loose spread staggers octaves
- **WHEN** a trio with octave 0 and spread Loose triggers all three voices on a pitch of 0.585 V/oct
- **THEN** the voices output 0.585, 1.585, and 2.585 V/oct respectively

### Requirement: Extra Timbre Distribution
The system SHALL latch three extra timbre values per voice at trigger time, taken from the selected section's per-accumulator coefficients (high count divided by total count, in [0, 1]; see lamejuis-sequencer), and hold them until the voice next triggers.
The latched values are emitted in `m_output.m_extraTimbre[voice][0..2]` and recorded with the note event.

#### Scenario: Timbres latch on trigger
- **WHEN** a voice triggers while its selected section has 2 of 4 operations high on an accumulator
- **THEN** the corresponding extra timbre output for that voice is 0.5
- **AND** the value persists unchanged until the voice's next trigger

### Requirement: Note Writer Recording
The system SHALL record note events through the NonagonNoteWriter: a note-on per trigger carrying the voice index, volt-per-octave pitch, start position (the independent master phasor), and the three extra timbres; a note-off setting the event's end position; and a start-of-loop marker each time the master loop wraps (independent top).
A new note-on for a voice that still has an open note first closes the open note at the new start position. At the start-of-loop marker, voices with open notes are closed at position 1.0 and re-recorded starting at position 0, so events never span the loop boundary.

#### Scenario: Master wrap splits held notes
- **WHEN** the master loop wraps while voice 3 has an open note
- **THEN** the open note's end position is set to 1.0
- **AND** a new note-on with identical pitch and timbres is recorded at start position 0

### Requirement: Transport Run-Out and Reset
The system SHALL keep the phasor timebase running while either the user transport is on or any voice gate is still high, and SHALL reset the Multi-Phasor Gate and LameJuis lanes whenever the timebase is not running.
This lets sounding notes finish after the performer stops the transport, and guarantees that the first selection after a restart registers as a pitch change (lane reset poisons the stored sections).

#### Scenario: Notes finish after stop
- **WHEN** the performer turns the transport off while a voice gate is high
- **THEN** the timebase keeps running until that gate deactivates
- **AND** once no gate is high the timebase stops and the Multi-Phasor Gate and LameJuis are reset
