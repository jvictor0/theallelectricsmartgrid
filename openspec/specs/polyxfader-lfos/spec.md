# PolyXFader LFOs Specification

## Purpose
The PolyXFader LFOs (`private/src/PolyXFader.hpp`, `PolyXFaderInternal`) are the primary phase-synchronized low-frequency oscillators of Smart Grid One. They generate no phase of their own: each LFO derives its output by shaping and mixing the explicitly selected modulated or unmodulated absolute phases of the Theory of Time loops (see phasor-timebase), so every modulation contour stays locked to the global clock and sequencer structure. Each voice carries two SquiggleLFO instances wrapping `PolyXFaderInternal`, whose outputs are published to the global modulator manager and gathered into the fixed voice groups' `ModulatorValues` slots and consumed by the encoder parameter system (see encoder-parameter-system). A matching pair of four-channel SquiggleLFO instances on the `QuadLFOs` bank publishes the sources assigned to the four Quad groups' slots 6 and 7, with quarter-cycle channel offsets and Sample-and-Hold captured from each channel's active delay loop. The Theory of Time itself uses a dedicated instance for clock phase modulation. These are distinct from the free-running ganged random LFOs used for effect sends and wavetable drift, which are a separate system.

## Requirements

### Requirement: Phase Derived from Continuous Time Loop Phasors
Each LFO SHALL derive loop phase by multiplying the selected continuous Theory of Time master phase by the loop cycle ratio. Voice and quad LFOs SHALL use the delay’s modulated continuous clock. The Theory of Time phase-modulation LFO SHALL read the unmodulated continuous clock before advancing it for the current sample. The generated core SHALL support up to six loops supplied by Theory of Time. It SHALL reduce phase in double inside waveform evaluation before narrowing to float.

#### Scenario: Integer phase shifts preserve the waveform
- **WHEN** a topology change changes a loop’s absolute phase by an integer with waveform controls unchanged
- **THEN** that loop’s shaped waveform value is unchanged

#### Scenario: Stopped transport
- **WHEN** the transport stops
- **THEN** the LFO waveforms SHALL continue on their continuous clocks at nominal tempo
- **AND** restarting transport SHALL preserve each continuous clock coordinate at the point of rebasing

### Requirement: Center/Slope Weighted Loop Mixing
The system SHALL mix the shaped per-loop signals with weights computed from `m_center` and `m_slope`: the loop index nearest `center × size` (within ±0.25) receives weight 1, and weights taper linearly away from the center with slope `1 / (slope × size)` (a fixed taper of 2 when `slope × size < 0.5`), clamped at 0. The weighted sum is divided by the total weight, and each loop's contribution is additionally scaled by a per-voice external weight `m_externalWeights[i]`.
Weights are recomputed only when size, slope, or center changes. When the center sits below 0.25 or above `size − 0.25`, the boundary loop receives full weight.

#### Scenario: Center selects a single loop
- **WHEN** `m_center × m_size` equals 2.0 and the slope is small enough that adjacent loops' weights taper to 0
- **THEN** the output equals loop 2's shaped signal alone

#### Scenario: Wide slope blends neighboring loops
- **WHEN** the slope is raised so loops 1, 2, and 3 have nonzero weights
- **THEN** the output is their weight-normalized sum, producing a compound contour locked to all three loop rates

### Requirement: Frequency Multiplier and Phase Shift
Each LFO SHALL offset the incoming loop phasor by `m_phaseShift + 0.75` (wrapped into [0, 1)) and multiply the wrapped phase by `m_mult`, producing `mult` cycles per loop period; for a fractional multiplier, the final partial cycle is time-normalized and its amplitude scaled by the fractional part, so the waveform stays continuous.
The voice LFO multiplier is an exponential parameter spanning 1 to 16.

#### Scenario: Integer multiplier yields repeated cycles
- **WHEN** `m_mult` is 2.0
- **THEN** the shaped waveform completes two full cycles per cycle of each contributing time loop

#### Scenario: Fractional multiplier scales the partial cycle
- **WHEN** `m_mult` is 2.5
- **THEN** two full-amplitude cycles are followed by a final cycle whose amplitude is scaled by 0.5

The existing partial-lobe construction SHALL remain periodic in the incoming loop phase, including for fractional multipliers. Center/slope controls and topology-dependent external weights SHALL retain their existing mapping. The audio-rate processing SHALL be generated from Lean, using Theory of Time phase, ratios and phase travel as inputs.

Each loop's effective waveform multiplier SHALL be the smaller of the mapped Mult and the nominal rate ceiling divided by that loop's nominal rate. The rate ceiling SHALL equal the maximum internal tempo times the largest ratio reachable from the grid. A loop SHALL retain its own effective multiplier when its blend weight changes or another loop changes rate.

#### Scenario: Mult at maximum nominal topology
- **WHEN** the internal tempo is at its maximum, all five links have multiplier five, and the mapped Mult exceeds one
- **THEN** the fastest loop uses effective Mult one while slower loops retain their own limits

