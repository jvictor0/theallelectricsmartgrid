# Partial Machine Specification

## Purpose
The Partial Machine (`PartialMachine` in `private/src/PartialMachine.hpp`) is the third global send effect beside quad-delay and quad-reverb. It sums its quad send input to mono, tracks prominent spectral atoms (partials) with `SpectralModelGeneric<12, FrequencyDependentParameter>` (`private/src/SpectralModel.hpp`), and resynthesizes the atoms into the quadraphonic field with frequency-dependent reduction, radius and azimuth placement, unison expansion, and bipolar pitch shift. Spectral controls are stored as four frequency-indexed lanes, so modulation can give lows, mids, and highs independent behavior. Send/return routing, including cross-feeds to and from quad-delay and quad-reverb, is owned by mixdown-mastering; visual presentation of the published atom state is owned by ui-visualization-pipeline.
## Requirements
### Requirement: Mono Summation and Hop-Based Spectral Analysis
The system SHALL sum each incoming quad sample to mono (`QuadFloat::Sum`) and write it into a 4096-sample circular analysis buffer. Every hop of H = 1024 samples, the buffer is Hann-windowed (`Math4096::Hann`) and passed to `SpectralModel::ExtractAtomsAndResidual`. Extraction SHALL use log-domain parabolic interpolation for frequency and fit a complex coefficient against the Hann synthesis kernel for magnitude and frame-start phase. Each fitted partial SHALL be subtracted in place before the next coefficient fit. The production input setter SHALL configure a 256-partial budget (`InputSetter::x_numAtoms`): analysis peaks SHALL be capped by analysis magnitude before tracking, and the final tracked set SHALL be capped by synthesis magnitude. Extraction SHALL still examine the full spectrum, and peaks discarded by this budget SHALL NOT be restored to the residual.

#### Scenario: Quad send collapses to one analysis stream
- **WHEN** voices feed the Partial Machine send with different quad pan positions
- **THEN** analysis sees the arithmetic sum of all four channels as a single mono stream, with no retained input pan position

#### Scenario: Peak interpolation lands between bins
- **WHEN** a sinusoid lies between two DFT bins
- **THEN** the extracted atom's analysis frequency is the parabola-interpolated peak position (k + p)/4096 cycles per sample rather than a bin center, and its magnitude and phase come from the fitted complex Hann coefficient

#### Scenario: Dense input and lingering tails respect the matching budget
- **WHEN** a frame contains more than 256 eligible analysis peaks
- **THEN** only the 256 strongest analysis peaks enter matching
- **AND** after merging, births, and pruning, at most 256 synthesis atoms remain for the next frame
- **AND** the next assignment has at most 256 old atoms and 256 incoming peaks, even when previous partials are still decaying

### Requirement: Ordered One-to-One Spectral Matching
The system SHALL compute the complete assignment of existing atoms to new analysis peaks before changing tracked frequencies or magnitudes. `AtomMatcher::Match` SHALL sort old atom pointers by their last analysis log2 frequency and sort analysis peaks by log2 frequency. Each old atom and each peak MAY participate in at most one match, and matches SHALL preserve frequency order without crossings. Synthesis magnitude, portamento, pitch shift, and unison SHALL NOT determine this order.

A pair SHALL be eligible only when `octaveDistance <= density` and `analysisMagnitude / synthesisMagnitude >= 0.001`. Here `octaveDistance = abs(peak.logAnalysisOmega - atom.logAnalysisOmega)` and density is evaluated at the old atom's stored parameter index. The score SHALL be `theta = analysisMagnitude * max(0, 1 - octaveDistance / density)`. Zero density SHALL permit only an exact-frequency match, scored at its full analysis magnitude.

The assignment SHALL maximize the sum of theta over all matches; only equal total scores SHALL prefer more matches. Zero-score pairs at an eligible window boundary MAY be matched by this tie-break. Reconstruction SHALL use the matcher's fixed member workspace and two score rows, with O(oldAtomCount * analysisPeakCount) time and O(x_maxAtoms) workspace, without an allocated matrix of all candidate pairs.

