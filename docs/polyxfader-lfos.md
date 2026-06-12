# PolyXFader LFOs

The primary Low-Frequency Oscillators (LFOs) in the Smart Grid One are implemented using the `PolyXFaderInternal` class (`private/src/PolyXFader.hpp`). Despite the somewhat unconventional name, this class serves as a powerful, phase-synchronized LFO engine.

## Phase-Synchronized Mixing

Unlike traditional free-running LFOs, the PolyXFader LFOs are deeply integrated with the [Theory of Time](theory-of-time.md). They do not generate their own internal phase; instead, they take the independent (unmodulated) phasors from the six time loops as inputs.

The LFO output is created by mixing these six simpler, phase-locked signals together. This ensures that the resulting complex modulation shape is always perfectly synchronized with the global clock and the rhythmic structure of the sequencer.

Synchronization is structural rather than triggered: `PolyXFaderInternal` has no reset or sync input, because its phase is always read directly from the time-loop phasors.

## Features

The PolyXFader LFOs support a variety of shaping and modulation options:

- **Skew (Attack Fraction)**: Controls the symmetry of the waveform, similar to a pulse-width or ramp-skew control (`m_attackFrac`).
- **Phase Shift**: Offsets the starting phase of the LFO (`m_phaseShift`).
- **Shape**: Morphs the waveform shape (`m_shape`). Below `0.45`, the shape crossfades from a raised-cosine curve toward a linear ramp. Between `0.45` and `0.55` the output is the plain ramp. Above `0.55`, the output is quantized to `numBits = round(16 × (1 − shape))` steps, with hysteresis (a new quantized value is held until the quantized level actually changes) to avoid jitter.
- **Multiplier**: Scales the frequency of the LFO relative to the input phasors (`m_mult`).
- **Sample and Hold (S+H) Mix**: The LFO output can be blended with a Sample and Hold (S+H) value. The S+H value is captured whenever the voice receives a trigger (`m_trig`), and the mix between the continuous LFO and the stepped S+H is controlled by the `m_shFade` parameter.

## Usage

These LFOs are used extensively throughout the system:
- **Phase-Modulation LFO**: The Theory of Time uses a dedicated PolyXFader to modulate the global clock phase.
- **Voice LFOs**: Each voice (`SquiggleBoyVoice`) has two dedicated `SquiggleLFO` instances (which wrap `PolyXFaderInternal`) for per-voice modulation.

## Related
- [Theory of Time](theory-of-time.md)
- [DSP Overview](dsp-overview.md)
