# Source Machine Oscillators Specification

## Purpose
Defines the per-voice sound generation stage of the Smart Grid One DSP engine: the four selectable source machines (Dual Wave Shaping VCO, Thru, Physical Modeling, and Sample) and the always-present per-voice sub-oscillator. The source machine is the first stage of each voice; its 4x-oversampled output feeds the per-voice filter machine described in filter-system, and its amplitude shaping is governed by the envelopes described in ahd-envelopes. Voice structure, triggering, and mixdown are covered by voice-architecture; the grain engine shared with the Sample source is covered by phase-vocoder.

## Requirements

### Requirement: Per-Voice Source Machine Selection
Each voice SHALL generate its raw signal with exactly one of four source machines selected by `VoiceConfig::SourceMachine`: Dual Wave Shaping VCO (`DualWaveShapingVCO`), Thru, Physical Modeling (`PhysicalModelingSource`), or Sample (`SampleSource`).
`SquiggleBoySource` owns all four machines and dispatches `ProcessUBlock` to the selected one; every machine renders a 4x-oversampled u-block (`x_oversample = 4`). The active selection is published to the UI thread as an atomic in `VoiceSourceUIState::m_sourceMachine`. For each trio, the selected `VoiceMachine::SourceMachine` value directly routes the Source slot to its corresponding page in `ForEachSmartGridOneSlot.hpp`; the page memberships in `ForEachSmartGridOnePage.hpp` determine which owned parameter cells are connected to the visible 4×4 grid.

#### Scenario: Switching the source machine changes dispatch and UI state
- **WHEN** a voice's `VoiceConfig::SourceMachine` is set to `Sample`
- **THEN** `SquiggleBoySource::ProcessUBlock` processes only the `SampleSource` for that voice and exposes its u-block output to the filter section
- **AND** `VoiceSourceUIState::m_sourceMachine` for that voice reads `Sample`

#### Scenario: Source-specific UI state is populated only for the active machine
- **WHEN** `PopulateVoiceSourceUIState` runs for a voice whose source machine is `PhysicalModeling`
- **THEN** `PhysicalModelingSource::UIState` is populated for that voice and the Sample-specific UI state is left untouched

### Requirement: Randomly Morphing Wavetables
Each of the Dual Wave Shaping VCO's two oscillators SHALL evaluate a `MorphingWaveTable` holding two randomly generated additive wavetables (Left and Right) crossfaded by a `wtBlend` value driven by a ganged random LFO.
Wavetable replacement is managed per track (trio): a `SquiggleBoyWaveTableGenerator` per oscillator slot builds a batch of three additive tables from one random multiplicative spectrum (coefficients fuzzed per table) and installs one per voice. When the crossfade sits fully at one side for every voice in the track (the other table completely inaudible), the inaudible table is replaced from a freshly generated batch, producing continuous timbral evolution. The LOD parameter (`m_morphHarmonics`, knob 0-1) maps exponentially to an effective maximum frequency `freq * (maxFreq / freq)^knob`, so the admitted harmonic count rises with the knob and is capped relative to oscillator frequency to avoid aliasing.

#### Scenario: Inaudible wavetable is regenerated
- **WHEN** the random crossfade LFO for a trio reaches 100% toward the Right table on all three voices, making the Left table inaudible
- **THEN** the wavetable generator builds a new random additive wavetable and installs it as the Left table for that trio's oscillators

#### Scenario: LOD limits harmonic complexity
- **WHEN** the LOD parameter is at 0
- **THEN** the effective maximum frequency collapses to the oscillator frequency itself, so the evaluated wavetable level contains only the fundamental and the output approaches a sine
- **AND** raising LOD toward 1 admits progressively more harmonics up to the frequency-dependent anti-aliasing cap