#### Scenario: Stationary partial identities survive changing magnitudes
- **WHEN** two existing partials have distinct analysis frequencies and the next frame contains those same frequencies with different magnitudes
- **AND** both exact-frequency continuations satisfy the gain threshold
- **THEN** the maximum-score assignment retains both continuations even if synthesis magnitudes are ordered differently
- **AND** portamento or unison cannot reverse the order used for matching

#### Scenario: Global score takes precedence over the number of matches
- **WHEN** a valid one-match assignment has higher total theta than every valid two-match assignment
- **THEN** the matcher chooses the one-match assignment
- **AND** the remaining atoms and peaks follow unmatched decay, domination, or birth rules

#### Scenario: Nearby continuation can beat a farther louder peak
- **WHEN** one old atom has two eligible analysis peaks and no competing old atoms
- **THEN** it chooses the peak with greater theta, which can be the closer but quieter peak

#### Scenario: Weak peak cannot reserve a match
- **WHEN** a peak's magnitude is less than 0.001 times an old atom's synthesis magnitude
- **THEN** that pair is excluded before the assignment is solved
- **AND** the peak remains available to another eligible atom or for birth

#### Scenario: Match results are complete and reusable
- **WHEN** `Match` completes
- **THEN** its result contains one entry for every existing atom and one for every analysis peak, marking actual matches separately from domination
- **AND** old atoms still retain their pre-match state until the caller applies the results
- **AND** the next call clears all previous results before computing the new assignment

### Requirement: Density Controls Replacement and Coexistence
The density value SHALL be a per-side frequency radius in octaves, exponentially mapped from one cent to one octave. The clockwise knob direction SHALL invert that range: counterclockwise gives the broadest window, and clockwise gives the narrowest. Density SHALL NOT impose minimum spacing between analysis peaks or directly determine the atom count.

After assignment, each unmatched old atom SHALL select the eligible current peak with the highest theta in its density window as a possible dominator. This peak MAY already be matched or MAY still be unclaimed. A peak with raw magnitude below the old synthesis magnitude SHALL NOT suppress it. Otherwise, when the domination theta exceeds the old magnitude `a`, the system SHALL reduce the magnitude to `a * a / theta`, then apply ordinary unmatched decay. Suppression SHALL use raw analysis magnitude without waiting for the new atom's attack envelope.

Selection and suppression SHALL both evaluate density at the old atom's stored parameter index, preserving the same theta cone even when the peak's parameter lanes differ.

#### Scenario: Different density lanes preserve the selected suppression cone
- **WHEN** a peak lies at the old atom's density-window boundary and has a wider density value at its own parameter index
- **THEN** its domination theta remains zero during application and it cannot suppress the old atom
- **AND** increasing the old atom's radius slightly past the boundary does not substitute the peak's wider cone

#### Scenario: New unclaimed peak can suppress an old tail
- **WHEN** the global noncrossing assignment leaves a peak unclaimed and an old atom unmatched
- **AND** that peak is the old atom's strongest eligible dominator
- **THEN** the peak can suppress the old atom and subsequently start a new atom with its own attack

#### Scenario: Density controls replacement versus mixing
- **WHEN** a strong analysis peak lies near an unmatched old atom and density lanes have equal values
- **THEN** a broad counterclockwise window increases its domination score and can suppress the tail before ordinary decay
- **AND** a narrow clockwise window reduces or eliminates this suppression, allowing the old and new partials to coexist longer

#### Scenario: Boundary candidates have zero theta
- **WHEN** octave distance equals density
- **THEN** theta is zero, while a same-magnitude peak at the old analysis frequency retains its full magnitude as theta

### Requirement: Spectral Atom Tracking Dynamics
The caller SHALL merge matched atoms with their selected peaks, slewing synthesis magnitude toward analysis magnitude using separate attack (`slewUpAlpha`, 0.01-2 s) and decay (`slewDownAlpha`, 0.01-10 s) rates. Synthesis frequency SHALL glide toward analysis frequency at the portamento rate (0.01-2 s). Matched atoms SHALL retain their synthesis phase and refresh their analysis frequency, cached log2 frequency, magnitude, phase, and parameter index from the selected peak.

