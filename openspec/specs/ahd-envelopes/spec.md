# AHD Envelopes Specification

## Purpose
AHD (Attack-Hold-Decay) envelopes (`AHD` in `private/src/AHD.hpp`) shape voice amplitude and modulation in the DSP engine. They are phase-driven: stage progression uses absolute modulated global phase with a trigger-captured source/global ratio and envelope period from the phasor-timebase rather than wall-clock time, so envelopes stretch, compress, or reverse with clock modulation while attack and decay use 48 kHz time-parameter mappings in phase-derived sample units. Envelopes are triggered and released by the `AHDControl` structure published by the multi-phasor-gate, and each voice in voice-architecture hosts two instances: the main amp envelope and a modulation envelope.

## Requirements

### Requirement: Phase-Driven Stage Progression
At trigger, the envelope SHALL capture its selected source phase's cycle ratio to global phase, the absolute modulated global phase origin, and its envelope period in samples. The trigger producer SHALL supply the ratio; the active envelope SHALL retain no source loop index, winding tracker, or topology-change history. During Running it SHALL compute elapsed samples as abs(currentModulatedGlobalPhase - capturedGlobalPhase) * capturedRatio * capturedEnvelopePeriodSamples and integrate the difference from the previous call using the current stage's rate. The previous elapsed position SHALL reset to zero at each trigger. Only a new trigger SHALL replace the captured timing values. Global phase modulation and reversal SHALL continue to affect progress. Time remaining after a stage boundary SHALL be applied to the following stage in the direction of travel.

#### Scenario: Topology edit does not change active timing
- **WHEN** an active envelope captured ratio 4 and the source topology later changes its ratio to 6
- **THEN** the active envelope continues using ratio 4 and its captured envelope period
- **AND** its progress and output do not jump solely because of the edit

#### Scenario: Retrigger captures new timing
- **WHEN** the same voice triggers again after the source ratio becomes 6
- **THEN** it captures ratio 6, the new global origin, and the new period
- **AND** its attack starts from the current output level

#### Scenario: Reversal retraces phase progress
- **WHEN** a running envelope's global phase moves back toward its captured origin
- **THEN** its phase-derived elapsed position decreases without an accumulated absolute-distance counter
- **AND** its stage progression reverses through decay, hold, and attack using the current control values
- **AND** reverse attack does not fall below the level captured at the trigger
- **AND** moving away on either side of the origin advances progression again; an Idle envelope does not restart

#### Scenario: Slower phase stretches the envelope
- **WHEN** global phase modulation halves phase advance with captured timing unchanged
- **THEN** elapsed position advances at half speed

### Requirement: Trigger and Release via AHDControl
The system SHALL start an envelope when `AHDControl.m_trig` is true, entering the Running state, and SHALL move a non-Idle envelope to the Release state when `AHDControl.m_release` is true without a trigger. Both flags are produced by the multi-phasor-gate; the envelope consumes no gate signal. A `m_changed` flag SHALL be set on any state transition. Each `Process(Input&)` call SHALL clear both input event flags after reading them, including a release ignored while Idle or superseded by a trigger. It SHALL report the consumed trigger through `m_triggered` for that call; a following call without a trigger SHALL reset `m_triggered` to false. The shared `AHDControl` SHALL remain unchanged so every envelope can receive its own copy. Scope recording SHALL use `m_triggered` to mark starts and `m_changed` with Idle to mark ends.

#### Scenario: Trigger starts the envelope
- **WHEN** `m_trig` is true while the envelope is Idle
- **THEN** the state becomes Running, `m_changed` is true for that frame, and the raw output begins its attack ramp

#### Scenario: Release interrupts a running envelope
- **WHEN** `m_release` is true and the state is Running
- **THEN** the state becomes Release and the raw output ramps down from its current value
- **AND** `m_release` is ignored while the envelope is already Idle

#### Scenario: Physical-model events are delivered once per microblock
- **WHEN** physical modeling processes a microblock at four times the base sample rate
- **THEN** trigger and release events are delivered only at internal sample zero
- **AND** a trigger on the immediately following microblock captures the previous raw output once and starts a new attack
- **AND** both input event flags are false after consumption without caller cleanup
- **AND** when both events are present, trigger takes precedence for the entire microblock

