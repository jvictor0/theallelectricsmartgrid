# Mixdown and Mastering Specification

## Purpose
The mixdown and mastering stage (`QuadMixerInternal` in `private/src/QuadMixer.hpp`, `DualMasteringChain` in `private/src/QuadMasterChain.hpp`, `MultibandSaturator` in `private/src/MultibandSaturator.hpp`) is the final stage of the DSP engine. It mixes the nine quad-panned voices (see quad-panning and voice-architecture) and the source-mixer inputs into a main quad bus, feeds three send buses for quad-delay, quad-reverb, and partial-machine, sums their returns with cross-feed routing, and simultaneously produces three output formats: the primary quadraphonic mix, a stereo mixdown built through a 30-degree rotation matrix, and a dedicated mono sub channel. Both quad and stereo mixes pass through parallel four-band mastering chains with Linkwitz-Riley crossovers, per-band gain, and arctan saturation before leaving the synthesizer.

## Requirements

### Requirement: Voice Mixing into the Quad Bus
The system SHALL mix up to 16 input slots into the main quad bus: the nine voices occupy slots 0-8 with per-voice quad pan coordinates (`m_x`, `m_y`) and gain, and the source-mixer's external sources occupy the following slots panned to center (0.5, 0.5) at unity gain. Each voice is panned with `QuadFloat::Pan(x, y, input)`, soft-limited by a per-voice meter whose arctan saturation yields a reduction factor applied to the post-fader signal, and added to the quad output when its monitor flag is set. Each voice's sub-oscillator output arrives on a separate mono lane (`m_monoIn`), is panned to center, and is summed post-fader with the voice (see voice-architecture for the sub-oscillator itself).

#### Scenario: Voice placement follows pan coordinates
- **WHEN** voice 0 plays with pan coordinates (x, y) = (1.0, 0.5) and gain 1.0
- **THEN** the mixer adds `QuadFloat::Pan(1.0, 0.5, sample)` to the quad bus, placing the voice's energy at the +x edge of the field
- **AND** the same panned signal, scaled by the voice's send gains, feeds the send buses

#### Scenario: Hot voices are soft-limited per voice
- **WHEN** a voice's post-gain sample plus its sub lane drives its meter beyond unity
- **THEN** the meter's arctan stage computes a reduction factor below 1 that scales that voice's contribution to the quad bus, leaving the other voices unaffected

### Requirement: Three Effect Send Buses
The system SHALL maintain exactly three quad send buses (`x_numSends = 3`): send 0 feeds quad-delay, send 1 feeds quad-reverb, and send 2 feeds partial-machine (`SquiggleBoy::ProcessSends`). Each voice contributes its panned signal times an independent per-voice, per-send gain, so any voice can feed any combination of effects. The source-mixer input slots have all send gains forced to zero.

#### Scenario: Independent per-voice send levels
- **WHEN** voice 0's delay send gain is 0.5 and voice 1's is 0
- **THEN** the delay's quad input contains voice 0's panned signal at half gain and nothing from voice 1
- **AND** voice 0 can simultaneously feed the reverb and partial machine at unrelated gains

### Requirement: Effect Return Summation
The system SHALL sum the three effect returns (quad-delay, quad-reverb, partial-machine outputs) into the main quad bus, each scaled by its return gain and passed through a per-return quad meter with arctan saturation (`ProcessReturns`). The delay and reverb also receive their own post-gain returns back as feedback inputs to their processors.

#### Scenario: Return gain scales an effect into the mix
- **WHEN** the reverb produces output and its return gain is 0.5
- **THEN** the quad bus receives the reverb's quad output at half amplitude after the return meter's saturation
- **AND** setting the return gain to 0 removes the reverb from the main output entirely

### Requirement: Return Cross-Feed Between Effects
The system SHALL route each effect's return into the other effects' send buses through a 3x3 gain matrix (`m_returnSendGain`, `ProcessReturnSends`), skipping the diagonal so a return never feeds its own send. This enables chains such as delay-into-reverb, reverb-into-partial-machine, and partial-machine-into-delay.

#### Scenario: Delay return feeds the reverb
- **WHEN** `m_returnSendGain[0][1]` is 0.5
- **THEN** the delay's return, scaled by 0.5, is added to the reverb's send bus on top of the voices' own reverb sends

#### Scenario: Self-feed is structurally impossible
- **WHEN** any value is assigned to the matrix diagonal
- **THEN** `ProcessReturnSends` never adds return i into send i, because equal indices are skipped

