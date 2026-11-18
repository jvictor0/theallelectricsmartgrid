# Naive Reupping Ladder

The **Naive Reupping Ladder** is one of the two filter options available per voice
in the DSP engine. `NaiveReuppingLadderGeneric<Number, Size>` implements the
low-pass filter; `NaiveReuppingLadderShapeGeneric<Size>` combines it with the
sample-rate reducer and high-pass filter. The mono and trio filter aliases are
`NaiveReuppingLadder` and `TrioNaiveReuppingLadder`.

## Why this topology has this name

**Naive** refers to the four Euler updates, `y ← y + alpha * (x - y)`, and the
explicit one-sample feedback delay. Feedback reads the previous unsaturated
fourth-stage state before any of the current sample's stage updates.

**Reupping** describes applying drive again at every stage. Each stage passes
its output through `S_g(x) = Tanh(g * x) / Tanh(g)`, using the existing clamped
rational tanh approximation. At low gain, this normalized saturator approaches
the identity, so it adds little coloration to the filter's linear response.
At high gain, the next stage multiplies its own input by `g` afresh before its
tanh. Each stage can therefore contribute saturation, rather than the first
tanh doing all the work and later stages merely giving the compressed signal
"haircuts." This repeated drive is intentional.

The name identifies this particular discrete topology so other ladder designs
can coexist with it. The C++ implementation is the specification for the planned
Lean `NaiveReuppingLadder` model, including its delay, rational saturation, and
adaptive damping. The existing ideal linear `TPTLadder` is a separate topology.

The visible machine label is **Naive Reupping Ladder**. Its saved machine ID
remains `0`; the rename preserves existing patch selection and DSP behavior.

## Structure

This filter machine consists of two 4-pole filters in series:
1. **Low-Pass Filter**: The four-stage `NaiveReuppingLadder` described above.
2. **High-Pass Filter**: A 4-pole linear State Variable Filter (SVF) configured as a high-pass (`TPTSVF4PoleHighPassFilterGeneric`).

The audio signal from the source machine passes through the low-pass ladder filter first, then the sample rate reducer, and the reduced signal is then processed by the high-pass SVF. The high-pass follows the reducer so it also removes low-frequency content the reducer creates.

## Features

- **Cutoff Tracking**: The HP cutoff frequency tracks the base frequency of the voice's oscillator (`m_vcoBaseFreq`) multiplied by `m_hpCutoffFactor`. In Hz, the LP cutoff is `261.6256 * (vcoHz / 261.6256)^0.707 * hpCutoffFactor * lpCutoffFactor`. LP tracks 0.707 octaves per VCO octave around middle C (261.6256 Hz), while the HP cutoff factor multiplies both cutoffs linearly. The LP knob retains its 12-octave span with both endpoints multiplied by `1 / sqrt(2^(1/4)-1) = 2.298959`: approximately 0.574740 to 2354.134. This preserves low-frequency physical tuning approximately under the new cutoff convention. The 0.707 pitch-tracking law and shared HP factor are unchanged. HP is capped at host Nyquist; ladder LP is capped at engine Nyquist.
- **Resonance**: Both filters feature adjustable resonance (`m_lpResonance` and `m_hpResonance`). The generated HP cascade preserves the same natural cutoff and `Q = 0.5 * 11^resonance` in each section. At zero resonance its total cutoff gain is -12.04 dB. The ladder low-pass uses the traditional individual-stage half-power corner, giving -12.04 dB for the unit-gain zero-feedback cascade. Its two band-pass envelope followers now use generated SVF sections with Q = 1, retaining unity center gain.
- **Saturation**: The ladder filter includes an internal saturation stage, controlled by `m_saturationGain`, which drives the filter into non-linear distortion.
- **Sample Rate Reduction**: Between the low-pass and high-pass stages, a sample rate reducer (`SampleRateReducer`) adds digital aliasing and grit. Aliases folded below the high-pass cutoff are attenuated by the high-pass.

## Ladder cutoff convention

`SetCutoffAndResonance(cutoff, resonance)` takes cycles per **engine** sample.
Cutoff is the traditional individual-stage half-power corner. Four unit-gain
linear stages with feedback disabled therefore have magnitude 1/4 (-12.0412 dB)
at this frequency. The API and ladder shape cap it at engine Nyquist (0.5),
not host Nyquist. This permits the ladder to open above the audible band. The
separate high-pass retains its host-Nyquist cap.

