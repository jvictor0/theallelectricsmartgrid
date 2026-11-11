## Context

Badges currently load seven white-on-transparent PNGs into a cache. Encoder UI publication copies static catalogue glyphs. Source visualizers already resolve the owning trio's source machine. The palette change is complete in this worktree and remains unchanged.

## Goals / Non-Goals

**Goals:** Six legible matching symbols, correct role assignments, machine-aware source badges, preserved colors, and a rendered preview at actual badge sizes.

**Non-Goals:** DSP or persistence changes, new source outputs for Sample/Thru, replacing the existing seven symbols, or changing live dial visualizers.

## Decisions

- Build the six new symbols as JUCE paths with rounded joins/caps, rasterized once into the existing white-on-transparent cache. This preserves precise waveform geometry and avoids binary asset regeneration or paint-time shape construction.
- Preserve existing glyph numeric values, including None; append new glyphs and an explicit count.
- Use a saturated sine cycle for Sub; a fixed three-partial cosine sum for the additive badge, subject to small-size visual inspection; outward/inward diagonals through open upper-right/upper-left box corners for output/send; three waves for wet; arc/ticks/needle for VU.
- `RandomWaveTable::GenerateWaveTable` and `FillHarmonicsBandLimited` write real FFT coefficients. `InverseTransform` mirrors them conjugately and takes the real output, giving cosine harmonics. The unused `WriteDistortedHarmonic` writes imaginary coefficients but is not on this generation path. The oscillator subsequently phase-warps and negates its wavetable output.
- Publish glyphs using an explicit array of the three current source machines, supplied by the synth's existing UI publication call. Resolve the owner from each bound source, never from the currently navigated trio. Keep the existing one-argument encoder publication overload for standalone/default Dual VCO contexts.
- Dual source outputs use the additive badge; physical outputs use Noise/ADSR. Sample and Thru publish None for the source pair because both currently produce zero modulation outputs. Colors and stored depths remain intact.
- VU represents all seven mixer followers; Send represents dry audio, Wet represents return audio, Output represents Voice and final output audio. Existing envelope generators retain ADSR.

## Risks / Trade-offs

- Fine waveform details can disappear at small sizes → inspect a contact sheet with existing badges and 16/24/32 px samples; use only a few partials.
- Source machine may change without navigating → test repeated publication and owning-trio lookup, including a visible source from a different trio.
- New glyphs could collide with the old None sentinel → preserve old values and validate cache coverage explicitly.