### Requirement: Parallel Stereo Mixdown via Rotation Matrix
The system SHALL build the stereo mix in parallel with the quad mix, not by folding the finished quad bus (`QuadToStereoMixdown.hpp`). Each voice is mixed directly to stereo from its pan coordinates: `GetPosition(x, y)` projects the 2D position onto a stereo axis rotated by 30 degrees (`x_theta` = pi/6), and the sample is split with constant-power sine/cosine weights. Effect returns, which are already quad, are folded through fixed left/right matrices built by evaluating the same projection at the four field corners. Mono sub lanes enter the stereo mix at center.

#### Scenario: Diagonal quad energy survives the fold
- **WHEN** a voice sits at a rear corner of the quad field
- **THEN** its stereo position is the 30-degree rotated projection of that corner rather than a simple rear-to-front collapse, preserving left/right separation of diagonally opposed sources

#### Scenario: Centered source folds to equal stereo
- **WHEN** a source is at center (0.5, 0.5)
- **THEN** `GetPosition` returns 0.5 and the left and right channels receive equal constant-power weights sin2pi(1/8)

### Requirement: Four-Band Linkwitz-Riley Crossover Split
The system SHALL split every mastering-chain channel into four frequency bands using fourth-order Linkwitz-Riley crossovers (each built from two cascaded second-order Butterworth sections, 24 dB/octave slopes; `LinkwitzRileyCrossover.hpp`), applied recursively: one crossover splits low half from high half, then one more on each side (`BuildBandsRecursive`). The three crossover frequencies derive from the encoder bank: band 0/1 at the low-band cutoff (`Param::LowBandCutoff` via `SetBassFreq`), band 1/2 at that frequency times the mid factor, and band 2/3 times the high factor (`SetCrossoverFreq`, factors range 1-32).

#### Scenario: A tone between crossovers lands in one band
- **WHEN** a sine sits well inside band 1 (above the low cutoff, below the mid crossover)
- **THEN** its energy appears in band 1's meter with at most the crossover skirts (24 dB/octave) leaking into bands 0 and 2

#### Scenario: Crossover frequencies are chained multiplicatively
- **WHEN** the low-band cutoff is raised
- **THEN** all three crossover points move up together, because the mid and high crossovers are computed as the bass frequency multiplied by their factors

### Requirement: Per-Band Gain and Arctan Saturation
The system SHALL apply an independent gain (exponential range 1/16 to 2) to each band and pass each band of each channel through its own arctan soft-clip stage `atan(x * pi/2) / (pi/2)` (`Meter::ProcessAndSaturate` in `private/src/Metering.hpp`) before recombination. Saturating per band prevents intermodulation between bands: heavy bass drive generates harmonics only within the bass band's path and does not modulate the high bands. Per-band and master meters are exposed through `MultibandSaturator::UIState`, whose `TransferFunction` reproduces the crossover-plus-gain response analytically for display.

#### Scenario: Band gains shape tone independently
- **WHEN** band 0's gain is raised to 2 and the other bands stay at 1
- **THEN** only content below the low crossover is boosted (and saturates earlier), while bands 1-3 pass unchanged

#### Scenario: Bass drive does not distort highs
- **WHEN** band 0 is driven hard into its arctan stage while band 3 carries a quiet high-frequency signal
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
The system SHALL scale each mastering chain's recombined signal by a master gain (exponential range 1/4 to 2, `Param::MasterGain`) and pass each channel through a final arctan saturation stage with master metering. After mastering, `SquiggleBoy` multiplies the quad, stereo, and sub outputs by the global master volume (`m_masterVolume`).

#### Scenario: Master gain drives the final saturator
- **WHEN** the master gain is raised toward 2 with a hot mix
- **THEN** every output channel is scaled then soft-clipped by the final atan stage, so peak output approaches but never exceeds the saturator's ceiling

### Requirement: Dual-Chain Parallel Mastering
The system SHALL master the stereo and quad mixes through separate `MasteringChain` instances (`DualMasteringChain`: a 2-channel and a 4-channel `MultibandSaturator`) with independent filter and meter state, while a single set of controls (band gains, master gain, bass frequency, crossover factors) updates both chains identically (`DualMasteringChain::Input` setters). The stereo chain's saturator state feeds the mastering UI.

#### Scenario: Both formats are mastered every sample
- **WHEN** `ProcessReturns` finishes a sample
- **THEN** the quad bus has passed through the 4-channel chain and the parallel stereo mix through the 2-channel chain, and the engine's output struct carries quad, stereo, and sub simultaneously

#### Scenario: One control surface drives both chains
- **WHEN** the performer changes a band EQ encoder (`Param::LowEq` through `Param::HighEq`)
- **THEN** `SetGain` updates the corresponding band gain in both the stereo and quad saturator inputs, keeping the two formats tonally matched
