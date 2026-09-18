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
- By looking up `F(t) - d` in the inverse buffer, the delay can estimate the wall-clock time `t_old` when that warped time occurred, and then read the audio sample `X(t_old)`.

Inverse-map timestamps use linear interpolation both when recording an ascending interval and when looking up a fractional warped position. Audio samples retain cubic interpolation. Signed absolute coordinates, including negative positions, wrap using Euclidean modulo at the physical array boundary.

Audio continues recording during time reversals. The inverse map updates only on forward motion, so backward motion reads previously mapped history; later forward motion replaces it. The turnaround sample is retained as an interpolation endpoint so the first forward interval also replaces its history. This is the intended reversal behavior.

## Read/Write Head Computation (`QuadDelayInputSetter`)

The read and write heads are produced in `QuadDelayInputSetter::Process` (`private/src/QuadDelay.hpp`) per quad channel `i`.

- **Loop selection**: `ReadTapeHead` receives the processed loop-selector knob value, but only accepts changes when both old and new loops are simultaneously at modulated cycle boundaries (`CrossedCycleBoundary`) to avoid discontinuities.
- **Glue offset**: `WriteTapeHead` owns the additive glue offset that preserves continuity across transport stops and tempo-scale changes.
  - When transport stops, glue is initialized from the current write head, then incremented each sample.
  - On a transport transition or global-period change, glue is recomputed from the previous actual position and the new theory position, preserving absolute position continuity.
- **Delay ratio quantization**: at loop top, delay-time factor is quantized to one of:
  - `0.8`, `2/3`, `1.0`, `3/4`, `5/8`
  and stored as `m_bufferFrac[i]`.
- **Read-head speed quantization**: read-head speed is quantized to the same ratio family and its negative counterparts:
  - `-4`, `-3`, `-2`, `-3/2`, `-4/3`, `-1`, `-1/2`, `-1/4`, `1/4`, `1/2`, `1`, `4/3`, `3/2`, `2`, `3`, `4`

The final head equations are:

- **Write head**
  - `writeHead = globalModulatedPhase * globalPeriodSamples + glue`
  - stored in `delayInput.m_writeHeadPosition[i]`
- **Read head**
  - `effectiveDelaySamples = (globalPeriodSamples / cycleRatio(selectedLoop)) * (bufferFrac * widen)`
  - `readTarget = writeHead * readHeadSpeed - effectiveDelaySamples`
  - `readHead = wrap_mod(writeHead - resynthesisHopSamples - selectedLoopSamples, writeHead - resynthesisHopSamples, readTarget)`
  - stored in `delayInput.m_readHeadPosition[i]`

Both heads use absolute warped-sample coordinates. The read head is projected into the selected-loop-length region behind the write head, offset by the resynthesis hop size, with delay shaped by quantized ratio and widener. This warped-coordinate window selects musical delay time; it cannot by itself ensure a complete real-time analysis window.

At grain launch, the delay maps the selected read head into real audio time, adds the delay LFO's sample offset, then limits the start to the latest complete analysis window:

```
requestedStart = inverse(readHead) + sampleOffset
latestStart = latestRecordedSample - (N - 1) - 2
startTime = min(requestedStart, latestStart)
```

Here `N = 4096` and the extra two samples support cubic audio interpolation. Starts already safely behind the writer are unchanged. Requests too close to the writer use the latest complete frame, preventing zeros or stale samples beyond the recording frontier from entering the grain. At these limits the grain follows the recording frontier until its requested time falls behind the limit again. Physical addressing remains circular, including for negative positions. Recorded sample-bank playback uses an unbounded frontier and retains its existing behavior.

## Phase Vocoder Done Right

Because the read head is moving through the audio buffer at a variable rate (due to the time warping), a simple read would result in severe pitch shifting (like scratching a vinyl record).

To preserve the original pitch while allowing the time-warping to stretch and compress the audio, the delay uses a **Phase Vocoder** (`Resynthesizer` in `private/src/Resynthesis.hpp`).
- The audio is processed in overlapping grains (`GrainManager`).
- For each synthesis frame, the system computes two analysis frames:
  1. One at the target read position `F⁻¹(F(t) - d)`, after applying the sample offset and real-time analysis-window limit above.
  2. One exactly `H` (hop size) absolute samples before that position.
- By comparing the phases of these two analysis frames, the resynthesizer can accurately update the synthesis phases, preserving the pitch of the original signal regardless of the playback speed. This roughly follows the "Phase Vocoder Done Right" methodology.

## Quad Delay Features

The Quad Delay supports a rich set of features:
- **Synced Delay Time**: Delay times are synchronized to the Theory of Time.
- **Feedback > 1**: Allows for self-oscillating, runaway feedback loops.
- **Per-Partial Fade In (Slew Up)**: The resynthesizer can slew the amplitudes of individual partials, creating a smooth, blooming fade-in effect for the delayed signal.
- **LP and HP Damping**: Low-pass and high-pass filters in the feedback path (`PostFeedbackFilter`) to dampen the echoes.
- **Unison and Pitch Shifting**: The phase vocoder allows for independent pitch shifting of the delayed signal.
- **Ping Pong**: 90-degree or 180-degree quadraphonic ping-pong routing.
- **Return Sends**: The output of the delay can be routed directly into the Quad Reverb or the Partial Machine.

## Related
- [Theory of Time](theory-of-time.md)
- [Phase Vocoder](phase-vocoder.md)
- [Quad Reverb](quad-reverb.md)
- [Partial Machine](partial-machine.md)
- [DSP Overview](dsp-overview.md)