The cascade half-power frequency is
`f3 = asin(sqrt(2^(1/4)-1) * sin(pi*cutoff)) / pi`, in engine cycles/sample.
At low frequencies this is approximately `0.434979442 * cutoff`; at a 24 kHz
stage corner with a 192 kHz engine it is 10.2208 kHz. The ratio changes only
modestly below host Nyquist. With cutoff at engine Nyquist (96 kHz), it reaches
27.5029 kHz, with zero-feedback attenuation of 1.708 dB at 20 kHz and 2.374 dB
at 24 kHz. These are unit-gain linear values; normalized saturation changes
absolute small-signal gain, while feedback changes the shape.

The detector follows the delayed loop's nominal oscillation frequency, not the
stage corner or the exact finite-resonance response peak. At a 24 kHz stage
corner its center is 36.0459 kHz and the unit-gain critical feedback is 9.68696.
The normalized saturators have positive small-signal gain, changing the critical
feedback but not that phase-crossing frequency. At a stage corner of about 37.615 kHz the crossing reaches engine Nyquist;
the detectors retain the equivalent of their existing 0.499 center cap to avoid the TPT
singularity. At engine Nyquist itself a TPT bandpass has zero gain, so the
Nyquist feedback guard remains important in that regime. The zero-feedback -12 dB convention and
delayed-loop tuning distinction are discussed in
[Huovilainen, DAFx 2004](https://dafx.de/paper-archive/2004/P_061.PDF).

`kSafe` does not necessarily reduce feedback when the detector moves above host
Nyquist. At the 24 kHz example it is approximately 41.548 at drive 0.5 (no clamp
on requested feedback up to 3.9), 3.234 at drive 2, and 0.08825 at drive 5.
At the fully open engine-Nyquist corner, `kSafe` is approximately 2.712 at
drive 0.5. Internal ultrasonic oscillation can still consume headroom and
interact with nonlinearities, so both bandpass detectors and their damping
remain active.

## Coefficients and sample recurrence

The setter calculates alpha and detector tuning together. With
`s = sin(pi*cutoff)` and `root = sqrt(1+s*s)`:

- `alpha = 2*s*(root-s)` sets the individual-stage half-power corner.
- `r = 2*s*(root+s)` is algebraically `alpha/(1-alpha)`, calculated directly
  from cutoff rather than by inverting the rounded alpha.
- For `r < 2`, the unit-gain loop crossing is `asin(r/2)/pi`, and its critical
  feedback is `(r+2)^2`. Otherwise the crossing is Nyquist and the critical
  feedback is `((r+2)/r)^4`.
- Both Q=1 band-pass cascades use that crossing as their center; their generated
  SVFs receive the prewarped gain directly:
  `g = tan(asin(r/2)) = (r/2) / sqrt(1-(r/2)^2)` below the Nyquist transition.
  No inverse sine or tangent is evaluated for the detector. The gain is capped
  at `tan(pi*0.499)`, approximately 318.309. Both regimes use that cap when the
  nominal crossing is at or beyond the detector's maximum center frequency.
  Power smoothing uses `powerAlpha = 0.0692291283 * s`,
  reusing the stage sine. The constant is `sqrt(2^(1/4)-1) / (2*pi)`. This gives
  approximately two cycles of the cascade half-power frequency per time constant.

The earlier exact envelope calculation, `-expm1(-f3/2)`, merely preserved the
old timing after changing cutoff conventions; the damping algorithm does not
require that precision. The simpler coefficient needs one multiply. A sweep
over the full cutoff range finds smoothing times at most 0.956% shorter than
before. At a 24 kHz stage corner and a 192 kHz engine, the time constant changes
from 0.19568 ms to 0.19398 ms. This timing parameter controls averaging of the
squared bandpass output; it is not another bandpass center or an exact EMA
half-power cutoff. The follower API now takes `powerAlpha` directly.

The setter uses vector `Math4096::Sin2pi`. Sine shifts
table indices rather than adding a fractional cycle to the float phase, preserving
small-cutoff relative accuracy. Cosine table initialization uses double angles
and exact quadrant zeros; the FFT root-of-unity tables are unchanged. Runtime
interpolation uses vector FMA and a shared gather with no runtime lane loop.
The two critical-feedback regimes use `IfLess`. Clamping the unused Nyquist
denominator at 2 keeps both selection operands finite even as cutoff approaches
zero. For detector tuning, `t = min(r/2,1)` and
`g = t / sqrt(max((1-t)*(1+t), t*t/gMax^2))`; this applies the gain cap before
division and keeps the square root and division valid at Nyquist.

The generated SVF exposes `ComputeCoefficientsFromSnapshot({g,k})`. It applies
the existing snapshot as prepared coefficients, including its cached response,
without changing integrator history. Both frequency/Q tuning and this direct
path use Lean's `prepareFromSnapshot`. Proofs establish their equivalence,
snapshot identity, preservation of all history fields, idempotence, channel
lifting, and the SVF `Tuned` invariant for nonnegative real g and k. This avoids
duplicating the generated SVF's coefficient equations in the handwritten ladder.

The sine form also avoids low-frequency cancellation in `1-cos(2*pi*cutoff)`.
The sample recurrence does not gain any transcendental calls; coefficient work
remains cached.
At engine Nyquist, alpha reaches `2*(sqrt(2)-1)`, approximately 0.828427, so
all four standalone one-poles remain strictly stable. This cap does not replace
the feedback guard or establish nonlinear closed-loop stability.

The resonance mapping remains `requestedFeedback = 3.9 * resonance`.
The setter caches the existing Nyquist guard `kSafe`, its minimum with requested
feedback, the reference power gain, and the drive-dependent damping scale.
`SetSaturationGain` refreshes those caches too. Neither setter clears stage or
envelope history; repeated identical parameters skip coefficient work.

`Process` still performs these operations in order:

1. Read the **previous sample's** input and feedback power estimates; reduce the
   cached base feedback using their excess-power ratio to obtain `kEff`.
2. Subtract `kEff * previousStage4` from the current input. `previousStage4`
   is the **unsaturated** fourth-stage state, not the final compensated output.
3. Advance each of the four one-pole states with the existing explicit FMA,
   saturating each result before passing it to the next stage.
4. Advance the input detector with the external input and the feedback detector
   with the current unsaturated fourth-stage state.
5. Return the saturated fourth-stage result multiplied by `1 + kEff`.

Here `Saturate(x) = Tanh(gain*x)/Tanh(gain)`, where the executable `Tanh` is
the rational function `x*(27+x*x)/(27+9*x*x)` clamped to [-1,1], not the
transcendental tanh. Three saturators are inside the feedback loop; the fourth
is on the output. The Nyquist guard retains the existing four-stage slope
heuristic; it does **not** establish stability of the complete nonlinear,
time-varying system. Debug meters and their periodic print dispatch are removed.

The snapshot still represents the frozen unit-gain linear model with effective
feedback. It excludes saturation slopes and envelope dynamics. This C++
refactor does not turn the existing generated Lean ladder into a model of the
app ladder. A subsequent implementation-as-spec model needs the exact state
ordering, feedback delay, rational saturation, detectors and adaptive damping
above; a transcendental tanh model alone would not describe this implementation.

## Vector tuning validation and cost

Regression coverage includes small positive/negative sine phases, the ladder's
stage-corner gain and detector centers, mixed-channel Nyquist transitions, and
the envelope time-constant approximation. Generated snapshot updates are checked
against ordinary tuning at both precisions and widths 1–4, including history,
idempotence, cached responses, and which state fields are written. The Lean
proof audit covers the six new SVF preparation theorems.

Validation passes the full core suite and alternate sample-rate target, all
eight performance tests, Lean execution/codegen checks and the proof audit,
and unsigned macOS/iOS app builds. The full suite exposed a pre-existing test
assumption that all concurrent snapshot pushes succeed; the test now counts
accepted publications, matching the nonblocking history contract. That test
also passes 100 consecutive runs; production snapshot history is unchanged.

A serial desktop comparison against `fe2675b` used the same stage-corner
parameters, six alternating runs, and 1,048,576 trio samples per workload.
Median time with drive-only modulation was 61.214 ms before and 61.130 ms after
(-0.14%, effectively unchanged). With cutoff, resonance and drive all changing
every sample, it was 123.991 ms before and 78.542 ms after (-36.65%). These are
desktop microbenchmarks; iPad callback cost has not been measured. Parameter
preparation is faster; the sample recurrence remains unchanged.

## Historical stage-corner validation and cost

The stage-corner change in `fe2675b`, before vector lookup and simplified envelope
timing, passed the full core suite, including the alternate
sample-rate target, all eight performance tests, and unsigned macOS and iOS app
builds. Regression tests cover the -12.0412 dB corner, the cascade half-power
relation, detector phase crossing and Nyquist fallback, envelope timing, the
engine-Nyquist cap, the 12-octave knob span and preserved 0.707 pitch tracking.
Very small cutoffs were checked explicitly; that revision used `std::sin` to
avoid the sine-table accuracy failure that the current lookup implementation fixes.

A serial desktop comparison against `83d2685` used matched physical tunings,
with the new stage-corner parameters prepared outside the timed loop. Six
alternating runs of 1,048,576 trio samples measured median times of 61.431 ms
before and 61.451 ms after for drive-only modulation (+0.03%), and 116.656 ms
before and 125.004 ms after when cutoff, resonance and drive all changed every
sample (+7.16%). The additional coefficient preparation preserves the envelope
timing and small-cutoff accuracy; the sample recurrence is unchanged. These
microbenchmarks are not iPad callback measurements.

## Historical coefficient-cache refactor comparison

The following results describe commit `83d2685` before the stage-corner tuning
change. They are retained as evidence for that refactor, not measurements of
the current cutoff convention.

Comparison against revision `f9e0165` used 300,000 samples across three lanes,
with cutoff modulated from 0.001 to 0.128, resonance from 0 to 1, and drive from
0.25 to 5. Stage alpha was bit-identical. When detector coefficients were held
identical, moving the parameter work out of `Process` was also bit-identical.
Direct detector tuning changes floating-point rounding: the maximum observed
critical-feedback difference was 2.20e-5. With default drive 0.5 the maximum
output difference was 1.29e-6. With the full drive sweep, nonlinear trajectories
diverged: output error RMS was 1.11% of reference RMS, with maximum instantaneous
difference 1.77. Thus the algebraic refactor preserves the tuning convention and
recurrence but does not promise sample-identical high-drive output.

Serial desktop `perf_voices` comparisons used the saved pre-refactor Release
binary and the new Release binary, five alternating runs per configuration,
10 audio seconds per run at 192 kHz. Median CPU milliseconds per audio second
(all cases process three channels):

| Implementation | Controls | Before | After | Change |
| --- | --- | ---: | ---: | ---: |
| Three mono filters | Process only | 19.189 | 14.279 | -25.6% |
| Three mono filters | Constant setters every sample | 19.280 | 14.843 | -23.0% |
| Three mono filters | One channel modulated | 23.698 | 23.541 | -0.7% |
| Three mono filters | All channels modulated | 33.236 | 35.177 | +5.8% |
| One trio filter | Process only | 10.990 | 11.148 | +1.4% |
| One trio filter | Constant setters every sample | 10.955 | 11.176 | +2.0% |
| One trio filter | One channel modulated | 19.584 | 19.948 | +1.9% |
| One trio filter | All channels modulated | 19.756 | 19.938 | +0.9% |

Those workloads keep drive at 0.5. A separate trio test with drive modulated
from 0.25 to 5 measured approximately +0.4% elapsed time with drive alone changing
and -2.9% with cutoff, resonance and drive all changing every sample (median of
six alternating runs, 1,048,576 samples per run). These are desktop microbenchmarks,
not iPad callback timings. They show similar trio cost, not a universal speedup.

Validation: full core suite (including the alternate sample-rate target), all
eight performance tests, visualizer tests, and unsigned macOS and iOS app builds
passed. New regression cases cover coefficient cache invalidation, both setter
orders, history preservation, delayed feedback and the retained half-power
cutoff convention. A pre-existing RMS test that advanced the filter twice per
input sample was corrected to square a single output.

## Related
- [DSP Overview](dsp-overview.md)
- [SVF Filter Machine (2-Pole)](filter-svf.md)
