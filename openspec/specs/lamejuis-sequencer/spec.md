# LameJuis Sequencer Specification

## Purpose
LameJuis (`private/src/LameJuis.hpp`, `private/src/HarmonicSheaf.hpp`, `private/src/IndexArp.hpp`) is the stateless pitch engine of the Nonagon. It maps the six time-loop gate bits from the phasor timebase (a point x in I⁶, see phasor-timebase) to just-intonation pitches through a matrix of logic operations feeding interval accumulators (the map M), a per-trio lens defining an equivalence on time slices, the resulting harmonic sheaf of available pitches, and a section choice strategy steered by the index arp. For a fixed accepted configuration and choice argument, pitch selection is deterministic. Requested edits become accepted configuration on the relevant input ticks; each channel retains its selected section until its next scheduled read or arp trigger.

## Requirements

### Requirement: Stateless Pitch Mapping from Gate Bits
The system SHALL compute all available pitches and each lane's chosen pitch as a pure function of the current 6-bit input vector (the six time-loop gates), the accepted configuration, and the choice argument. Requested configuration edits SHALL remain pending until their specified acceptance events, and each lane channel SHALL retain its previous selection between scheduled updates.
The input vector and separate per-bit tick flags SHALL be supplied each time LameJuis processes. Tick flags SHALL report modulated loop crossings even when the Boolean gate value is unchanged.

#### Scenario: Same slice yields same pitch
- **WHEN** the timebase revisits the same 6-bit time slice under an unchanged accepted configuration and the same choice argument
- **THEN** each updating lane channel selects the identical section and pitch as before

### Requirement: Logic Operations as Generalized Walsh Functions
The system SHALL evaluate 6 logic operations, each mapping the 6-bit input vector to a boolean: per bit a MatrixSwitch of Muted (ignore), Normal (use), or Inverted (use negated) yields an active/inverted mask. An operation with at least one accepted non-muted input SHALL output `m_rhs[countHigh]`, a 7-entry lookup table indexed by the count of high bits after masking and inversion. An operation with no accepted non-muted inputs SHALL output false for every time slice, including when `m_rhs[0]` is true.
The default table is `rhs[j] = (j % 2 == 1)` — odd counts pass — which is parity (a Walsh function); editing the table generalizes this to any count-based boolean function on an active row.

#### Scenario: Default parity table
- **WHEN** an operation has bits 0 and 1 Normal (others Muted) and both bits are high with the default RHS table
- **THEN** countHigh is 2 and the operation outputs false
- **AND** with exactly one of the two bits high it outputs true

#### Scenario: Inversion flips a bit's contribution
- **WHEN** a bit's MatrixSwitch is Inverted and that input bit is high
- **THEN** the bit contributes 0 to countHigh, and contributes 1 when the input bit is low

#### Scenario: Empty row stays false
- **WHEN** the last non-muted input's mute edit is accepted while RHS entry zero is true
- **THEN** the operation's countTotal and countHigh are zero and its output is false
- **AND** it stays false until an unmute is accepted on that input's tick

### Requirement: Accumulators and the Pitch Map M
The system SHALL assign each logic operation an output target among 3 accumulators (switch Down/Middle/Up selecting accumulator 2/1/0), where each accumulator has a just-intonation interval in volt-per-octave. Only operations with at least one accepted non-muted input SHALL contribute to an accumulator: each such operation contributes one to that accumulator's total count and, when true, one to its high count. An empty row SHALL contribute to neither count in any accumulator, regardless of its output target.
The pitch of a time slice SHALL be the sum over accumulators of `intervalValue × highCount`. Available intervals are Off (0), Octave (1.0), Perfect Fifth (log₂ 3/2), Major Third (log₂ 5/4), Perfect Fourth, Minor Third, Whole Step, Half Step, and the 7th, 11th, 13th, and 31st harmonics. Each section SHALL record the three total counts and high counts separately from the per-operation count of active input bits.

#### Scenario: Pitch sums interval times high count
- **WHEN** a time slice drives 2 operations high into an Octave accumulator and 1 operation high into a Perfect Fifth accumulator
- **THEN** the evaluated pitch is 2 × 1.0 + 1 × 0.5849625 ≈ 2.585 V/oct

#### Scenario: Last mute removes a row from every accumulator
- **WHEN** the last non-muted input's mute edit is accepted for a row
- **THEN** the next sheaf rebuild in that processing frame excludes the row from all accumulator totals and high counts
- **AND** editing its output target while all its inputs remain muted does not make it contribute anywhere

### Requirement: Lens and Time-Slice Equivalence
The system SHALL give each of the 3 lanes a lens: a 6-bit mask where bit 1 means the dimension is read (must agree for equivalence) and bit 0 means the dimension is co-muted (ignored for equivalence but still contributing to pitch through M); two slices are equivalent under lens U exactly when `(a XOR b) AND U == 0`.
The UI sets the lens per trio as co-mutes: lens bit i is 1 when dimension i is not co-muted. Lane initialization SHALL initialize the grid's time-slice coordinate converter with the initial accepted lens, before any co-mute edits occur.

