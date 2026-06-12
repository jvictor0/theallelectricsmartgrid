# Phase Vocoder Specification

## Purpose
The phase vocoder (`Resynthesizer` in `private/src/Resynthesis.hpp`, driven by `GrainManager`/`QuadGrainManager` in `private/src/DelayLine.hpp`) is a grain-based spectral resynthesis engine that decouples perceived pitch from playback rate. The quad-delay reads its delay lines through a warped-time read head; without correction, variable read speed would shift pitch. The phase vocoder analyzes overlapping Hann-windowed grains, measures per-bin instantaneous frequency from inter-frame phase deviation, builds PVDR leader/follower phase relationships (after the "phase vocoder done right" principle) so partial groups stay coherent, and synthesizes each grain through three unison oscillators with optional rational pitch shift and per-partial magnitude slew. This capability also covers the Deep Vocoder (`private/src/DeepVocoder.hpp`), a spectral quantizer integrated into `SquiggleBoy` that snaps sequencer note pitches to prominent spectral atoms of an external audio input and gates triggers when no atom qualifies.

## Requirements

### Requirement: Grain Launch and Dual-Frame Analysis
The system SHALL launch a new analysis/synthesis grain every hop of H = N/4 samples, where N = 4096 is the grain size (`Resynthesizer::x_bits = 12`, `x_hopDenom = 4`, so H = 1024 samples, ~21.3 ms at 48 kHz). Each launch captures two Hann-windowed frames from the delay-line buffer: the current frame at the warped-time read position and the previous frame H samples earlier (`GrainManager::Grain::Start`). The previous frame primes `m_prevAnalysisPhase`/`m_prevAnalysisMagnitudes`; the current frame is DFT-transformed for analysis. Completed synthesis grains are Hann-windowed (`Resynthesizer::Grain::Start`) and summed sample-by-sample with all other running grains, giving 75% overlap-add reconstruction.

#### Scenario: Grains overlap at 75%
- **WHEN** the quad delay runs steadily at 48 kHz
- **THEN** `GrainManager::Process` launches one grain every 1024 samples (`Resynthesizer::GetGrainLaunchSamples()`)
- **AND** at any instant approximately four 4096-sample grains are running and their Hann-windowed outputs sum into the delay output

#### Scenario: Analysis frames are separated by exactly one hop
- **WHEN** a grain launches at warped read position `startTime`
- **THEN** the current analysis frame reads samples `startTime .. startTime + 4095` and the previous frame reads `startTime - 1024 .. startTime + 3071`, both multiplied by `Math4096::Hann`

### Requirement: Instantaneous Frequency from Phase Deviation
The system SHALL compute a per-bin instantaneous frequency `m_omegaInstantaneous[bin]` (cycles per sample) by subtracting the expected phase advance `omegaBin * H` from the measured inter-frame phase difference, wrapping the residual to the principal argument in (-0.5, 0.5], and adding the correction back: `omega = omegaBin + PrincArg(deltaPhi - omegaBin * H) / H` (`Resynthesizer::SetAnalysisOmega`). This is what keeps pitch stable when the read head moves at non-unity speed.

#### Scenario: Bin-centered sinusoid measures its own frequency
- **WHEN** the analyzed signal is a stationary sinusoid exactly at bin k's center frequency k/4096
- **THEN** the measured phase advance equals the expected advance, the principal-argument residual is 0, and `m_omegaInstantaneous[k]` equals k/4096

#### Scenario: Off-center energy is corrected
- **WHEN** a sinusoid lies between bins so that bin k's phase advances more per hop than `OmegaBin(k) * H` predicts
- **THEN** `m_omegaInstantaneous[k]` is raised above k/4096 by the wrapped phase residual divided by H, so synthesis reproduces the true frequency rather than the bin center

### Requirement: PVDR Leader/Follower Relationship Build
The system SHALL build a PVDR (phase relationship tracker) for each grain (`PVDR::Analyze`). Bins are drained from a magnitude-ordered priority queue so high-energy bins are processed first. A bin popped on its previous-frame magnitude becomes a leader (`m_parent == m_bin`) with phase delta `m_omegaInstantaneous[bin] * H`; its immediate neighbors (bin ± 1) that are not yet computed attach as followers, storing the analysis-phase difference to their parent as `m_delta`. `PushResult` flattens ancestry so every follower references a root leader with an accumulated delta. Bins whose analysis magnitude is below `maxMagnitude * 1e-8` are marked small and given a uniformly random phase delta.

#### Scenario: Strong fundamental leads its neighborhood
- **WHEN** a grain contains a strong component in bin 38 and weaker energy in bins 37 and 39
- **THEN** bin 38 becomes a leader with delta `m_omegaInstantaneous[38] * 1024`, and bins 37 and 39 become followers whose `m_parent` is 38 and whose deltas are their analysis-phase differences from bin 38