### Requirement: Attack Stage With Constant-Time Configuration
The system SHALL ramp the raw output from `m_startOutput` toward 1.0 during attack by accumulating `deltaSamples * attackIncrement`. The attack increment is derived from a user time setting in the range 1 ms to 2.5 s (`x_attackTimeMin`/`x_attackTimeMax`) as `1 / (48000 * attackTime)` via an exponential parameter, using the fixed 48 kHz constant in phase-derived sample units. Wall-clock attack duration follows global phase motion and the captured source ratio and envelope period; those two captured factors need not cancel in the production voice path. On retrigger, `m_startOutput` SHALL be set to the current raw output so the attack continues from the present level instead of snapping to zero.

#### Scenario: Attack reaches full scale at the configured time
- **WHEN** an Idle envelope with a 500 ms attack is triggered and its phase-derived elapsed position reaches 24000 samples
- **THEN** the raw output rises linearly in phase-derived samples and reaches 1.0 at 24000 phase-derived samples

#### Scenario: Retrigger from a non-zero level
- **WHEN** an envelope whose raw output is 0.6 receives a new trigger
- **THEN** `m_startOutput` becomes 0.6 and the attack ramps from 0.6 to 1.0, taking only the remaining 40% of the configured attack time

### Requirement: Hold Stage in Loop Divisions
The system SHALL hold the raw output at 1.0 after the attack completes for a duration expressed in envelope-period divisions, not absolute time: the hold parameter maps through a zeroed exponential curve to 0-16 loops (`m_hold.SetMax(16.0)`, centered at 1/32), and `holdSamples = holdLoops * capturedEnvelopePeriodSamples`. The hold therefore scales with the captured voice cycle period from the multi-phasor-gate, whose ratio uses the undoubled LCM of selected clock and read loop ratios. Hold SHALL accumulate fractional progress as `deltaSamples / holdSamples`, entering decay when that progress reaches 1 during forward motion. A zero-length hold SHALL be skipped in either direction on the next nonzero elapsed-time step.

#### Scenario: Hold tracks the envelope period
- **WHEN** the hold parameter maps to 0.5 loops and the voice's `capturedEnvelopePeriodSamples` is 60000
- **THEN** the raw output stays at 1.0 for 30000 phase-derived samples after the attack ends
- **AND** a subsequent trigger capturing twice the envelope period doubles hold length at the same hold setting; a topology edit alone does not change the active note's captured period

### Requirement: Decay Stage and Completion
The system SHALL ramp the raw output down from 1.0 after the hold ends by accumulating `deltaSamples * decayIncrement` and subtracting that progress from 1, with the decay increment derived from a time setting in the range 10 ms to 10 s (`x_decayTimeMin`/`x_decayTimeMax`) at the unmodulated 48 kHz rate. When the raw output reaches 0, it SHALL clamp to 0 and the state SHALL return to Idle with `m_changed` set.

#### Scenario: Decay completes to Idle
- **WHEN** a Running envelope with a 1 s decay passes the end of its hold stage
- **THEN** the raw output falls linearly in phase-derived samples over 48000 samples
- **AND** on the frame the value reaches or would pass zero, the output is 0.0, the state is Idle, and `m_changed` is true

### Requirement: Continuous Live Timing Controls
Changing attack, hold, or decay SHALL preserve the current stage progress and raw output when phase-derived time does not move. Subsequent elapsed time SHALL use the new rate without recalculating past progress. A hold edit SHALL preserve the completed fraction of an active hold and SHALL NOT reposition an active decay. Reversal SHALL use current rates rather than replay historical control values.

#### Scenario: Attack knob changes the future slope
- **WHEN** attack has reached 0.25 and its increment changes from 1/16 to 1/8 with phase stationary
- **THEN** the raw output remains 0.25
- **AND** two additional phase-derived samples raise the output to 0.5

#### Scenario: Hold knob preserves progress
- **WHEN** a hold is halfway complete and its duration changes from 16 to 32 phase-derived samples
- **THEN** it remains halfway complete with raw output 1.0
- **AND** 16 more forward phase-derived samples complete the remaining half

