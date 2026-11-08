# Quad Delay Specification

## Purpose
The Quad Delay (`QuadDelay` and `QuadDelayInputSetter` in `private/src/QuadDelay.hpp`, head logic in `private/src/TapeHead.hpp`, buffers in `private/src/DelayLine.hpp`) is the first of three global quadraphonic send effects. It is a time-warped delay synchronized to the Theory of Time (see phasor-timebase): instead of a fixed wall-clock delay it implements `D(F⁻¹(t + d)) = X(F⁻¹(t))`, where F is the post-modulation logical time, using a movable write head and an inverse time buffer. Playback is resynthesized through phase-vocoder grains (see phase-vocoder) so pitch is preserved while logical time stretches, compresses, or reverses. Per-channel feedback with saturation and band-pass damping, quantized delay ratios and read speeds, pitch shifting with unison, per-partial slew-up, and quadraphonic rotation complete the effect. Send and return routing between the delay, reverb, and partial machine is specified in mixdown-mastering.

## Requirements

### Requirement: Time-Warped Delay Line with Movable Write Head
The system SHALL saturate each engine-rate input sample, decimate it through an order-8 Butterworth `DownsamplerGeneric`, and write the decimated sample once per host frame into a `QuadDelayLineMovableWriter` of 2^24 host samples per channel at one shared warped-time write head position in host samples, and SHALL maintain one shared inverse buffer (`m_writeHeadInverse`) that maps integer warped-time coordinates back to the wall-clock sample time at which they occurred. Audio is stored indexed by wall-clock time; reads first convert a warped-time position to wall-clock time through the inverse buffer (`GetRealTime`) and then read the audio buffer at that wall-clock index with cubic interpolation.

#### Scenario: One write per host frame
- **WHEN** the delay processes `4096 * x_oversample` engine samples
- **THEN** the delay line's wall-clock time advances by 4096 host samples

#### Scenario: Inverse buffer is populated by scattering
- **WHEN** the warped-time write head ascends across one or more integer warped-time values
- **THEN** the writer interpolates the wall-clock time at each crossed integer coordinate using linear interpolation between the adjacent ascending (warped time, wall-clock time) pairs and stores it in `m_writeHeadInverse`

#### Scenario: Descending warped time defers inverse updates
- **WHEN** the warped-time position decreases from one sample to the next (time modulation runs the clock backward)
- **THEN** audio continues to be recorded at the wall-clock index, but the inverse buffer is not updated until warped time ascends again, and the turnaround warped time is recorded when ascent resumes

### Requirement: Write Head Continuity via Glue Offset
The shared write head SHALL be computed by one `WriteTapeHead` as `unwound master phase * masterLoopSamples + glue`, in host samples, where `masterLoopSamples` is the Theory of Time global period divided by `x_oversample` and the additive glue offset preserves continuity of the absolute write position across transport state and tempo-scale changes. While the transport is stopped the write head advances by `1 / x_oversample` host samples per engine sample and the glue is re-derived each sample; when the running state or the master loop length changes, the glue is re-anchored so the write head does not jump.

#### Scenario: Transport stop freezes warped time but not recording
- **WHEN** the Theory of Time stops running
- **THEN** the write head continues to advance by one host sample per host frame from its current position
- **AND** when the transport resumes, the glue offset makes the theory-derived position continuous with the position reached during the stop

#### Scenario: Master loop length change does not jump the head
- **WHEN** `masterLoopSamples` changes while the transport is running
- **THEN** the glue offset is recomputed as the difference between the current actual position and the new theory position, so the write head position is continuous across the change

#### Scenario: Delay phase continues while transport is stopped
- **WHEN** transport stops and Theory of Time resets its phase to zero
- **THEN** the delay's local phase continues from its last position, advancing by `1 / masterLoopSamples` cycles per host frame at the current tempo
- **AND** the read heads keep their selected playback speeds within the region behind the advancing write head
- **AND** starting transport returns the local playback phase to the modulated global phase

### Requirement: Loop Selection Gated at Loop Tops
The system SHALL only apply a requested delay loop change when the currently selected loop and the requested loop are both simultaneously crossing a modulated cycle boundary (`CrossedCycleBoundary`). Until that dual-top condition occurs, the read head keeps using the previously selected loop. The loop selector switch value is inverted against the loop hierarchy (`x_numLoops - switchVal - 1`) to choose the requested Theory of Time loop.