#### Scenario: Near-silent bins are randomized
- **WHEN** a bin's analysis magnitude is below 1e-8 of the loudest bin in the grain
- **THEN** its PVDR result is marked `m_small` with a random delta in [-0.5, 0.5), it is excluded from leader status (`PVDR::IsLeader` is false), and shifted synthesis layers skip it

### Requirement: Synthesis Phase Fixup
The system SHALL advance each oscillator's persistent per-bin synthesis phase once per grain from the PVDR results (`Oscillator::FixupPhases`): a leader's phase advances by its delta multiplied by the oscillator's detune factor; a follower's phase is set to its parent's new phase plus its stored delta, and the follower's `m_omegaInstantaneous` is overwritten with the parent's so the group shares one frequency.

#### Scenario: Followers track leader phase across grains
- **WHEN** consecutive grains keep bin 38 as leader of followers 37 and 39
- **THEN** each grain advances `m_synthesisPhase[38]` by `m_omegaInstantaneous[38] * 1024 * detune` and sets `m_synthesisPhase[37]` and `m_synthesisPhase[39]` relative to it, so the three-bin group reconstructs one phase-coherent partial with no drift between bins

### Requirement: Rational Pitch Shift with Dry/Shift Crossfade and Alias Guard
The system SHALL synthesize each oscillator as two shift layers (`Oscillator::x_numShifts = 2`): layer 0 always unshifted (Q(1,1)) and layer 1 at a rational musical ratio selected from a fixed table in `QuadDelayInputSetter` (octave down 1/2; fifth down 2/3; fourth down 3/4; minor third down 3/5; major third down 4/5; major third up 5/4; major sixth up 5/3; fourth up 4/3; fifth up 3/2; octave up 2/1). Layer gains crossfade with a fourth-power curve: `gainDry = 1 - fade^4`, `gainShifted = fade^4`. For a shifted layer, each leader's target bin is `BinOmega(omegaInstantaneous * shift)` with phase `synthesisPhase * shift`; followers keep their bin offset from the parent (`targetParent - parent + bin`) and phase delta. A target bin is skipped when it falls outside (0, 2048) or when `WillAlias` holds, i.e. the shifted parent frequency differs from the target bin's center frequency by more than 0.5/H (~23.4 Hz at 48 kHz).

#### Scenario: Fade knob morphs dry to fifth-up
- **WHEN** the shift switch selects 3/2 and the shift-fade knob is 0.5
- **THEN** the dry layer is synthesized with gain 1 - 0.5^4 = 0.9375 and the shifted layer with gain 0.0625, so the spectrum is still dominated by the unshifted map
- **AND** at fade = 1.0 only the 3/2-shifted layer sounds, with every leader bin remapped to `BinOmega(omega * 1.5)` and harmonic followers preserving their PVDR offsets

#### Scenario: Aliasing shifted bins are rejected
- **WHEN** a leader's shifted frequency `omegaInstantaneous * shift` lands more than 0.5/1024 cycles-per-sample away from its target bin's center frequency, or the target bin index falls outside the spectrum
- **THEN** that bin contributes nothing to the shifted layer instead of writing an incoherent component

### Requirement: Three-Oscillator Unison Synthesis
The system SHALL synthesize each grain through exactly three oscillators (`x_numOscillators = 3`) with detune factors 1.0, `m_unisonDetune`, and `1/m_unisonDetune`, each holding independent synthesis phases. Gains are energy-normalized from the unison gain u: the center oscillator gets `sqrt(1 - 2u²/3)` and each side oscillator gets `u/sqrt(3)` (`Input::GetUnisonGainForOscillator`), keeping total energy constant as spread increases.

#### Scenario: Unison off leaves only the center voice
- **WHEN** `m_unisonGain` = 0
- **THEN** the center oscillator's gain is 1.0 and both detuned oscillators' gains are 0, so the grain is synthesized by the center voice alone

#### Scenario: Full unison weights all three voices equally
- **WHEN** `m_unisonGain` = 1
- **THEN** center gain is sqrt(1/3) ≈ 0.577 and each side gain is 1/sqrt(3) ≈ 0.577, so the three detuned copies carry equal energy and the summed energy matches the unison-off case

### Requirement: Per-Partial Magnitude Slew-Up
The system SHALL slew each bin's synthesis magnitude toward its analysis magnitude through a per-bin `SlewUp` filter (`m_synthesisMagnitudes`); synthesis reads the slewed `m_output`, not the raw analysis magnitude, so newly appearing partials bloom gradually rather than stepping in. The slew-up rate is set from `Input::m_slewUp` at every grain start (`SetSlewUp` in `Resynthesizer::StartGrain`), driven by the quad-delay's slew knob with a fastest rate of 0.25 cycles per hop.

