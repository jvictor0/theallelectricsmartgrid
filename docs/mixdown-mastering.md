# Mixdown and Mastering

The final stages of the Smart Grid One DSP engine (`private/src/QuadMixer.hpp` and `private/src/QuadMasterChain.hpp`) handle the routing of effects, the mixdown to different channel formats, and the final mastering saturation.

## Mixdown

The `QuadMixerInternal` takes the 9 voices (which have already been panned quadraphonically) and routes them:
1. **Direct Out**: The quadraphonic signals are summed.
2. **Effects Sends**: The signals are sent to the Quad Delay and Quad Reverb based on per-voice send levels.
3. **Effects Returns**: The quadraphonic outputs of the Delay and Reverb are mixed back into the main quad bus.

The system simultaneously generates three parallel mixdowns:
- **Quadraphonic**: The primary 4-channel output.
- **Stereo**: A folded-down stereo mix (`QuadToStereoMixdown`). It uses a specific matrix to fold the rear channels into the front channels while preserving spatial width.
- **Sub**: A dedicated mono sub-woofer channel, fed by the per-voice sub oscillators and the mono low-band sum in the mastering chain. See [Sub Oscillator](sub-oscillator.md).

## Multiband Saturator

Before leaving the synthesizer, both the Quad and Stereo mixdowns pass through a `DualMasteringChain`, which contains a `MultibandSaturator` (`private/src/MultibandSaturator.hpp`).

The Multiband Saturator splits the signal into 4 frequency bands using 4th-order Linkwitz-Riley crossovers (`LinkwitzRileyCrossover`).
- Each band can have its gain adjusted independently.
- Each band is then passed through an arctan saturator (`ProcessAndSaturate` in `Metering`), providing soft clipping and harmonic saturation.
- Because the saturation is applied per-band, it prevents intermodulation distortion (e.g., heavy bass notes won't cause the high frequencies to duck or distort).
- The bands are then summed back together, providing a loud, glued, and polished final output.

## Related
- [DSP Overview](dsp-overview.md)
- [Quad Delay](quad-delay.md)
- [Quad Reverb](quad-reverb.md)