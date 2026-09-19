# Partial Machine

The **Partial Machine** (`PartialMachine` in `private/src/PartialMachine.hpp`) is a global send effect that analyzes the incoming send bus as mono, tracks spectral partials, and resynthesizes them into the quadraphonic field.

Unlike the source machines, this is not selected per voice. It is the third global send/return processor beside the Quad Delay and Quad Reverb.

## Routing

`QuadMixerInternal` now has three send buses:

- Send 0: Quad Delay.
- Send 1: Quad Reverb.
- Send 2: Partial Machine.

Voices can feed the Partial Machine through `PartialMachineSend` in the Filter and Amp bank. The Delay and Reverb returns can also feed it through `DelayPartialMachineSend` and `ReverbPartialMachineSend`.

The Partial Machine return is mixed into the main quad output, and it can feed the other send effects through `PartialMachineDelaySend` and `PartialMachineReverbSend`. Encoder 3,3 (`PartialMachineVolume`) is the mixer return gain, the same scalar path as Delay Return and Reverb Return. Internal reduction volume is held at unity.

## Analysis and Resynthesis

The processor sums each quad sample to mono and writes it into a 4096-sample analysis buffer. Every 1024 samples, it Hann-windows the buffer and calls `SpectralModelGeneric<12, FrequencyDependentParameter>::ExtractAtomsAndResidual`. Peak frequencies are interpolated between bins; magnitude and frame-start phase are fitted against the same Hann kernel used to write partials. Each fitted partial is subtracted immediately from the analysis spectrum. The remaining bin magnitudes feed the smoothed residual model.

Each tracked atom carries:

- an analysis frequency, cached log2 frequency, magnitude, and frame-start phase,
- a synthesis frequency and magnitude,
- a synthesis phase,
- a frequency-dependent parameter index.

During synthesis, each atom is reduced, pitch-shifted, optionally expanded into unison copies, panned into quad, and written to a `QuadDFT`. The residual model adds smoothed broadband energy with randomized phase to the same frame. `QuadOLA` overlap-adds the frames into a continuous quad signal. Tracked atoms come from the input analysis; synthetic harmonic atoms and their separate mix gains have been removed.

## Tracking and Density

`PartialMachineDensity` controls a frequency window measured in octaves. Its range is exponential from one cent to one octave. Counterclockwise gives a broad window, allowing a strong new partial to replace nearby older partials even when their decay is long. Clockwise gives a narrow window, allowing old and new partials to coexist more readily.

Density does not enforce minimum spacing between partials or choose how many partials the sound may contain. A separate 256-partial budget keeps the strongest analysis peaks before matching and the strongest synthesis magnitudes after each update. This bounds matching to at most 256 old atoms by 256 incoming peaks during normal processing. Extraction still examines the full spectrum; discarded analysis peaks have already been subtracted and are not restored to the residual.

`AtomMatcher` sorts existing atoms by their last analysis frequency and sorts new peaks by frequency. It chooses a one-to-one assignment that preserves that order and maximizes the total score:

`theta = analysisMagnitude * max(0, 1 - octaveDistance / density)`

The distance is measured from the old atom's last analysis frequency, before portamento, detune, or pitch shift. Matching uses density at the old atom's stored parameter index and excludes peaks below 0.001 times its synthesis magnitude. Equal total scores prefer more matches. Assignment is completed before any tracked frequency or magnitude changes, so synthesis-magnitude order cannot give the first atom a greedy claim on a neighbor's continuation. The matcher uses Hirschberg reconstruction with quadratic time in the two atom counts and linear member-owned workspace.

A matched atom follows its selected peak with the attack/decay and portamento controls. An unmatched old atom continues decaying; a strong nearby analysis peak can also suppress it, whether that peak matched another atom or will become a new one. Suppression is applied once per hop before ordinary decay: if its weighted score `theta` exceeds the old magnitude `a`, the magnitude becomes `a * a / theta`. This can remove a quiet tail quickly even with a long decay setting.

Selection and suppression both evaluate density at the old atom's stored parameter index, so differing lane values do not change the cone between those two steps.

