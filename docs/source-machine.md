# Source Machine

The source machine is the first stage in each `SquiggleBoyVoice` and is responsible for generating (or forwarding) the raw signal before filtering and amplitude shaping.

## Source Types

Each voice currently supports four source machine modes:

1. **Dual Wave Shaping VCO**: dual wavetable oscillator source with vector phase shaping and cross-modulation. Implemented in `DualWaveShapingVCO.hpp`.
2. **Physical Modeling**: noise-excited source with sample-rate reduction, morphable SVF, AHD modulation, and a one-pole damping comb. Implemented in `PhysicalModelingSource.hpp`.
3. **Thru**: external-input passthrough source.
4. **Dual Sample**: dual sample-bank source that crossfades two independently selected directories of WAV files and resynthesizes them with the grain engine. Implemented in `DualSampleSource.hpp`.

## Dual Wave Shaping VCO Source Machine

The primary sound source is the **Dual Wave Shaping VCO** (`DualWaveShapingVCO`), which consists of two complex wavetable oscillators (`VectorPhaseShaperInternal`) that can modulate each other.

### Random Morphing Wavetables

The core of each oscillator is a randomly generated, constantly morphing additive wavetable (`MorphingWaveTable`).

- There are two wavetables active at any time (Left and Right).
- A random LFO lazily crossfades between them.
- When the crossfade reaches 100% to one side (making the other table completely inaudible), the inaudible wavetable is discarded and replaced with a newly generated random wavetable. This ensures continuous, infinite timbral evolution.
- The harmonic complexity of the generated wavetables is controlled by a **Level of Detail (LOD)** parameter (`m_morphHarmonics`), which determines how many harmonics are included.

### Vector Phase Shaping

The oscillators use **Vector Phase Shaping (VPS)** to distort the read phase of the wavetable. This is controlled by two parameters per oscillator:

- **v (Vertical)**: controls the vertical inflection point of the phase distortion.
- **d (Horizontal)**: controls the horizontal inflection point (duty cycle/symmetry) of the phase distortion.

### Dual Oscillator Interaction

The Dual Wave Shaping VCO contains two such oscillators (VCO 0 and VCO 1):

- **Pitch**: VCO 0 runs at the base frequency (modified by `detune`). VCO 1 runs at `baseFreq * offsetFreqFactor / detune`.
- **Mutual phase modulation**: each oscillator can phase-modulate the other. The amount of modulation is controlled by `m_crossModIndex`.
- **Mix**: the outputs of VCO 0 and VCO 1 are mixed together using an equal-power crossfade (`fade`).
- **Bitcrusher**: the mixed output is finally passed through a bit-rate reducer (`BitRateReducer`) for digital degradation.

## Thru Source Machine

The **Thru** source machine allows external audio to be routed through the voice's filter and amp sections. It is currently a simple pass-through for external signals, allowing the synthesizer to act as a polyphonic filter bank and envelope shaper for outside audio.

## Physical Modeling Source Machine

The **Physical Modeling** source machine (`PhysicalModelingSource`) is a noise-driven resonant source built around a damped comb. The chain is:

1. White noise excitation.
2. Sample-rate reduction for texture control.
3. Morphable 2-pole SVF pre-filter (LP/BP/HP blend).
4. Inverted AHD envelope modulation.
5. One-pole damping comb (`CombFilterWithOnePole`) with delay compensation.

This source owns a `UIState` that implements `TransferFunction`, so the visualizer can draw the combined pre-comb SVF and comb response in the Source bank.

## Dual Sample Source Machine

The **Dual Sample** source machine (`DualSampleSource`) reads from two `AudioBufferBank` slots (`slot1` and `slot2`) that are assigned per voice in the Voice Config grid.

- Each slot points at a directory under the Smart Grid One `samples/` root.
- Each slot loads all `.wav` files in that directory into an `AudioBufferBank`.
- Playback is driven by Theory of Time phase (`m_theoryOfTime`) and rendered through the same grain/resynthesis path used by the phase-vocoder components.
- Slot 1 and slot 2 are mixed with an equal-power crossfade (`m_mix`).

Directory navigation and loading are asynchronous:

- UI commands are sent through `IoTaskThread`.
- `DirectoryExplorer` handles folder traversal and selection.
- Confirming a selection (`Yes`) builds a new `AudioBufferBank` and atomically swaps it into the selected slot during acknowledgment.

The JUCE voice config page now displays:

- active trio name and source/filter mode,
- per-voice sample directory labels for both slots,
- a directory explorer view while browsing.

## Parameter Visibility

Parameters are tagged with which source and filter machines they apply to (see [Encoder System](encoder-system.md#machine-specific-parameters)). When a source machine is selected, only matching source-specific controls are connected (e.g. VCO harmonic/phase controls for Dual VCO, comb/damping controls for Physical Modeling). This keeps the interface focused on controls relevant to the active machine.

`DualSampleSource::SetEncoderParams(...)` currently exists as a hook but does not yet map machine-specific encoder controls.

## Related

- [DSP Overview](dsp-overview.md)
- [Ladder Filter Machine](filter-ladder.md)
- [SVF Filter Machine](filter-svf.md)
