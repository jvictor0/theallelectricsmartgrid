# Quadraphonic Delay and Phase Vocoder

The **Quad Delay** (`QuadDelay` in `private/src/QuadDelay.hpp`) is a highly complex, time-warped delay effect deeply integrated with the [Theory of Time](theory-of-time.md).

## Time Warping and the Inverse Buffer

A traditional delay line implements the equation `D(t + d) = X(t)`, where `t` is wall-clock time, `d` is the delay time, and `X` is the input signal.

The Smart Grid One implements a **time-warped** version of this equation. Let `F(t)` be the post-modulation logical time from the Theory of Time. The quad delay implements:
`D(F⁻¹(t + d)) = X(F⁻¹(t))`
This means: "At the wall-clock time that produces the warped time `t + d`, output the sample that was recorded at the wall-clock time that produced the warped time `t`."

Assuming `F` is injective (which it sometimes is, though extreme modulation can break this), this can be rewritten as:
`D(t) = X(F⁻¹(F(t) - d))`

To implement this, the delay requires a "moveable writehead." It must compute and store the inverse function `F⁻¹`.
- `DelayLineMovableWriter` maintains two parallel circular buffers:
  1. `m_delayLine`: The audio samples `X(t)`.
  2. `m_writeHeadInverse`: The wall-clock time `t` at which each warped time `F(t)` occurred.
- By looking up `F(t) - d` in the inverse buffer, the delay can find the exact wall-clock time `t_old` when that warped time occurred, and then read the audio sample `X(t_old)`.

## Read/Write Head Computation (`QuadDelayInputSetter`)

The read and write heads are produced in `QuadDelayInputSetter::Process` (`private/src/QuadDelay.hpp`) per quad channel `i`.

- **Loop selection**: `m_totLoopSelector[i]` is chosen from the delay loop-selector knob, but changes are only accepted when both old and new loops are simultaneously at top (`m_top`) to avoid discontinuities.
- **Glue offset**: both heads share an additive offset `m_glue[i]` that preserves continuity across transport stops, loop-selector changes, and tempo-scale changes.
  - When transport stops, `m_glue[i]` is initialized from the current write head, then incremented each sample.
  - On master-loop-size change, `m_glue[i]` is rescaled so absolute position continuity is maintained.
  - On loop-selector switch, `m_glue[i]` is shifted by the difference of unwound-sample positions between old and new loops.
- **Delay ratio quantization**: at loop top, delay-time factor is quantized to one of:
  - `0.8`, `2/3`, `1.0`, `3/4`, `5/8`
  and stored as `m_bufferFrac[i]`.

The final head equations are:

- **Write head**
  - `writeHead = PhasorUnwoundSamples(selectedLoop) + glue`
  - stored in `delayInput.m_writeHeadPosition[i]`
- **Read head**
  - `readHead = LoopSamplesFraction(selectedLoop, bufferFrac * widen) + glue`
  - stored in `delayInput.m_readHeadPosition[i]`

So both heads live in the same unwound sample coordinate system and differ by a loop-fraction delay term shaped by quantized ratio and widener.

## Phase Vocoder Done Right

Because the read head is moving through the audio buffer at a variable rate (due to the time warping), a simple read would result in severe pitch shifting (like scratching a vinyl record).

To preserve the original pitch while allowing the time-warping to stretch and compress the audio, the delay uses a **Phase Vocoder** (`Resynthesizer` in `private/src/Resynthesis.hpp`).
- The audio is processed in overlapping grains (`GrainManager`).
- For each synthesis frame, the system computes two analysis frames:
  1. One at the target read position `F⁻¹(F(t) - d)`.
  2. One exactly `H` (hop size) unwound samples before that position.
- By comparing the phases of these two analysis frames, the resynthesizer can accurately update the synthesis phases, preserving the pitch of the original signal regardless of the playback speed. This roughly follows the "Phase Vocoder Done Right" methodology.

## Quad Delay Features

The Quad Delay supports a rich set of features:
- **Synced Delay Time**: Delay times are synchronized to the Theory of Time.
- **Feedback > 1**: Allows for self-oscillating, runaway feedback loops.
- **Per-Partial Fade In (Slew Up)**: The resynthesizer can slew the amplitudes of individual partials, creating a smooth, blooming fade-in effect for the delayed signal.
- **LP and HP Damping**: Low-pass and high-pass filters in the feedback path (`PostFeedbackFilter`) to dampen the echoes.
- **Unison and Pitch Shifting**: The phase vocoder allows for independent pitch shifting of the delayed signal.
- **Ping Pong**: 90-degree or 180-degree quadraphonic ping-pong routing.
- **Reverb Send**: The output of the delay can be routed directly into the Quad Reverb.

## Related
- [Theory of Time](theory-of-time.md)
- [Phase Vocoder](phase-vocoder.md)
- [Quad Reverb](quad-reverb.md)
- [DSP Overview](dsp-overview.md)