## MODIFIED Requirements

### Requirement: Phase Derived from Unmodulated Time Loop Phasors
Each LFO SHALL read absolute loop phases through the shared GetPhase API using an explicit Unmodulated or Modulated domain, retaining the domain selected by its current integration. The ToT phase-modulation LFO SHALL use Unmodulated. The LFO SHALL generate no independent phase accumulator and SHALL support up to 16 loops, with six in voice LFOs. It SHALL reduce phase in double inside waveform evaluation before narrowing to float; topology changes SHALL NOT be handled by locally reconstructing winding.

#### Scenario: Integer phase shifts preserve the waveform
- **WHEN** an aligned topology change changes a loop's absolute phase by an integer with waveform controls unchanged
- **THEN** that loop's shaped waveform value is unchanged

#### Scenario: Stationary phase
- **WHEN** phase and waveform controls stop changing
- **THEN** the shaped per-loop waveform remains constant


### Requirement: Frequency Multiplier and Phase Shift
Each LFO SHALL offset the incoming loop phasor by `m_phaseShift + 0.75` (wrapped into [0, 1)) and multiply the wrapped phase by `m_mult`, producing `mult` cycles per loop period; for a fractional multiplier, the final partial cycle is time-normalized and its amplitude scaled by the fractional part, so the waveform stays continuous.
The voice LFO multiplier is an exponential parameter spanning 1 to 16.

#### Scenario: Integer multiplier yields repeated cycles
- **WHEN** `m_mult` is 2.0
- **THEN** the shaped waveform completes two full cycles per cycle of each contributing time loop

#### Scenario: Fractional multiplier scales the partial cycle
- **WHEN** `m_mult` is 2.5
- **THEN** two full-amplitude cycles are followed by a final cycle whose amplitude is scaled by 0.5

The existing partial-lobe construction SHALL remain periodic in the incoming loop phase, including for fractional multipliers. Center/slope controls, topology-dependent external weights, quantization, sample-and-hold, and output slew SHALL retain existing behavior. No additional continuity guarantee SHALL be imposed on user control changes or the existing unmodulated/modulated topology-boundary mismatch.

#### Scenario: Fractional shaping remains loop-periodic
- **WHEN** multiplier is 2.5 and the absolute source phase differs by exactly one cycle
- **THEN** the per-loop waveform is identical, including the shortened half-amplitude final lobe