#### Scenario: Mid-cycle loop change is deferred
- **WHEN** the loop selector switch changes while the current or requested loop is mid-cycle
- **THEN** `ReadTapeHead::m_loopSelector` retains its previous value and delay timing is unchanged

#### Scenario: Dual top commits the change
- **WHEN** both the old and the newly requested loop report a modulated cycle boundary on the same engine sample
- **THEN** the loop selector switches to the requested loop and subsequent delay lengths derive from the new loop's cycle ratio

### Requirement: Quantized Delay Ratio and Read Head Speed
The system SHALL quantize the delay-time factor per channel to one of {0.8, 2/3, 1.0, 3/4, 5/8}, selected by the delay-time-factor switch and latched into `m_bufferFrac[i]` only when the selected loop is crossing a modulated cycle boundary. The read head speed SHALL be one of sixteen discrete values {-4, -3, -2, -3/2, -4/3, -1, -1/2, -1/4, 1/4, 1/2, 1, 4/3, 3/2, 2, 3, 4}, selected directly by the read-head-speed switch (negative values play the buffer backward).

#### Scenario: Ratio change waits for loop top
- **WHEN** the delay-time-factor switch moves to a new index mid-loop
- **THEN** `m_bufferFrac[i]` keeps its previous value until the selected loop next crosses a modulated cycle boundary, at which point it latches the new quantized ratio

#### Scenario: Negative speed reverses playback
- **WHEN** the read-head-speed switch selects -1
- **THEN** the projected read position moves backward through warped time at one warped sample per warped sample while grains continue to be synthesized at the original pitch

### Requirement: Read Head Projection into the Selected Loop
The read head SHALL be computed per channel by `ReadTapeHead` from `delayPhase * readHeadSpeed - effectiveDelaySamples / masterLoopSamples`, wrapped by `WrapMod` in phase units into the selected-loop region behind the write phase and offset by one resynthesis hop. `delayPhase` follows modulated global phase while running and free-runs while stopped. Here `loopSamples = masterLoopSamples / cycleRatio(selectedLoop)`, `effectiveDelaySamples = loopSamples * bufferFrac`, and `hopSamples` is the resynthesis hop size. The resulting `readPhase` SHALL drive both the display and the absolute audio position, with `readHead = writeHead + (readPhase - delayPhase) * masterLoopSamples`. Thus the audio head lies in `[writeHead - hopSamples - loopSamples, writeHead - hopSamples)`, and the continuity glue translates both heads equally without being multiplied by read-head speed. If the master loop or selected loop length is not positive, the read head equals the write head.

#### Scenario: Non-unit speed preserves agreement between audio and display
- **WHEN** the write head has a nonzero continuity glue offset and the read speed is slow, fast, or negative
- **THEN** the audio and displayed read heads have the same displacement from their respective write heads, after converting phase to host samples
- **AND** changing the glue offset translates the audio read head and write head equally without changing the displayed read phase

#### Scenario: Read head stays one hop behind the write head
- **WHEN** the read head is computed with any speed and delay ratio
- **THEN** the wrapped read position is at most `writeHead - hopSamples` and at least `writeHead - hopSamples - loopSamples`, so reads always trail the write head by at least the resynthesis hop

#### Scenario: Matching controls produce matching read positions
- **WHEN** channels have the same loop, delay ratio, read speed, and history
- **THEN** their read positions coincide without channel-dependent widening factors

#### Scenario: Widen is removed
- **WHEN** the delay page is displayed or an older patch containing DelayWiden is loaded
- **THEN** cell `(0, 2)` is empty and the legacy DelayWiden field has no effect
- **AND** all remaining parameters retain their names and page positions

### Requirement: Pitch-Preserving Grain Resynthesis
The system SHALL read the delay line through one packed four-channel `GrainManagerGeneric` with a shared launch schedule and independent per-channel analysis and synthesis state: every synthesis hop (`Resynthesizer::GetSynthesisHopSamples()`, 1024 * `x_oversample` engine samples, the duration of the 1024-host-sample analysis hop), a new 4096-host-sample Hann-windowed grain is captured starting at the wall-clock time mapped from the current read head warped time, along with a second analysis frame exactly one hop earlier, and resynthesized by the `Resynthesizer` into an engine-rate frame so the original pitch is preserved regardless of read head rate (phase-vocoder internals are specified in phase-vocoder). Frames are overlap-added by `OLAGeneric` and the sum is scaled by 1/1.5.

