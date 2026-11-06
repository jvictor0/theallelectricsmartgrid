## Why

Scope history currently reserves a GiB of sample storage while every bank receives the same capacity regardless of its channel count. A hierarchy of vector sample histories can retain recent samples at full resolution and roughly four minutes at reduced resolution using substantially less memory.

## What Changes

- Add a reusable, vectorized linear-phase `FIRDownsamplerGeneric` alongside the Butterworth filter.
- Replace fixed scope storage with writers parameterized by vector width and number of scopes, six equal-capacity rings, and cascaded factor-four decimation.
- Use trio vectors in voice banks, with physical scope index `logicalScopeIx * 3 + trioIx` (39 physical trio scopes for audio and control together).
- Store every scope at the engine rate in three banks (mono, trio, quad), with a 0.25-second base and six levels for 256 seconds of retention. Use 17-tap decimators.
- Expand scope FFTs from 1,024 to 4,096 samples at the 192 kHz engine rate, preserving bin spacing and the display low edge while extending the axis to 96 kHz. Convert host-rate overlays and capture mastered signals before output decimation.
- Write whole vectors from DSP producers. Preserve cycle markers and original sample coordinates; readers select and retain one LOD for their complete requested history interval.
- Leave only level-zero vector writes and atomic publication on the audio thread. Before painting, the UI accepts a fixed boundary for each bank and builds the lower levels from new samples. Visualizers use that accepted position and a matching marker snapshot.
- Use SIMD FFTs across trio/quad lanes, with four interleaved SIMD subsequences for mono spectra, and reuse preallocated FFT workspaces.
- Preserve atomic publication metadata, using ordinary vector sample storage as requested. Existing event history limits remain independent.

## Capabilities

### New Capabilities

- `fir-downsampler`: Reusable streaming vector FIR decimation with known delay.

### Modified Capabilities

- `ui-visualization-pipeline`: Multiresolution vector histories, original-coordinate reads, and fixed-LOD readers.
- `voice-architecture`: Whole-trio scope writes and logical-scope/trio addressing.
- `ahd-envelopes`: Vector envelope scope writes with unchanged per-voice event markers.

## Impact

DSP scope producers, `ScopeWriter.hpp`, `ButterworthFilter.hpp`, UI scope/spectrum readers, secondary modulator sampling, and core/visualizer tests. No new external dependencies; no synchronization of random or note event retention with sample history.
