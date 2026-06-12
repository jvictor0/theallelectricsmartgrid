# Quad Delay Specification

## Purpose
The Quad Delay (`QuadDelay` and `QuadDelayInputSetter` in `private/src/QuadDelay.hpp`, head logic in `private/src/TapeHead.hpp`, buffers in `private/src/DelayLine.hpp`) is the first of three global quadraphonic send effects. It is a time-warped delay synchronized to the Theory of Time (see phasor-timebase): instead of a fixed wall-clock delay it implements `D(F⁻¹(t + d)) = X(F⁻¹(t))`, where F is the post-modulation logical time, using a movable write head and an inverse time buffer. Playback is resynthesized through phase-vocoder grains (see phase-vocoder) so pitch is preserved while logical time stretches, compresses, or reverses. Per-channel feedback with saturation and band-pass damping, quantized delay ratios and read speeds, pitch shifting with unison, per-partial slew-up, and quadraphonic rotation complete the effect. Send and return routing between the delay, reverb, and partial machine is specified in mixdown-mastering.

## Requirements

### Requirement: Time-Warped Delay Line with Movable Write Head
The system SHALL write each input sample into a `QuadDelayLineMovableWriter` of 2^24 samples per channel at the warped-time write head position supplied per channel, and SHALL maintain a parallel inverse buffer (`m_writeHeadInverse`) that maps integer warped-time coordinates back to the wall-clock sample time at which they occurred. Audio is stored indexed by wall-clock time; reads first convert a warped-time position to wall-clock time through the inverse buffer (`GetRealTime`) and then read the audio buffer at that wall-clock index with cubic interpolation.

#### Scenario: Inverse buffer is populated by scattering
- **WHEN** the warped-time write head ascends across one or more integer warped-time values
- **THEN** the writer interpolates the wall-clock time at each crossed integer coordinate using a four-point cubic Lagrange fit of recent (warped time, wall-clock time) pairs and stores it in `m_writeHeadInverse`

#### Scenario: Descending warped time defers inverse updates
- **WHEN** the warped-time position decreases from one sample to the next (time modulation runs the clock backward)
- **THEN** audio continues to be recorded at the wall-clock index, but the inverse buffer is not updated until warped time ascends again, and the turnaround warped time is recorded when ascent resumes

### Requirement: Write Head Continuity via Glue Offset
The write head SHALL be computed per channel by `WriteTapeHead` as `unwound master phase * masterLoopSamples + glue`, where the additive glue offset preserves continuity of the absolute write position across transport state and tempo-scale changes. While the transport is stopped the write head advances by exactly one sample per sample and the glue is re-derived each sample; when the running state or the master loop length changes, the glue is re-anchored so the write head does not jump.

#### Scenario: Transport stop freezes warped time but not recording
- **WHEN** the Theory of Time stops running
- **THEN** the write head continues to advance by 1.0 per sample from its current position
- **AND** when the transport resumes, the glue offset makes the theory-derived position continuous with the position reached during the stop

#### Scenario: Master loop length change does not jump the head
- **WHEN** `masterLoopSamples` changes while the transport is running
- **THEN** the glue offset is recomputed as the difference between the current actual position and the new theory position, so the write head position is continuous across the change

### Requirement: Loop Selection Gated at Loop Tops
The system SHALL only apply a requested delay loop change when the currently selected loop and the requested loop are both simultaneously at their indirect top (`TheoryOfTime::GetIndirectTop`). Until that dual-top condition occurs, the read head keeps using the previously selected loop. The loop selector switch value is inverted against the loop hierarchy (`x_numLoops - switchVal - 1`) to choose the requested Theory of Time loop.

#### Scenario: Mid-cycle loop change is deferred
- **WHEN** the loop selector switch changes while the current or requested loop is mid-cycle
- **THEN** `ReadTapeHead::m_loopSelector` retains its previous value and delay timing is unchanged

#### Scenario: Dual top commits the change
- **WHEN** both the old and the newly requested loop report indirect top on the same control step
- **THEN** the loop selector switches to the requested loop and subsequent delay lengths derive from the new loop's external multiplier

