# Ladder Filter Machine (4-Pole)

The **4-Pole Ladder Filter Machine** is one of the two filter options available per voice in the DSP engine (`FilterSection` in `private/src/SquiggleBoy.hpp`).

## Structure

This filter machine consists of two 4-pole filters in series:
1. **Low-Pass Filter**: A 4-pole transistor ladder filter emulation (`LadderFilterLP`).
2. **High-Pass Filter**: A 4-pole linear State Variable Filter (SVF) configured as a high-pass (`TPTSVF4PoleHighPassFilterGeneric`).

The audio signal from the source machine passes through the low-pass ladder filter first, then the sample rate reducer, and the reduced signal is then processed by the high-pass SVF. The high-pass follows the reducer so it also removes low-frequency content the reducer creates.

## Features

- **Cutoff Tracking**: The HP cutoff frequency tracks the base frequency of the voice's oscillator (`m_vcoBaseFreq`) multiplied by `m_hpCutoffFactor`. In Hz, the LP cutoff is `261.6256 * (vcoHz / 261.6256)^0.707 * hpCutoffFactor * lpCutoffFactor`. LP tracks 0.707 octaves per VCO octave around middle C (261.6256 Hz), while the HP cutoff factor multiplies both cutoffs linearly. At a middle-C VCO frequency, the original LP/HP relationship is preserved for every HP cutoff factor. The `lpCutoffFactor` remains an `ExpParam(0.25, 1024)`; both cutoffs are independently clamped at the base-rate Nyquist frequency.
- **Resonance**: Both filters feature adjustable resonance (`m_lpResonance` and `m_hpResonance`). The generated HP cascade preserves the same natural cutoff and `Q = 0.5 * 11^resonance` in each section. At zero resonance its total cutoff gain is -12.04 dB. The ladder low-pass tuning is unchanged. Its two band-pass envelope followers now use generated SVF sections with Q = 1, retaining unity center gain.
- **Saturation**: The ladder filter includes an internal saturation stage, controlled by `m_saturationGain`, which drives the filter into non-linear distortion.
- **Sample Rate Reduction**: Between the low-pass and high-pass stages, a sample rate reducer (`SampleRateReducer`) adds digital aliasing and grit. Aliases folded below the high-pass cutoff are attenuated by the high-pass.

## Ladder cutoff convention

`SetCutoffAndResonance(cutoff, resonance)` takes cycles per **engine** sample.
Cutoff retains the existing convention: the four-stage **unit-gain linear**
cascade with feedback disabled has magnitude squared 1/2 at that frequency
(-3.0103 dB). Saturation and adaptive feedback change the actual audio response.
The cutoff is capped at 0.499; the owning shape applies its existing base-rate
Nyquist clamp first. The LP knob range and pitch-tracking mapping above are
unchanged.

A conventional analogue Moog ladder instead names the individual stage corner
as cutoff: each stage is about -3 dB there, so the four-stage cascade is about
-12 dB with resonance off. The resonant peak depends on feedback; it is not
generally the same frequency. Our explicit feedback delay further separates the
loop's -180-degree crossing from the cascade's -12 dB point. See
[Huovilainen, DAFx 2004](https://dafx.de/paper-archive/2004/P_061.PDF).

At low frequencies the stage corner is approximately
`1 / sqrt(2^(1/4) - 1) = 2.29896` times our cutoff. That is not a constant
conversion over the full digital range. At sufficiently high alpha the cascade
does not even reach -12 dB before Nyquist, while the loop crossing reaches
Nyquist at alpha = 2/3. Retaining the existing convention avoids losing the
upper knob range or introducing a new pitch-dependent remapping in this refactor.

## Coefficients and sample recurrence

The setter calculates alpha and detector tuning together. With
`k = 2^(-1/4)`, `d = k * (1 - cos(2*pi*cutoff))`, and
`root = sqrt(d * (d + 2*(1-k)))`:

- `alpha = (root - d)/(1-k)` retains the old floating-point calculation.
- `r = (root + d)/(1-k)` is algebraically `alpha/(1-alpha)`, calculated
  directly from cutoff rather than by inverting the rounded alpha.
- For `r < 2`, the unit-gain loop crossing is `asin(r/2)/pi`, and its critical
  feedback is `(r+2)^2`. Otherwise the crossing is Nyquist and the critical
  feedback is `((r+2)/r)^4`.
- Both Q=1 band-pass cascades use that crossing as their center; their generated
  SVFs cap it at 0.499. Their power smoothing retains the original cutoff:
  `powerAlpha = -expm1(-cutoff/2)`.

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

## Refactor comparison

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
