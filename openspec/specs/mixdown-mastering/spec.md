# Mixdown and Mastering Specification

## Purpose
The mixdown and mastering stage (`QuadMixerInternal` in `private/src/QuadMixer.hpp`, `DualMasteringChain` in `private/src/QuadMasterChain.hpp`, `MultibandSaturator` in `private/src/MultibandSaturator.hpp`) is the final stage of the DSP engine. It mixes the nine quad-panned voices (see quad-panning and voice-architecture) and the source-mixer inputs into a main quad bus, feeds three send buses for quad-delay, quad-reverb, and partial-machine, sums their returns with cross-feed routing, and simultaneously produces three output formats: the primary quadraphonic mix, a stereo mixdown built through a 30-degree rotation matrix, and a dedicated mono sub channel. Both quad and stereo mixes pass through parallel four-band mastering chains with Linkwitz-Riley crossovers, per-band gain, and tanh saturation before leaving the synthesizer.

## Requirements

### Requirement: Voice Mixing into the Quad Bus
The mixer SHALL have SquiggleBoy's fixed input layout (`QuadMixerInternal::Input`): three track inputs (`m_tracks`), one per voice trio, and four external inputs (`m_externals`), one per source-mixer source. Each track carries a `TrioFloat` signal, `TrioFloat` quad pan coordinates (`m_x`, `m_y`), a trio fader gain (`m_gain`), three trio send gains (`m_sendGain`), and the trio's `TrioFloat` sub-oscillator output (`m_sub`). Each external input carries a `StereoFloat` signal at unity gain with `StereoFloat` pan coordinates determined by the source's mono/stereo configuration, and a monitor flag. One generic `Mix` function handles every lane group: it applies fader gain before the meter's tanh stage, then pans each saturated lane into the group's live bus and the stereo mixdown when monitored. Send buses receive the raw lane before both fader gain and saturation when the group has send gains. The fader therefore drives the dry saturation and the voice level meter follows the dry contribution; subs and external inputs use unity gain. Tracks are always monitored and feed the dry bus. Track subs are saturated by their own meters and panned to center into the sub bus; they have no mixer gain and do not feed effect sends (see voice-architecture for the sub-oscillator itself). External inputs feed the dry bus only while their monitor flag is set.

#### Scenario: Voice placement follows pan coordinates
- **WHEN** voice 0 plays with pan coordinates (x, y) = (1.0, 0.5) and gain 1.0
- **THEN** the mixer adds `QuadFloat::Pan(1.0, 0.5, saturated)` to the quad bus, placing the voice's energy at the +x edge of the field
- **AND** the raw, panned signal, scaled by the voice's send gains, feeds the send buses

#### Scenario: Hot voices are soft-limited per voice after the fader
- **WHEN** a voice's sample times fader gain drives its meter beyond unity
- **THEN** the meter's tanh stage limits that voice's dry contribution to the quad bus, leaving its sends, sub lane and other voices unaffected
- **AND** lowering the voice's fader gain reduces both its saturation drive and metered dry level

#### Scenario: Sub levels bypass the voice mixer gain and sends
- **WHEN** a sub lane carries audio while its voice mixer gain is zero
- **THEN** the sub is independently saturated and mixed at center without that gain
- **AND** changing the sub level does not change the voice saturation or any effect send

### Requirement: Three Effect Send Buses
The system SHALL maintain exactly three quad send buses (`x_numSends = 3`): send 0 feeds quad-delay, send 1 feeds quad-reverb, and send 2 feeds partial-machine (`SquiggleBoy::ProcessSends`). Each voice contributes its raw pre-fader, pre-saturation panned signal times an independent per-voice, per-send gain, so any voice can feed any combination of effects independently of its fader gain. External inputs and subs have no sends.

#### Scenario: Independent per-voice send levels
- **WHEN** voice 0's delay send gain is 0.5 and voice 1's is 0
- **THEN** the delay's quad input contains voice 0's raw, panned signal at half gain and nothing from voice 1
- **AND** voice 0 can simultaneously feed the reverb and partial machine at unrelated gains

#### Scenario: Sends are independent of the fader
- **WHEN** a voice's fader gain is zero while its send gains are not
- **THEN** that voice contributes nothing to the dry bus and its level meter decays to silence
- **AND** its raw, panned signal still feeds the send buses at its send gains

### Requirement: Effect Return Summation
The system SHALL sum the three effect returns (quad-delay, quad-reverb, partial-machine outputs) into the main quad bus, each scaled by its return gain and passed through a per-return quad meter with tanh saturation (`ProcessReturns`). The delay and reverb also receive their own post-gain returns back as feedback inputs to their processors. Partial Machine return gain SHALL come from `PartialMachineVolume` (encoder 3,3), the same `m_returnGain` path as `DelayReturn` and `ReverbReturn`.

