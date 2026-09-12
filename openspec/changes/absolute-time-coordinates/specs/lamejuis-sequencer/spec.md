## MODIFIED Requirements

### Requirement: Index Arp Maps Monodromy to the Choice Argument
The system SHALL run one index arp per voice that converts the trio's gate-step index (see phasor-timebase) into a float: rhythmIndex = FloorMod(totalIndex, rhythmLength) and motiveIndex = FloorDiv(totalIndex, rhythmLength) (rhythm length 1–8, default `x_rhythmLength` = 8); on a clocked on-step the index becomes the ordinal of the current step among the on steps; the output is `offset + physicalIndex × interval + motiveIndex × pageInterval`, wrapped (triangle-folded when cycle is on, otherwise taken mod 1), optionally inverted (1 − value), then scaled to [min, max].
Retro mode reverses the physical index as `numOnSteps − index`. The output is recomputed on a trigger or a read flag and passed to the lane chooser as the choice argument. Per trio, voice ranges stack: voice 0's min is 0, each later voice's min is the previous voice's max minus `zoneHeight × zoneOverlap`, and each max is min + zoneHeight.

#### Scenario: Monodromy decomposes into rhythm and motive
- **WHEN** the trio's total index is 11 with rhythm length 8
- **THEN** the rhythm index is 3 and the motive index is 1
- **AND** if only steps 0 and 3 of the first four are on, the step index is 1

#### Scenario: Off-step produces no trigger
- **WHEN** the clock advances onto a rhythm step that is off
- **THEN** the arp does not trigger and its output is only refreshed if the read flag is set

#### Scenario: Output is scaled into the voice's zone
- **WHEN** an arp with min 0.25, max 0.75 computes a pre-scale value of 0.5
- **THEN** the emitted choice argument is 0.25 + 0.5 × 0.5 = 0.5

The absolute gate-step and motive coordinates SHALL retain signed 64-bit range through decomposition and SHALL NOT narrow to int or float before bounded output reduction. Existing retro, inversion, folding, trigger/read, and voice-zone behavior SHALL remain unchanged.

#### Scenario: Negative step uses the preceding motive
- **WHEN** totalIndex is -1 and rhythmLength is 8
- **THEN** rhythmIndex is 7 and motiveIndex is -1, with no negative array access

#### Scenario: Index exceeds 32-bit range
- **WHEN** totalIndex is 4294967299 and rhythmLength is 8
- **THEN** rhythmIndex is 3 and motiveIndex is 536870912
