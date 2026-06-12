# Quad Reverb Specification

## Purpose
The Quad Reverb (`QuadReverb` and `QuadReverbInputSetter` in `private/src/QuadReverb.hpp`, filter primitives in `private/src/DelayLine.hpp` and `private/src/Filter.hpp`) is the second global quadraphonic send effect. Unlike the phase-vocoder quad-delay, it follows a traditional algorithmic reverb topology scaled to four channels: a parallel all-pass diffusion bank on the input, an all-pass stage on the feedback return, a modulated main delay line, a Hadamard matrix remix of the delayed signal, and a post-feedback chain of all-pass, band-pass damping, and all-pass filters, with tanh saturation bounding the feedback loop. Send and return routing between reverb, delay, and partial machine is specified in mixdown-mastering.

## Requirements

### Requirement: Parallel All-Pass Input Diffusion
The system SHALL diffuse the incoming quad signal through a bank of eight parallel all-pass filters per channel (`QuadParallelAllPassFilter<8>`), whose outputs are summed and divided by eight. The eight filters use the prime delay lengths 37, 61, 113, 179, 281, 431, 613, and 877 samples (the same set on each of the four channels) to smear transients into dense early reflections while minimizing comb coloration.

#### Scenario: Impulse is smeared before the delay loop
- **WHEN** a single-sample impulse arrives at a reverb input channel
- **THEN** the diffusion stage output spreads its energy across a cluster of reflections at the prime delay offsets rather than passing a single dominant impulse into the main loop

#### Scenario: Prime delays avoid common periodicity
- **WHEN** the eight parallel all-pass outputs are averaged
- **THEN** no two filter delay lengths share a common factor, so their reflection patterns do not reinforce a single periodic resonance

