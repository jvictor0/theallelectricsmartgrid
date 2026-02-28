# Source Machine

The source machine is the first stage in each `SquiggleBoyVoice` and is responsible for generating (or forwarding) the raw signal before filtering and amplitude shaping.

## Source Types

Each voice currently supports two source machine modes:

1. **Dual Wave Shaping VCO**: dual wavetable oscillator source with vector phase shaping and cross-modulation. Implemented in `DualWaveShapingVCO.hpp`.
2. **Thru**: external-input passthrough source.

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

## Parameter Visibility

Parameters are tagged with which source and filter machines they apply to (see [Encoder System](encoder-system.md#machine-specific-parameters)). When Thru is selected, encoder knobs that only apply to the Dual Wave Shaping VCO (e.g. harmonics, phase modulation) are shown as disconnected. This keeps the interface focused on the relevant controls for the active machine.

## Related

- [DSP Overview](dsp-overview.md)
- [Ladder Filter Machine](filter-ladder.md)
- [SVF Filter Machine](filter-svf.md)
