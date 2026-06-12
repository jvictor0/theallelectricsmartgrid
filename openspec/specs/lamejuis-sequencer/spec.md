# LameJuis Sequencer Specification

## Purpose
LameJuis (`private/src/LameJuis.hpp`, `private/src/HarmonicSheaf.hpp`, `private/src/IndexArp.hpp`) is the stateless pitch engine of the Nonagon. It maps the six time-loop gate bits from the phasor timebase (a point x in I⁶, see phasor-timebase) to just-intonation pitches through a matrix of logic operations feeding interval accumulators (the map M), a per-trio lens defining an equivalence on time slices, the resulting harmonic sheaf of available pitches, and a section choice strategy steered by the index arp. At each time slice the available notes and the chosen note are pure functions of x and the configuration, so time modulation reshapes the music without desynchronizing it.

## Requirements

### Requirement: Stateless Pitch Mapping from Gate Bits
The system SHALL compute all available pitches and each lane's chosen pitch as a pure function of the current 6-bit input vector (the six time-loop gates) and the performer's configuration, with no sequencer state beyond the previously chosen sections used for trigger detection.
The input vector is latched each time LameJuis processes; per-bit changed flags drive configuration latching and downstream updates.

#### Scenario: Same slice yields same pitch
- **WHEN** the timebase revisits the same 6-bit time slice under an unchanged configuration and the same choice argument
- **THEN** each lane channel selects the identical section and pitch as before

### Requirement: Logic Operations as Generalized Walsh Functions
The system SHALL evaluate 6 logic operations, each mapping the 6-bit input vector to a boolean: per bit a MatrixSwitch of Muted (ignore), Normal (use), or Inverted (use negated) yields an active/inverted mask; the operation output is `m_rhs[countHigh]`, a 7-entry lookup table indexed by the count of high bits after masking and inversion.
The default table is `rhs[j] = (j % 2 == 1)` — odd counts pass — which is parity (a Walsh function); editing the table generalizes this to any count-based boolean function.

#### Scenario: Default parity table
- **WHEN** an operation has bits 0 and 1 Normal (others Muted) and both bits are high with the default RHS table
- **THEN** countHigh is 2 and the operation outputs false
- **AND** with exactly one of the two bits high it outputs true

#### Scenario: Inversion flips a bit's contribution
- **WHEN** a bit's MatrixSwitch is Inverted and that input bit is high
- **THEN** the bit contributes 0 to countHigh, and contributes 1 when the input bit is low

### Requirement: Accumulators and the Pitch Map M
The system SHALL route each logic operation's output to exactly one of 3 accumulators (switch Down/Middle/Up selecting accumulator 2/1/0), where each accumulator has a just-intonation interval in volt-per-octave, and SHALL compute the pitch of a time slice as the sum over accumulators of `intervalValue × highCount`.
Available intervals are Off (0), Octave (1.0), Perfect Fifth (log₂ 3/2), Major Third (log₂ 5/4), Perfect Fourth, Minor Third, Whole Step, Half Step, and the 7th, 11th, 13th, and 31st harmonics. A section records, per accumulator, the count of operations targeting it and the count of those that are high, so M(x) is a product of simple intervals raised to small integer exponents.

#### Scenario: Pitch sums interval times high count
- **WHEN** a time slice drives 2 operations high into an Octave accumulator and 1 operation high into a Perfect Fifth accumulator
- **THEN** the evaluated pitch is 2 × 1.0 + 1 × 0.5849625 ≈ 2.585 V/oct

### Requirement: Lens and Time-Slice Equivalence
The system SHALL give each of the 3 lanes a lens: a 6-bit mask where bit 1 means the dimension is read (must agree for equivalence) and bit 0 means the dimension is co-muted (ignored for equivalence but still contributing to pitch through M); two slices are equivalent under lens U exactly when `(a XOR b) AND U == 0`.
The UI sets the lens per trio as co-mutes: lens bit i is 1 when dimension i is not co-muted.

#### Scenario: Equivalence ignores co-muted bits
- **WHEN** a lane's lens reads dimensions 0 and 1 (co-mutes 2–5)
- **THEN** slices 0b000011 and 0b111111 are equivalent
- **AND** slices 0b000001 and 0b000010 are not

### Requirement: Harmonic Sheaf of Available Pitches
The system SHALL define, for the current slice x and lens U, the sheaf F^M_x(U) = { M(y) | y ~_U x }, enumerated by iterating all 2^(number of co-muted dimensions) slices equivalent to x.
Sections for all 64 base points are cached and rebuilt whenever any matrix switch, RHS table entry, output switch, or accumulator interval changes.

#### Scenario: Sheaf size follows the co-dimension
- **WHEN** a lane co-mutes 4 of the 6 dimensions
- **THEN** its sheaf enumeration at any slice visits exactly 2⁴ = 16 sections

#### Scenario: Configuration change rebuilds the cache
- **WHEN** an accumulator's interval changes
- **THEN** all 64 cached sections are recomputed before the next lane selection

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

### Requirement: Index Arp Maps Monodromy to the Choice Argument
The system SHALL run one index arp per voice that converts the trio's monodromy count (see phasor-timebase) into a float: rhythmIndex = totalIndex mod rhythmLength and motiveIndex = totalIndex / rhythmLength (rhythm length 1–8, default `x_rhythmLength` = 8); on a clocked on-step the index becomes the ordinal of the current step among the on steps; the output is `offset + physicalIndex × interval + motiveIndex × pageInterval`, wrapped (triangle-folded when cycle is on, otherwise taken mod 1), optionally inverted (1 − value), then scaled to [min, max].
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

### Requirement: Pitch-Changed Triggers per Lane Channel
The system SHALL update a lane channel's selection only when that voice's read flag is set or its index arp triggered, and SHALL raise the channel's pitch-changed trigger exactly when the newly chosen section (including its evaluated value) differs from the previously stored one.
On lane reset (transport stop, see nonagon-sequencer), stored sections are poisoned so the first selection after restart always registers as changed.

#### Scenario: Unchanged selection does not trigger
- **WHEN** a channel updates and the chooser returns the same section and value as currently stored
- **THEN** the channel's trigger flag is false for that frame

#### Scenario: New section triggers
- **WHEN** a channel updates and the chosen section differs from the stored one
- **THEN** the trigger flag is true and the new section is stored

### Requirement: Extra Timbre Coefficients
The system SHALL expose, for every section, one coefficient per accumulator equal to `highCount / totalCount` (0 when no operations target the accumulator), yielding three values in [0, 1] that the Nonagon distributes to voices as extra timbres (see nonagon-sequencer).

#### Scenario: Coefficient is the high ratio
- **WHEN** an accumulator is targeted by 4 operations of which 3 are high in the selected section
- **THEN** that accumulator's timbre coefficient is 0.75

### Requirement: Configuration Latching on Input Transitions
The system SHALL latch performer edits to co-mutes and to a logic operation's matrix elements, RHS table, and output switch only on frames where a relevant input bit changes, and SHALL recompute an operation's output only on such frames; between transitions the previous gate values and configuration remain in effect.
For co-mutes, dimension i's setting latches when input bit i changes. For an operation, edits latch when any input bit changes that is non-muted in either the current or the requested element configuration.

#### Scenario: Co-mute edit waits for its bit to tick
- **WHEN** the performer toggles co-mute on dimension 4 while dimension 4's gate is static
- **THEN** the lane's lens is unchanged until the frame dimension 4's gate next flips
- **AND** on that frame the new lens takes effect