Every unclaimed analysis peak at or above the death magnitude, `1e-5`, starts a new atom. Its initial synthesis magnitude is the attack slew from zero, floored at `1e-5`. Attack governs new-atom fade-in and upward tracking; it does not delay suppression by raw analysis peaks. No input peaks means ordinary decay of the existing pad. No existing atoms means all eligible peaks can start new atoms.

Pitch shift and unison scale both the emitted frequency and emitted phase. The stored synthesis phase keeps its original unshifted advance of `1024 * synthesisOmega` per hop; each emitted copy uses `synthesisPhase * detune * pitchShiftRatio`.

## Frequency-Dependent Parameters

The spectral controls use four parameter lanes through `FrequencyDependentParameter`. A spectral atom asks for the parameter index associated with its analysis frequency, then interpolates between neighboring lanes. Positive parameters interpolate geometrically; bipolar pitch and unison use linear interpolation. The lane pattern wraps from the fourth lane back to the first. Return gain and cross-effect sends are mixer controls.

By default, a Quad bank parameter has the same base value in all four lanes, so the parameter behaves like a normal scalar control. If modulation, gestures, or scene state make the four lanes differ, each frequency maps to a different interpolated value. This turns the Partial Machine into a frequency-dependent processor where lows, mids, highs, and moving in-between regions can evolve with unique attack, decay, density, bandwidth, panning, unison, pitch, and level behavior.

`PartialMachineLinearFrequency` controls how the frequency axis is folded across the four lanes. Increasing it makes the parameter pattern repeat more quickly across frequency, creating denser spectral variation.

## Parameter Groups

The Partial Machine bank exposes four rows of quad-bank controls:

- **Tracking**: `PartialMachineAttack`, `PartialMachineDecay`, `PartialMachineDensity`, and `PartialMachinePortamento` control magnitude slew, replacement versus coexistence, and frequency glide.
- **Bandwidth and feedback**: `PartialMachineBWBase`, `PartialMachineBWWidth`, and `PartialMachineReductionFeedback` shape how strongly each atom survives the reduction curve.
- **Spatialization and sends**: `PartialMachineBassCutoff`, `PartialMachineAzimuthFactor`, `PartialMachineUnison`, `PartialMachineDelaySend`, and `PartialMachineReverbSend` place atoms in the quad field and route the return into the other effects.
- **Frequency mapping, pitch, and output**: `PartialMachineLinearFrequency`, `PartialMachinePitchShiftDepth`, and `PartialMachinePitchShift` control frequency-dependent indexing and pitch offset. `PartialMachineVolume` is the mixer return gain, not a frequency-dependent atom level.

## Spatial Model

`PartialMachineBassCutoff` controls radius. Frequencies below the cutoff collapse toward the center. Radius rises as `log2(frequency / cutoff) / 3` and reaches one at eight times the cutoff. Pan coordinates use a tanh-shaped orbit with input gain `2 * radius`.

`PartialMachineAzimuthFactor` maps frequency to azimuth. With frequency-dependent modulation, different regions of the spectrum can orbit, widen, or cluster independently.

`PartialMachineUnison` creates up to five copies per atom: the center and two detuned, azimuth-offset pairs. Their gains are RMS-normalized; overlapping copies can still reinforce or cancel, so this is not a loudness guarantee.

## Visualizers

The Partial Machine bank adds three visualizers:

- `PartialMachineAnalyzer`: quad analyzer for the Partial Machine return, including the per-speaker frequency response overlay.
- `PartialMachineInputSpectrum`: mono input spectrum with tracked atoms drawn over the incoming Partial Machine send.
- `PartialMachineSpatial`: spatial atom view showing each tracked atom's quad position and reduced magnitude.

The shared quad analyzer also includes a Partial Machine scope, and effect-bank analyzer overlays can show Delay, Reverb, and Partial Machine responses together.

## Related

- [DSP Overview](dsp-overview.md)
- [Mixdown and Mastering](mixdown-mastering.md)
- [Encoder System](encoder-system.md)
- [Quad Delay](quad-delay.md)
- [Quad Reverb](quad-reverb.md)
- [Smart Grid Visualizers](smart-grid-visualizers.md)