Matched peaks SHALL NOT spawn duplicate atoms. Each unclaimed peak at or above `x_deathMag = 1e-5` SHALL start a new atom at its analysis frequency with zero synthesis phase and initial magnitude `max(attackSlew(0, analysisMagnitude), x_deathMag)`. New atoms SHALL come only from input analysis, without synthetic harmonic generation or separate organic/synthetic mix gains. Unmatched old atoms SHALL decay toward zero while retaining their last analysis frequency as the portamento target. After merging and births, the system SHALL retain at most 256 atoms (`InputSetter::x_numAtoms`) by synthesis magnitude and remove nonfinite magnitudes or magnitudes below `x_deathMag`.

#### Scenario: Atom survives a one-hop dropout
- **WHEN** a tracked partial disappears for one hop and ordinary decay leaves it above the death magnitude
- **AND** no current peak suppresses it
- **THEN** the atom survives and can recover if a later peak is matched within its density window

#### Scenario: No old atoms or no analysis peaks
- **WHEN** no old atoms exist
- **THEN** every eligible analysis peak can start a new atom
- **WHEN** no analysis peaks exist
- **THEN** all old atoms follow ordinary decay without a match or a dominator

#### Scenario: Attack applies to births and upward tracking
- **WHEN** a peak starts a new atom
- **THEN** its initial magnitude follows the attack slew from zero, floored at the death magnitude
- **WHEN** an existing atom matches a stronger peak
- **THEN** its magnitude follows attack from its existing magnitude without restarting its phase or envelope

### Requirement: Frequency-Dependent Parameter Lanes
The system SHALL store frequency-dependent spectral controls as four lanes (`FrequencyDependentParameter::x_numParameters = 4`). Each atom is assigned a parameter index from its frequency at extraction time (`GetIndexForFrequency`), and positive parameter evaluation interpolates geometrically between neighboring lanes. Bipolar pitch and unison SHALL use linear interpolation. The fourth lane SHALL interpolate back to the first lane at the wrap boundary. `PartialMachineLinearFrequency` (range 0.1-100) scales how fast the lane pattern repeats across the frequency axis. When all four lanes hold the same value the parameter behaves as a scalar; when modulation, gestures, or scenes differentiate the lanes, each frequency region sees its own interpolated value for attack, decay, density, bandwidth, panning, unison, and pitch. Internal reduction volume SHALL be the constant 1. Encoder 3,3 (`PartialMachineVolume`) SHALL be the Partial Machine mixer return gain, the same scalar `m_returnGain` path used by delay and reverb.

#### Scenario: Equal lanes behave as a scalar control
- **WHEN** a quad-bank parameter has identical values in all four lanes
- **THEN** every atom receives the same interpolated value regardless of its frequency

#### Scenario: Differentiated lanes split the spectrum
- **WHEN** modulation makes lane values differ
- **THEN** atoms at different frequencies interpolate different parameter values, and increasing `PartialMachineLinearFrequency` makes the lane pattern repeat more often across the spectrum

#### Scenario: Encoder 3,3 is mixer return
- **WHEN** `PartialMachineVolume` is 0
- **THEN** the Partial Machine return is silent in the mix
- **AND** tracked-atom reduction still uses volume 1

### Requirement: Bell-Curve Reduction with Feedback
The system SHALL scale each atom's synthesis magnitude by a reduction factor (`SynthesisContext::GetReduction`): unity between the base frequency (`m_bwBaseFrequency`) and base × width (`m_bwWidth`), falling at -12 dB per octave below the base and -12 dB per octave above base × width. The reduction-feedback parameter (`m_reductionFeedback`) interpolates the atom's stored synthesis magnitude toward the reduced value (floored at the death magnitude 1e-5), so at high feedback repeated reduction progressively re-shapes the tracked atom itself rather than only its output.