#### Scenario: Pitch is stable under time warping
- **WHEN** Theory of Time phase modulation makes the read head move through the buffer at a varying rate
- **THEN** the delayed signal retains the pitch of the originally recorded audio rather than shifting pitch with the read rate

#### Scenario: Tape heads determine grain start positions
- **WHEN** a grain launches
- **THEN** each channel maps its own read head through the shared inverse time map and clamps the result to the latest complete analysis window
- **AND** Quad Delay supplies zero sample offset and has no internal LFO frequency, depth, or phase controls

#### Scenario: Grain storage is bounded
- **WHEN** grains launch on every synthesis hop
- **THEN** their output accumulates in one preallocated overlap-add ring of `6144 * x_oversample` quad samples (6H, where H is the synthesis hop), with no per-grain allocation

### Requirement: Pitch Shift, Unison, and Per-Partial Slew Controls
The system SHALL forward per-channel resynthesis controls to the grain resynthesizer: a pitch-shift ratio selected from ten just-intonation ratios {1/2, 2/3, 3/4, 3/5, 4/5, 5/4, 5/3, 4/3, 3/2, 2/1}, a shift fade amount crossfading shifted synthesis against unshifted, a unison detune ratio `2^(C*u/1200)` with maximum spread C = 40, 45, 50, and 55 cents on channels 1–4 and normalized knob u, a unison gain `(20^min(1, 3*u) - 1) / 19` that reaches full level at one third of the unison knob's travel, and a per-partial slew-up rate mapped exponentially from 0.5 down to 0.25/1024 cycles per analysis hop as the slew-up knob increases (higher knob values produce a slower spectral bloom). The unison copies SHALL use the center pitch and reciprocal upward/downward detune ratios.

#### Scenario: Shift selection is a musical ratio
- **WHEN** the resynthesis shift switch selects index 8
- **THEN** the resynthesizer's shift ratio is exactly the rational 3/2 (a fifth up), and the shift fade knob sets the balance between shifted and unshifted grains

#### Scenario: Slew-up knob slows partial fade-in
- **WHEN** the slew-up knob is at maximum and a new grain starts
- **THEN** each partial's synthesis magnitude rises toward its analysis magnitude with a one-pole cutoff of 0.25/1024 cycles per analysis hop instead of appearing at full magnitude immediately

#### Scenario: Equal unison knobs give distinct channel spreads
- **WHEN** all four unison knobs are at maximum
- **THEN** the side copies have pitch offsets of ±40, ±45, ±50, and ±55 cents on channels 1–4 respectively
- **AND** each center copy remains at the original pitch

#### Scenario: Unison spread scales in cents
- **WHEN** all four unison knobs are at one half
- **THEN** the side copies have pitch offsets of ±20, ±22.5, ±25, and ±27.5 cents on channels 1–4 respectively
- **AND** all three copy gains remain equal

#### Scenario: Zero unison preserves the original pitch
- **WHEN** all four unison knobs are zero
- **THEN** every channel's detune ratio is one and only its center copy is audible

#### Scenario: Unison copies fade in over the first third
- **WHEN** the unison knob moves from zero to one third
- **THEN** the side-copy gains rise from zero to `1/sqrt(3)` while the center gain falls from one to `1/sqrt(3)`
- **AND** all three copy gains remain equal above one third while pitch spread continues increasing

