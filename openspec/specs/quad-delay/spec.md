# Quad Delay Specification

## Purpose
The Quad Delay (`QuadDelay` and `QuadDelayInputSetter` in `private/src/QuadDelay.hpp`, head logic in `private/src/TapeHead.hpp`, buffers in `private/src/DelayLine.hpp`) is the first of three global quadraphonic send effects. It is a time-warped delay synchronized to the Theory of Time (see phasor-timebase). Its write trajectory F maps real time to continuous master phase, and its chosen forward-history partial inverse G maps phase back to real time. A logical delay d requests `X(G(F(t) - d))` before real-time analysis-window constraints. Playback is resynthesized through phase-vocoder grains (see phase-vocoder) so pitch is preserved while logical time stretches, compresses, or reverses. Per-channel feedback with saturation and band-pass damping, quantized delay ratios and read speeds, pitch shifting with unison, per-partial slew-up, and quadraphonic rotation complete the effect. Send and return routing between the delay, reverb, and partial machine is specified in mixdown-mastering.

## Requirements

### Requirement: Time-Warped Delay Line with Movable Write Head
The system SHALL saturate each engine-rate input sample, decimate it through an order-8 Butterworth `DownsamplerGeneric`, and write the decimated sample once per host frame into a `QuadDelayLineMovableWriter` of 2^24 host samples per channel, at the absolute host sample completed by the engine clock. The Theory of Time SHALL own the shared phase-to-real inverse, wire it to the delay at startup, and observe the continuous master phase and absolute `SampleTimer` engine sample every engine sample. The inverse SHALL use a tempo-independent grid of 2^18 cells per master cycle with 2^20 cells spanning four cycles and the compact branch storage specified in warped-scope-history. The caller SHALL supply each real timestamp; the inverter SHALL NOT maintain an independently advancing clock. The delay converts returned real timestamps by division by `x_oversample`, and tracks its actual recorded-audio bounds independently of inverse writes. Audio is stored indexed by wall-clock time; reads first convert a continuous-phase position to wall-clock time through the inverse (`GetRealTime`) and then read the audio buffer at that wall-clock index with cubic interpolation.

#### Scenario: One write per host frame
- **WHEN** the delay processes `4096 * x_oversample` engine samples
- **THEN** the delay line's wall-clock time advances by 4096 host samples

#### Scenario: Inverse buffer is populated by scattering
- **WHEN** the continuous-phase write head ascends across one or more fixed phase-grid boundaries
- **THEN** the writer interpolates the real engine timestamp at each crossed boundary between adjacent ascending observations and commits the corresponding cell and frontier transitions

#### Scenario: Descending warped time defers inverse updates
- **WHEN** the continuous-phase position decreases from one sample to the next (time modulation runs the clock backward)
- **THEN** audio continues to be recorded at the wall-clock index while the inverse and forward frontier retain their selected forward history
- **AND** ascent resumes the compact cell transitions, retaining the minimum cut within one confined visit and replacing it for a separate visit

### Requirement: Write Head Continuity via Phase Offset
The shared write head SHALL be computed every engine sample by one `PhaseClock`, owned by the Theory of Time, in continuous master cycles. While running, its phase SHALL equal modulated musical phase plus a constant offset. While stopped, it SHALL advance by the nominal phase increment. Restart SHALL set the offset to continuous phase minus the reset musical phase without changing continuous phase. Read heads and the delay input SHALL use this same continuous coordinate. Tempo edits SHALL change future travel without rescaling stored phase coordinates or rebasing the offset.

#### Scenario: Transport stop freezes warped time but not recording
- **WHEN** the Theory of Time stops running
- **THEN** the write head continues from its current phase at the nominal tempo while audio recording continues at the host sample rate
- **AND** when the transport resumes, the phase offset preserves the position reached during the stop

#### Scenario: Master loop length change does not jump the head
- **WHEN** the nominal master-loop duration changes while the transport is running
- **THEN** future phase increments reflect the new duration without rescaling the write position or previously recorded inverse coordinates

#### Scenario: Delay phase continues while transport is stopped
- **WHEN** transport stops and Theory of Time resets its phase to zero
- **THEN** the continuous phase advances from its last position at the current nominal tempo
- **AND** the read heads keep their selected playback speeds within the region behind the advancing write head
- **AND** starting transport rebases the musical offset without moving the continuous write or read phases

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
- **THEN** the projected read position moves backward through continuous phase at one cycle per write-phase cycle while grains continue to be synthesized at the original pitch

