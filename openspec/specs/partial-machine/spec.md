# Partial Machine Specification

## Purpose
The Partial Machine (`PartialMachine` in `private/src/PartialMachine.hpp`) is the third global send effect beside quad-delay and quad-reverb. It sums its quad send input to mono, tracks prominent spectral atoms (partials) with `SpectralModelGeneric<12, FrequencyDependentParameter>` (`private/src/SpectralModel.hpp`), and resynthesizes the atoms into the quadraphonic field with frequency-dependent reduction, radius and azimuth placement, unison expansion, and bipolar pitch shift. Every parameter is stored as four frequency-indexed lanes, so modulation can give lows, mids, and highs independent behavior. Send/return routing, including cross-feeds to and from quad-delay and quad-reverb, is owned by mixdown-mastering; visual presentation of the published atom state is owned by ui-visualization-pipeline.
## Requirements
### Requirement: Mono Summation and Hop-Based Spectral Analysis
The system SHALL sum each incoming quad sample to mono (`QuadFloat::Sum`) and write it into a 4096-sample circular analysis buffer. Every hop of H = 1024 samples, the buffer is Hann-windowed (`Math4096::Hann`) and passed to `SpectralModel::ExtractAtoms`, which DFT-transforms it, finds local spectral maxima, and refines each peak's frequency and magnitude by log-domain parabolic interpolation. The tracked atom set is capped at 1024 atoms (`InputSetter::x_numAtoms`), keeping the strongest by magnitude.

#### Scenario: Quad send collapses to one analysis stream
- **WHEN** voices feed the Partial Machine send with different quad pan positions
- **THEN** analysis sees the sum of all four channels as a single mono stream, so an atom's extracted magnitude does not depend on where its source was panned

#### Scenario: Peak interpolation lands between bins
- **WHEN** a sinusoid lies between two DFT bins
- **THEN** the extracted atom's analysis frequency is the parabola-interpolated peak position (k + p)/4096 cycles per sample rather than a bin center, and its magnitude is the interpolated peak magnitude

### Requirement: Spectral Atom Tracking Dynamics
The system SHALL track atoms across hops: each existing atom searches the new analysis peaks within its frequency-dependent density window (`m_omegaDensity`) and merges with the preferred match, slewing its synthesis magnitude toward the analysis magnitude with separate attack (`slewUpAlpha`, 0.01-2 s) and decay (`slewDownAlpha`, 0.01-10 s) rates and gliding its synthesis frequency toward the analysis frequency at the portamento rate (0.01-2 s). Within the density window, preferred-match selection SHALL preserve organic-over-synthetic priority and otherwise score candidates with theta = analysisMagnitude * max(0, 1 - distance / density), where distance is the absolute linear frequency difference from the tracked atom and density is the per-side density-window radius in the same linear frequency units. Distance SHALL be measured in Hz-equivalent cycles per sample, not volt-per-octave or `FrequencyToLinear` space. Matched peaks within the density window are consumed so they cannot spawn duplicates; remaining peaks above the death magnitude (`x_deathMag` = 1e-5) are born as new atoms whose magnitude slews up from zero. Unmatched atoms decay toward zero at the decay rate. Atoms whose synthesis magnitude falls below 1e-5 die and are removed.

#### Scenario: Atom survives a one-hop dropout
- **WHEN** a tracked atom's partial disappears from the analysis for one hop and the decay time is long
- **THEN** the atom is not deleted; its synthesis magnitude slews toward zero at the decay rate and recovers when the partial reappears within the density window

#### Scenario: Density window prunes near-duplicates
- **WHEN** two analysis peaks fall within one atom's `m_omegaDensity` window
- **THEN** the atom merges with the peak that has the strongest preferred-match theta after applying organic-over-synthetic priority, and the other peak is consumed, so no second atom is created at nearly the same frequency

