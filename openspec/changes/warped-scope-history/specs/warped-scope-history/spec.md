## ADDED Requirements

### Requirement: Shared warped scope reads
Bound scopes SHALL construct cycle axes in the continuous Theory of Time master-phase coordinate and read real-time scope history through the chosen inverse. Scope origins, decimation, and existing LOD delays SHALL be respected. Unavailable inverse or sample history SHALL produce an unavailable point. Modulation overlays SHALL use the same mapped real timestamp as their scope axis.

#### Scenario: Nonuniform forward time
- **WHEN** two real-time intervals cover different warped distances
- **THEN** scope points and overlays occupy their warped positions while reading the corresponding real samples

### Requirement: Reversible scope boundaries
Event-scope markers SHALL retain real and warped positions. Backward travel SHALL undo every crossed end and start, reopening paired starts and restoring prior interval state. Backward start callbacks, including envelope triggers, SHALL NOT append a scope start. Existing starts at unchanged coordinates SHALL remain permitted. Transport phase reset SHALL NOT itself be interpreted as rewind.

#### Scenario: Rewind across multiple boundaries
- **WHEN** travel descends through an end, its start, and an earlier end
- **THEN** the corresponding intervals reopen, disappear, and reopen in reverse order

#### Scenario: Backward envelope trigger
- **WHEN** an envelope triggers while the continuous coordinate decreases
- **THEN** its scope records no new start

#### Scenario: Transport restart
- **WHEN** musical phase resets while the continuous coordinate stays unchanged
- **THEN** the existing restart start is accepted without undoing older scope history

### Requirement: Bounded concurrent publication
UI inverse and scope reads SHALL use race-free shared payload access and a published real-time cutoff. Reused inverse coordinates outside the retained span and interpolation endpoints newer than the cutoff SHALL be rejected. Marker acceptance SHALL use a coherent publication while the writer remains nonblocking.

#### Scenario: Writer advances during display
- **WHEN** an inverse endpoint is overwritten with a timestamp after the accepted publication
- **THEN** the UI query is unavailable rather than interpolating that endpoint into accepted history

### Requirement: Continuous phase clock and delay reads
The continuous coordinate SHALL equal modulated musical phase plus a constant offset while running. Tempo edits SHALL affect future travel without rescaling stored coordinates. Stopped samples SHALL advance by the nominal phase increment. Restart SHALL rebase the offset while preserving continuous phase. Delay read heads SHALL project and wrap continuous phase into their selected loop strictly behind the write head, with real-time analysis-window safety applied after inversion.

#### Scenario: Tempo edit retains history
- **WHEN** tempo changes after samples have been recorded
- **THEN** their phase coordinates and inverse interpretation remain unchanged
- **AND** future phase travel follows the new increment

#### Scenario: All speeds survive restart
- **WHEN** transport restarts with fixed read speed, delay and loop selection
- **THEN** the read coordinate is unchanged for forward, fractional and reverse playback speeds

### Requirement: Explicit periodic axes
LFO and delay scopes SHALL publish a positive period and origin rather than infer a period from start markers. A display coordinate ahead of the current cursor SHALL use the corresponding point one period earlier. LFO periods SHALL cover all participating loop periods and their origin SHALL track the musical-to-continuous offset. Overlays SHALL use the same axis.

#### Scenario: Partial first cycle
- **WHEN** a periodic scope has recorded less than one full period
- **THEN** its axis retains the explicit full width and unavailable past history remains unavailable

#### Scenario: Backwards periodic travel
- **WHEN** the current phase moves backwards
- **THEN** the display selects coordinates from the latest logical period using the retained forward inverse, without undoing periodic start markers

### Requirement: Phase-distance LFO slew
Modulated LFO mix slew SHALL use the magnitude of the published modulated phase increment. Unmodulated warp generation and stopped settling SHALL use the nominal increment. Zero travel SHALL hold the mix, and large increments SHALL retain a finite coefficient in [0,1] without an audio cutoff clamp.

#### Scenario: Changing forward speed
- **WHEN** a fixed waveform traverses the same forward phase path at different speeds
- **THEN** the slew follows phase distance, up to coefficient and discrete integration error