### Requirement: Vector Phase Shaping
Each oscillator SHALL distort its wavetable read phase with Vector Phase Shaping (VPS) controlled by two per-oscillator parameters: v (vertical inflection) and d (horizontal inflection/duty), each a 0-1 knob.
The v knob scales by 4 to a vertical inflection in [0, 4]; the warped phase is wrapped modulo 1, so vertical inflections above 1 wind the read phase through multiple wavetable cycles per oscillator period. The phase warp is piecewise linear through the inflection point (d, v) with elbows of width `0.25 * min(d, 1 - d)` smoothed by C2-continuous quintic Hermite segments. The d knob is shaped by a raised-cosine curve, and its effective deviation from 0.5 is scaled by `max(0, 1 - 16 * freq)` so the duty distortion narrows to neutral as oscillator frequency rises. Both v and d targets are smoothed by 25 Hz one-pole filters.

#### Scenario: Matched inflection leaves the phase undistorted
- **WHEN** the scaled vertical inflection equals the effective horizontal inflection (for example v knob = 0.125 and d knob = 0.5, giving v = d = 0.5)
- **THEN** both segment slopes equal 1, the phase mapping is the identity, and the wavetable is read with an undistorted linear phase

#### Scenario: Duty distortion collapses at high frequency
- **WHEN** the oscillator's normalized frequency reaches 1/16 or above
- **THEN** the d scale factor `max(0, 1 - 16 * freq)` reaches 0 and the effective horizontal inflection is pinned to 0.5 regardless of the d knob

### Requirement: Dual Oscillator Tuning, Cross-Modulation, and Mix
The Dual Wave Shaping VCO SHALL run two oscillators with VCO 0 at `baseFreq * detune` and VCO 1 at `baseFreq * offsetFreqFactor / detune`, mutually phase-modulating each other with per-direction cross-modulation indices, mixed by an equal-power crossfade and passed through a `BitRateReducer`.
`offsetFreqFactor` and `detune` are exponential parameters; the cross-modulation indices (`m_crossModIndex[2]`, knob 0-1, zeroed exponential) scale each oscillator's output as a phase-modulation input to the other. The mix gains are `Cos2pi(fade / 4)` for VCO 0 and `Cos2pi(fade / 4 + 0.75)` (= `Sin2pi(fade / 4)`) for VCO 1, a quarter-turn equal-power law. The bit crush amount slews and degrades the mixed output. All control parameters are slewed across the oversampled u-block.

#### Scenario: Equal-power crossfade endpoints
- **WHEN** fade = 0
- **THEN** the mixed output contains only VCO 0 (gain 1) and VCO 1 at gain 0
- **AND** at fade = 1 the output contains only VCO 1, with both gains equal to sqrt(2)/2 at fade = 0.5

#### Scenario: Cross-modulation disabled at zero index
- **WHEN** both cross-modulation indices are 0
- **THEN** each oscillator's phase-modulation input is 0 and the two oscillators run independently at their configured frequencies

### Requirement: Physical Modeling Signal Chain
The Physical Modeling source machine SHALL process white noise through a morphable 2-pole SVF, apply inverted exponential AHD amplitude modulation, apply sample-rate reduction, and feed the result into a `TunedDecayComb` whose delay frequency tracks the voice base frequency.
The SVF morph SHALL use shared quarter-circle weights: LP at 0, BP at 0.5, HP at 1, with nonnegative sine/cosine gains between endpoints. The source SHALL retain 4× processing, inverted amplitude-depth semantics, a 100 µs–2.5 s attack range, and a 500 µs–2.5 s decay range. AHD durations SHALL advance in base-rate samples and block trigger/release events SHALL be delivered once per microblock.

Factory defaults SHALL be CUT=1, RES=0, MRPH=0, RATE=0, FDBK=0.8, DAMP=0.5, ATK=0, HOLD=0.25, DEC=0, and AMP=1. RATE=0 SHALL reach full-rate 192 kHz bypass after parameter slewing; the exponential mapping SHALL retain its 48 Hz lower endpoint. Existing saved parameter values SHALL remain intact.

DECAY SHALL map `u=abs(2*knob-1)` to `T60=0.001*5000^u` seconds, negative polarity below 0.5, positive above, dry at 0.5. Every noncenter setting SHALL apply the exponential radial decay without additional feedback scaling. Settled fundamental T60 SHALL equal the selected value at every DAMP setting, for fractional periods, both signs, and clamped pitch boundaries. The selected T60 SHALL remain a ceiling for all other stationary modal lifetimes.