#### Scenario: Return gain scales an effect into the mix
- **WHEN** the reverb produces output and its return gain is 0.5
- **THEN** the quad bus receives the reverb's quad output at half amplitude after the return meter's saturation
- **AND** setting the return gain to 0 removes the reverb from the main output entirely

#### Scenario: Partial Machine return uses the shared return gain
- **WHEN** `PartialMachineVolume` is 0.5
- **THEN** the Partial Machine return is scaled by that gain on the same `m_returnGain[2]` path as delay and reverb

### Requirement: Return Cross-Feed Between Effects
The system SHALL route each effect's return into the other effects' send buses through a 3x3 gain matrix (`m_returnSendGain`, `ProcessReturnSends`), skipping the diagonal so a return never feeds its own send. This enables chains such as delay-into-reverb, reverb-into-partial-machine, and partial-machine-into-delay.

#### Scenario: Delay return feeds the reverb
- **WHEN** `m_returnSendGain[0][1]` is 0.5
- **THEN** the delay's return, scaled by 0.5, is added to the reverb's send bus on top of the voices' own reverb sends

#### Scenario: Self-feed is structurally impossible
- **WHEN** any value is assigned to the matrix diagonal
- **THEN** `ProcessReturnSends` never adds return i into send i, because equal indices are skipped

### Requirement: Parallel Stereo Mixdown via Rotation Matrix
The system SHALL build the stereo mix in parallel with the quad mix, not by folding the finished quad bus (`QuadToStereoMixdown.hpp`). Each voice is mixed directly to stereo from its pan coordinates: `GetPosition(x, y)` projects the 2D position onto a stereo axis rotated by 30 degrees (`x_theta` = pi/6), and the sample is split with constant-power sine/cosine weights. Effect returns, which are already quad, are folded through fixed left/right matrices built by evaluating the same projection at the four field corners. Sub lanes enter the stereo mix at center.

#### Scenario: Diagonal quad energy survives the fold
- **WHEN** a voice sits at a rear corner of the quad field
- **THEN** its stereo position is the 30-degree rotated projection of that corner rather than a simple rear-to-front collapse, preserving left/right separation of diagonally opposed sources

#### Scenario: Centered source folds to equal stereo
- **WHEN** a source is at center (0.5, 0.5)
- **THEN** `GetPosition` returns 0.5 and the left and right channels receive equal constant-power weights sin2pi(1/8)

### Requirement: Four-Band Crossover Split
The system SHALL split every mastering-chain channel into four frequency bands using the existing fourth-order Butterworth LP/HP branches (section Qs approximately 0.541196 and 1.306563, 24 dB/octave slopes; legacy name `LinkwitzRileyCrossover.hpp`), applied recursively: one crossover splits low half from high half, then one more on each side (`BuildBands`). Each branch SHALL retain −3 dB at cutoff and their unsaturated sum +3 dB there; this is not a textbook LR4 response. The crossover SHALL use linear trapezoidal state-variable sections with a shared first section and separate second sections for LP and HP, retaining unity LP DC gain at low engine-rate cutoffs without direct-form pole cancellation. Each crossover (`LinkwitzRileyCrossoverGeneric<float, NumChannels>`) SHALL process all chain channels as one vector with shared coefficients, and each channel SHALL match an independent mono crossover. The three crossover frequencies derive from the encoder bank: band 0/1 at the low-band cutoff (`Param::LowBandCutoff` via `SetBassFreq`), band 1/2 at that frequency times the mid factor, and band 2/3 times the high factor (`SetCrossoverFreq`, factors range 1-32). The low-band cutoff range SHALL be defined in Hz (0.5/1024 to 0.5/1024^(2/3) cycles per host sample, about 23 Hz to 240 Hz at 48 kHz) and converted to cycles per engine sample, so crossover frequencies do not move with the oversampling ratio.

#### Scenario: A tone between crossovers lands in one band
- **WHEN** a sine sits well inside band 1 (above the low cutoff, below the mid crossover)
- **THEN** its energy appears in band 1's meter with at most the crossover skirts (24 dB/octave) leaking into bands 0 and 2

#### Scenario: Crossover frequencies are chained multiplicatively
- **WHEN** the low-band cutoff is raised
- **THEN** all three crossover points move up together, because the mid and high crossovers are computed as the bass frequency multiplied by their factors