#### Scenario: Skirts fall at -12 dB per octave
- **WHEN** the base frequency corresponds to 500 Hz, the width factor is 2, and volume is unity
- **THEN** atoms between 500 Hz and 1000 Hz pass at full magnitude, an atom at 250 Hz is reduced by 12 dB, and an atom at 2000 Hz is reduced by 12 dB

#### Scenario: Feedback erodes attenuated atoms
- **WHEN** reduction feedback is at maximum and an atom sits deep in the reduction skirt
- **THEN** each synthesis frame writes the reduced magnitude back into the atom's synthesis magnitude, so the atom decays toward the death floor and is eventually pruned, while at zero feedback the stored magnitude is unaffected by reduction

### Requirement: Bipolar Pitch Shift
The system SHALL multiply each atom's synthesis frequency by `pitchShiftDepth^pitchShift` (`GetPitchShiftFactor`), where the pitch-shift control is mapped bipolar to [-1, +1] (knob value 2k - 1) and the depth parameter ranges exponentially from 5 cents (2^(5/1200)) to one octave (2.0). Full clockwise shifts every atom up by the depth interval, full counterclockwise shifts down by the same interval, and center applies no shift. Each emitted copy SHALL use frequency `synthesisOmega * detune * pitchShiftRatio` and phase `synthesisPhase * detune * pitchShiftRatio`. The stored phase accumulator SHALL continue to advance by the unshifted `H * synthesisOmega`.

#### Scenario: Octave depth sweeps two octaves of shift
- **WHEN** `PartialMachinePitchShiftDepth` is at maximum (factor 2.0)
- **THEN** the pitch-shift knob at maximum doubles every atom's frequency, at minimum halves it, and at center leaves frequencies unchanged (factor 2^0 = 1)

### Requirement: Bass-Collapse Radius Mapping
The system SHALL map each atom's frequency to a quad radius (`GetRadius`): frequencies below the bass cutoff get radius 0 (center), frequencies at or above eight times the cutoff get radius 1 (outer field), and frequencies in between ramp as log2(frequency / cutoff) / 3.

#### Scenario: Low partials collapse to center
- **WHEN** the bass cutoff corresponds to 100 Hz
- **THEN** a 50 Hz atom is placed at radius 0, an 800 Hz atom at radius 1, and an approximately 282.84 Hz atom at radius 0.5

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
The system SHALL expand each atom into a center copy plus two symmetric detuned pairs (`UnisonContext::x_numVoices = 5`). Pair p (1-2) uses interpolation `interp = min(1, (3 - p) × unison)`; each side voice gets gain `ZeroedExpParam::Compute(4, interp)`, detune `1.06^(±interp)`, and azimuth offset `±interp / 2^(4-p)`. All five gains SHALL then be RMS-normalized so their squared gains sum to one. Correlation between overlapping copies can still change the resulting loudness.

#### Scenario: Unison at zero leaves one copy
- **WHEN** the unison parameter is 0
- **THEN** every pair's interpolation is 0, side gains are 0, and the atom synthesizes as a single copy at its own frequency and azimuth with gain 1 after normalization

#### Scenario: Raising unison staggers the pairs
- **WHEN** the unison parameter is 0.5
- **THEN** pair 1 reaches full interpolation (min(1, 2 × 0.5) = 1) with detunes 1.06 and 1/1.06 and azimuth offsets ±1/8, pair 2 is at interpolation 0.5, and the active copies' gains are scaled by the common RMS factor so the squared gains sum to one