### Requirement: Checked phase model
The repository SHALL provide Lean proofs of clock coherence, restart continuity, chosen-inverse soundness, periodic sampling, read-head bounds and ideal phase slew, with reachable-state Veil invariants. Source anchors SHALL associate transitions with C++ operations and reject unreviewed drift. Proof documentation SHALL state the floating-point, interpolation, finite-storage, concurrency and pure-signal boundaries.

#### Scenario: Verification run
- **WHEN** make lean-test succeeds
- **THEN** all listed contracts name existing checked theorems, source hashes and anchors match, and model axioms are restricted to the standard Lean whitelist


### Requirement: Compact branch-aware inverse
The main inverse SHALL retain a 32 MiB payload with independent phase retention,
exact phase cuts and relative float replacement timestamps. Each cell SHALL own
its fallback endpoints and at most one replacement. A new visit after leaving
the cell SHALL replace that replacement and reassign older islands below the new
cut to the fallback. Turns within a visit confined to the closed cell SHALL retain
the minimum cut and its first timestamp, moving it only for a deeper minimum.
Reverse observations SHALL NOT rewrite G or its forward frontier.

#### Scenario: A jump lies inside one cell
- **WHEN** a query falls to either side of the replacement cut
- **THEN** it uses that traversal's endpoints without blending across the jump

#### Scenario: Read lies before the first full cell traversal
- **WHEN** the forward frontier has not completed the cell
- **THEN** entry/frontier interpolation SHALL use a time interval confined to the cell
- **AND** unavailable history before a fractional initial entry SHALL remain unavailable

#### Scenario: Repeated turns occupy one cell
- **WHEN** a separate visit replaces an older cut
- **THEN** queries below the new cut SHALL use the preserved fallback even if they used the previous replacement
- **AND** reversal count SHALL NOT exhaust storage or grow lookup work

#### Scenario: Small turns remain within the cell
- **WHEN** a visit makes repeated forward and backward movements inside the cell
- **THEN** the replacement cut SHALL retain the minimum phase and the first timestamp at that minimum
- **AND** the forward endpoint SHALL update without adding boundary records

#### Scenario: The visit touches the right boundary
- **WHEN** forward travel reaches the right boundary exactly and then turns back inside the cell
- **THEN** coalescing SHALL retain the minimum and reopen the live endpoint
- **AND** only travel beyond the boundary SHALL permit a new visit to replace that minimum

#### Scenario: Neighboring grid entry is overwritten
- **WHEN** a forward pass starts at the next cell's left edge
- **THEN** the preceding cell's owned fallback endpoint SHALL remain unchanged

### Requirement: Explicit inverse proof assumptions
The Lean inverse model SHALL state its finite spectral input contract separately
from finite retention, traversal confinement, floating-point perturbation and lower-speed
hypotheses. The current generator SHALL NOT be claimed to enforce the deferred
band limit. The model SHALL bound phase residual, conditional timestamp error,
and propagation into a pure logical signal.

#### Scenario: Band-limited warp reaches zero speed
- **WHEN** the chosen branch approaches a turnaround
- **THEN** a positive lower speed SHALL NOT be inferred merely from band limitation

### Requirement: Distinguish timestamp error from observable analysis error
The mathematical model SHALL distinguish inverse timestamp error from its effect
on paired analysis. It SHALL state the stationarity, logical-feature regularity,
analysis-support and stable synthesis-state hypotheses explicitly. Timestamp
thresholds in the density benchmark SHALL NOT alone be treated as audibility
thresholds.

#### Scenario: Both frames shift over a stationary partial
- **WHEN** both H-separated frames of an ideal resolved stationary partial shift by the same amount
- **THEN** measured phase advance and the wrapped frequency estimate remain unchanged
- **AND** a non-triggered leader update from equal prior state remains unchanged

#### Scenario: Tiny reversals stay inside a phase band
- **WHEN** the complete intervening trajectory stays inside a band of width w
- **THEN** outer-timestamp interpolation can retain a phase residual bounded by w without exact latest-branch identity
- **AND** analysis-feature stability requires a corresponding bound across both full shifted windows, not merely their starting points