DAMP SHALL retain persisted identifier `PMCombDampenCutoff`, default 0.5, and label DAMP. For clamped `d=DAMP`, its actual prototype −3 dB cutoff SHALL be `fc/Fs=min(0.49,(f0/Fs)*exp2(1+6*d))`. Increasing DAMP SHALL open the cutoff exponentially, with 0/0.25/0.5/0.75/1 selecting 2×/4√2×/16×/32√2×/128× fundamental before the processing Nyquist clamp. Equal knob distances SHALL give equal octave intervals; full-open SHALL reach seven octaves above the fundamental when Nyquist permits. Neither DAMP endpoint SHALL bypass damping; exact DECAY center SHALL be the only bypass. The filter SHALL have two poles, variable Q tending to Butterworth as it opens, and a unity prototype magnitude peak at the fundamental; no prototype frequency SHALL exceed unity. Sub-fundamental gain SHALL be lower, with closed DC gain approaching 0.948683298 at low normalized frequency. There SHALL be no dry/bandpass blend or uniform boost of the old Butterworth filter. Each branch SHALL filter `input+sign*return`, and the mixed filtered output SHALL feed the shared delay and output. Exact DECAY center SHALL bypass L and remain dry. Bypassed low-pass states SHALL stay bounded when damping is re-enabled. The changed default midpoint sound (16× cutoff rather than 32×) SHALL be documented.

The prototype SHALL be `H0(z)=L(z)/(1-sign*z^-N*A(z)*L(z))`. With `omega=2*pi*f0/Fs`, `t=tan(pi*f0/Fs)`, `v=tan(pi*fc/Fs)/t`, `shape=hypot(1,v*v-1)`, and `u2=1/shape`, coefficients SHALL be `g=t*sqrt(shape)`, `k=sqrt(2*(1-u2))`, and `gain=sqrt((1-u2)*(1+u2))`. The low-pass SHALL be `L(z)=gain*g^2*(1+z^-1)^2/((1+k*g+g^2)+2*(g^2-1)*z^-1+(1-k*g+g^2)*z^-2)`. Phase compensation SHALL use `D=(theta+filterPhase)/omega`, `filterPhase=-atan2(k*sqrt(u2),1-u2)` except zero at bypass, where theta is 2π positive and π negative. Integer delay SHALL be `N=max(1,floor(D)-1)`, and delta SHALL be `D-N`. The unity-magnitude allpass SHALL use `A(z)=(a+z^-1)/(1+a*z^-1)`, `a=(tan(omega/2)-tan(omega*delta/2))/(tan(omega/2)+tan(omega*delta/2))`. Radial substitution `H(z)=H0(z/r)`, `r=exp(-ln(1000)/(Fs*T60))`, SHALL apply to integer memory (`r^N`), allpass memory, and both low-pass memories. No stationary mode SHALL decay more slowly than the selected T60.

At fundamental 220 Hz, 192 kHz processing, DAMP=1/3 (8× prototype cutoff), and selected T60=5 s, negative/positive third-mode tuning SHALL agree within 0.2 cents with independent 658.545543665752/659.270770269177 Hz (−3.819368033379/−1.913886817808 cents), and T60 SHALL agree within 1% with 1.427293671700/2.210498614261 s. Full-open third modes SHALL remain near harmonic and near 5 s. At minimum DAMP, independent negative/positive third-mode predictions are 636.849346134625/654.515578308296 Hz and 0.013876206521/0.028245094224 s T60. The implementation SHALL document their rapid loss and the fundamental leakage limitation of late-time third-mode projection, and verify rapid upper-mode suppression through emitted audio.