#### Scenario: Equivalence ignores co-muted bits
- **WHEN** a lane's lens reads dimensions 0 and 1 (co-mutes 2–5)
- **THEN** slices 0b000011 and 0b111111 are equivalent
- **AND** slices 0b000001 and 0b000010 are not

#### Scenario: Initial grid agrees with the accepted lens
- **WHEN** a fresh lane starts with all dimensions read and the performer changes matrix, rhythm, topology, or reset controls without editing co-mutes
- **THEN** grid cells map to the correct time slices and cached sections under that lens
- **AND** toggling a co-mute and restoring the original setting does not repair or otherwise alter the mapping for the same accepted configuration

### Requirement: Harmonic Sheaf of Available Pitches
The system SHALL define, for the current slice x and lens U, the sheaf F^M_x(U) = { M(y) | y ~_U x }, enumerated by iterating all 2^(number of co-muted dimensions) slices equivalent to x.
Sections for all 64 base points SHALL be cached and rebuilt after any accepted matrix switch, RHS table entry, output switch, or accumulator interval change, before lane selection in the same processing frame. Pending edits SHALL NOT alter the cache. Rebuilds SHALL use accepted active-input masks and exclude empty rows.

#### Scenario: Sheaf size follows the co-dimension
- **WHEN** a lane co-mutes 4 of the 6 dimensions
- **THEN** its sheaf enumeration at any slice visits exactly 2⁴ = 16 sections

#### Scenario: Configuration change rebuilds the cache
- **WHEN** an accumulator's interval changes
- **THEN** all 64 cached sections are recomputed before the next lane selection

#### Scenario: Row activation and removal rebuild immediately
- **WHEN** an input tick accepts the first unmute or the last mute for a row
- **THEN** all 64 cached sections reflect the row's resulting participation before lane selection in that processing frame

### Requirement: Active-Trio RHS Count Lighting
The system SHALL light RHS count column k for a logic operation when there exists an assignment of the active trio's co-muted input bits such that, paired with the current non-co-muted input bits, that operation's countHigh equals k.
The cell's toggle state remains the RHS table entry `m_rhs[k]`. Columns whose count exceeds the operation's number of active bits remain dimmed.
Because each co-muted active bit independently contributes 0 or 1 to countHigh, the lit columns are the contiguous interval from the count contributed by the read active bits through that count plus the number of co-muted active bits.

#### Scenario: No co-mutes lights the current count only
- **WHEN** the active trio co-mutes no dimensions and an operation's current countHigh is 2
- **THEN** only column 2 flashes

#### Scenario: Co-muted active bits light a contiguous range
- **WHEN** an operation treats bits 0 and 1 as Normal, the active trio co-mutes bit 1, and the current value of bit 0 is high
- **THEN** columns 1 and 2 flash
- **AND** columns 0 and 3–6 do not flash

#### Scenario: Co-muting a muted bit does not add counts
- **WHEN** an operation mutes bit 2 and the active trio co-mutes only bit 2
- **THEN** the flashing column is the current countHigh from the remaining bits