### Requirement: Quantized Delay Ratio and Read Head Speed
The system SHALL quantize the delay-time factor per channel to one of {0.8, 2/3, 1.0, 3/4, 5/8}, selected by the delay-time-factor switch and latched into `m_bufferFrac[i]` only when the selected loop is at its indirect top. The read head speed SHALL be one of sixteen discrete values {-4, -3, -2, -3/2, -4/3, -1, -1/2, -1/4, 1/4, 1/2, 1, 4/3, 3/2, 2, 3, 4}, selected directly by the read-head-speed switch (negative values play the buffer backward).

#### Scenario: Ratio change waits for loop top
- **WHEN** the delay-time-factor switch moves to a new index mid-loop
- **THEN** `m_bufferFrac[i]` keeps its previous value until the selected loop next reaches its indirect top, at which point it latches the new quantized ratio

#### Scenario: Negative speed reverses playback
- **WHEN** the read-head-speed switch selects -1
- **THEN** the projected read position moves backward through warped time at one warped sample per warped sample while grains continue to be synthesized at the original pitch

### Requirement: Read Head Projection into the Selected Loop
The read head SHALL be computed per channel by `ReadTapeHead` as `writeHead * readHeadSpeed - effectiveDelaySamples`, wrapped by `WrapMod` into the window `(writeHead - hopSamples - loopSamples, writeHead - hopSamples]`, where `loopSamples = masterLoopSamples / externalLoopMultiplier(selectedLoop)`, `effectiveDelaySamples = loopSamples * bufferFrac * widen`, and `hopSamples` is the resynthesis hop size. A parallel computation in phasor units produces the relative read head position. If the master loop or selected loop length is not positive, the read head equals the write head.

#### Scenario: Read head stays one hop behind the write head
- **WHEN** the read head is computed with any speed and delay ratio
- **THEN** the wrapped read position is at most `writeHead - hopSamples` and at least `writeHead - hopSamples - loopSamples`, so reads always trail the write head by at least the resynthesis hop

#### Scenario: Widen knob spreads channel delay times
- **WHEN** the widen knob is raised from 0 toward 1
- **THEN** each channel's effective delay scales by its own widener ratio (exponential interpolation from 1.0 toward per-channel constants approximately 0.9786, 1.0, 0.9868, and 0.9897), slightly decorrelating the four delay lengths

### Requirement: Pitch-Preserving Grain Resynthesis
The system SHALL read the delay line through a per-channel `GrainManager` rather than directly: every hop interval (`Resynthesizer::GetGrainLaunchSamples()`, 1024 samples), a new 4096-sample Hann-windowed grain is captured starting at the wall-clock time mapped from the current read head warped time, along with a second analysis frame exactly one hop earlier, and resynthesized by the `Resynthesizer` so the original pitch is preserved regardless of read head rate (phase-vocoder internals are specified in phase-vocoder). Active grain outputs are summed, each scaled by 1/1.5.

#### Scenario: Pitch is stable under time warping
- **WHEN** Theory of Time phase modulation makes the read head move through the buffer at a varying rate
- **THEN** the delayed signal retains the pitch of the originally recorded audio rather than shifting pitch with the read rate

#### Scenario: LFO modulation offsets grain start positions
- **WHEN** the modulation depth knob is above zero
- **THEN** each grain's wall-clock start time is offset by `lfoOutput * 1000 * modDepth` samples per channel, with the LFO frequency set by the modulation frequency knob (exponential range 0.05 Hz to 51.2 Hz at 48 kHz)

#### Scenario: Grain pool exhaustion is non-fatal
- **WHEN** all 1024 grains of a channel's fixed allocator are in use at a launch point
- **THEN** no new grain starts for that hop and processing of existing grains continues

### Requirement: Pitch Shift, Unison, and Per-Partial Slew Controls
The system SHALL forward per-channel resynthesis controls to the grain resynthesizer: a pitch-shift ratio selected from ten just-intonation ratios {1/2, 2/3, 3/4, 3/5, 4/5, 5/4, 5/3, 4/3, 3/2, 2/1}, a shift fade amount crossfading shifted synthesis against unshifted, a unison detune ratio (exponential, up to 1.03) with a unison gain that reaches full level by one tenth of the unison knob's travel, and a per-partial slew-up rate mapped exponentially from 0.5 down to 0.25/1024 per sample as the slew-up knob increases (higher knob values produce a slower spectral bloom).

