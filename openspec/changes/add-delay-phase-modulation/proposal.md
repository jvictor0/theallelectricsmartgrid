## Why

Quad Delay needs audio-rate phase modulation of its reconstructed output. Retaining past overlap-add samples allows an interpolated read head to move in either direction without waiting for another spectral frame.

## What Changes

- Use a 6H accumulation ring for Quad Delay's 4H synthesis frames, with H of nominal added latency.
- Add per-channel cubic-interpolated output reads with a linear PM range of `[-H + 2, H - 2]` engine samples. The two-sample margin protects the interpolation stencil.
- Place a bipolar, zero-default PM knob in the delay page's cell `(0, 3)`, using the existing modulation-depth system.
- Retire old samples independently of the PM read position and preserve other OLA clients' existing timing.
- Remove the Quad Delay Widen knob and its channel-dependent delay-length scaling. Leave its former cell `(0, 2)` empty.
- Give the four delay channels distinct maximum unison detunes of 40, 45, 50, and 55 cents through a generic unison-base helper.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `quad-delay`: Bounded interpolated output phase modulation and its knob, storage, and latency contract; removal of the Widen control and delay-length scaling.
- `phase-vocoder`: Optional retained-history overlap-add reads for the Quad Delay caller.

## Impact

OLA and grain-manager storage/read contracts, Quad Delay input mapping, encoder catalogue/page placement, DSP wiring, regression tests, and delay/phase-vocoder documentation. No new dependency or audio-thread allocation.
