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

The generated Lean core splits the signal into four bands using phase-compensated fourth-order Linkwitz–Riley crossovers. Each split uses two Butterworth-2 TPT SVF sections (Q = 1/√2): its LP and HP branches are −6.02 dB at the crossover, and their sum is allpass. Sibling allpass compensation aligns the branches throughout the tree, so the complete small-signal sum has flat magnitude at unity band and master gains (for common-channel input when mono bass is enabled). This replaces the old handwritten Butterworth-4 branches, which were −3 dB at crossover and did not reconstruct flat.

All audio channels are processed as one vector with shared coefficients. The three cutoff calculations remain batched across SIMD lanes using Math4096. Knob mappings and cutoff clamps are unchanged: at a 48 kHz host rate, the low cutoff spans 23.4–236.2 Hz, and each subsequent cutoff is 1–32 times the previous one. Default crossovers are approximately 74.4 Hz, 420.9 Hz, and 2.381 kHz.

- Each band has independent gain followed by saturation. Lean proofs use tanh; generated audio uses the clamped cubic rational approximation shared with `TanhSaturator`.
- With mono bass enabled, the lowest band is averaged across channels before gain and saturation, then added to each channel and exposed as the sub output.
- The bands are summed, multiplied by master gain, and saturated once more.
- The C++ wrapper owns the meters and records the generated before/after samples through `RecordSaturated`, preserving meter timing without saturating twice.
- UI snapshots capture coefficients and gains together. The generated magnitude-squared kernel implements the proved small-signal response, including phase during recombination; the UI plots its sampled lower/median/upper power curves in dB.

See the [Lean model and theorem inventory](../formal/DSP/Filters/MultibandSaturator/README.md) for proof assumptions and the generated interface.

## Engine and Host Rates

The mixer and mastering chains run at the engine rate. The mastered quad, stereo, and sub outputs pass through order-8 Butterworth decimators to the host rate before master volume is applied, and the low-band cutoff range is defined in Hz so the crossovers do not move with the oversampling ratio. Meters accumulate engine samples and update once per host frame, so peak hold and RMS smoothing are counted in host frames. Performance recording decimates each recorded stream and commits one frame per host frame; the master tracks come from the output decimator. All recorded audio is scaled by 0.5 (about -6 dB) to reserve headroom for decimator overshoot, without changing the live audio or pan coordinates. Stereo extraction peak-normalizes both channels with one shared gain; see [recording extraction](streaming-recording-format.md).

## Related
- [DSP Overview](dsp-overview.md)
- [Quad Delay](quad-delay.md)
- [Quad Reverb](quad-reverb.md)
- [Partial Machine](partial-machine.md)