#### Scenario: New partial blooms over several grains
- **WHEN** a partial absent from previous grains appears at full analysis magnitude and the slew-up knob is at its slowest setting
- **THEN** the partial's synthesized magnitude starts near zero and rises toward the analysis magnitude over multiple consecutive grains instead of appearing at full level in one grain

### Requirement: Per-Channel Quad Grain Processing with Sample Offsets
The system SHALL run four independent `GrainManager` instances — one per quad channel, each with its own `Resynthesizer` state — inside `QuadGrainManager` (`private/src/DelayLine.hpp`). Each channel's grain capture position is the channel's warped read-head position plus a per-channel sample offset; the quad-delay derives these offsets from a quad LFO scaled by 1000 samples times the modulation depth (`QuadDelay.hpp`), decorrelating the four channels for quad width and motion. Read-head warping itself is owned by quad-delay.

#### Scenario: Modulated offsets decorrelate channels
- **WHEN** delay modulation depth is nonzero so the four channels receive distinct sample offsets
- **THEN** each channel's grains are analyzed from a different buffer position and resynthesized by that channel's own oscillator phases, producing four decorrelated outputs from the same delay content
- **AND** with modulation depth 0 all four channels analyze at their unmodified read-head positions

### Requirement: Deep Vocoder Atom Tracking and V-Shaped Selection
The system SHALL run the Deep Vocoder (`DeepVocoder::Process`, called per sample from `SquiggleBoy::ProcessSample`) on a rolling 4096-sample buffer of the source-mixer's vocoder send, extracting spectral atoms via `SpectralModel::ExtractAtoms` every 1024-sample hop. When a sequencer voice triggers, `TransformNote` evaluates every current atom against a V-shaped magnitude threshold `gainThreshold * ratio^slope`, where ratio is the larger of (atomFreq/targetFreq) and (targetFreq/atomFreq), slope is `m_slopeUp` above the target and `m_slopeDown` below, and targetFreq is the voice's pitch center times its `m_pitchRatioPre`. Atoms below their threshold are ignored; among the rest, the atom with the highest score (magnitude / threshold) wins and the voice's frequency becomes the winning atom's `m_synthesisOmega` times `m_pitchRatioPost`. Each of the nine voices carries independent `gainThreshold` (range 0.001-0.2), `slopeUp`/`slopeDown` (range 1/16-16), and pitch-ratio parameters.

#### Scenario: Strongest nearby atom wins
- **WHEN** a voice triggers with target frequency 440 Hz, slopeUp = 2, gainThreshold = 0.01, and atoms exist at 440 Hz (magnitude 0.05) and 880 Hz (magnitude 0.05)
- **THEN** the 440 Hz atom's threshold is 0.01 (ratio 1) giving score 5, while the 880 Hz atom's threshold is 0.01 × 2² = 0.04 giving score 1.25
- **AND** the voice's pitch snaps to the 440 Hz atom's exact synthesis frequency

#### Scenario: Asymmetric slopes prefer atoms below the target
- **WHEN** slopeUp = 3 and slopeDown = 1 for a voice targeting 440 Hz
- **THEN** an atom one octave below (ratio 2) needs magnitude above gainThreshold × 2, while an atom one octave above needs magnitude above gainThreshold × 8, biasing selection toward lower partials

### Requirement: Deep Vocoder Trigger Cancellation and Bypass
The system SHALL cancel a voice's trigger (`ahdControl->m_trig = false`) when the Deep Vocoder is enabled and either no atoms exist or no atom meets its threshold, so the voice does not sound. When the global enable flag (`Input::m_enabled`, controlled by the source mixer) is false, `TransformNote` returns the unquantized pitch (`m_pitchCenter * m_pitchRatioPost`) and never cancels triggers. The per-voice outcome is observable through `DeepVocoder::UIState`: `m_usedOmega` holds the winning atom's frequency, or -1.0 when no atom is held, and the current atom set is published as a frequency/magnitude snapshot.

#### Scenario: Silent input gates the sequencer
- **WHEN** the vocoder is enabled and the analyzed input is silent so the spectral model holds no atoms
- **THEN** every triggering voice's `m_trig` is forced false and no note sounds
- **AND** the corresponding `UIState` voice entries report `m_usedOmega` = -1.0 and `m_atomMagnitude` = 0

#### Scenario: Disabling the vocoder restores normal triggering
- **WHEN** `m_enabled` is false
- **THEN** every trigger proceeds with the sequencer's own pitch times `m_pitchRatioPost`, regardless of the atom set, and the voice's tracked atom is cleared
