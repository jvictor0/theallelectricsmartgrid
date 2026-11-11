# Quadraphonic Delay and Phase Vocoder

The **Quad Delay** (`QuadDelay` in `private/src/QuadDelay.hpp`) is a highly complex, time-warped delay effect deeply integrated with the [Theory of Time](theory-of-time.md).

## Time Warping and the Inverse Buffer

Let `F : realTime -> continuousPhase` be the write trajectory in master cycles,
and `G : continuousPhase -> realTime` its chosen forward-history partial inverse.
For ideal available history, `F(G(q)) = q`; the stored approximation has the
phase-error bound described below. `G(F(t))` need not equal t when a phase has
been visited more than once. A delay at logical distance d requests `X(G(F(t)-d))`,
before the grain manager's real-time analysis-window constraint.

Two circular buffers retain different coordinates:

- `DelayLineMovableWriter::m_delayLine` holds one audio sample per host frame.
- `TimeWarpInverter::m_writeHeadInverse` holds absolute `SampleTimer` engine
  timestamps on a fixed grid of 2^18 points per master cycle. TheoryOfTime owns
  the inverter and records continuous phase every engine sample. Its 2^20 grid
  entries span four master cycles. Each cell owns two double fallback timestamps,
  one exact double replacement phase, and two float replacement timestamp deltas:
  32 MiB total. The audio ring independently retains 2^24 host samples.

The inverse grid does not depend on tempo. A tempo edit changes future travel,
without changing where previously recorded audio lies in phase. DelayLine
passes phase directly to G and divides the resulting real timestamp by
`x_oversample`. Host frame k completes at engine sample `k * x_oversample`.

Unwritten inverse entries are unavailable (NaN). A cell selects either its
preserved first traversal or its latest replacement, with the last forward endpoint
closing a partial traversal. At grain launch, unavailable history or a window
before retained audio suppresses that channel's new grain; scheduled overlap-add
tails continue. Unavailable waveform history displays as a gap.

Forward intervals scatter timestamps onto the fixed phase grid. A new visit after
leaving the cell replaces its single replacement; older interior islands below the new cut
revert to the fallback. Its two endpoints are stored independently of neighboring
cells. Within a closed-cell visit, turns keep the minimum cut and its first
timestamp, including touches of the right edge. A deeper minimum moves the cut lower; a
separate visit can move it higher again. The frontier freezes during reverse
travel. Before a cell's first complete traversal, internal reversals are represented by the entry/frontier interval, whose
entire trajectory remains inside the cell. Audio continues recording during
reverse travel, while G retains its selected forward history. Audio reads retain
cubic interpolation and physical array indices use Euclidean modulo.

There is no boundary pool or reversal-count capacity limit. The exact-arithmetic
model bounds sampled-phase residual by one cell under the traversal confinement
certificate. Selecting an older valid traversal is intentional; latest-forward
timestamp error alone is therefore not a correctness failure. Even on the same
branch, small timestamp error additionally needs a positive lower speed. The
current warp generator is not claimed to satisfy the declared spectral range;
this change adds no band-limit filter or general audibility guarantee.

The exact mathematical contracts, Veil transitions and reviewed C++ references
are documented in [the phase model](../formal/PhaseTime/README.md).

## Rate Split

The delay runs at the engine rate (`SampleTimer::x_oversample` engine samples per host frame) around host-rate storage:

```
engine input + return * feedback -> tanh saturation -> order-8 Butterworth decimator
    -> delay line write once per host frame -> 4096-sample analysis / PVDR
    -> (4096 * x_oversample)-point IFFT -> engine-rate overlap-add, PM read, rotation, damping
```

The decimator follows the saturator so its harmonics are filtered too. The delay line, analysis window and hop use host samples. All tape heads use continuous master phase. The inverse maps phase to engine time; DelayLine converts that result to host time, preserving physical grain durations. Each grain's spectrum stays on the 4096-point host-rate grid and is interpolated by a larger inverse transform into an engine-rate frame of the same 85.333 ms duration (see [Phase Vocoder](phase-vocoder.md)); there is no separate output upsampler. Rotation and damping run every engine sample. At `x_oversample = 1` the decimator is a passthrough. The delay has no internal LFO or LFO frequency, depth, or phase controls.

## Read/Write Head Computation (`QuadDelayInputSetter`)

`PhaseClock` belongs to TheoryOfTime. While running it retains a constant offset
and sets `q = modulatedMusicalPhase + offset`. While stopped it advances by
`1 / globalPeriodSamples` each engine sample. Restart sets
`offset = q - modulatedMusicalPhase`, leaving q unchanged. Genuine warp reversals
remain backwards movement. Tempo edits never rebase recorded coordinates.