### Requirement: All-Pass Filtering of the Feedback Return
The system SHALL pass the feedback return signal (the reverb's own previous output) through a quadraphonic all-pass filter (`m_preFeedbackFilter`) with per-channel delay lengths 3, 5, 7, and 11 samples before it is scaled by the feedback gain and summed with the diffused input. The dry input path is diffused by the parallel bank only; it is the recirculating path that passes this additional all-pass stage on every loop.

#### Scenario: Each recirculation adds diffusion
- **WHEN** a signal recirculates through the feedback loop N times
- **THEN** it has passed the return all-pass stage N times in addition to its single pass through the input diffusion bank, so the tail grows progressively denser

### Requirement: Saturated Feedback Summation
The system SHALL form the main delay line input as `diffusedInput + filteredReturn * feedback` per channel, where the feedback gain is the feedback knob mapped through a zeroed exponential curve and scaled by 1.25 (allowing loop gain above unity), and SHALL pass the sum through a `TanhSaturator` before writing it into the delay line, bounding the loop and adding harmonic warmth at high levels.

#### Scenario: High feedback is soft-limited
- **WHEN** the feedback knob is at maximum (effective gain 1.25)
- **THEN** the recirculating signal grows until the tanh stage compresses it, sustaining a bounded, saturated tail instead of overflowing

#### Scenario: Low feedback decays cleanly
- **WHEN** the feedback gain is well below unity and signal levels are small
- **THEN** the saturator operates in its near-linear region and the tail decays geometrically with each pass

### Requirement: Modulated Main Delay Line
The system SHALL read the reverberant tail from a four-channel delay line of 65536 samples per channel at a per-channel delay time `delayTimeSamples * widen + lfoOutput * 1000 * modDepth`. The base delay time maps the reverb-time knob exponentially over 60 to 61440 samples and is slewed by a one-pole filter; the widen factors are fixed per-channel constants (approximately 0.9786, 1.0, 0.9868, 0.9897) that decorrelate the four loop lengths; the `QuadLFO` (frequency knob mapped exponentially over 0.05 Hz to 51.2 Hz at 48 kHz, with per-channel phase offsets) adds up to ±1000 samples scaled by the modulation depth, chorusing the tail and breaking up static resonances.

#### Scenario: Modulation depth controls tail motion
- **WHEN** the modulation depth knob is raised from zero
- **THEN** each channel's read position varies by the channel's LFO output times 1000 times the depth, and at zero depth the delay times are static

#### Scenario: Channel loop lengths are intentionally unequal
- **WHEN** all four channels share the same reverb-time knob value
- **THEN** the effective per-channel delay times differ by the fixed widener ratios, so the four channels' echo periods never coincide exactly

### Requirement: Hadamard Channel Remix in the Loop
The system SHALL remix the four delayed channels with an orthogonal Hadamard transform (`QuadFloat::Hadamard`, the 4x4 matrix of ±1/2 entries) each pass: outputs are (a+b+c+d)/2, (a-b+c-d)/2, (a+b-c-d)/2, and (a-b-c+d)/2. This spreads energy evenly across all four channels without favoring a left/right or front/back swap, in contrast to the quad-delay's `RotateLinear` mixing.

#### Scenario: Single-channel energy spreads to all four
- **WHEN** the delayed signal is nonzero on exactly one channel with amplitude a
- **THEN** after the Hadamard transform every channel carries amplitude a/2, and total energy is preserved (orthogonal matrix)

#### Scenario: Repeated application returns to identity
- **WHEN** the Hadamard transform is applied twice to the same vector
- **THEN** the result equals the original vector, so successive feedback passes alternate between mixed and unmixed channel alignments rather than drifting

### Requirement: Post-Feedback All-Pass and Band-Pass Damping
The system SHALL filter the Hadamard-mixed signal through a `PostFeedbackFilter` chain of an all-pass stage (per-channel delays 17, 31, 53, 23 samples), a band-pass damping stage (`QuadOPBaseWidthFilter`, a one-pole high-pass plus one-pole low-pass per channel), and a second all-pass stage (per-channel delays 149, 211, 113, 293 samples). The band-pass base and width are exponential mappings of the damping knobs (base 1/2048 to 0.5 normalized frequency, width 1 to 2048). The chain's output is the reverb output and becomes the next pass's feedback return.

#### Scenario: Damping rolls off the tail's extremes
- **WHEN** the damping base and width knobs narrow the band-pass
- **THEN** frequencies outside the band are attenuated on every recirculation, so spectral extremes decay faster than the passband

#### Scenario: Damping state is observable in the UI
- **WHEN** `PopulateUIState` runs
- **THEN** each channel's current high-pass and low-pass one-pole alphas are stored in `UIState::m_dampingFilter[i].m_hpAlpha` and `m_lpAlpha`, reflecting the active damping knob settings

### Requirement: Unified All-Pass Gain
The system SHALL set a single gain coefficient for every all-pass filter in the reverb (input diffusion bank, return-path all-pass, and both post-feedback all-pass stages) through `SetAPFGain`. The gain is fixed at 0.6 at construction; no runtime knob modifies it.

#### Scenario: One gain governs all diffusion stages
- **WHEN** `SetAPFGain(g)` is called
- **THEN** all 8x4 input-bank filters, the four return-path filters, and the eight post-feedback filters use feedback/feedforward coefficient g

#### Scenario: Default diffusion gain
- **WHEN** a `QuadReverb` is constructed
- **THEN** every all-pass filter's gain is 0.6 and the LFO slew limit is set to 10/48000

### Requirement: Smoothed Control Mapping
The `QuadReverbInputSetter` SHALL map the per-channel knob inputs to DSP parameters once per control pass: reverb time through a one-pole slew filter then an exponential map, modulation depth through a zeroed exponential map then a one-pole slew filter, damping base/width and modulation frequency through exponential maps, and feedback through a zeroed exponential map (centered at 0.25) scaled by 1.25. The per-channel widen parameter is driven at its maximum constant value; the widen knob input is not consulted.

#### Scenario: Reverb time changes glide
- **WHEN** the reverb-time knob jumps between two values
- **THEN** the delay time in samples moves smoothly along the slewed, exponentially mapped trajectory rather than stepping, avoiding clicks in the tail

#### Scenario: Feedback knob midpoint
- **WHEN** the feedback knob is at 0.5
- **THEN** the zeroed exponential mapping centered at 0.25 yields 0.25 before scaling, so the effective feedback gain is approximately 0.3125