### Requirement: Section Choice Strategies
The system SHALL select one section from the sheaf per lane channel using a section choice strategy with the index arp output as the choice argument: None (zero section), Lowest (lowest evaluated pitch), GCD (component-wise minimum of high counts), Closest (pitch nearest the argument), ClosestModOne (nearest modulo one octave, placed in the argument's octave with ±1 adjustment when closer; the default), and Percentile (sorts the class by pitch and indexes with the fractional part of the argument, adding its integer part as octaves).
Each lane also has a base strategy (default None); the chooser first runs the base strategy and adds its evaluated value to the choice argument before running the main strategy, allowing strategy composition.

#### Scenario: ClosestModOne wraps into the argument's octave
- **WHEN** the choice argument is 2.3 and the class's pitch classes are {0.0, 0.585}
- **THEN** the section with pitch class 0.585 is chosen (circular distance 0.285 < 0.3)
- **AND** the reported value is placed near the argument as 2.585

#### Scenario: Percentile indexes the sorted class
- **WHEN** the choice argument is 1.6 over a 16-element equivalence class
- **THEN** the section at sorted index floor(0.6 × 16) = 9 is chosen
- **AND** 1 (the integer part) is added to the chosen value as an octave offset

### Requirement: Index Arp Maps Gate-Step Indices to the Choice Argument
The system SHALL run one index arp per voice that converts the trio's signed whole-cycle gate-step index, sampled through AnyTick and GetGateStepIndex (see phasor-timebase) into a float: rhythmIndex = FloorMod(totalIndex, rhythmLength) and motiveIndex = FloorDiv(totalIndex, rhythmLength) (rhythm length 1–8, default `x_rhythmLength` = 8); on a clocked on-step the index becomes the ordinal of the current step among the on steps; the output is `offset + physicalIndex × interval + motiveIndex × pageInterval`, wrapped (triangle-folded when cycle is on, otherwise taken mod 1), optionally inverted (1 − value), then scaled to [min, max].
Retro mode reverses the physical index as `numOnSteps − index`. The output is recomputed on a trigger or a read flag and passed to the lane chooser as the choice argument. Per trio, voice ranges stack: voice 0's min is 0, each later voice's min is the previous voice's max minus `zoneHeight × zoneOverlap`, and each max is min + zoneHeight.

#### Scenario: Gate-step index decomposes into rhythm and motive
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

#### Scenario: Equal loop gate values still clock the arp
- **WHEN** the selected clock loop ticks between equal rhythm gate values
- **THEN** the arp receives a clock event and samples its whole-cycle step index
- **AND** its per-voice rhythm is evaluated independently of the loop's gate pattern

### Requirement: Pitch-Changed Triggers per Lane Channel
The system SHALL update a lane channel's selection only when that voice's read flag is set or its index arp triggered, and SHALL raise the channel's pitch-changed trigger exactly when the newly chosen section or its evaluated value differs from the previously stored one. Section equality SHALL compare every accumulator's high count and total count, including totals whose high count is zero. A denominator-only change MAY therefore request a new note at the same pitch, subject to the existing trigger and gate controls; it SHALL NOT schedule an additional read or bypass the channel's rhythmic update timing.
On lane reset (transport stop, see nonagon-sequencer), stored sections are poisoned so the first selection after restart always registers as changed.

#### Scenario: Unchanged selection does not trigger
- **WHEN** a channel updates and the chooser returns the same section and value as currently stored
- **THEN** the channel's trigger flag is false for that frame

#### Scenario: New section triggers
- **WHEN** a channel updates and the chosen section differs from the stored one
- **THEN** the trigger flag is true and the new section is stored

#### Scenario: Denominator change is observed on the next allowed update
- **WHEN** a cache rebuild changes an accumulator's total count while its high count and evaluated pitch remain unchanged
- **THEN** a channel with neither a read nor an arp trigger retains its stored section and emits no pitch-changed trigger
- **AND** its next allowed selection emits a pitch-changed trigger if the newly selected section differs only in that total count, including a zero-high case such as 0/4 becoming 0/3

### Requirement: Extra Timbre Coefficients
The system SHALL expose, for every section, one coefficient per accumulator equal to `highCount / totalCount` (0 when no active operations target the accumulator), yielding three values in [0, 1]. Only rows with accepted non-muted inputs SHALL count toward either quantity. The Nonagon SHALL capture these coefficients per voice when that voice triggers and hold them until its next trigger (see nonagon-sequencer).

#### Scenario: Coefficient is the high ratio
- **WHEN** an accumulator is targeted by 4 active operations of which 3 are high in the selected section
- **THEN** that accumulator's timbre coefficient is 0.75

#### Scenario: Empty accumulator has zero timbre
- **WHEN** every row targeting an accumulator has all inputs muted in the accepted configuration
- **THEN** the section's total count and timbre coefficient for that accumulator are zero

### Requirement: Configuration Latching on Input Ticks
The system SHALL latch each co-mute setting and each logic-operation matrix element for dimension i only when input i reports a modulated loop tick, independently of whether its gate value changes. A tick on another dimension SHALL NOT accept dimension i's pending edit. An operation SHALL accept its RHS table and output target, and recompute its current output and input counts, when any ticked dimension is non-muted in either that row's accepted or requested matrix configuration. Accumulator intervals SHALL be accepted whenever LameJuis processes, without this per-input latching rule.
When the last non-muted input's mute is accepted, the row SHALL become false and contribute to no accumulator in that frame's sheaf rebuild. While all inputs remain muted, requested RHS and routing edits SHALL remain pending and the row SHALL remain inactive. A requested unmute SHALL reactivate it only on that input's next tick, accepting the current RHS and routing requests at the same time.

#### Scenario: Co-mute edit accepts an equal-gate tick
- **WHEN** the performer toggles co-mute on dimension 4 between its ticks and its next rhythm step repeats the same gate value
- **THEN** the lane's lens stays unchanged until dimension 4 ticks
- **AND** on that tick the new lens takes effect even though the gate did not flip

#### Scenario: Faster input does not accept a slower matrix edit
- **WHEN** dimension 4 has a pending matrix edit and dimension 0 ticks first
- **THEN** dimension 4's accepted matrix element and its contribution to the row's active-input count remain unchanged
- **AND** the edit is accepted when dimension 4 ticks, even if its gate repeats

#### Scenario: Reactivation accepts pending row controls
- **WHEN** all row inputs are muted, the performer edits its RHS and target and requests dimension 2 Normal
- **THEN** other input ticks leave the row inactive and its RHS and target pending
- **AND** dimension 2's next tick accepts its unmute, the requested RHS and target, and the resulting row contribution