Each read head uses this same q, including while stopped and across restart:

```
loopPeriod = 1 / cycleRatio(selectedLoop)
projected = q * readHeadSpeed - loopPeriod * bufferFraction
readPhase = wrap_mod(q - loopPeriod, q, projected)
```

The interval includes its lower endpoint and excludes q. With fixed speed,
buffer fraction and loop selection, restart preserves every read head exactly.
There is no independent musical reset or display offset for a read head.
`QuadDelay::Input` publishes `m_writeHeadPhase` and `m_readHeadPhase` directly.

Loop selection still changes only when the old and requested loops cross
together. At loop top the delay fraction is selected from `0.8`, `2/3`, `1`,
`3/4`, `5/8`. Read speeds are `-4`, `-3`, `-2`, `-3/2`, `-4/3`, `-1`, `-1/2`,
`-1/4`, `1/4`, `1/2`, `1`, `4/3`, `3/2`, `2`, `3`, `4`. Those parameter changes
can reposition a head; transport alone does not.

The delay waveform publishes an explicit period of one and origin zero. It
samples the current cycle up to q and the preceding cycle beyond q. It requires
no starts, no inferred startup width, and no special transport drawing path.
The same inverse drives audio queries and waveform queries. The former Widen
control remains removed; old `DelayWiden` patch fields are ignored.

The phase interval selects musical delay time. It does not impose a
sample-derived phase hop gap: complete analysis-window safety is applied in
real time after inversion.

At grain launch, the delay maps each selected read head into real audio time, then limits the start to the latest complete analysis window. The generic grain manager accepts optional sample offsets, but Quad Delay passes zero:

```
requestedStart = inverse(readHead)
latestStart = latestRecordedSample - (N - 1) - 2
startTime = min(requestedStart, latestStart)  # only for an available mapping
earliestStart = oldestRecordedSample + H + 1
# A start before earliestStart has unavailable previous-frame/interpolation support.
# It contributes no new grain.
```

Here `N = 4096`, `H = 1024` is the analysis hop, and the extra samples support cubic audio interpolation. Starts within retained audio and safely behind the writer are unchanged. Requests too close to the writer use the latest complete frame, preventing zeros or stale samples beyond the recording frontier from entering the grain. At these limits the grain follows the recording frontier until its requested time falls behind the limit again. Physical addressing remains circular, including for negative positions. Recorded sample-bank playback uses an unbounded frontier and retains its existing behavior.

## Phase Vocoder Done Right

Because the read head is moving through the audio buffer at a variable rate (due to the time warping), a simple read would result in severe pitch shifting (like scratching a vinyl record).

To preserve the original pitch while allowing the time-warping to stretch and compress the audio, the delay uses a **Phase Vocoder** (`Resynthesizer` in `private/src/Resynthesis.hpp`).
- One packed four-channel `GrainManagerGeneric` launches all channels together, with independent read positions, PVDR relationships and oscillator phases. Its FFTs and overlap-add ring carry four channels together.
- For each synthesis frame, the system computes two analysis frames:
  1. One at the target read position `G(F(t) - d)`, after applying the real-time analysis-window limit above.
  2. One exactly `H` (hop size) absolute samples before that position.
- By comparing the phases of these two analysis frames, the resynthesizer can accurately update the synthesis phases, preserving the pitch of the original signal regardless of the playback speed. This roughly follows the "Phase Vocoder Done Right" methodology.

## Output Phase Modulation

The **OFFS** knob occupies the delay page's bottom-left cell `(0, 3)`. It is a
continuous bipolar control, centered at zero, with independent modulation per
quad channel. Its mapping is linear; modulation depth uses the existing
exponential depth controls.

The shared encoder path smooths manual scene values with a one-pole filter
whose time constant is 20 ms, then interpolates host-frame values across the
engine samples. Live modulation stays outside that manual-control filter.
The delay adds no further smoothing to this value, so live modulation reaches
the offset read directly. Manual control smoothing is separate from the cubic
interpolation of audio samples.

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

## Shared warped scope display

The delay scope records the magnitude of each completed saturated, decimated
write and holds it between host frames on the engine sample grid. Existing FIR
LODs form its envelope. The visualizer composes those reads through G, converts
the result to dB, and fills from negative to positive display level about each
lane center. Read and write markers use the same explicit master-cycle axis.
The signed delay return remains available to the spectrum view.
