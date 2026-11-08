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

## Rate Split

The delay runs at the engine rate (`SampleTimer::x_oversample` engine samples per host frame) around host-rate storage:

```
engine input + return * feedback -> tanh saturation -> order-8 Butterworth decimator
    -> delay line write once per host frame -> 4096-sample analysis / PVDR
    -> (4096 * x_oversample)-point IFFT -> engine-rate overlap-add, PM read, rotation, damping
```

The decimator follows the saturator so its harmonics are filtered too. The delay line, inverse buffer, tape-head positions, analysis window and hop are all in host samples, so delay capacity and grain timing keep their physical durations. Each grain's spectrum stays on the 4096-point host-rate grid and is interpolated by a larger inverse transform into an engine-rate frame of the same 85.333 ms duration (see [Phase Vocoder](phase-vocoder.md)); there is no separate output upsampler. Rotation and damping run every engine sample. At `x_oversample = 1` the decimator is a passthrough. The delay has no internal LFO or LFO frequency, depth, or phase controls.

## Read/Write Head Computation (`QuadDelayInputSetter`)

`QuadDelayInputSetter::Process` (`private/src/QuadDelay.hpp`) produces one shared write head and a read head for each quad channel `i`. All channels share the inverse time map and positional recorder; audio and envelope values remain independent per channel.

- **Loop selection**: `ReadTapeHead` receives the processed loop-selector knob value, but only accepts changes when both old and new loops are simultaneously at modulated cycle boundaries (`CrossedCycleBoundary`) to avoid discontinuities.
- **Glue offset**: `WriteTapeHead` owns the additive glue offset that preserves continuity across transport stops and tempo-scale changes.
  - When transport stops, glue is initialized from the current write head, then the head advances one host sample per host frame (`1 / x_oversample` per engine sample).
  - On a transport transition or global-period change, glue is recomputed from the previous actual position and the new theory position, preserving absolute position continuity.
- **Delay phase while stopped**: the delay's local phase continues from its last position at `1 / globalPeriodSamples` cycles per host frame, even though Theory of Time resets to zero. Read heads retain their selected speeds. Starting transport rejoins musical phase for playback; a shared display offset keeps the waveform and write marker continuous across that phase reset.
- **Delay ratio quantization**: at loop top, delay-time factor is quantized to one of:
  - `0.8`, `2/3`, `1.0`, `3/4`, `5/8`
  and stored as `m_bufferFrac[i]`.
- **Read-head speed quantization**: read-head speed is quantized to the same ratio family and its negative counterparts:
  - `-4`, `-3`, `-2`, `-3/2`, `-4/3`, `-1`, `-1/2`, `-1/4`, `1/4`, `1/2`, `1`, `4/3`, `3/2`, `2`, `3`, `4`

The final head equations are:

- **Write head**
  - `writeHead = globalModulatedPhase * globalPeriodSamples + glue`, where `globalPeriodSamples` is the Theory of Time period converted to host samples
  - stored in the shared scalar `delayInput.m_writeHeadPosition`
- **Read head**
  - `effectiveDelaySamples = (globalPeriodSamples / cycleRatio(selectedLoop)) * bufferFrac`
  - `readTarget = writeHead + delayPhase * globalPeriodSamples * (readHeadSpeed - 1) - effectiveDelaySamples`
  - `readHead = wrap_mod(writeHead - resynthesisHopSamples - selectedLoopSamples, writeHead - resynthesisHopSamples, readTarget)`
  - stored in `delayInput.m_readHeadPosition[i]`

The projection is calculated once in phase units, then converted to absolute samples relative to the actual write head: `readHead = writeHead + (readPhase - delayPhase) * globalPeriodSamples`. Here `delayPhase` follows the modulated global phase while running and free-runs while stopped. The continuity glue translates both heads equally; playback speed applies only to delay phase. Multiplying the glue by read-head speed would select different audio from the displayed position whenever the speed is not 1.

The waveform recorder and both displayed heads use that same delay phase plus a shared display offset. On transport restart, the offset absorbs the reset to musical phase, preserving the waveform's horizontal coordinates. Read markers still reflect any playback repositioning caused by resynchronizing to music. While stopped, the waveform continues sweeping and recording across the display instead of overwriting one frozen column.

Both heads use absolute warped-sample coordinates. The read head is projected into the selected-loop-length region behind the write head, offset by the resynthesis hop size, with delay shaped by the quantized ratio. This warped-coordinate window selects musical delay time; it cannot by itself ensure a complete real-time analysis window. The former Widen control and its channel-dependent delay scaling have been removed; old `DelayWiden` patch fields are ignored.

