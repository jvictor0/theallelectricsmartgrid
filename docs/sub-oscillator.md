# Sub Oscillator

The sub oscillator is a per-voice, mono signal path that runs one octave below the main pitch and is mixed alongside each voice before the global mixdown. It is always driven by the current voice pitch (even in Physical Modeling mode), but only becomes audible when the amp section enables it.

## Volume and Drive

The Sub page exposes separate exponential controls, with normalized knob position `x` from 0 to 1:

- **VOL (Sub Volume)**: `(20^x - 1) / 19`, ranging from exact silence to unity. It sets the sub envelope level after saturation.
- **DRIV (Sub Drive)**: `0.125 * 256^x`, ranging from 0.125× (−18.1 dB) to 32× (+30.1 dB), with 2× at the midpoint. It drives the normalized tanh saturator, increasing waveform saturation rather than multiplying the final output level by the same amount.

The Drive maximum was increased from 8× to 32×. Saved knob positions retain their normalized values, so nonzero Drive settings produce more saturation with the expanded range; the default minimum remains 0.125×.

## Signal Path

1. **Per-voice oscillator**: Each `SquiggleBoyVoice` owns a `VCO` that generates a sine at `baseFreq / 2` (one octave down) on every audio sample. The base frequency is always set in the voice input (`m_subInput.m_baseFreq`).
2. **Amp-section shaping**: The sub signal runs through a per-voice tanh saturator and low-pass filter inside the amp section before it is added to the mix.
3. **Mono injection**: The amp section produces a `m_subOut` mono signal per voice, which is injected into the mixer as `monoIn`.

## Mono Mixing and Saturation

The sub oscillator stays mono by design, but it is saturated alongside the main voice:

- In `QuadMixerInternal::ProcessInputs`, the per-voice meter is driven by **voice + mono sub** together. The resulting reduction is then applied to both the panned voice and the centered mono sub. This makes the sub saturate with the rest of the voice rather than living on a separate island.
- The mono sub is panned to the center (`QuadFloat::Pan(0.5f, 0.5f, monoIn)`) and mixed into the quad bus.
- In the master chain, `MultibandSaturator` is configured with `monoTheBass = true`, which sums the low band across channels into `m_sub` and saturates it, keeping the final sub output mono.

## Unison Behavior

When unison is active, only the **unison master** voice is allowed to trigger the sub:

- `TheNonagonSquiggleBoy` sets `m_ampInput.m_subTrig` based on `IsUnisonMaster(i)`.
- The amp section only enables the sub (`m_subRunning`) when this trigger is true.

This keeps a single, coherent sub in unison modes rather than stacking multiple sub oscillators on the same pitch.

## Related Code

Key locations in the codebase:

- `private/src/SquiggleBoy.hpp`: `SquiggleBoyVoice::Processs` (per-sample sub oscillator), `AmpSection` sub shaping, and `m_subOut` generation.
- `private/src/QuadMixer.hpp`: mono injection and per-voice saturation.
- `private/src/MultibandSaturator.hpp`: mono low-band summing into `m_sub`.
- `private/src/TheNonagonSquiggleBoy.hpp`: unison master trigger for the sub.