#### Scenario: Fractional shaping remains loop-periodic
- **WHEN** multiplier is 2.5 and the absolute source phase differs by exactly one cycle
- **THEN** the per-loop waveform is identical, including the shortened half-amplitude final lobe

### Requirement: Attack-Fraction Skew
The system SHALL skew each cycle with `m_attackFrac` (clamped to [0.001, 0.999]): for cycle position t ≤ attackFrac the shaping function receives the rising ramp `t / attackFrac`, and for t > attackFrac it receives the mirrored falling ramp `1 − (t − attackFrac) / (1 − attackFrac)`, so the same shape is applied to both an attack segment and its time-reversed decay.

#### Scenario: Quarter attack fraction
- **WHEN** `m_attackFrac` is 0.25
- **THEN** the waveform rises over the first quarter of the cycle and falls over the remaining three quarters, peaking at cycle position 0.25

#### Scenario: Extreme skew is clamped
- **WHEN** `m_attackFrac` is set to 0
- **THEN** the effective attack fraction is 0.001 and no division by zero occurs

### Requirement: Continuous Shape Morph with Fixed Saturation
The system SHALL morph from the skewed ramp at Shape 0 to the raised cosine at Shape 0.5 and to the fixed gain-8 saturated cosine at Shape 1. Each half SHALL interpolate continuously between its endpoints. Mono, trio and quad callers SHALL share this mapping lane by lane.

### Requirement: Phase-Distance Slew and Triggered Sample-and-Hold
The system SHALL smooth the normalized mix using a one-pole with double state. Its cutoff SHALL scale with eight times the phase travel per sample and the largest product of loop ratio and that loop's effective multiplier among loops with nonzero geometric weight on the lane. The cutoff SHALL be capped at the maximum nominal internal tempo times the largest topology ratio reachable from the grid. At a 192 kHz engine sample rate this is 9,155.2734375 Hz, or 3,125/65,536 cycles per sample. The coefficient SHALL be calculated each sample using the same polynomial throughout that range. Running voice and quad LFOs SHALL use the magnitude of modulated continuous phase travel. The time-warp LFO and stopped LFOs SHALL use nominal travel.

The system SHALL capture the smoothed mix whenever the trigger input is asserted. A 500 Hz one-pole SHALL process the held value every sample, including at zero S+H fade. The raw output SHALL blend the filtered hold value with the smoothed live mix. The published output SHALL be `raw × amplitude + (1 − amplitude) × 0.5`. Voice triggers are supplied by the AHD gate. Quad triggers follow the accepted delay loop’s musical crossings.

### Requirement: Voice LFO Integration into the Modulation Matrix
The system SHALL run two SquiggleLFO instances per voice and publish their raw (pre-amplitude-centering) outputs and amplitudes to the corresponding globally indexed manager sources; the instrument's default voice groups SHALL gather these sources into slots 6 and 7; encoder cells consume these per-voice values as modulation sources (see encoder-parameter-system).
Voice and quad LFO scopes SHALL use continuous phase zero as the periodic origin and the reciprocal of the GCD of the active loop ratios as their period.

#### Scenario: LFO modulates a routed parameter per voice
- **WHEN** voice 4's LFO 1 outputs 0.8 and a parameter has modulation slot 6 at nonzero depth
- **THEN** the parameter's voice-4 channel output reflects 0.8 through the crossfade modulation math while other voices follow their own LFO values

### Requirement: Quad LFO Integration into the Modulation Matrix
The system SHALL run the two quad LFOs as four-lane `SquiggleLFOGeneric<4>` instances, one lane per quad channel, controlled from the `QuadLFOs` encoder bank through interpolated four-lane encoder reads (`GetValue<4>(param, 0)`). Each lane SHALL match an independent mono LFO given that channel's parameters, trigger and phase offset. The system SHALL publish their raw (pre-amplitude-centering) outputs and amplitudes to the corresponding globally indexed manager sources; the instrument's four Quad groups SHALL gather these sources into slots 6 and 7.
Phase-shift knobs SHALL offset channel `i` by `knob × i / 4`, so full spread places the four channels a quarter-cycle apart. Each channel's Sample-and-Hold trigger SHALL be the modulated cycle-boundary crossing of that channel's active delay read-head loop (`ReadTapeHead::m_loopSelector`), not the merely requested delay loop index.

#### Scenario: Quarter-cycle channel spread
- **WHEN** the quad LFO 1 phase-shift knob is 1.0
- **THEN** channels 0, 1, 2, and 3 are offset by 0, 0.25, 0.5, and 0.75 cycles

#### Scenario: Delay-loop top captures Sample-and-Hold
- **WHEN** channel 2's active delay loop reports a modulated cycle crossing and `m_shFade` is 1
- **THEN** that channel's pre-slew Sample-and-Hold value updates to the live mixed output on that sample and holds until the next crossing of the same active loop
