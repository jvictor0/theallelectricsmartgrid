# PolyXFader LFOs Specification

## Purpose
The PolyXFader LFOs (`private/src/PolyXFader.hpp`, `PolyXFaderInternal`) are the primary phase-synchronized low-frequency oscillators of Smart Grid One. They generate no phase of their own: each LFO derives its output by shaping and mixing the explicitly selected modulated or unmodulated absolute phases of the Theory of Time loops (see phasor-timebase), so every modulation contour stays locked to the global clock and sequencer structure. Each voice carries two SquiggleLFO instances wrapping `PolyXFaderInternal`, whose outputs are published to the global modulator manager and gathered into the fixed voice groups' `ModulatorValues` slots and consumed by the encoder parameter system (see encoder-parameter-system). A matching pair of four-channel SquiggleLFO instances on the `QuadLFOs` bank publishes the sources assigned to the Quad group's slots 6 and 7, with quarter-cycle channel offsets and Sample-and-Hold captured from each channel's active delay loop. The Theory of Time itself uses a dedicated instance for clock phase modulation. These are distinct from the free-running ganged random LFOs used for effect sends and wavetable drift, which are a separate system.

## Requirements

### Requirement: Phase Derived from Unmodulated Time Loop Phasors
Each LFO SHALL read absolute loop phases through the shared GetPhase API using an explicit Unmodulated or Modulated domain, retaining the domain selected by its current integration. The ToT phase-modulation LFO SHALL use Unmodulated. The LFO SHALL generate no independent phase accumulator and SHALL support up to 16 loops, with six in voice LFOs. It SHALL reduce phase in double inside waveform evaluation before narrowing to float; topology changes SHALL NOT be handled by locally reconstructing winding.

#### Scenario: Integer phase shifts preserve the waveform
- **WHEN** an aligned topology change changes a loop's absolute phase by an integer with waveform controls unchanged
- **THEN** that loop's shaped waveform value is unchanged

#### Scenario: Stationary phase
- **WHEN** phase and waveform controls stop changing
- **THEN** the shaped per-loop waveform remains constant

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

The existing partial-lobe construction SHALL remain periodic in the incoming loop phase, including for fractional multipliers. Center/slope controls, topology-dependent external weights, quantization, sample-and-hold, and output slew SHALL retain existing behavior. No additional continuity guarantee SHALL be imposed on user control changes or the existing unmodulated/modulated topology-boundary mismatch.

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

### Requirement: Continuous Shape Morph with Driven Sine
The system SHALL morph the waveform with `m_shape`, applied to each skewed ramp `in`. At shape 0 the ramp passes unchanged, a triangle at centered skew. From 0 to 0.5 the ramp crossfades toward the raised-cosine sine `(1 − cos(2π · in / 2)) / 2` with sine weight `shape × 2`. Above 0.5 the bipolar sine passes through the normalized fast tanh `(tanh(drive · (2 · sine − 1)) / tanh(drive) + 1) / 2`, with `drive = ExpParam::Compute(0.25, 8, 2 · shape − 1)`: exponential from 0.25 just above 0.5 to 8 at shape 1. Mono, trio and quad callers SHALL share this generic mapping lane by lane. No quantizer or quantizer history is used.
Normalization keeps every shape in [0, 1]. Because the minimum drive is positive, the waveform steps slightly just above shape 0.5 rather than continuing exactly from the sine.

#### Scenario: Shape zero is the skewed ramp
- **WHEN** `m_shape` is 0
- **THEN** each loop's shaped value equals its skewed ramp

#### Scenario: Shape one half is the sine
- **WHEN** `m_shape` is 0.5
- **THEN** each loop's shaped value is the raised-cosine sine of its skewed ramp

#### Scenario: Full shape drives the sine at 8
- **WHEN** `m_shape` is 1
- **THEN** the drive is 8, the endpoints of the ramp still map to 0 and 1, and the waveform approaches a square

#### Scenario: Drive starts at 0.25 above the sine boundary
- **WHEN** `m_shape` rises just above 0.5
- **THEN** the drive is approximately 0.25 and the output differs slightly from the undriven sine

