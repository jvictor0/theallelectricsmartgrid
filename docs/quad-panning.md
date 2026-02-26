# Quadraphonic Panning

The Smart Grid One synthesizer features a dedicated quadraphonic panning system for its 9 voices, allowing for complex spatial movement and distribution.

## Global Phase and Voice Offsets

The panning system is driven by a single, global phase oscillator (`m_panPhase`). Instead of each voice having its own independent panning LFO, all voices share this global phase.

To ensure the voices are distributed spatially rather than stacked on top of each other, each voice receives a unique phase offset:
- **Static Offset**: A fixed offset calculated based on the voice's index and track (`staticOffset`). This guarantees that even with no user modulation, the 9 voices are spread out evenly across the panning cycle.
- **User Phase Shift**: An additional, modulatable offset controlled by the `PanPhaseShift` parameter.

## Lissajous LFO

The actual X and Y coordinates for each voice's position in the quadraphonic field are computed by a Lissajous LFO (`LissajousLFOInternal`).

The Lissajous LFO takes the offset global phase and generates 2D coordinates using the following parameters per voice:
- **Mult X / Mult Y**: Multipliers for the X and Y axes, allowing the creation of complex Lissajous figures (e.g., figure-eights, circles, or chaotic tangles).
- **Center X / Center Y**: The origin point of the Lissajous figure within the quadraphonic field.
- **Radius**: The size of the Lissajous figure.

The resulting X and Y coordinates (normalized to `[0, 1]`) are then used by the quadraphonic mixer to distribute the voice's audio signal across the four output channels.

## Related
- [DSP Overview](dsp-overview.md)