#### Scenario: Nearby continuation can beat a farther louder peak
- **WHEN** an existing organic atom searches two organic analysis peaks inside its density window
- **AND** one peak is closer to the atom's current frequency while the other peak has slightly higher raw magnitude
- **THEN** the atom merges with the closer peak if the closer peak's theta is greater than the farther peak's theta

#### Scenario: Boundary candidates have no distance preference score
- **WHEN** an analysis peak lies at the edge of the atom's density window, where distance equals density
- **THEN** its preferred-match theta is zero, while a same-magnitude peak at the atom's current frequency keeps its full magnitude as theta

### Requirement: Frequency-Dependent Parameter Lanes
The system SHALL store every Partial Machine parameter as four lanes (`FrequencyDependentParameter::x_numParameters = 4`). Each atom is assigned a parameter index from its frequency at extraction time (`GetIndexForFrequency`), and parameter evaluation interpolates between the two neighboring lanes for that index. `PartialMachineLinearFrequency` (range 0.1-100) scales how fast the lane pattern repeats across the frequency axis. When all four lanes hold the same value the parameter behaves as a scalar; when modulation, gestures, or scenes differentiate the lanes, each frequency region sees its own interpolated value for attack, decay, density, bandwidth, panning, unison, pitch, and level.

#### Scenario: Equal lanes behave as a scalar control
- **WHEN** a quad-bank parameter has identical values in all four lanes
- **THEN** every atom receives the same interpolated value regardless of its frequency

#### Scenario: Differentiated lanes split the spectrum
- **WHEN** modulation makes lane values differ
- **THEN** atoms at different frequencies interpolate different parameter values, and increasing `PartialMachineLinearFrequency` makes the lane pattern repeat more often across the spectrum

### Requirement: Bell-Curve Reduction with Feedback
The system SHALL scale each atom's synthesis magnitude by a reduction factor (`SynthesisContext::GetReduction`): unity between the base frequency (`m_bwBaseFrequency`) and base × width (`m_bwWidth`), falling at -12 dB per octave below the base and -12 dB per octave above base × width, all multiplied by the volume parameter. The reduction-feedback parameter (`m_reductionFeedback`) interpolates the atom's stored synthesis magnitude toward the reduced value (floored at the death magnitude 1e-5), so at high feedback repeated reduction progressively re-shapes the tracked atom itself rather than only its output.

#### Scenario: Skirts fall at -12 dB per octave
- **WHEN** the base frequency corresponds to 500 Hz, the width factor is 2, and volume is unity
- **THEN** atoms between 500 Hz and 1000 Hz pass at full magnitude, an atom at 250 Hz is reduced by 12 dB, and an atom at 2000 Hz is reduced by 12 dB

#### Scenario: Feedback erodes attenuated atoms
- **WHEN** reduction feedback is at maximum and an atom sits deep in the reduction skirt
- **THEN** each synthesis frame writes the reduced magnitude back into the atom's synthesis magnitude, so the atom decays toward the death floor and is eventually pruned, while at zero feedback the stored magnitude is unaffected by reduction

### Requirement: Bipolar Pitch Shift
The system SHALL multiply each atom's synthesis frequency by `pitchShiftDepth^pitchShift` (`GetPitchShiftFactor`), where the pitch-shift control is mapped bipolar to [-1, +1] (knob value 2k - 1) and the depth parameter ranges exponentially from 5 cents (2^(5/1200)) to one octave (2.0). Full clockwise shifts every atom up by the depth interval, full counterclockwise shifts down by the same interval, and center applies no shift.

#### Scenario: Octave depth sweeps two octaves of shift
- **WHEN** `PartialMachinePitchShiftDepth` is at maximum (factor 2.0)
- **THEN** the pitch-shift knob at maximum doubles every atom's frequency, at minimum halves it, and at center leaves frequencies unchanged (factor 2^0 = 1)

### Requirement: Bass-Collapse Radius Mapping
The system SHALL map each atom's frequency to a quad radius (`GetRadius`): frequencies below the bass cutoff get radius 0 (center), frequencies above twice the cutoff get radius 1 (outer field), and frequencies in between ramp as log2(frequency / cutoff).

