# Sub Oscillator

Each voice has a mono sine oscillator one octave below its current pitch, including in Physical Modeling mode. Its amp section determines whether it sounds. The Sub page controls its volume, wavefolder, and envelope smoothing independently for each voice, with the usual trio, scene, gesture, and modulation behavior.

## Controls

For normalized knob position `x` from 0 to 1:

- **VOL**, top row column 0: `(20^x - 1) / 19`, from exact silence to unity, applied after shaping and DC removal.
- **DRIV**, top row column 1: `0.125 * 64^x`, from 0.125× to 8×, with unity at the midpoint.
- **RISE**, top row column 2: `0.001 * 1000^x` seconds, from 1 ms to 1 s.
- **FALL**, top row column 3: `0.001 * 5000^x` seconds, from 1 ms to 5 s.
- **SHAP**, second row column 1: a continuous scan through seven wavefolder landmarks, defaulting to tanh saturation.

The former 32× Drive maximum is replaced by 8×. Saved normalized Drive positions and their persisted `SubTanhGain` names remain intact, but nonzero positions now produce less drive. Shape is appended as `SubShape`, preserving existing parameter IDs. A default instance starts at Shape zero; partial patch loads retain the normal rule that missing encoder roots preserve their current values.

The first visualizer slot shows the sub's time-domain waveform using the standard audio scope, synchronized to each sub oscillator's fractional phase-wrap top. The spectrum analyzer spans the remaining two slots. Both show shaped, enveloped, DC-blocked sub audio before the final voice gain, follow the active trio, and share the usual all-voices/individual-voice selector and voice colors. The analyzer omits the main filter-response overlay.

## Wavefolder

With `z = drive * envelopedSine`, compute `t = tanh(z)` using the existing rational approximation and `s = sin(z)` using the shared sine table. Shape moves between these equally spaced landmarks with adjacent linear crossfades:

| Shape | Raw landmark | Harmonics for a steady sine |
| --- | --- | --- |
| 0 | `t` | Odd; saturation |
| 1/6 | `t-t*t*t` | Odd; tanh cubic fold |
| 2/6 | `t-t*t*s` | Odd; mixed cubic fold |
| 3/6 | `t-t*s*s` | Odd; mixed cubic fold |
| 4/6 | `t-s*s*s` | Odd; mixed cubic fold |
| 5/6 | `s-s*s*s` | Odd; single and triple sine folding |
| 1 | `s` | Odd; sine folding |

This order groups similar functions: after introducing the tanh cubic fold, each step replaces one factor of tanh with sine, then removes the sine cubic fold. Every landmark and crossfade is odd-symmetric and approaches a sine at low drive, retaining its fundamental. At higher drive, folding can suppress or reverse the fundamental. Modulation and note envelopes change the stationary spectrum.

The processor uses the sine table's output directly, including its small numerical residue at zero; there is no zero-input override. One shared 5 Hz one-pole high-pass per voice follows the crossfade. Its state persists across note and parameter changes and may produce a short settling tail.

Gain compensation uses reachable peaks for a full-scale input, recalculated only when Drive changes. Pure tanh/sine cubic folds use their interior maximum, at `min(reachablePeak, 1/sqrt(3))`. The three mixed folds compare the endpoint with their first interior peak once it is reachable, then retain their global maximum once the input range reaches it. Those constants use the existing rational tanh approximation and are checked against dense numerical sweeps. This avoids division by a folded endpoint that can be zero. The tanh landmark retains the previous normalized tanh curve before DC removal. Compensation does not follow the instantaneous envelope, does not equalize RMS loudness, and does not guarantee unity peaks after DC removal.

`WavefolderGeneric<Size>` in `private/src/Wavefolder.hpp` encapsulates this processing for the supported vector widths, with `Wavefolder` and `TrioWavefolder` aliases. `SetGain` accepts positive physical gain; `Process` accepts audio in [-1,1] and Shape in [0,1]. One call consumes one engine sample per lane, computes one audio sine and tanh, and updates independent DC state without allocations. It relies on the engine's existing oversampling/decimation, not an internal resampler; extreme harmonics can still exceed the output bandwidth.

## Signal Path

1. The continuously running sub sine uses half the current voice base frequency.
2. The exponential main amp envelope supplies its target when `m_subRunning` is true, and zero otherwise. Independent rise/fall smoothing produces the sub envelope.
3. The envelope multiplies the sine before the wavefolder, so each note's dynamics also change the degree of shaping.
4. The wavefolder applies Drive, Shape, level compensation, and DC removal.
5. Frequency-dependent voice gain and Sub Volume scale the result. The Sub scope observes this audio before the final voice gain.
6. The final voice gain produces `m_subOut`, which enters its own mono lane's mixer saturation and metering before center panning into the quad bus. Main and sub have independent lane meters/saturation.
7. The master multiband chain sums the low band to mono and saturates it. The new DC blocker belongs to the sub wavefolder; main-voice and master signal paths are unchanged.

## Unison Behavior

On each main amp trigger, `m_subRunning` latches whether this voice is the unison master. With unison disabled, every voice is eligible. With unison enabled, only the selected master enables its sub; other voices' envelopes decay toward zero. The flag starts false and is an eligibility latch, not an indication that the current envelope is audible.
