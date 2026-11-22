# Quad Reverb Specification

## Purpose

Quad Reverb is the second global quadraphonic send effect. The audio core is generated from `DSP.SquiggleBoyQuadReverb` in Lean. Send and return routing between reverb, delay, and partial machine is specified in mixdown-mastering.

## Requirements

### Requirement: Serial All-Pass Input Diffusion

The system SHALL diffuse the incoming quad signal through one cascade of eight all-pass filters. In processing order, their nominal delays are 293, 487, 907, 1433, 4493, 6899, 9811, and 14033 samples at 192 kHz. Each stage shares its nominal delay across channels.

#### Scenario: Impulse is smeared before the delay loop

- **WHEN** a single-sample impulse arrives at one reverb input channel
- **THEN** each stage diffuses the previous stage's output before the last stage feeds the delay loop

### Requirement: Feedback Summation and Saturation

The generated core SHALL multiply its previous wet output by the per-channel feedback coefficient and sum the result with the diffused input at the main delay write. The core SHALL own the feedback return, histories, and write indices.

#### Scenario: Feedback control compensates for SAT gain

- **WHEN** the feedback knob moves from minimum to maximum
- **THEN** the input setter divides the zeroed exponential feedback map, scaled to a maximum of 1.25, by the current per-channel SAT gain
- **AND** the resulting coefficient spans 0 to 2.5 at minimum SAT
- **AND** equal SAT settings across the lanes preserve the 0 to 1.25 small-signal range before damping in the frozen loop

#### Scenario: SAT changes the feedback saturator gain

- **WHEN** one channel's SAT control at `(0, 2)` moves from minimum to maximum
- **THEN** the tanh input gain for that channel maps exponentially from 0.5 to 4, with a default of 0.5

### Requirement: Modulated Main Delay Line

The production engine SHALL run the reverb at 192 kHz with a four-channel main delay capacity of 262144 samples. The core SHALL read the main line using cubic interpolation at `delayTimeSamples * widen + delayOffsetSamples`. The reverb-time knob SHALL be slewed by a 1 Hz one-pole and mapped exponentially from 240 to 245760 samples. Fixed widening factors per channel SHALL be approximately 0.9786, 1.0, 0.9868, and 0.9897.

#### Scenario: Delay offset is centered and linear

- **WHEN** a channel's delay-offset knob is 0, 0.5, or 1
- **THEN** its offset at 192 kHz is respectively −200, 0, or +200 samples

#### Scenario: Regular modulation moves the delay offset

- **WHEN** a regular modulator varies the delay-offset control
- **THEN** the resulting offset is added directly to the widened base delay on each engine sample

### Requirement: Givens Channel Remix in the Loop

The system SHALL apply the generated orthogonal Givens mixer to the delayed quad signal on every pass.

#### Scenario: The mixer preserves signal energy

- **WHEN** the delayed signal enters the Givens mixer
- **THEN** the sum of squares over the four output lanes equals the input sum of squares in exact arithmetic

### Requirement: Feedback All-Pass and Damping Stages

The remixed signal SHALL pass through an all-pass, tanh with the per-channel SAT input gain, a one-pole high-pass and low-pass damping stage, and a second all-pass. At 192 kHz, the nominal tap delays in samples SHALL be 2039, 3719, 6359, and 2753 for the first stage, and 11923, 16879, 9041, and 23447 for the second stage. The last stage supplies the wet output.

The damping base SHALL map exponentially from 23.4375 Hz to 24 kHz in engine cycles per sample. The width SHALL map exponentially from 1 to 2048. Both resulting cutoffs SHALL be limited to 0.499 cycles per sample.

#### Scenario: Damping state is observable in the UI

- **WHEN** the reverb publishes a damping response sample
- **THEN** the analyzer uses the generated damping stage's current snapshot for all four channels

### Requirement: Wandering All-Pass Taps

Each of the ten all-pass stages SHALL own a seeded spline modulator and use gain 0.6. The excursion SHALL be bounded by ±128 samples, with enough buffer history for all four interpolation taps. Every 32 samples the phase clock SHALL draw an exponential speed with mean `1 / 1920000` knots per sample and smooth it with a one-pole time constant of 48000 samples. The spline SHALL be evaluated at each frame boundary and linearly interpolated between evaluations. Audio tap reads SHALL use cubic interpolation.

#### Scenario: The audio core receives physical settings and quad audio

- **WHEN** the app updates encoder controls for an engine sample
- **THEN** it SHALL install the mapped delay, feedback, and damping settings in the generated core and process the quad send through it
