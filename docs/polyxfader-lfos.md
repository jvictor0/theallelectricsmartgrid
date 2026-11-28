# PolyXFader LFOs

The primary Low-Frequency Oscillators (LFOs) in the Smart Grid One use the generated DSP in `formal/DSP/PolyXFader.lean` through `private/src/PolyXFader.hpp`.

## Phase-Synchronized Mixing

PolyXFader uses a continuous master phase from Theory of Time and multiplies it by each contributing loop’s cycle ratio. Voice and quad LFOs use the same modulated clock as the delay. The Theory of Time phase-modulation LFO reads the previous sample of the unmodulated continuous clock. Both clocks advance at nominal tempo while transport is stopped and rebase on resume. PolyXFader has no independent phase accumulator.

Waveform evaluation first reduces `phase + phaseShift + 0.75` to a fractional cycle in double precision, then applies the effective shaping multiplier. An effective multiplier of 2.5 produces two full lobes and a final shorter lobe with half amplitude in each input cycle. This periodic shaping is preserved even at large absolute phase values.

Running topology edits still require simultaneous old and new parent modulated musical boundaries. A ratio change applies immediately to the continuous master phase. An integer change in the resulting loop phase leaves the waveform unchanged. After a transport rebase, the musical boundary and the continuous oscillator’s seam can occur at different phases.

Components are shaped, weighted and mixed; components with zero blend weight are skipped. There is no waveform quantization. The normalized mix then passes through one `OPLowPassFilter`. The cutoff is `8 × abs(modulated phase increment) × max(loop ratio × effective Mult)` over the loops with nonzero blend weight on each lane. This keeps the smoothing scale tied to logical travel as warp speed changes. Zero travel holds the mix. Reverse travel uses the same positive decay as equal forward travel; it does not undo the filter's history.

The two Quad and six Trio LFOs share a snapshot of the modulated master phase, phase travel, loop ratios and clock-derived multiplier ceilings. Each LFO owns its controls, blend weights, effective multipliers, maximum contributing speeds and filter state. The smallest clock-derived ceiling is a common bound the generated processor can use to tell whether any of its own multipliers need limiting.

Lean's `Input.HasMappedMultipliers` contract puts each requested Mult in the nominal range [1, 16]. The exact exponential encoder map proves that range for normalized input in [0, 1]. The generated processor accepts the native Float32 requests directly and applies the same per-loop ceilings to values affected by rounding.

The warp-producing oscillator uses nominal unmodulated phase travel, avoiding a self-dependent derivative. Stopped LFOs use nominal travel as both continuous clocks free-run. The mix slew caps its cutoff at the maximum internal tempo times the largest grid topology ratio, `2.9296875 Hz × 5⁵ = 9155.2734375 Hz`. Each sample computes its coefficient with a sixth-degree polynomial on the bounded interval, retaining the double filter history. The ideal forward law applies below the cap. Sampled varying inputs still have integration error. [Phase model](../formal/PhaseTime/README.md)

Voice and quad LFO scopes anchor at continuous phase zero and publish a period of `1 / gcd(active cycle ratios)`. The period GCD and axis update run during `PopulateUIState`, before the scope bank publishes, rather than on each engine sample. They do not record start markers or aggregate loop-top offsets. The display reads the corresponding phase in the current or previous cycle through the shared inverse, keeping a full logical-period axis even before one cycle has been recorded.

Voice, quad and tempo LFOs use the same blend, slew and sample-and-hold algorithm. Sample-and-hold captures the smoothed mix, then its held value passes through a 500 Hz `OPLowPassFilter` before blending with the continuous smoothed mix. This S+H filter runs every sample even at zero S+H mix, preserving its history across triggers and blend changes. There is no additional filter after the blend. Voice, quad and tempo LFOs use the Shape curve below. The tempo LFO retains its narrower Skew range.

## Features

The PolyXFader LFOs support a variety of shaping and modulation options:

- **Skew (Attack Fraction)**: Controls the symmetry of the waveform, similar to a pulse-width or ramp-skew control (`m_attackFrac`).
- **Phase Shift**: Offsets the starting phase of the LFO (`m_phaseShift`).
- **Shape**: Crossfades from triangle to raised cosine over 0–0.5, then from raised cosine to the fixed gain-8 saturated cosine over 0.5–1. The cosine and saturated cosine use a shared interpolated table. Fractional multiplier lobes retain their reduced amplitude.
- **Multiplier**: Scales the frequency of the LFO relative to the input phasors. Each loop uses the smaller of the mapped Mult and `9155.2734375 Hz / that loop's nominal rate`. The nominal rate is the master tempo times the loop's cycle ratio. A loop keeps its own effective Mult as the blend moves or another loop changes rate. The mix slew uses the resulting loop speeds.
- **Sample and Hold (S+H) Mix**: The LFO output can be blended with a Sample and Hold (S+H) value. The S+H value is captured whenever `m_trig` is asserted, and the mix between the continuous LFO and the stepped S+H is controlled by the `m_shFade` parameter. Voice LFOs trigger from the voice AHD gate; quad LFOs trigger from each channel's active delay-loop top.

## Usage

These LFOs are used extensively throughout the system:
- **Phase-Modulation LFO**: The Theory of Time uses a dedicated PolyXFader to modulate the global clock phase. It uses the same triangle → sine → tanh-shaped sine curve as the voice and quad LFOs. Its normalized Skew knob maps to attack fractions 0.1–0.9 on an evaluation copy; the stored knob and its slow control filter remain normalized. Time reversals and fractional-Mult lobes are preserved.
- **Voice LFOs**: Each voice (`SquiggleBoyVoice`) has two dedicated `SquiggleLFO` instances (which wrap `PolyXFaderInternal`) for per-voice modulation. Their phase-shift knobs spread the nine voices by thirds (`voice % 3 / 3`).
- **Quad LFOs**: `SquiggleBoy` runs the same two PolyXFader shapes as two four-lane `SquiggleLFOGeneric<4>` instances, one lane per quad channel, on the `QuadLFOs` encoder bank. Their parameters are read as interpolated four-lane encoder values. Phase-shift knobs spread those channels by quarters (`channel / 4`), and each channel's Sample-and-Hold captures when that channel's active delay loop crosses its modulated cycle boundary. Outputs feed Quad modulator slots 6 and 7.

## Related
- [Theory of Time](theory-of-time.md)
- [DSP Overview](dsp-overview.md)