#### Scenario: Decay knob changes the future slope
- **WHEN** decay has reached raw output 0.75 and its increment changes from 1/32 to 1/16 with phase stationary
- **THEN** the raw output remains 0.75
- **AND** two additional phase-derived samples lower the output to 0.625

### Requirement: Release Ramp on Interruption
The system SHALL, while in the Release state, decrement the raw output by `decayIncrement * input.m_releaseFactor` per call (a linear wall-clock ramp at the configured decay rate, independent of phase tracking). `Process(Input&)` SHALL use the input's release factor, which defaults to 1 base sample per call; physical modeling SHALL initialize it to 0.25 for each of its four internal samples. This factor SHALL affect only explicit release, leaving phase-driven attack, hold, and running decay increments unchanged. When the raw output reaches or falls below 0 it SHALL clamp to 0 and the state SHALL return to Idle.

#### Scenario: Release ramps at the decay rate
- **WHEN** an envelope holding at raw output 1.0 with a 100 ms decay setting enters Release
- **THEN** the raw output reaches 0 after 4800 base samples of elapsed time and the state becomes Idle
- **AND** a clock-phase modulation during the release does not alter the release ramp speed

#### Scenario: Oversampled release retains elapsed time
- **WHEN** the same envelope is processed at four times the base sample rate with `input.m_releaseFactor` set to 0.25 base samples per call
- **THEN** its release reaches zero after the same elapsed base-sample time as base-rate processing, allowing floating-point rounding at the final sample
- **AND** running attack, hold, and decay continue to use phase-derived fractional base-sample positions

#### Scenario: Physical-model short timing range is preserved
- **WHEN** the physical-model envelope uses its shortest attack and decay settings
- **THEN** attack is 100 µs and decay is 500 µs in phase-derived sample units, with a maximum setting of 2.5 s for each

### Requirement: Amplitude Scaling With Polarity
The system SHALL maintain both a raw output (`m_rawOutput`, the pure 0-1 envelope shape) and an amplitude-scaled output (`m_output`). With amplitude polarity true, `m_output = m_rawOutput * amplitude`; with polarity false (inverted), `m_output = (1 - amplitude) + m_rawOutput * amplitude`, so amplitude 0 yields a constant 1.0. The voice amp envelope uses polarity false (amplitude blends between drone and full envelope shaping) and the modulation envelope uses polarity true (see voice-architecture).

#### Scenario: Inverted polarity at zero amplitude
- **WHEN** an envelope has polarity false and amplitude 0.0
- **THEN** `m_output` is 1.0 regardless of the envelope stage while `m_rawOutput` still traces the attack-hold-decay shape

#### Scenario: Positive polarity scales the shape
- **WHEN** an envelope has polarity true and amplitude 0.5
- **THEN** `m_output` equals half of `m_rawOutput` at every point, peaking at 0.5 during hold

### Requirement: Two Envelope Instances Per Voice With Scope Output
Each voice SHALL host two AHD instances driven by the same `AHDControl` timing — the main amp envelope (`m_ahd`) and the modulation envelope (`m_modulationAHD`) — with independent attack, hold, decay, and amplitude parameters set through separate `InputSetter`s. Their outputs SHALL be written once per control frame to the per-voice scope channels `ControlScopes::AmpEnvelope` and `ControlScopes::ModulationEnvelope`, with record-start marked on trigger and record-end marked when the envelope transitions to Idle.

#### Scenario: Independent shapes from shared timing
- **WHEN** a voice is triggered with the amp envelope set to a short decay and the modulation envelope set to a long decay
- **THEN** both envelopes enter attack on the same control frame, the `AmpEnvelope` scope trace returns to zero first, and the `ModulationEnvelope` trace continues until its own decay completes

#### Scenario: Scope recording brackets the envelope
- **WHEN** voice i's amp envelope is triggered and later returns to Idle
- **THEN** the `ControlScopes::AmpEnvelope` channel records a start mark at the trigger frame and an end mark on the frame `m_changed` is set with state Idle
