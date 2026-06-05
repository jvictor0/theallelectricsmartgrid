# Partial Machine

The **Partial Machine** (`PartialMachine` in `private/src/PartialMachine.hpp`) is a global send effect that analyzes the incoming send bus as mono, tracks spectral partials, and resynthesizes them into the quadraphonic field.

Unlike the source machines, this is not selected per voice. It is the third global send/return processor beside the Quad Delay and Quad Reverb.

## Routing

`QuadMixerInternal` now has three send buses:

- Send 0: Quad Delay.
- Send 1: Quad Reverb.
- Send 2: Partial Machine.

Voices can feed the Partial Machine through `PartialMachineSend` in the Filter and Amp bank. The Delay and Reverb returns can also feed it through `DelayPartialMachineSend` and `ReverbPartialMachineSend`.

The Partial Machine return is mixed into the main quad output, and it can feed the other send effects through `PartialMachineDelaySend` and `PartialMachineReverbSend`. Its return level is currently fixed at unity in the DSP path.

## Analysis and Resynthesis

The processor receives a quad sample, sums it to mono, and writes that mono stream into a spectral analysis buffer. Every hop, the buffer is Hann-windowed and passed to `SpectralModelGeneric<12, FrequencyDependentParameter>` to extract and track prominent spectral atoms.

Each tracked atom carries:

- an analysis frequency and magnitude,
- a synthesis frequency and magnitude,
- a synthesis phase,
- a frequency-dependent parameter index,
- a synthetic/organic flag for future synthetic-harmonic support.

During synthesis, each atom is reduced, pitch-shifted, optionally expanded into unison copies, panned into quad, and written to a `QuadDFT`. `QuadOLA` overlap-adds the resulting frames back into a continuous quad signal.

## Frequency-Dependent Parameters

Partial Machine parameters are stored as four parameter lanes using `FrequencyDependentParameter`. A spectral atom asks for the parameter index associated with its frequency, then interpolates between neighboring lanes.

By default, a Quad bank parameter has the same base value in all four lanes, so the parameter behaves like a normal scalar control. If modulation, gestures, or scene state make the four lanes differ, each frequency maps to a different interpolated value. This turns the Partial Machine into a frequency-dependent processor where lows, mids, highs, and moving in-between regions can evolve with unique attack, decay, density, bandwidth, panning, unison, pitch, and level behavior.

`PartialMachineLinearFrequency` controls how the frequency axis is folded across the four lanes. Increasing it makes the parameter pattern repeat more quickly across frequency, creating denser spectral variation.

## Parameter Groups

The Partial Machine bank exposes four rows of quad-bank controls:

- **Tracking**: `PartialMachineAttack`, `PartialMachineDecay`, `PartialMachineDensity`, and `PartialMachinePortamento` control spectral atom slew, pruning density, and frequency glide.
- **Bandwidth and feedback**: `PartialMachineBWBase`, `PartialMachineBWWidth`, and `PartialMachineReductionFeedback` shape how strongly each atom survives the reduction curve.
- **Spatialization and sends**: `PartialMachineBassCutoff`, `PartialMachineAzimuthFactor`, `PartialMachineUnison`, `PartialMachineDelaySend`, and `PartialMachineReverbSend` place atoms in the quad field and route the return into the other effects.
- **Frequency mapping, pitch, and output**: `PartialMachineLinearFrequency`, `PartialMachinePitchShiftDepth`, `PartialMachinePitchShift`, and `PartialMachineVolume` control frequency-dependent indexing, pitch offset, and final per-atom level.

## Spatial Model

`PartialMachineBassCutoff` controls radius. Frequencies below the cutoff collapse toward the center. Frequencies above twice the cutoff can reach the outer quad field, with a logarithmic transition between those points.

`PartialMachineAzimuthFactor` maps frequency to azimuth. With frequency-dependent modulation, different regions of the spectrum can orbit, widen, or cluster independently.

`PartialMachineUnison` creates up to seven detuned and azimuth-offset copies per atom. The copies are RMS-normalized so adding unison changes spread and density more than raw loudness.

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