#### Scenario: Low partials collapse to center
- **WHEN** the bass cutoff corresponds to 100 Hz
- **THEN** a 50 Hz atom is placed at radius 0, a 200 Hz atom at radius 1, and a 141 Hz atom at radius ≈ 0.5 (log2(141/100))

### Requirement: Frequency-to-Azimuth Mapping
The system SHALL compute each atom's azimuth as `azimuthFactor × FrequencyToLinear(frequency) + azimuthOffset`, wrapped to [0, 1) (`GetAzimuth`). The azimuth factor ranges from 1/32 to 1, controlling how much of the orbit the spectrum spans; the shared azimuth offset rotates all atoms together.

#### Scenario: Azimuth factor compresses the spectral orbit
- **WHEN** the azimuth factor is at maximum (1.0)
- **THEN** atoms spread across the full [0, 1) orbit as frequency rises, while at 1/32 the same spectrum clusters within a 1/32 arc of the quad field, and azimuth values wrap continuously past 1.0

### Requirement: Shared Pan-Phase Azimuth Offset
The system SHALL set the Partial Machine synthesis azimuth offset from the same shared pan phase that drives voice Lissajous panning. The offset value SHALL be the current `SquiggleBoy::m_panPhase.m_phase` value after the pan phase advances for the audio sample, so Partial Machine atom placement rotates in phase with the Lissajous pan motion.

#### Scenario: Partial Machine offset follows the Lissajous pan phase
- **WHEN** `SquiggleBoy::ProcessSample` advances the shared pan phase for an audio sample
- **THEN** the Partial Machine synthesis context receives that same phase value as `m_azimuthOffset`
- **AND** voice pan inputs for that sample are driven from the same phase value

#### Scenario: Existing frequency azimuth mapping is preserved
- **WHEN** the Partial Machine computes an atom azimuth
- **THEN** the frequency-derived azimuth factor and linear frequency mapping are still added before wrapping to [0, 1)
- **AND** the shared pan-phase offset only rotates the resulting spectral orbit

### Requirement: Five-Voice Unison Expansion
The system SHALL expand each atom into a center copy plus two symmetric detuned pairs (`UnisonContext::x_numVoices = 5`). Pair p (1-2) uses interpolation `interp = min(1, (3 - p) × unison)`; each side voice gets gain `ZeroedExpParam::Compute(4, interp)`, detune `1.06^(±interp)`, and azimuth offset `±interp / 2^(4-p)`. All five gains are then RMS-normalized so unison changes spread and density without changing loudness.

#### Scenario: Unison at zero leaves one copy
- **WHEN** the unison parameter is 0
- **THEN** every pair's interpolation is 0, side gains are 0, and the atom synthesizes as a single copy at its own frequency and azimuth with gain 1 after normalization

#### Scenario: Raising unison staggers the pairs
- **WHEN** the unison parameter is 0.5
- **THEN** pair 1 reaches full interpolation (min(1, 2 × 0.5) = 1) with detunes 1.06 and 1/1.06 and azimuth offsets ±1/8, pair 2 is at interpolation 0.5, and the active copies' gains are scaled by the common RMS factor so total energy stays constant

### Requirement: Quad Panning and Overlap-Add Synthesis
The system SHALL place each unison copy in the quad field by converting azimuth and radius to coordinates `x = 0.5 + 0.5 × radius × cos2pi(azimuth)`, `y = 0.5 + 0.5 × radius × sin2pi(azimuth)` and panning with `QuadFloat::Pan(x, y, 1.0)` (`SynthesisContext::Pan`, `private/src/QuadUtils.hpp`). Every copy is written into a 4-channel `QuadDFT` as a windowed partial with its magnitude, phase, frequency, and quad distribution; each atom's synthesis phase then advances by `H × synthesisOmega`. `QuadOLA` (`private/src/OLA.hpp`) inverse-transforms and overlap-adds the frames at 75% overlap into the continuous quad return, which is produced every sample.

