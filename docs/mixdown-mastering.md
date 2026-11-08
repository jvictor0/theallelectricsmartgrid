# Mixdown and Mastering

The final stages of the Smart Grid One DSP engine (`private/src/QuadMixer.hpp` and `private/src/QuadMasterChain.hpp`) handle the routing of effects, the mixdown to different channel formats, and the final mastering saturation.

## Mixdown

`QuadMixerInternal` is SquiggleBoy's mixer, not a general-purpose one. Its input is three trio tracks (each a `TrioFloat` signal with trio pan coordinates, fader gain, send gains, and the trio's sub-oscillators) plus four stereo external inputs from the source mixer. One generic `Mix` function handles every lane group: each lane is routed as follows:
1. **Direct Out**: Each signal is scaled by the track fader gain, saturated and metered, then summed into the dry bus; subs go to the sub bus at center. External inputs reach the live buses only while monitored. The fader drives saturation, and the voice level meter follows the dry contribution.
2. **Effects Sends**: The raw track signals, before both fader gain and saturation, are sent to the Quad Delay, Quad Reverb, and Partial Machine based on per-voice send levels. Subs and external inputs have no sends.
3. **Effects Returns**: The quadraphonic outputs of the Delay, Reverb, and Partial Machine are mixed back into the main quad bus.

The three send returns can cross-feed each other. Delay and Reverb can send into the Partial Machine, and the Partial Machine can send back into Delay or Reverb. A return never feeds its own send bus.

The system simultaneously generates three parallel mixdowns:
- **Quadraphonic**: The primary 4-channel output.
- **Stereo**: A folded-down stereo mix (`QuadToStereoMixdown`). It uses a specific matrix to fold the rear channels into the front channels while preserving spatial width.
- **Sub**: A dedicated mono sub-woofer channel, fed by the per-voice sub oscillators and the mono low-band sum in the mastering chain. See [Sub Oscillator](sub-oscillator.md).

## Multiband Saturator

Before leaving the synthesizer, both the Quad and Stereo mixdowns pass through a `DualMasteringChain`, which contains a `MultibandSaturator` (`private/src/MultibandSaturator.hpp`).

The Multiband Saturator splits the signal into 4 frequency bands using fourth-order Butterworth branches (the legacy type name is `LinkwitzRileyCrossoverGeneric`). The retained section Qs are approximately 0.541196 and 1.306563: each branch is −3 dB at cutoff, and their unsaturated sum is +3 dB there, rather than a flat LR4 sum. Each crossover uses linear trapezoidal state-variable sections to avoid low-cutoff float precision loss at the engine rate. LP and HP share the first section, with a separate second section for each branch. All channels are processed as one vector, with shared coefficients and the three cutoff calculations batched across SIMD lanes.
- Each band can have its gain adjusted independently.
- Each band is then passed through a tanh saturator (`ProcessAndSaturate` in `Metering`), using the clamped rational approximation from `TanhSaturator` to provide soft clipping and harmonic saturation.
- Because the saturation is applied per-band, it prevents intermodulation distortion (e.g., heavy bass notes won't cause the high frequencies to duck or distort).
- The bands are then summed back together, providing a loud, glued, and polished final output.

## Engine and Host Rates

The mixer and mastering chains run at the engine rate. The mastered quad, stereo, and sub outputs pass through order-8 Butterworth decimators to the host rate before master volume is applied, and the low-band cutoff range is defined in Hz so the crossovers do not move with the oversampling ratio. Meters accumulate engine samples and update once per host frame, so peak hold and RMS smoothing are counted in host frames. Performance recording decimates each recorded stream and commits one frame per host frame; the master tracks come from the output decimator. All recorded audio is scaled by 0.5 (about -6 dB) to reserve headroom for decimator overshoot, without changing the live audio or pan coordinates. Stereo extraction peak-normalizes both channels with one shared gain; see [recording extraction](streaming-recording-format.md).

## Related
- [DSP Overview](dsp-overview.md)
- [Quad Delay](quad-delay.md)
- [Quad Reverb](quad-reverb.md)
- [Partial Machine](partial-machine.md)