### Requirement: Per-Band Gain and Arctan Saturation
The system SHALL apply an independent gain (exponential range 1/16 to 2) to each band and pass each band of each channel through its own tanh soft-clip stage using `TanhSaturator`'s rational approximation `x * (27 + x*x) / (27 + 9*x*x)`, clamped to [-1, 1] (`Meter::ProcessAndSaturate` in `private/src/Metering.hpp`) before recombination. Saturating per band prevents intermodulation between bands: heavy bass drive generates harmonics only within the bass band's path and does not modulate the high bands. Per-band and master meters are exposed through `MultibandSaturator::UIState`, whose sampled response snapshots capture crossovers and gains together. Each snapshot SHALL reproduce the existing analytic crossover-plus-gain response, including complex phase during band recombination; the UI SHALL draw its lower/median/upper magnitude order statistics without estimating saturation.

#### Scenario: Band gains shape tone independently
- **WHEN** band 0's gain is raised to 2 and the other bands stay at 1
- **THEN** only content below the low crossover is boosted (and saturates earlier), while bands 1-3 pass unchanged

#### Scenario: Bass drive does not distort highs
- **WHEN** band 0 is driven hard into its tanh stage while band 3 carries a quiet high-frequency signal
- **THEN** band 3's output remains clean because each band saturates in isolation before the bands are summed

### Requirement: Mono Bass Extraction
The system SHALL always run the mastering chains with mono bass enabled (`monoTheBass` is hard-coded true in `MasteringChain::Process`): band 0 of all channels is averaged into one mono signal, saturated once through the band-0 gain stage, then added equally back to every channel and exposed as the chain's sub output (`m_sub`). The dedicated sub output of the engine is the quad chain's mono bass (`DualMasteringChain::Process`); per-channel stereo or quad bass imaging below the low crossover is therefore collapsed to mono.

#### Scenario: Bass is mono below the low crossover
- **WHEN** bass-band content exists only in the front-left channel
- **THEN** after mastering, all four quad channels carry that bass equally at one quarter of its level (the channel average), and the sub output carries the same mono bass signal

#### Scenario: Sub output tracks the quad chain
- **WHEN** the engine produces its outputs for a sample
- **THEN** `QuadFloatWithStereoAndSub::m_sub` equals the quad mastering chain's saturated mono band 0, scaled afterwards by the master volume alongside the quad and stereo outputs

### Requirement: Master Gain and Output Stage
The system SHALL scale each mastering chain's recombined signal by a master gain (exponential range 1/4 to 2, `Param::MasterGain`) and pass each channel through a final tanh saturation stage with master metering. The mastered quad, stereo, and sub outputs SHALL then pass through order-8 Butterworth decimators (`QuadMixerInternal::OutputDecimator`) to the host rate; the engine publishes one of every `x_oversample` decimated samples as the host frame. After decimation, `SquiggleBoy` multiplies the quad, stereo, and sub outputs by the global master volume (`m_masterVolume`).

#### Scenario: Master gain drives the final saturator
- **WHEN** the master gain is raised toward 2 with a hot mix
- **THEN** every output channel is scaled then soft-clipped by the final tanh stage, so peak output approaches but never exceeds the saturator's ceiling

### Requirement: Dual-Chain Parallel Mastering
The system SHALL master the stereo and quad mixes through separate `MasteringChain` instances (`DualMasteringChain`: a 2-channel and a 4-channel `MultibandSaturator`) with independent filter and meter state, while a single set of controls (band gains, master gain, bass frequency, crossover factors) updates both chains identically (`DualMasteringChain::Input` setters). The stereo chain's saturator state feeds the mastering UI.

#### Scenario: Both formats are mastered every sample
- **WHEN** `ProcessReturns` finishes a sample
- **THEN** the quad bus has passed through the 4-channel chain and the parallel stereo mix through the 2-channel chain, and the engine's output struct carries quad, stereo, and sub simultaneously

#### Scenario: One control surface drives both chains
- **WHEN** the performer changes a band EQ encoder (`Param::LowEq` through `Param::HighEq`)
- **THEN** `SetGain` updates the corresponding band gain in both the stereo and quad saturator inputs, keeping the two formats tonally matched

### Requirement: Host-Frame Metering
Each `Meter` SHALL accumulate its engine-rate samples and update its RMS smoothing, peak, peak-hold, and reduction state once per host frame from the frame's mean square, peak magnitude, and mean reduction. Peak-hold and smoothing lengths SHALL therefore be counted in host frames, and at `x_oversample = 1` metering SHALL be identical to per-sample metering.

#### Scenario: Peak hold lasts one second at any ratio
- **WHEN** a meter sees a single peak and then silence
- **THEN** the held peak persists for 48000 host frames regardless of the oversampling ratio