The implementation SHALL clamp normalized pitch to `[cycles/16383,0.25]` (cycles=1 positive/center, 0.5 negative) and retain the 16384-sample shared delay. At the highest negative-feedback pitches D may lie below 2; N SHALL remain 1 with a positive delta below 1, avoiding underflow and a zero-delay read. The cutoff SHALL clamp below processing-rate Nyquist; host output retains its base-rate Nyquist limit. Coefficients SHALL update outside the oversampled loop. Shared-delay dual branches SHALL crossfade filtered outputs over 5 ms, coalescing new targets during transitions without allocations, locks, or delay-buffer copies; stationary guarantees apply after settling.

#### Scenario: Comb resonance tracks voice pitch
- **WHEN** the voice base frequency changes within the representable bounds
- **THEN** integer delay, allpass, and low-pass phase SHALL converge to the played fundamental at `baseFreq/x_oversample`
- **AND** changing settled DAMP SHALL retain fundamental tuning and the exact selected fundamental T60

#### Scenario: Radial decay includes all filter memories
- **WHEN** DECAY changes between two settled non-center settings of the same sign
- **THEN** the impulse responses SHALL differ sample by sample by the corresponding radial factor ratio raised to the sample index

#### Scenario: AHD trigger shapes the noise excitation
- **WHEN** the voice's AHD control issues a trigger
- **THEN** the inverted AHD envelope modulates the filtered noise amplitude through the exponential curve `ZeroedExpParam::Compute(10, env)` before sample-rate reduction and the comb stage

### Requirement: Physical Modeling Transfer Function UIState
The Physical Modeling source SHALL own a `UIState` implementing the `TransferFunction` interface whose `TransferFunctionValue(freq)` is the product of the morph-blended SVF transfer function and the comb filter transfer function, computed entirely from atomic coefficients.
The atomics are `m_mainSVFG`, `m_mainSVFK`, `m_mainSVFMorph`, and the nested `TunedDecayComb::UIState` (two branches of integer delay, allpass, low-pass `g`/`k`/`gain`, radial loss, bypass and signed feedback coefficients, plus transition mix), populated by `PopulateUIState` from the live filter coefficients. `FrequencyResponse(freq)` returns the magnitude of `TransferFunctionValue(freq)`. The comb SHALL evaluate its actual transformed integer delay, allpass and low-pass transfer, rather than an ideal fractional delay. During transitions it SHALL display `((1-m)*P0+m*P1)/(1-((1-m)*R0+m*R1))`, where P are transformed low-pass numerators and R are complete signed return loops. Low-pass numerators SHALL evaluate `gain*g*g*(1+q)^2/((1+k*g+g*g)+2*(g*g-1)*q+(1-k*g+g*g)*q*q)` with `q=r*exp(-j*omega)` and each branch's atomic `k` and `gain` snapshots. Numerators SHALL use identity only for the exact dry-center state, and both numerator and return loops SHALL include every radial term. Settled and frozen-transition responses SHALL agree with rendered audio. This lets the Source visualizer bank draw the combined SVF-plus-comb response without touching DSP state.

#### Scenario: Morph at zero yields a pure low-pass times comb response
- **WHEN** `m_mainSVFMorph` is 0 and `TransferFunctionValue(freq)` is evaluated
- **THEN** the SVF factor equals `LinearStateVariableFilter::LowPassTransferFunction(g, k, freq)` exactly (LP gain 1, BP gain 0)
- **AND** the returned value is that factor multiplied by the comb UIState's `TransferFunctionValue(freq)`

#### Scenario: Frequency response is the transfer function magnitude
- **WHEN** `FrequencyResponse(f)` is called on the populated UIState for any normalized frequency f in [0, 0.5]
- **THEN** it returns `std::abs(TransferFunctionValue(f))`

### Requirement: Sample Source Playback
The Sample source machine SHALL read audio from the voice's assigned `AudioBufferBank` at a play-head position generated by `PhasorPlayHead` from a selected Theory of Time loop, rendered through a `GrainManager` and upsampled into the voice's 4x-oversampled domain.
`SampleLoopIndex` selects which of the six Theory of Time loops drives the read head. `SampleStart` and `SampleLength` define a normalized wrapped window (length clamped to at least 1e-6). `SampleReadSpeed` selects one of 17 quantized rates: -4, -3, -2, -3/2, -4/3, -1, -1/2, -1/4, 0, 1/4, 1/2, 1, 4/3, 3/2, 2, 3, 4. `SampleBankPosition` scans the bank's WAV files: even segments select a single file, odd segments linearly blend adjacent files. Grain resynthesis uses the same grain path as phase-vocoder; grains are rendered at the base control rate and upsampled by 4 into the oversampled u-block.