At grain launch, the delay maps each selected read head into real audio time, then limits the start to the latest complete analysis window. The generic grain manager accepts optional sample offsets, but Quad Delay passes zero:

```
requestedStart = inverse(readHead)
latestStart = latestRecordedSample - (N - 1) - 2
startTime = min(requestedStart, latestStart)
```

Here `N = 4096` and the extra two samples support cubic audio interpolation. Starts already safely behind the writer are unchanged. Requests too close to the writer use the latest complete frame, preventing zeros or stale samples beyond the recording frontier from entering the grain. At these limits the grain follows the recording frontier until its requested time falls behind the limit again. Physical addressing remains circular, including for negative positions. Recorded sample-bank playback uses an unbounded frontier and retains its existing behavior.

## Phase Vocoder Done Right

Because the read head is moving through the audio buffer at a variable rate (due to the time warping), a simple read would result in severe pitch shifting (like scratching a vinyl record).

To preserve the original pitch while allowing the time-warping to stretch and compress the audio, the delay uses a **Phase Vocoder** (`Resynthesizer` in `private/src/Resynthesis.hpp`).
- One packed four-channel `GrainManagerGeneric` launches all channels together, with independent read positions, PVDR relationships and oscillator phases. Its FFTs and overlap-add ring carry four channels together.
- For each synthesis frame, the system computes two analysis frames:
  1. One at the target read position `F⁻¹(F(t) - d)`, after applying the real-time analysis-window limit above.
  2. One exactly `H` (hop size) absolute samples before that position.
- By comparing the phases of these two analysis frames, the resynthesizer can accurately update the synthesis phases, preserving the pitch of the original signal regardless of the playback speed. This roughly follows the "Phase Vocoder Done Right" methodology.

## Output Phase Modulation

The **PHAS** knob occupies the delay page's bottom-left cell `(0, 3)`. It is a
continuous bipolar control, centered at zero, with independent modulation per
quad channel. Its mapping is linear; modulation depth uses the existing
exponential depth controls.

The shared encoder path smooths manual scene values with a one-pole filter
whose time constant is 20 ms, then interpolates host-frame values across the
engine samples. Live modulation stays outside that manual-control filter.
The delay adds no further smoothing to this value. This control smoothing is
separate from the cubic interpolation of audio samples.

Let `H = 1024 * x_oversample` engine samples. Synthesis frames remain 4H long
and launch every H samples. Quad Delay accumulates them in a 6H ring, retaining
2H of past output. Every engine sample, each channel reads at
`clock - H + PM`, where `PM = knob * (H - 2)`. Positive PM advances the read
position and negative PM moves it farther into the retained history. Four-point
Catmull-Rom cubic interpolation handles fractional positions. The two-engine-
sample margin keeps all interpolation taps within retained, completed output.

At neutral PM, this adds exactly H samples (21.333 ms) to the delay's previous
output timing. At either endpoint the added delay is 2 or `2H - 2` engine
samples. Cleanup follows the fixed clock, so moving the PM head backward can
revisit intact samples. Clearing a channel clears both its history and queued
output. Sample playback and Partial Machine keep their existing OLA timing.

PM acts on the reconstructed waveform before rotation and damping, including
the output returned to feedback. It modulates the timing of transients and
envelopes as well as oscillator phase. Cubic interpolation and engine
oversampling do not guarantee alias-free output under fast, deep modulation.

## Quad Delay Features

The Quad Delay supports a rich set of features:
- **Synced Delay Time**: Delay times are synchronized to the Theory of Time.
- **Feedback > 1**: Allows for self-oscillating, runaway feedback loops.
- **Per-Partial Fade In (Slew Up)**: The resynthesizer can slew the amplitudes of individual partials, creating a smooth, blooming fade-in effect for the delayed signal.
- **LP and HP Damping**: Low-pass and high-pass filters in the feedback path (`PostFeedbackFilter`) to dampen the echoes.
- **Unison and Pitch Shifting**: The phase vocoder allows for independent pitch shifting of the delayed signal. Unison adds copies above and below each channel's pitch, reaching ±40, ±45, ±50, and ±55 cents on channels 1–4 at full knob travel. Spread scales linearly in cents with the knob; side-copy gain reaches full level at one third of the knob's travel.
- **Ping Pong**: 90-degree or 180-degree quadraphonic ping-pong routing.
- **Return Sends**: The output of the delay can be routed directly into the Quad Reverb or the Partial Machine.

## Related
- [Theory of Time](theory-of-time.md)
- [Phase Vocoder](phase-vocoder.md)
- [Quad Reverb](quad-reverb.md)
- [Partial Machine](partial-machine.md)
- [DSP Overview](dsp-overview.md)