#### Scenario: Radius zero reaches the center
- **WHEN** an atom's radius is 0
- **THEN** its pan coordinates are (0.5, 0.5) and `QuadFloat::Pan` distributes it equally toward all four speakers, while radius 1 at azimuth 0 pushes it fully toward the field edge in the +x direction

#### Scenario: Continuous output between hops
- **WHEN** `PartialMachine::Process` is called on samples between analysis hops
- **THEN** it returns the next overlap-added quad sample from the OLA buffer without recomputing the spectral frame

### Requirement: UI State Publication
The system SHALL publish its state through `PartialMachine::UIState` for the visualization pipeline: a lock-free snapshot of the full tracked atom set (count plus per-atom analysis/synthesis frequency, magnitude, phase, and lane index), atomic copies of the synthesis parameters, and analytic transfer functions. `FrequencyResponse(f)` evaluates the reduction curve at frequency f through the same `GetReduction` code path as the DSP, and `QuadComponentFrequencyResponse(f, speaker)` multiplies that reduction by the pan distribution from the radius/azimuth mapping, giving the per-speaker response overlay.

#### Scenario: Spatial view reflects tracked atoms
- **WHEN** the UI reads the current snapshot
- **THEN** it observes exactly the atoms alive in the spectral model at the last commit, with the magnitudes and frequencies the synthesis stage used

#### Scenario: Per-speaker overlay matches DSP placement
- **WHEN** the UI asks for `QuadComponentFrequencyResponse` at a frequency below the bass cutoff
- **THEN** the returned per-speaker gains are equal across all four speakers (radius 0), and above twice the cutoff they differ according to the azimuth mapping, matching where the DSP actually pans atoms of that frequency

### Requirement: Residual Spectral Analysis
The system SHALL compute an analysis residual on each Partial Machine analysis hop after extracting input atoms. Each organic `AnalysisAtom` SHALL store its analysis phase from the source DFT, and the analysis residual SHALL be produced by writing extracted organic analysis atoms back into the input DFT at opposite phase so the modeled partials cancel. The remaining DFT magnitudes SHALL be copied into `SpectralModel::ResidualModel::Input::m_analysisResidualMagnitudes` with one residual bucket per DFT component, so residual envelope bucket `k` equals the remaining DFT magnitude at component `k`. Synthetic harmonics SHALL NOT be canceled from the input residual because they are generated model content rather than original input content.

#### Scenario: Pure tracked partial leaves low residual
- **WHEN** the analysis frame contains a single sinusoid that is extracted as an organic atom
- **THEN** residual analysis stores that atom's analysis phase and writes the atom back into the DFT at opposite phase
- **AND** the residual bucket magnitudes near the atom frequency are lower than the original DFT magnitudes at those bucket frequencies

#### Scenario: Broadband energy remains after atom cancellation
- **WHEN** the analysis frame contains a sinusoid plus broadband noise
- **THEN** residual analysis cancels the extracted sinusoid from the DFT
- **AND** residual bucket magnitudes still report the remaining broadband energy component-by-component

#### Scenario: Synthetic harmonics do not erase input residual
- **WHEN** synthetic harmonics are enabled for atom tracking
- **THEN** residual analysis skips synthetic analysis atoms during DFT cancellation
- **AND** only organic input atoms are subtracted before residual magnitudes are sampled

### Requirement: Residual Model Bucket Smoothing
The system SHALL store a `ResidualModel` inside `SpectralModelGeneric`. The residual model SHALL contain a float magnitude array with `DFT::x_maxComponents` entries, fixed DFT bucket frequencies, and a fixed precomputed log-frequency helper array for parameter indexing. Its processing function SHALL take both `SpectralModel::Input` and `ResidualModel::Input`; for each DFT bucket, it SHALL map the precomputed log-frequency value to a frequency-dependent parameter index without recomputing the logarithmic conversion every hop, then fold the analysis residual magnitude into the stored residual magnitude using the existing attack and decay parameters for that index. The residual model SHALL expose a query function that returns the residual envelope at a requested DFT bucket index.