#### Scenario: Stopped read speed freezes the play head
- **WHEN** `SampleReadSpeed` selects the middle switch position (index 8)
- **THEN** the play-head speed is 0 and the sample position holds still while grains continue to resynthesize the frozen position

#### Scenario: Sample UIState reports play-head and window
- **WHEN** `SampleSource::PopulateUIState` runs
- **THEN** `SampleSource::UIState` atomically publishes `m_samplePosition` (total phase time), `m_start`, and `m_length` for the waveform visualizer

#### Scenario: Loop wrap marks a u-block top
- **WHEN** the play head's total phase time crosses an integer boundary between consecutive base-rate samples
- **THEN** the corresponding `m_uBlockTop` entry is set true, marking a playback wrap for downstream consumers

The play head SHALL apply speed and window scaling to absolute modulated loop phase before wrapping at the playback output. It SHALL NOT restart fractional-speed playback at every source-loop boundary. All existing bank selection, grain rendering, upsampling, window, and speed controls SHALL remain available.

#### Scenario: Half speed spans two source loops
- **WHEN** start is 0, length is 1, speed is 0.5, and absolute loop phase progresses through 0.5, 1.5, and 2.5
- **THEN** output positions are 0.25, 0.75, and 0.25

#### Scenario: Negative speed crosses source boundaries
- **WHEN** start is 0, length is 1, speed is -0.5, and absolute phase progresses through 0.5 and 1.5
- **THEN** output positions are 0.75 and 0.25

### Requirement: Source Mixer Input Configuration
The SourceMixer SHALL expose four configured external input sources. Each source SHALL have a DSP-owned configuration object that includes whether the source is Mono or Stereo. The SourceMixer SHALL expose the source color used by UIState consumers.

When source inputs are populated from the JUCE audio input buffer, a Mono source SHALL populate its left source lane from the configured input and SHALL populate its right source lane with silence for voice routing. A Stereo source SHALL populate both its left and right source lanes from the configured stereo input pair. The configured stereo input pair SHALL map source `i` to left physical input `2 * i` and right physical input `2 * i + 1`. Source colors SHALL be read from SourceMixer DSP code or SourceMixer UIState by config controls, meters, and source mixer visualizers.

When source lanes are sent to the quad mixer, a Mono source SHALL preserve the existing centered X/Y panning for both lanes. A Stereo source SHALL set each lane's X coordinate from bit 0 of `source * 2 + lane + 1` and SHALL set each lane's Y coordinate from bit 1 of `source * 2 + lane + 1`, so each stereo pair is assigned to opposite quad corners.

When source trim is sent to K-Mix, the system SHALL map each source index to two physical trim channels. Source `i` SHALL set trim for physical even channel `2 * i` from the source's current trim value. If source `i` is Mono, physical odd channel `2 * i + 1` SHALL be trimmed to zero. If source `i` is Stereo, physical odd channel `2 * i + 1` SHALL receive the same current trim value.

#### Scenario: Four external source slots are available
- **WHEN** the source mixer input state is initialized
- **THEN** it exposes four source configuration objects and four source signal slots

#### Scenario: Mono source has a silent right lane
- **WHEN** source 2 is configured as Mono and its left input receives audio
- **THEN** source 2 left lane contains the input audio
- **AND** source 2 right lane contains silence

#### Scenario: Stereo source exposes both lanes
- **WHEN** source 3 is configured as Stereo and its configured stereo input pair receives different left and right audio
- **THEN** source 3 left lane contains the left input audio
- **AND** source 3 right lane contains the right input audio

#### Scenario: Source color comes from DSP-owned state
- **WHEN** a source mixer meter or config-grid source button requests the color for source 2
- **THEN** the color is read from SourceMixer DSP code or SourceMixer UIState for source 2