### Requirement: Separate Panned and Mono Recording Taps
Performance recording SHALL use a fixed track layout: one `panned_mono` track per voice (tracks 0-8, track-major), one per external input lane (tracks 9-16, source-major), one `mono` track per voice sub (tracks 17-25), the three returns (tracks 26-28), and the two masters (tracks 29-30). For ordinary mixing, recorded panned audio SHALL use the saturation of input times fader gain, with the current pan coordinates; recorded mono audio SHALL use its independently saturated sub lane without the voice fader gain. All audio taps SHALL receive the fixed recording attenuation described below. Their tap labels SHALL be `post_fader_post_saturation` and `post_saturation`, respectively. Recording SHALL remain independent of the external monitor flags and SHALL NOT change the live mix, sends, meters, or saturation processing. While recording, every recorded stream (panned inputs, mono lanes, returns) SHALL pass through its own order-8 Butterworth decimator, and the recorder SHALL begin, receive, and commit one frame per host frame. The master tracks SHALL record the output decimator's host-rate quad and stereo output at the fixed recording gain. Recording decimators run only while recording, so each take starts with the decimator's short warm-up.

#### Scenario: Voice and sub have independent saturation
- **WHEN** a voice and its centered mono lane both carry audio
- **THEN** each recorded contribution uses the saturation already computed for that lane
- **AND** a hot or opposite-polarity sub cannot change the recorded voice's saturation

#### Scenario: Monitor-disabled input remains recordable
- **WHEN** a source monitor flag is false while the source has audio
- **THEN** its panned-mono recording track still captures its post-gain, post-reduction signal
- **AND** its contribution to live output buses retains the existing monitor behavior

### Requirement: Recording Headroom
Every recorded audio lane, including voices, external inputs, subs, returns, and both masters, SHALL receive a fixed gain of 0.5 (approximately -6.02 dB) before PCM24 conversion. Panned, sub, and return recording paths SHALL apply this gain before their recording decimators; masters SHALL apply it after the shared output decimator. The gain SHALL leave live audio, meters, send levels, pan coordinates, phase, and event data unchanged. This reserves headroom for decimator overshoot without another saturation stage.

#### Scenario: Saturated transients retain recording headroom
- **WHEN** a saturated full-scale square wave overshoots unity during recording decimation
- **THEN** the attenuated recorded audio retains that transient below the PCM24 ceiling
- **AND** recorded pan coordinates retain their original scale

### Requirement: Quad Return Recording Taps
Performance recording SHALL capture the delay, reverb, and Partial Machine returns as three stable quad tracks using each return's post-gain, post-saturation contribution to the quad bus, preserving the engine's q0 through q3 order.

#### Scenario: Return recording matches the quad contribution
- **WHEN** an effect return is scaled and saturated in `ProcessReturns`
- **THEN** its recorded track contains that resulting quad value after recording attenuation, decimation, and PCM24 quantization
- **AND** the return is not processed through saturation a second time for recording

### Requirement: Independently Recorded Mastered Outputs Before Listening Volume
Performance recording SHALL capture both `DualMasteringChain` output formats directly after their respective mastering chains and before `SquiggleBoy` applies global master volume. The tracks SHALL have unique `master_quad` and `master_stereo` roles and `post_mastering_pre_master_volume` tap metadata. The stereo master SHALL come from the parallel stereo mastering chain. Both SHALL use the fixed recording gain and be captured regardless of hardware output selection. The global master-volume control SHALL affect listening level without affecting recorded master samples.

#### Scenario: Listening volume changes during recording
- **WHEN** otherwise identical processing runs use different global master-volume values
- **THEN** the recorded quad and stereo PCM24 master samples are identical between runs
- **AND** the live outputs reflect the different listening volumes

#### Scenario: Mastering controls remain audible in the recording
- **WHEN** mastering EQ or mastering-chain gain changes
- **THEN** each master recording contains the corresponding output of its own changed mastering chain
- **AND** stereo extraction does not fold down the recorded quad or recompute mastering

#### Scenario: Both output formats are always available
- **WHEN** the device is configured for stereo or for quad playback
- **THEN** the recording contains both master track definitions and their frame-aligned samples, subject to silent-block omission

### Requirement: One Complete Recording Frame per Mixer Sample
The mixer SHALL preserve its `ProcessInputs`/`ProcessReturns` processing sequence and existing start/stop/toggle control shape while replacing flat WAV-channel writes with typed track submissions. `ProcessReturns` SHALL complete exactly one recording frame after submitting both mastered outputs. Frame storage SHALL start with zero values so skipped submissions cannot retain data from earlier frames. Noise mode SHALL preserve skipped input/return recording behavior while still recording mastered outputs and advancing the timeline.

#### Scenario: Split and combined mixer calls
- **WHEN** a caller processes one sample using `Process` or a paired `ProcessInputs` and `ProcessReturns`
- **THEN** exactly one complete frame is submitted with matching input, return, and master timestamps

#### Scenario: Noise mode does not corrupt frame layout
- **WHEN** a recording enters noise mode or an input becomes unused
- **THEN** skipped input/return samples are zero for the affected frames and both masters remain recorded
- **AND** subsequent frames retain their correct track identities and contiguous positions