### Requirement: Read Head Projection into the Selected Loop
The read head SHALL be computed per channel by `ReadTapeHead` from `projected = writePhase * readHeadSpeed - loopPeriod * bufferFrac`, where `loopPeriod = 1 / cycleRatio(selectedLoop)` is positive. `WrapMod(writePhase - loopPeriod, writePhase, projected)` SHALL place the read head in `[writePhase - loopPeriod, writePhase)`. The resulting continuous `readPhase` SHALL drive both the displayed marker and the audio query through the shared inverse. No sample-derived hop offset SHALL be applied in phase coordinates; complete analysis-window safety SHALL be applied in real time after inversion.

#### Scenario: Non-unit speed preserves agreement between audio and display
- **WHEN** the write head has a nonzero continuity phase offset and the read speed is slow, fast, or negative
- **THEN** the audio query and displayed read marker use the same continuous read phase
- **AND** restarting transport with fixed controls preserves the write and read coordinates

#### Scenario: Read head stays behind the write head
- **WHEN** the read head is computed with any speed and delay ratio
- **THEN** the wrapped read phase is at least `writePhase - loopPeriod` and strictly less than `writePhase`
- **AND** the grain manager constrains the inverted real timestamp to a complete pair of analysis windows with cubic interpolation support

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
The system SHALL form the delay line input as `input + return * feedback` per channel, where the return is the delay's own previous output, the feedback gain is the feedback knob mapped through a zeroed exponential curve and scaled by 1.25 (values above 1.0 permit self-oscillation), and the sum is passed through a tanh saturator before being written. The output side SHALL pass the rotated, resynthesized signal through a `PostFeedbackFilter` band-pass (one-pole high-pass plus low-pass per channel) whose base and width are exponential mappings of the damping knobs (base `QuadOPBaseWidthFilter::x_minBaseHz` to `x_maxBaseHz`, i.e. 1/2048 to 0.5 cycles per host sample (23.4375 Hz to 24 kHz at 48 kHz), converted to cycles per engine sample; width 1 to 2048).

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
The system SHALL publish per-channel damping filter alphas and continuous master-phase head positions, and expose a common scope of the magnitude of each actual completed delay-line write. Its periodic axis SHALL have origin zero and period one master cycle, without start/end events. The UI SHALL sample through the shared phase-to-real inverse, convert filtered magnitude to dB, and draw symmetrically from negative to positive level about each lane center. Separate min/max envelope buckets and the positional waveform recorder SHALL be removed. Signed delay-return scopes used for spectra SHALL retain their existing signal.

#### Scenario: Head positions track the tape heads
- **WHEN** the delay publishes its head positions
- **THEN** markers and waveform use the same explicit master-cycle axis

#### Scenario: Stopped recording remains visible across the whole display
- **WHEN** audio is recorded while transport is stopped
- **THEN** continuous phase advances at nominal tempo and the display retains its full period even before the first cycle completes

#### Scenario: Restart preserves the recorded waveform
- **WHEN** transport restarts with fixed read speed, delay and loop selection
- **THEN** a phase offset absorbs the musical reset without moving the write or read coordinates
- **AND** existing waveform coordinates retain their meaning

#### Scenario: Envelope reflects recorded audio
- **WHEN** audio with a distinct amplitude profile is written into the delay
- **THEN** its magnitude passes through the existing LOD pipeline before symmetric dB display conversion

### Requirement: Unavailable Inverse History
Unwritten inverse history SHALL be unavailable. A lookup SHALL select the cell's fallback or replacement traversal and interpolate only within that traversal's available endpoints, using entry/frontier interpolation for a partial first traversal as specified in warped-scope-history. Grain launch SHALL preserve unavailable status before applying the latest-complete-window clamp. A channel with unavailable inverse history or insufficient retained audio for its previous/current analysis windows and cubic interpolation support SHALL contribute no new grain. Other channels and already scheduled overlap-add tails SHALL continue. Recorded sample-bank playback SHALL retain its unbounded-history behavior.

#### Scenario: Cold start does not substitute recent audio for missing delay history
- **WHEN** a one-second delay is requested before that history has been recorded
- **THEN** an unmapped read position contributes silence rather than selecting the latest complete audio window
- **AND** playback resumes once the mapping and required audio support become available

#### Scenario: Inverse recording advances before a host write
- **WHEN** the engine records another inverse observation between completed host samples
- **THEN** the delay's recorded-audio frontier remains at its last actual host write

#### Scenario: One channel loses its history
- **WHEN** one quad channel has an unavailable mapping at a grain boundary
- **THEN** its new grain is silent, the other channels continue, and its previously scheduled overlap-add tail is preserved

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