#### Scenario: Stereo source lanes feed independent mixer input meters
- **WHEN** source 3 is configured as Stereo and its left and right lanes have different levels
- **THEN** source 3's left and right lanes are sent to separate mixer input meter slots

#### Scenario: Mono source lanes stay centered in the quad mixer
- **WHEN** source 1 is configured as Mono and its lanes are sent to the quad mixer
- **THEN** both source lanes keep the existing centered X/Y panning

#### Scenario: Stereo source lanes use incremented source input index bits for quad panning
- **WHEN** source 2 is configured as Stereo and its lanes are sent to the quad mixer
- **THEN** each source lane sets X from bit 0 of `source * 2 + lane + 1`
- **AND** each source lane sets Y from bit 1 of `source * 2 + lane + 1`
- **AND** the source's left and right lanes occupy opposite quad corners

#### Scenario: Mono source trim mutes the unused physical input
- **WHEN** source 1 is configured as Mono and its current trim value is sent to K-Mix
- **THEN** physical input channel 2 receives the current trim value
- **AND** physical input channel 3 receives zero trim

#### Scenario: Stereo source trim applies to both physical inputs
- **WHEN** source 2 is configured as Stereo and its current trim value is sent to K-Mix
- **THEN** physical input channel 4 receives the current trim value
- **AND** physical input channel 5 receives the current trim value

### Requirement: Thru Source Machine
The Thru source machine SHALL pass the external `SourceMixer` source channel assigned by the voice's source assignment through the voice, upsampling the source's base-rate u-block by 4 into the oversampled domain so the voice's filter and amp sections shape outside audio.

A source assignment SHALL identify one of three states: Silent, Source Left, or Source Right. A Silent assignment SHALL produce zero source output. A Source Left assignment SHALL read the selected source's left lane. A Source Right assignment SHALL read the selected source's right lane, which is silence when the selected source is configured as Mono.

#### Scenario: External mono audio is forwarded into the voice chain
- **WHEN** a voice in Thru mode processes a u-block and its assignment is Source Left for a Mono source carrying external input audio
- **THEN** the voice's source output equals the 4x-upsampled left source block, which then feeds the voice's filter machine whose cutoff anchor in Thru mode is specified in filter-system

#### Scenario: External stereo right audio is forwarded into the voice chain
- **WHEN** a voice in Thru mode processes a u-block and its assignment is Source Right for a Stereo source carrying right-channel external input audio
- **THEN** the voice's source output equals the 4x-upsampled right source block, which then feeds the voice's filter machine

#### Scenario: Silent assignment produces silence
- **WHEN** a voice in Thru mode processes a u-block and its source assignment is Silent
- **THEN** the voice's source output is silence before the filter machine

### Requirement: Sub-Oscillator
Each voice SHALL run a sine sub-oscillator (`VCO`) at half the voice base frequency (one octave down) at the base sample rate, saturate it with a per-voice tanh stage, gate its level with the main voice AHD envelope, and emit it as a per-voice mono signal.
The sub runs every sample regardless of source machine. Its tanh input gain is an exponential parameter spanning 0.125 to 8. The sub only sounds when the amp section's sub trigger was true at the most recent AHD trigger (latched into `m_subRunning`); its gain is the sub gain parameter multiplied by the main AHD envelope's exponential value, smoothed by a 170 Hz one-pole. The result is scaled by the voice's frequency-dependent gain and voice gain, published as `m_subOut`, and injected into the quad mixer as a center-panned mono input (mixdown behavior is covered by voice-architecture).

#### Scenario: Sub is silent without its trigger
- **WHEN** an AHD trigger arrives with the amp section's sub trigger input false
- **THEN** `m_subRunning` latches false and the smoothed sub gain decays to 0, producing no sub output for that note

#### Scenario: Sub level follows the main amp envelope
- **WHEN** the sub is running and the main AHD envelope decays
- **THEN** the sub output level tracks the product of the sub gain parameter and the exponential amp envelope value, so the sub fades with the voice