### Requirement: Triggered Sample-and-Hold Blend
The system SHALL capture the current mixed output into a held value whenever the trigger input `m_trig` is asserted, and blend the held value with the live output as `held × m_shFade + live × (1 − m_shFade)` before smoothing.
In the voice LFOs the trigger is the voice's AHD gate trigger (see ahd-envelopes and multi-phasor-gate), so a fully faded S+H output steps to a new value exactly once per note.

#### Scenario: Full S+H fade steps per trigger
- **WHEN** `m_shFade` is 1 and the voice receives a trigger
- **THEN** the pre-slew output jumps to the value captured at the trigger and holds it until the next trigger

#### Scenario: Half fade mixes stepped and continuous
- **WHEN** `m_shFade` is 0.5
- **THEN** the pre-slew output is the average of the held capture and the live continuous waveform

### Requirement: Output Slew and Amplitude Centering
The system SHALL smooth the blended output with a one-pole low-pass slew filter (natural frequency 500 Hz at 48 kHz) to produce `m_rawOutput`, and additionally publish `m_output = m_rawOutput × m_amplitude + (1 − m_amplitude) × 0.5`, which collapses toward the 0.5 midpoint as amplitude decreases.
The slew rounds off S+H transitions.

#### Scenario: Zero amplitude centers the output
- **WHEN** `m_amplitude` is 0
- **THEN** `m_output` is 0.5 regardless of the raw waveform

#### Scenario: Held steps are smoothed
- **WHEN** a trigger steps the held S+H value at full fade
- **THEN** `m_rawOutput` approaches each new level exponentially rather than discontinuously

### Requirement: Voice LFO Integration into the Modulation Matrix
The system SHALL run two SquiggleLFO instances per voice and publish their raw (pre-amplitude-centering) outputs and amplitudes to the corresponding globally indexed manager sources; the instrument's default voice groups SHALL gather these sources into slots 6 and 7; encoder cells consume these per-voice values as modulation sources (see encoder-parameter-system).
The LFO also exposes a top flag, true when every weighted contributing loop reports its top (cycle start), used to restart the LFO scope display.

#### Scenario: LFO modulates a routed parameter per voice
- **WHEN** voice 4's LFO 1 outputs 0.8 and a parameter has modulation slot 6 at nonzero depth
- **THEN** the parameter's voice-4 channel output reflects 0.8 through the crossfade modulation math while other voices follow their own LFO values

#### Scenario: Top flag marks cycle start
- **WHEN** all loops with nonzero mix weight report their top on the same control sample
- **THEN** the LFO's top flag is true on that sample and the scope recording restarts

### Requirement: Quad LFO Integration into the Modulation Matrix
The system SHALL run the two quad LFOs as four-lane `SquiggleLFOGeneric<4>` instances, one lane per quad channel, controlled from the `QuadLFOs` encoder bank through interpolated four-lane encoder reads (`GetValue<4>(param, 0)`). Each lane SHALL match an independent mono LFO given that channel's parameters, trigger and phase offset. The system SHALL publish their raw (pre-amplitude-centering) outputs and amplitudes to the corresponding globally indexed manager sources; the instrument's default Quad group SHALL gather these sources into slots 6 and 7.
Phase-shift knobs SHALL offset channel `i` by `knob × i / 4`, so full spread places the four channels a quarter-cycle apart. Each channel's Sample-and-Hold trigger SHALL be the modulated cycle-boundary crossing of that channel's active delay read-head loop (`ReadTapeHead::m_loopSelector`), not the merely requested delay loop index.

#### Scenario: Quarter-cycle channel spread
- **WHEN** the quad LFO 1 phase-shift knob is 1.0
- **THEN** channels 0, 1, 2, and 3 are offset by 0, 0.25, 0.5, and 0.75 cycles

#### Scenario: Delay-loop top captures Sample-and-Hold
- **WHEN** channel 2's active delay loop reports a modulated cycle crossing and `m_shFade` is 1
- **THEN** that channel's pre-slew Sample-and-Hold value updates to the live mixed output on that sample and holds until the next crossing of the same active loop