#### Scenario: Shift selection is a musical ratio
- **WHEN** the resynthesis shift switch selects index 8
- **THEN** the resynthesizer's shift ratio is exactly the rational 3/2 (a fifth up), and the shift fade knob sets the balance between shifted and unshifted grains

#### Scenario: Slew-up knob slows partial fade-in
- **WHEN** the slew-up knob is at maximum and a new grain starts
- **THEN** each partial's synthesis magnitude rises toward its analysis magnitude at 0.25/1024 per sample (full rise spread over thousands of samples) instead of appearing at full magnitude immediately

### Requirement: Feedback Path with Saturation and Band-Pass Damping
The system SHALL form the delay line input as `input + return * feedback` per channel, where the return is the delay's own previous output, the feedback gain is the feedback knob mapped through a zeroed exponential curve and scaled by 1.25 (values above 1.0 permit self-oscillation), and the sum is passed through a tanh saturator before being written. The output side SHALL pass the rotated, resynthesized signal through a `PostFeedbackFilter` band-pass (one-pole high-pass plus low-pass per channel) whose base and width are exponential mappings of the damping knobs (base 1/2048 to 0.5 normalized frequency, width 1 to 2048).

#### Scenario: Feedback above unity is bounded by saturation
- **WHEN** the feedback knob is at maximum (effective gain 1.25)
- **THEN** recirculating audio grows until the tanh saturator soft-limits it, producing sustained self-oscillation rather than unbounded output

#### Scenario: Damping shapes repeats
- **WHEN** the damping base and width knobs change
- **THEN** the band-pass corner frequencies applied to the delay output change accordingly, and the resulting one-pole filter alphas per channel are observable in `UIState::m_dampingFilter[i]` as `m_hpAlpha` and `m_lpAlpha`

### Requirement: Quadraphonic Rotation Routing
The system SHALL remix the four delayed channels with a linear rotation (`QuadFloat::RotateLinear`) before the post-feedback filter, with the rotation angle per channel set to `rotateSwitchVal / 4` (0 = no rotation, 1 = 90 degrees, 2 = 180 degrees) and smoothed through a one-pole low-pass so switch changes glide rather than click. This rotates each echo generation around the quad field, unlike the reverb's Hadamard mixing (see quad-reverb).

#### Scenario: Quarter rotation shifts channels by one position
- **WHEN** the rotate switch is 1 (angle 0.25) and the smoothing filter has settled
- **THEN** output channel i carries the delayed signal of channel (i + 1) mod 4, so successive feedback passes walk each echo around the four channels

#### Scenario: Intermediate angles crossfade neighbors
- **WHEN** the smoothed rotation angle is between switch positions
- **THEN** each output channel is a linear blend of the two adjacent input channels weighted by the fractional angle

### Requirement: Delay UI State Observability
The system SHALL publish a `QuadDelay::UIState` containing per-channel damping filter alphas and a double-buffered `UISnapshot` of four `DelayEnvelopeState` entries; each entry holds the channel's relative read and write head positions (phasor units in [0, 1), or -1 before first computation) and 1024-point min/max amplitude envelopes sampled by mapping recorded master-phasor positions through the positional buffer recorder, the inverse buffer (`GetRealTime`), and the writer's 1024-sample envelope buckets.

#### Scenario: Head positions track the tape heads
- **WHEN** `PopulateUIState` runs while the delay is processing
- **THEN** `m_relativeWriteHeadPosition` equals the Theory of Time indirect phasor used by the write tape head and `m_relativeReadHeadPosition` equals the read tape head's wrapped relative position for each channel

#### Scenario: Envelope reflects recorded audio
- **WHEN** audio with a distinct amplitude profile has been written into a channel's delay line over the last loop
- **THEN** that channel's `m_maxEnvelope`/`m_minEnvelope` arrays trace the per-bucket extrema of the recorded audio at the warped-time positions held in the positional buffer