### Requirement: Feedback Path with Saturation and Band-Pass Damping
The system SHALL form the delay line input as `input + return * feedback` per channel, where the return is the delay's own previous output, the feedback gain is the feedback knob mapped through a zeroed exponential curve and scaled by 1.25 (values above 1.0 permit self-oscillation), and the sum is passed through a tanh saturator before being written. The output side SHALL pass the rotated, resynthesized signal through a `PostFeedbackFilter` band-pass (one-pole high-pass plus low-pass per channel) whose base and width are exponential mappings of the damping knobs (base `DampingFilter::x_minBaseHz` to `x_maxBaseHz`, i.e. 1/2048 to 0.5 cycles per host sample (23.4375 Hz to 24 kHz at 48 kHz), converted to cycles per engine sample; width 1 to 2048).

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
The system SHALL publish a `QuadDelay::UIState` containing per-channel damping filter alphas and a double-buffered `UISnapshot` of four `DelayEnvelopeState` entries; each entry holds the channel's relative read and write head positions (phasor units in [0, 1), or -1 before first computation; the write position is shared) and 1024-point min/max amplitude envelopes sampled by mapping recorded display-phase positions through the positional buffer recorder, the inverse buffer (`GetRealTime`), and the writer's 1024-sample envelope buckets. The waveform recorder and both head markers SHALL share a display phase offset that preserves their coordinate origin when playback resynchronizes to musical phase on transport start.

#### Scenario: Head positions track the tape heads
- **WHEN** `PopulateUIState` runs while the delay is processing
- **THEN** `m_relativeWriteHeadPosition` equals the write tape head's local phase plus the shared display offset, reduced to its fractional part
- **AND** each channel's `m_relativeReadHeadPosition` uses its read phase plus the same display offset, reduced to its fractional part

#### Scenario: Stopped recording remains visible across the whole display
- **WHEN** audio is recorded while transport is stopped
- **THEN** the waveform recorder advances with the delay's free-running phase, filling the display over a global loop period instead of repeatedly replacing one column
- **AND** the write and read markers move in the same coordinates as that waveform

#### Scenario: Restart preserves the recorded waveform
- **WHEN** transport restarts after the delay has been free-running
- **THEN** the display offset absorbs the playback phase reset without moving the write marker or remapping the recorded waveform
- **AND** read markers reflect the audio heads' resynchronization to musical phase

#### Scenario: Envelope reflects recorded audio
- **WHEN** audio with a distinct amplitude profile has been written into a channel's delay line over the last loop
- **THEN** that channel's `m_maxEnvelope`/`m_minEnvelope` arrays trace the per-bucket extrema of the recorded audio at the warped-time positions held in the positional buffer

### Requirement: Interpolated Output Phase Modulation
Quad Delay SHALL add H engine samples of nominal output latency and read the summed overlap-add output at `clock - H + pm` per channel on every engine sample. The PM offset SHALL range linearly from `-H + 2` to `H - 2` engine samples and SHALL use four-point Catmull-Rom cubic interpolation. The two-sample epsilon SHALL keep the interpolation support inside retained, finalized samples. Cleanup SHALL advance with the clock, retaining samples for the entire PM range independent of the modulated read positions.

#### Scenario: Neutral PM
- **WHEN** PM is zero
- **THEN** output equals the existing overlap-add result delayed by exactly one synthesis hop

#### Scenario: Modulation revisits history
- **WHEN** PM jumps between either endpoint or fractional positions across ring wraps
- **THEN** each channel returns the corresponding interpolated overlap sum without prematurely cleared history or contributions from unfinished future frames

#### Scenario: Clear retained history
- **WHEN** a channel is cleared
- **THEN** its queued output and retained history are silent at every PM setting and other channels are unaffected

### Requirement: Linear PM Knob
The delay page SHALL expose a continuous bipolar Quad knob named DelayPhaseModulation, short label OFFS to match Reverb, in cell `(0, 3)`. Its default SHALL be zero. The combined bipolar value, including live modulation, SHALL map directly and linearly to `pm = value * (H - 2)` engine samples without an additional filter or depth curve. The shared manual encoder smoothing SHALL remain outside the live modulation path. Existing modulation depth controls SHALL apply normally.

#### Scenario: Endpoint and center mapping
- **WHEN** the processed PM knob value is at minimum, center, or maximum
- **THEN** its offset is respectively `-H + 2`, zero, or `H - 2` engine samples

#### Scenario: Live offset modulation is unfiltered
- **WHEN** a channel's combined OFFS value steps to a new constant target
- **THEN** its offset reflects that value on the next engine sample without additional delay-input filtering

#### Scenario: Existing patch has no PM field
- **WHEN** a patch omits DelayPhaseModulation
- **THEN** the PM knob defaults to zero on all channels
