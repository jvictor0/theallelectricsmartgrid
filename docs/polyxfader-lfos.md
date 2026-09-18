# PolyXFader LFOs

The primary Low-Frequency Oscillators (LFOs) in the Smart Grid One are implemented using the `PolyXFaderInternal` struct (`private/src/PolyXFader.hpp`). Despite the somewhat unconventional name, this struct evaluates and mixes periodic waveforms.

## Phase-Synchronized Mixing

PolyXFader reads absolute loop phase through `GetPhase`, with an explicit `PhaseDomain`. The Theory of Time's own phase-modulation LFO uses unmodulated phase; voice LFOs use modulated phase. It has no independent phase accumulator or reset input.

Waveform evaluation first reduces `phase + phaseShift + 0.75` to a fractional cycle in double precision, then applies the shaping multiplier. A multiplier of 2.5 produces two full lobes and a final shorter lobe with half amplitude in each input cycle. This periodic shaping is preserved even at large absolute phase values.

Running topology edits require simultaneous old/new parent modulated boundaries. Integer phase offsets at an exact shared boundary therefore evaluate to the same periodic waveform. Interpolation uses global phase and the interval's topology, avoiding a sweep through the absolute integer jump. Sampled boundary overshoot still reflects the new rate, and unmodulated LFO drive can differ from the modulated boundary used to accept an edit.

Blend weights, sample-and-hold and output slew are shared across LFO uses. Voice and quad LFOs retain the default Shape/Skew behavior below; the Theory of Time explicitly selects a continuous Shape mode for time warping.

## Features

The PolyXFader LFOs support a variety of shaping and modulation options:

- **Skew (Attack Fraction)**: Controls the symmetry of the waveform, similar to a pulse-width or ramp-skew control (`m_attackFrac`).
- **Phase Shift**: Offsets the starting phase of the LFO (`m_phaseShift`).
- **Shape (default mode)**: Morphs the waveform shape (`m_shape`). Below `0.45`, the shape crossfades from a raised-cosine curve toward a linear ramp. Between `0.45` and `0.55` the output is the plain ramp. Above `0.55`, the output is quantized to `numBits = round(16 × (1 − shape))` steps, with hysteresis (a new quantized value is held until the quantized level actually changes) to avoid jitter.
- **Multiplier**: Scales the frequency of the LFO relative to the input phasors (`m_mult`).
- **Sample and Hold (S+H) Mix**: The LFO output can be blended with a Sample and Hold (S+H) value. The S+H value is captured whenever `m_trig` is asserted, and the mix between the continuous LFO and the stepped S+H is controlled by the `m_shFade` parameter. Voice LFOs trigger from the voice AHD gate; quad LFOs trigger from each channel's active delay-loop top.

## Usage

These LFOs are used extensively throughout the system:
- **Phase-Modulation LFO**: The Theory of Time uses a dedicated PolyXFader to modulate the global clock phase. It enables `m_continuousShape`: the whole Shape range blends sine (zero) to triangle (one), bypassing quantization. Its normalized Skew knob maps to attack fractions 0.1–0.9 on an evaluation copy; the stored knob and its slow control filter remain normalized. Time reversals and fractional-Mult lobes are preserved.
- **Voice LFOs**: Each voice (`SquiggleBoyVoice`) has two dedicated `SquiggleLFO` instances (which wrap `PolyXFaderInternal`) for per-voice modulation. Their phase-shift knobs spread the nine voices by thirds (`voice % 3 / 3`).
- **Quad LFOs**: `SquiggleBoy` runs the same two PolyXFader shapes across four quad channels on the `QuadLFOs` encoder bank. Phase-shift knobs spread those channels by quarters (`channel / 4`), and each channel's Sample-and-Hold captures when that channel's active delay loop crosses its modulated cycle boundary. Outputs feed Quad modulator slots 6 and 7.

## Related
- [Theory of Time](theory-of-time.md)
- [DSP Overview](dsp-overview.md)