#### Scenario: Residual attack follows frequency-dependent lane
- **WHEN** a residual bucket receives a larger analysis residual magnitude than its stored magnitude
- **THEN** the residual model updates that bucket using `m_slewUpAlpha` evaluated at the parameter index for the bucket's precomputed log-frequency value

#### Scenario: Residual decay follows frequency-dependent lane
- **WHEN** a residual bucket receives a smaller analysis residual magnitude than its stored magnitude
- **THEN** the residual model updates that bucket using `m_slewDownAlpha` evaluated at the parameter index for the bucket's precomputed log-frequency value

#### Scenario: Equal parameter lanes make residual smoothing scalar
- **WHEN** all attack and decay parameter lanes contain equal values
- **THEN** residual buckets use the same smoothing behavior regardless of their bucket frequency

#### Scenario: Residual envelope query matches DFT component index
- **WHEN** Partial Machine synthesis asks the residual model for the envelope at DFT bucket `k`
- **THEN** the residual model returns the smoothed residual magnitude stored at index `k`

### Requirement: Residual Quad Synthesis
The Partial Machine SHALL contain a `ResidualMachine` that adds residual energy into the same `QuadDFT` frame as tracked partial atoms. For each target quad DFT bucket, the residual machine SHALL read the residual envelope at the same DFT bucket index, compute frequency-dependent reduction and quad pan placement from that bucket frequency, compute per-channel magnitude as `residualEnvelope * reduction * pan[channel]`, choose a random phase for the synthesis frame, create a complex value with that magnitude and phase, and add it directly into the target quad DFT component without using a windowed-partial write. The residual machine SHALL also apply the reduction-feedback parameter to the residual bucket's stored magnitude, writing the feedback-shaped reduced magnitude back into the residual model with the same floor policy used by tracked atom reduction feedback.

#### Scenario: Residual buckets share the partial synthesis frame
- **WHEN** a Partial Machine synthesis frame is built
- **THEN** tracked atoms and residual buckets are written into the same `QuadDFT`
- **AND** the frame is submitted to `QuadOLA` once for continuous quad output

#### Scenario: Residual reduction matches frequency response
- **WHEN** a residual bucket frequency is outside the bell-curve pass band
- **THEN** the residual machine reduces the queried envelope magnitude with the same `SynthesisContext::GetReduction` behavior used for atoms at that frequency

#### Scenario: Residual reduction feedback writes back to model
- **WHEN** residual synthesis processes bucket `k` with nonzero reduction feedback
- **THEN** it writes a feedback-shaped magnitude back into residual model magnitude `k`
- **AND** the feedback target is the reduced residual magnitude using the same death-magnitude floor policy as tracked atom reduction feedback

#### Scenario: Residual pan follows bucket frequency
- **WHEN** a target quad DFT bucket is synthesized from the residual envelope
- **THEN** its quad distribution is computed from the same radius and azimuth mapping used for atoms at that bucket frequency

#### Scenario: Residual envelope and target bucket have matching indexes
- **WHEN** residual synthesis writes quad DFT component `k`
- **THEN** it uses residual model magnitude `k` as the envelope input for that component

#### Scenario: Residual phase is decorrelated per synthesis frame
- **WHEN** residual energy is added to target quad DFT buckets in a synthesis frame
- **THEN** each target bucket uses a random phase rather than the canceled input atom phase or a fixed zero phase

#### Scenario: Residual synthesis preserves existing DFT contents
- **WHEN** tracked atoms have already written partial energy into a quad DFT component
- **THEN** residual synthesis adds its random-phase complex value to the existing component value
- **AND** it does not clear or replace the existing partial energy
