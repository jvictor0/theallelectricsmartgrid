# Source Machine

The source machine is the first stage in each `SquiggleBoyVoice` and is responsible for generating (or forwarding) the raw signal before filtering and amplitude shaping.

## Source Types

Each voice currently supports four source machine modes:

1. **Dual Wave Shaping VCO**: dual wavetable oscillator source with vector phase shaping and cross-modulation. Implemented in `DualWaveShapingVCO.hpp`.
2. **Physical Modeling**: noise-excited source with sample-rate reduction, morphable SVF, AHD modulation, and a one-pole damping comb. Implemented in `PhysicalModelingSource.hpp`.
3. **Thru**: external-input passthrough source.
4. **Sample**: per-voice sample-directory source that reads a bank of WAV files, blends between adjacent files, and resynthesizes playback with the grain engine. Implemented by `SampleSource` in `DualSampleSource.hpp`.

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

## Sample Source Machine

The **Sample** source machine (`SampleSource`) reads from one `AudioBufferBank` per voice. The bank is assigned in the Voice Config grid and points at a directory under the Smart Grid One `samples/` root.

- Each bank loads all `.wav` files in the selected directory.
- `SampleBankPosition` scans across the loaded WAV files. Even segments select a single file; odd segments linearly blend adjacent files.
- Playback position comes from `PhasorPlayHead`, which reads a selected Theory of Time loop through `GetIndirectPhasor(...)`.
- `SampleStart` and `SampleLength` define a wrapped window inside the normalized sample domain.
- `SampleReadSpeed` selects quantized reverse, stopped, and forward rates from `-4x` through `4x`.
- Audio is rendered through `GrainManager<AudioBufferBank>`, reusing the grain/resynthesis path used by the phase-vocoder components.

Directory navigation and loading are asynchronous:

- UI commands are sent through `IoTaskThread`.
- `DirectoryExplorer` handles folder traversal and selection.
- Confirming a selection (`Yes`) builds a new `AudioBufferBank` and swaps it into the selected voice during acknowledgment.
- Restoring saved state queues `LoadAudioBufferBankFromDirectory` tasks for each saved relative sample directory.

The JUCE voice config page now displays:

- active trio name and source/filter mode,
- one per-voice sample directory label for the active trio,
- a directory explorer view while browsing.

The Source visualizer bank includes `SampleTrioWaveformVisualizerComponent` for Sample voices. It draws min/max waveform buckets for the active trio's three sample banks, highlights the active start/length window in each voice color, and overlays the current read-head position.

## Parameter Visibility

Parameters are tagged with which source and filter machines they apply to (see [Encoder System](encoder-system.md#machine-specific-parameters)). When a source machine is selected, only matching source-specific controls are connected (e.g. VCO harmonic/phase controls for Dual VCO, comb/damping controls for Physical Modeling). This keeps the interface focused on controls relevant to the active machine.

Sample-specific Source bank controls are only visible for Sample voices:

- `SampleLoopIndex`: chooses which of the six Theory of Time loops drives the read head.
- `SampleStart`: normalized start position of the playback window.
- `SampleReadSpeed`: quantized read speed, including reverse and stopped playback.
- `SampleLength`: normalized wrapped length of the playback window.
- `SampleBankPosition`: position across the WAV files loaded in the selected bank.

## Related

- [DSP Overview](dsp-overview.md)
- [Ladder Filter Machine](filter-ladder.md)
- [SVF Filter Machine](filter-svf.md)
