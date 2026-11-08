## MODIFIED Requirements

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

## ADDED Requirements

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