### Requirement: Quad Panning and Overlap-Add Synthesis
The system SHALL place each unison copy in the quad field by converting azimuth and radius through an unnormalized `TanhSaturator<false>` (`SetInputGain(2 * radius)`, then `x = 0.5 + 0.5 × saturator.Process(cos2pi(azimuth))`, `y = 0.5 + 0.5 × saturator.Process(sin2pi(azimuth))`) and panning with `QuadFloat::Pan(x, y, 1.0)` (`SynthesisContext::GetPanCoordinates` / `SynthesisContext::Pan`, `private/src/QuadUtils.hpp`). Every copy is written into a 4-channel `QuadDFT` as a windowed partial with its magnitude, phase, frequency, and quad distribution; each atom's synthesis phase then advances by `H × synthesisOmega`. `QuadOLA` (`private/src/OLA.hpp`) inverse-transforms and overlap-adds the frames at 75% overlap into the continuous quad return, which is produced every sample. `QuadComponentFrequencyResponse` SHALL use this same `Pan` mapping so the per-speaker overlay matches DSP placement.

#### Scenario: Radius zero reaches the center
- **WHEN** an atom's radius is 0
- **THEN** its pan coordinates are (0.5, 0.5) and `QuadFloat::Pan` distributes it equally toward all four speakers, while radius 1 at azimuth 0 sits on a tanh-shaped circle at `x = 0.5 + 0.5 × TanhSaturator<false>(2).Process(1)` rather than the field edge

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
- **THEN** the returned per-speaker gains are equal across all four speakers (radius 0), and above eight times the cutoff they differ according to the azimuth mapping, matching where the DSP actually pans atoms of that frequency

### Requirement: Residual Spectral Analysis
The system SHALL compute an analysis residual on each Partial Machine analysis hop after extracting input atoms. Each `AnalysisAtom` SHALL store the frame-start phase of its fitted complex Hann coefficient and an analysis magnitude equal to half that coefficient's magnitude (A/4 for an isolated cosine of amplitude A). Extraction SHALL write each fitted atom back into the same input DFT at `analysisPhase + 0.5` and magnitude `2 * analysisMagnitude`, using the same estimated frequency, so its modeled component cancels without a second analysis pass. The remaining DFT magnitudes SHALL be copied into `SpectralModel::ResidualModel::Input::m_analysisResidualMagnitudes` with one residual bucket per DFT component, so residual envelope bucket `k` equals the remaining DFT magnitude at component `k`.

#### Scenario: Pure tracked partial leaves low residual
- **WHEN** the analysis frame contains a single sinusoid that is extracted as an atom
- **THEN** residual analysis stores that atom's analysis phase and writes the atom back into the DFT at opposite phase
- **AND** the residual bucket magnitudes near the atom frequency are lower than the original DFT magnitudes at those bucket frequencies

#### Scenario: Broadband energy remains after atom cancellation
- **WHEN** the analysis frame contains a sinusoid plus broadband noise
- **THEN** residual analysis cancels the extracted sinusoid from the DFT
- **AND** residual bucket magnitudes still report the remaining broadband energy component-by-component

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
The Partial Machine SHALL contain a `ResidualMachine` that adds residual energy into the same `QuadDFT` frame as tracked partial atoms. For each target quad DFT bucket, the residual machine SHALL read the residual envelope at the same DFT bucket index, compute frequency-dependent reduction and quad pan placement from that bucket frequency, compute per-channel magnitude as `residualEnvelope * reduction * pan[channel]`, choose a random phase for the synthesis frame, create a complex value with that magnitude and phase, and write it with `WriteBinCenteredWindowedPartial`. That write SHALL add the on-bin Hann kernel `0.5` at bin `k` and `-0.25` at `k-1` and `k+1`, omit DC, omit a missing upper neighbor at the last stored bin, and leave true DC writes as a no-op. The residual machine SHALL also apply the reduction-feedback parameter to the residual bucket's stored magnitude, writing the feedback-shaped reduced magnitude back into the residual model with the same floor policy used by tracked atom reduction feedback.

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
- **THEN** residual synthesis adds its Hann-windowed residual energy to the existing component value
- **AND** it does not clear or replace the existing partial energy

#### Scenario: Residual synthesis uses a bin-centered Hann kernel
- **WHEN** residual synthesis writes bucket `k` with complex value `v`
- **THEN** it adds `0.5 * v` to component `k` and `-0.25 * v` to the neighboring stored bins
- **AND** it does not write DC
- **AND** it omits a neighbor tap that would fall outside the stored DFT bins
