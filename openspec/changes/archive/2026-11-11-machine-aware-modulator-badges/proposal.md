## Why

Sub, source outputs, mixer audio, and envelope followers currently reuse LFO/envelope badges that do not identify their roles. Source-output badges also cannot follow the selected synthesis machine.

## What Changes

- Add six matching rounded monochrome symbols: saturated Sub sine, additive harmonic waveform, outward audio output, inward send, three wet waves, and a VU needle.
- Resolve source-output badges by their owning trio's machine: harmonic waveform for dual waveshaping and existing Noise/Envelope for physical modeling.
- Assign VU to mixer followers, inward send to dry audio, wet waves to return audio, and the shared output symbol to Voice/main audio.
- Verify the generator's harmonic phase convention and choose a compact waveform that remains legible at small badge sizes.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `grid-visualizers`: Role-specific modulator badges and machine-aware source glyph resolution.

## Impact

Glyph definitions/rendering, source skin metadata, encoder UI publication, native rendering checks, core catalogue checks, and documentation. The existing palette, DSP, source identities, and saved modulation depths are preserved.
