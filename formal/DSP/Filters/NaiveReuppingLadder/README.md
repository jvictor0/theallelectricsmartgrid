# Naive Reupping Ladder implementation

[NaiveReuppingLadder.lean](../NaiveReuppingLadder.lean) models the app's discrete
recurrence. This is a nonlinear state machine, with no transfer-function or
small-signal claim yet. The app still uses the handwritten implementation while
we decide what response the eventual UI snapshot should represent.

## Composition happens in Lean

- [NormalizedTanh](../NormalizedTanh.lean) prepares a per-channel gain and its
  normalization denominator, then applies `tanh(g*x)/tanh(g)`.
- [BandPassedEnvelopeFollower](../BandPassedEnvelopeFollower.lean) contains two
  existing `TPTSVF` states. Tuning calls `TPTSVF.prepareFromSnapshot` with damping
  1 (Q=1); processing calls the existing `TPTSVF4Pole.stepBandPass` cascade,
  which feeds the first SVF bandpass into the second. Squared bandpass output
  enters the shared one-pole `relax` helper.
- The ladder calls `OnePoleZOHLowPass.onePoleStep` for all four audio stages.
  This reuses the existing Lean recurrence and its FMA order; it does not use
  ZOH analog tuning. The stages share an alpha and store four unsaturated outputs.
  Each stage output passes through `NormalizedTanh.apply` before the next stage.

No component calls generated C++ from Lean. The backend unfolds these Lean
compositions into one C++ header per exported component. The power smoother's
unfused difference form and the audio one-poles' explicit FMA are deliberately
preserved. `OnePoleZOHLowPass.relax_eq_onePoleStep` proves their exact-arithmetic
equivalence; it does not assert equal floating-point rounding.

The ladder computes effective feedback from **previous** detector powers and
feeds back the **previous unsaturated fourth-stage output**. It advances the
input detector using the external input and the feedback detector using the
new unsaturated fourth-stage output. Final output is the fourth saturated
output multiplied by `1+kEff`.

## Numeric interpretations

[Nonlinear.lean](../../Nonlinear.lean) defines `Tanh` and `SquareRoot`, including
pointwise channel instances. Over reals, the default implementations are
`Real.tanh` and `Real.sqrt`. `Float` and `Float32` use the clamped cubic rational
approximation for tanh, retaining the app's explicit FMA in the denominator:

```
clamp(x * (x*x + 27) / fma(x*9, x, 27), -1, 1)
```

The same rational definition is also available over reals as `Tanh.rational`.
An approximation-specific analysis can choose `letI : Tanh ℝ := ⟨Tanh.rational⟩`
without changing the ladder. No equality between this rational approximation
and transcendental tanh is assumed. In particular, normalization uses the chosen
interpretation's value at the drive gain, so their cached gain factors differ.

Tuning uses exact sine in the real interpretation, libm in native Lean float
reference execution, and `Math4096::Sin2pi` in generated C++. Square roots lower
to vector `Sqrt` (scalar `std::sqrt`). The binary32 damping epsilon and its
binary32-squared power floor are preserved even in the double implementation,
matching the handwritten constants.

## Generated interface

Exports are `NormalizedTanh[Generic]`, `BandPassedEnvelopeFollowerCore[Generic]`,
and `NaiveReuppingLadderCore[Generic]`. `Core` avoids colliding with the current
app wrappers. Each provides nested `State`, `Input`, and `LogicalSpec`, plus
`Process`, `ComputeCoefficients`, and `Reset`. Initialize with `{}` and prepare
coefficients before processing. Reset clears histories while preserving tuning.

```cpp
using Core = NaiveReuppingLadderCoreGeneric<float, 3>;
using Vector = Core::Vector;
Core filter{};
filter.ComputeCoefficients({Vector::Splat(0.125f),
    Vector::Splat(0.7f), Vector::Splat(2.0f)});
Core::Input input{Vector::Splat(0.1f)};
filter.Process(input);
auto output = filter.m_state.m_output;
filter.Reset();
```

Ladder logical spec fields are `cutoff`, `resonance`, and `gain`. Valid inputs are
positive cutoff in cycles per engine sample, resonance in [0,1], and positive
gain; cutoff is capped at engine Nyquist. There is no zero-gain limiting branch.
The individual-stage half-power convention, 3.9 resonance mapping, detector
phase-crossing tuning and cap, envelope smoothing rate, and feedback guard all
follow the handwritten code. `ComputeCoefficients` updates coefficient fields
in place and preserves histories. It recomputes for each call; an eventual app
wrapper can retain the current unchanged-parameter checks.

There is deliberately no `Snapshot`, `MagnitudeSquared`, or `UIState` yet.
The current handwritten frozen unit-gain response is not automatically a
small-signal response of this nonlinear system.

## Current theorem inventory

[Contracts.lean](Contracts.lean) contains:

| Component | Established contracts |
| --- | --- |
| One-pole | Difference-form smoothing equals the existing one-pole recurrence in exact arithmetic |
| Normalized tanh | Real interpretation; zero maps to zero; one maps to one when the normalization denominator is nonzero; tuning preserves output and is idempotent; step/tuning commute with lane selection |
| Bandpass envelope | Tuning preserves both SVF histories and power; tuning is idempotent; reset clears history, preserves coefficients and is idempotent; step/tuning/reset commute with lane selection |
| Ladder | Feedback reads the old fourth stage and old detector powers; detectors receive the intended current samples; process preserves coefficients; tuning preserves all histories and is idempotent; reset clears history and preserves coefficients; step/tuning/reset commute with lane selection |

Small-signal measurements are defined below. Derivative/transfer correctness for
the ladder, cutoff and nominal oscillation theorems, and stability-region
theorems remain open. The
existing Nyquist feedback guard is retained as implementation behavior; it is
not a proved stability condition for the nonlinear adaptive system.

## Envelope ripple: first analytic contracts

[OnePoleZOHLowPass/Ripple.lean](../OnePoleZOHLowPass/Ripple.lean) proves the
response of the existing `relax` recurrence to a squared sinusoid in exact real
arithmetic. For bandpass output `v[n] = a cos(omega*n + phase)`, smoother
coefficient `0 < alpha <= 1`, and power observed **after** sample n:

```
p[n] = pSteady[n] + (1-alpha)^n * (p[0] - pSteady[0])
pSteady[n] = a^2/2 + ripple[n]
|ripple[n]| <= (a^2/2) * rho(alpha, omega)
rho(alpha, omega)^2 = alpha^2 / (alpha^2 + 4*(1-alpha)*sin(omega)^2)
```

The ripple is a sine/cosine combination at twice the input frequency. The
startup error converges to zero; the ripple persists. For frequencies strictly
between DC and Nyquist, the constant term is the sinusoid's mean power. At the
endpoints, the double-frequency term aliases to DC; the algebraic decomposition
still holds, but those terms cannot separately be interpreted as time means.

[BandPassedEnvelopeFollower/Ripple.lean](../BandPassedEnvelopeFollower/Ripple.lean)
connects this recurrence to `step` and retains both existing SVF bandpasses. If
`B(f)` is their combined transfer and the external sinusoid has amplitude `eps`,
the ideal mean and absolute ripple bound are

```
mean = eps^2/2 * |B(f)|^2
absoluteRippleBound = mean * rho(alpha, 2*pi*f)
```

`prepared_bandPass_realizes` projects the actual tuned detector onto the existing
SVF cascade realization theorem; `bandPassPowerGain_eq_normSq` reuses its
magnitude proof. Both bandpass power gain and absolute ripple bound are proved
zero at DC and Nyquist. A large relative ripple on a heavily attenuated signal
does not by itself imply a large absolute error.

| Result | Proven scope |
| --- | --- |
| `harmonic_step`, `error_eq`, `tendsto_error_zero` | Exact particular solution, geometric startup error, convergence to the rippling response for a sinusoid at the squarer input |
| `steadyPower_deviation_le` | Frequency-dependent ripple bound, including both bandpasses in the detector-level theorem |
| `tracking_bound`, `tendsto_tracking` | Uniform forcing errors are not amplified by the smoother; vanishing forcing errors give vanishing state errors |
| `powerOutput_error_eq`, `powerOutput_tendsto_ripple` | Apply those results to the actual detector; the latter assumes its squared bandpass waveform approaches the specified squared sinusoid |
| `prepared_bandPass_realizes` | Actual tuned bandpasses realize the existing two-SVF transfer from rest |
| `modulation_components` | Multiplying a sinusoidal signal by a sinusoidal coefficient modulation produces sum/difference frequencies |

[TPTSVF/Convergence.lean](../TPTSVF/Convergence.lean) and
[BandPassedEnvelopeFollower/Convergence.lean](../BandPassedEnvelopeFollower/Convergence.lean)
now close the detector's transfer-to-trajectory gap. For Q=1, the actual SVF
histories have the complex coordinate

```
w = ic1 + ic2/2 + i*sqrt(3)*ic2/2
wNext = pole(g)*w + drive(g)*input
|pole(g)|^2 = (1-g+g^2)/(1+g+g^2) < 1, for g > 0
```

This is a change of coordinates proved from the existing step, not a duplicate
filter. A shared contraction lemma in [Asymptotic.lean](../../Analysis/Asymptotic.lean)
proves that a vanishing input disturbance gives a vanishing state disturbance.
The result composes across the two SVFs, then through squaring and the existing
power smoother. Arbitrary initial histories are allowed. The settled bandpass
amplitude and phase are proved equal to the existing snapshot transfer.

| Result | Proven scope |
| --- | --- |
| `TPTSVF.Convergence.bandPass_tendsto` | One actual Q=1 SVF approaches its sinusoidal response when its input approaches a sinusoid; g > 0 |
| `prepared_bandPass_tendsto` | Both detector SVFs converge, including arbitrary initial histories |
| `convergence_cascade_eq_snapshot` | The time-domain sinusoidal gain equals the existing two-SVF snapshot transfer |
| `prepared_power_tendsto_snapshot` | Complete detector converges to the explicit persistent power ripple; g > 0 and 0 < alpha <= 1 |
| `prepared_power_eventually_positive` | Nonzero passed tone, sin(omega) != 0 and 0 < alpha < 1 give an eventually positive lower bound for detector power |
| `prepared_bandPass_smallSignal` | Both nested coherent-response limits exist for the detector's linear bandpass output, away from DC/Nyquist |

The input detector can use the external sinusoid directly. Applying the same
theorem to the feedback detector requires proving suitable behavior of the
ladder's fourth-stage signal. That is a closed-loop obligation, not a premise
we have discharged by proving the detector alone.

The next ladder estimate must weight detector error by the modulation's effect
through the feedback loop. Tight bounds matter near the resonant peak; away from
it, lower amplification can help. Generated frequencies must also be checked:
a 2*f coefficient modulation on an f signal produces f and 3*f (modulo digital
aliasing), and the latter could approach resonance. The reciprocal feedback law
can generate additional harmonics. Consequently a small power ripple alone is
not a proved small output sideband. Normalizing feedback power by input power
can also cancel their common bandpass attenuation for a single tone.

This work changes proofs only. The fixed detector power floor and all runtime
processing, tuning, generated code, and app behavior remain unchanged.

## Floor-free controller analysis

The selected analysis model removes the power floor; the runtime does not.
[FloorFree.lean](FloorFree.lean) defines only the proof-side controller:

```
x = feedbackPower/inputPower
kappa(x) = K / (1 + d*max(0, x-R))
```

Here K is base feedback, d is dampingScale, and R is referencePowerGain.
For positive input power this equals the original excess-power expression with
its floor removed. Results interpreting the quotient require positive input
power. These quotient identities alone do not specify a startup or silence
policy. The eventual-positive detector theorem helps after startup,
but its lower bound depends on stimulus amplitude and is not uniform as that
amplitude goes to zero.

| Result | Proven scope |
| --- | --- |
| `fromPowers_scaled` | A common nonzero amplitude scaling cancels from the controller's power ratio |
| `feedback_bounds` | 0 <= kappa <= K when K,d >= 0 |
| `feedback_lipschitz` | `abs(kappa(x)-kappa(y)) <= K*d*abs(x-y)` |
| `ratio_ripple_bound`, `fromPowers_ripple_bound` | Relative ripple in both detector powers bounds the ratio and controller ripple, with no phase-alignment assumption |
| `feedback_tendsto` | If the power ratio converges to a constant, its controller coefficient converges; the premise is explicit |
| `below_critical_iff` | Exact scalar inequality for frozen `kappa*slope^3 < critical`; no time-varying stability claim |

[FloorFreeDynamics.lean](FloorFreeDynamics.lean) supplies a separate exact-real
ladder trajectory for this study. Its `advance` calls the existing one-poles,
normalized tanh and detectors. `runtime_step_eq_advance` proves definitionally
that it matches all production sample operations when given the same feedback
coefficient. The only change in `FloorFree.step` is how that coefficient is
chosen; both previous-power reads and the old fourth-stage feedback are kept.
At exactly zero input detector power, this analysis model uses base feedback K.
That convention is explicit in `effectiveFeedback_zero_power`, allowing reset
to start the model; it is not a new runtime silence policy. For positive input
power, `effectiveFeedback_eq_fromPowers` connects the trajectory to the ratio
theorems above. `inputPowerOutput_eq` identifies its input detector with the
existing detector trajectory, so the detector convergence results apply
independently of what the adaptive loop does. No such decoupling is claimed for
the feedback detector.

For positive input mean I, nonnegative feedback mean F, and relative ripple at
most rho < 1 in each power, the bound is

```
|feedbackPower/inputPower - F/I| <= (F/I)*2*rho/(1-rho)
|kappa(feedbackPower/inputPower) - kappa(F/I)|
    <= K*d*(F/I)*2*rho/(1-rho)
```

For single tones at the same frequency, the detectors' common bandpass power
gain cancels in F/I. Bandpass attenuation alone therefore does not make this
floor-free controller modulation small. This bound concerns the coefficient,
not the output sidebands; it can be loose when rho approaches one. It also does
not justify replacing the rippling coefficient by `kappa(F/I)` in the ladder.

## Long-time response and the small-signal limit

[ResponseMeasurement.lean](../../ResponseMeasurement.lean) defines two separate
measurements for real stimulus `eps*cos(omega*n)`, eps > 0:

```
coherent(eps,N) = 2/(eps*N) * sum[n < N] output_eps[n]*exp(-i*omega*n)
powerGain(eps,N) = 2/(eps^2*N) * sum[n < N] output_eps[n]^2
```

The proposed response takes N to infinity first, then eps to zero from above.
`HasSmallSignalResponse` and `HasSmallSignalPowerGain` make both existence
requirements explicit; inner limits are required for sufficiently small
positive amplitudes, not arbitrarily large stimuli. RMS gain is the square root of power gain. Asymptotic
RMS is a seminorm on signals with suitable averages (a norm only after
identifying signals that differ by zero asymptotic energy). Coherent measurement
is a complex Fourier projection, not a norm of the full signal.

The proofs establish that decaying startup errors disappear from coherent
measurement and that a sinusoid converges to its expected complex amplitude.
Distinct unit-frequency modes average to zero, with a finite-window bound
`2/(N*|z-1|)` after demodulation. The detector bandpasses provide a complete
calibration: their nested coherent limit equals their existing transfer. The
RMS/power measurements have exact finite-window scaling theorems, but no
analogous sinusoidal calibration theorem yet.

Projection can reject other output frequencies, but it cannot remove their
effect inside the feedback loop or remove a contribution they generate at the
stimulus frequency. RMS retains their energy. An autonomous oscillation could
therefore be hidden by coherent projection while making normalized RMS
unbounded; both measurements matter.

**Still unproved:** convergence of the coupled floor-free ladder and controller,
a bound on the output sidebands, and existence of the ladder's outer
small-amplitude limit. The defined trajectory needs a closed-loop
attracting-response argument and amplitude-uniform estimates. A
constant-power-ratio hypothesis or a frozen-k transfer calculation would assume
away the difficult part.
Scale invariance of the controller alone does not prove linearity, nor does
convergence at each fixed amplitude prove the outer limit exists.

## Exact amplitude normalization

[FloorFreeScaling.lean](FloorFreeScaling.lean) proves scaling of the complete
state: audio and SVF histories scale by a, detector powers by a^2, and the
controller coefficient does not change. The proofs call the existing SVF and
one-pole steps. `SlopeModel` replaces each normalized tanh by `slopeZero*x`
while retaining the complete adaptive controller and both envelope detectors.
From reset, this model's output obeys `output(a*u) = a*output(u)` at every
sample for a != 0. Relative harmonics, modulation and oscillations therefore do
not disappear merely by lowering the stimulus amplitude in this model.

[FloorFreeNormalization.lean](FloorFreeNormalization.lean) connects this study
to the actual real-tanh equations without assuming a small-signal transfer:

```
S_a(x) = tanh(g*a*x)/(a*tanh(g))           for a != 0
S_0(x) = (g/tanh(g))*x
output_tanh(a*u)[n]/a = output_with_S_a(u)[n]
```

The output equality is exact at every finite sample, including previous-power
reads and the one-sample feedback delay. The normalized saturator converges to
S_0 jointly when a -> 0 and its input x also approaches a finite value. This
does not by itself establish convergence of the whole trajectory or exchange
the time and amplitude limits. In particular, the limit retains the adaptive
controller; it does not produce a constant-k LTI system.

| Result | Proven scope |
| --- | --- |
| `svf_step_scale`, `envelope_step_scale` | Existing SVFs scale linearly; complete detector powers scale quadratically |
| `effectiveFeedback_scale` | Exact invariance of floor-free feedback under nonzero amplitude scaling, including the zero-power convention |
| `SlopeModel.output_scale_from_reset` | Exact homogeneity of the complete adaptive slope model |
| `NormalizedTanh.hasDerivAt_zero` | The real normalized tanh has derivative `gain/tanhGain` at zero |
| `NormalizedTanh.normalized_tendsto` | Joint convergence of the normalized saturator to that slope |
| `NormalizedModel.actual_output_normalized` | Exact full-trajectory amplitude normalization from reset |
| `not_continuousAt_silence` | For K,d > 0 and R >= 0, the power-ratio controller has different limits along different power ratios approaching silence |
| `normalizedCoherent_scale`, `normalizedRms_scale` | Scaling an entire waveform leaves its normalized finite-window measurements unchanged |

The discontinuity result concerns the controller itself; it does not rule out
a directional sinusoidal response of the whole system. It prevents taking the
controller value at silence as its unique small-amplitude coefficient.

To complete the proposed response theorem, we still need long-time coherent
and power averages for the unit-driven normalized model at sufficiently small
a, and convergence of those averages as a -> 0. Waveform periodicity would be
one sufficient route but is not required by the chosen measurements. A proof
only about each finite sample, or about a frozen average coefficient, does not
establish either long-time claim.

## Floor-free numerical probe

`make lean-dsp-floor-free-probe` runs
[FloorFreeProbe.lean](../../Tests/FloorFreeProbe.lean) in Lean `Float`, which is
IEEE binary64 (C++ double). It uses native `tanh`, matching the transcendental
proof interpretation up to floating-point error, and separately uses its
zero-input slope. Tuning and all sample operations reuse the existing Lean
implementation; only the chosen feedback coefficient is replaced. The app's
rational tanh and its fixed power floor are not the model in this probe.
Optional modes below also replace the detector measurement and smoothing rate.

For cutoff 0.01 cycles/engine sample, resonance 0.8, gain 2 and stimulus period
96 samples, one recorded window after 245,760 startup samples gave:

| Input peak amplitude | Fundamental gain | Total RMS gain | Absolute output RMS |
| --- | --- | --- | --- |
| 1e-5 | 135.092 | 306.304 | 0.00216590 |
| 1e-7 | 135.101 | 306.313 | 0.0000216596 |

These are the same window index (5), each covering 49,152 samples. Decreasing
the input by 100 decreases the absolute output by approximately 100; the
normalized gain stays nearly unchanged. The 306-versus-135 comparison is total
RMS versus fundamental gain **at the same amplitude**, not a gain increase
caused by decreasing the input. The slope model closely matches the 1e-7 case.

The late windows retain a large difference between output and its one-period
delayed copy. This suggests substantial motion beyond a periodic sinusoidal
response; it is not proof of the existence or failure of infinite-time
averages. A low-drive control case (gain 0.1) has matching fundamental/RMS gain
3.485118 and a period-repeat error below the printed resolution. Other probe
frequencies use periods 384 and 32. These experiments neither prove convergence
nor establish a floating-point error bound. Raw local output is retained under
`build/results/naive-reupping-ladder-convergence/`.

## Detector and periodic-response experiments

The experimental [QuadratureEnvelopeFollower](../QuadratureEnvelopeFollower.lean)
reuses the existing Q=1 `TPTSVF.step` and `OnePoleZOHLowPass.relax`. It measures
`(bandPass² + lowPass²)/2` before smoothing. At the SVF center the two outputs
approach equal-amplitude quadratures, so the sum is constant. This is proved
from the actual state recurrence and arbitrary initial histories, not assumed
from an ideal quadrature source. The exact digital center satisfies
`(1 - i*g)*z = 1 + i*g`, where g is the prepared SVF integrator gain and
`z = exp(i*omega)` advances one sample. The explicit carrier `(1+i*g)/(1-i*g)`
satisfies that condition and has unit norm.

This measurement changes the detector's frequency weighting. In particular,
the single-SVF version includes a lowpass path and responds to DC. The
cascaded experiment retains an upstream bandpass and hence DC rejection, but
has different selectivity from the original two-bandpass power detector.
Neither version is generated into C++ or installed in the runtime ladder.

Away from center, an ideal quadrature pair with amplitudes a and b has energy

```
((a*cos(theta))² + (b*sin(theta))²)/2
  = (a²+b²)/4 + (a²-b²)/4 * cos(2*theta).
```

Equal amplitudes cancel the twice-frequency term exactly. Unequal amplitudes
leave ripple. This identity does not say the closed-loop output is a single
tone: other components can still move the measurement and the controller.

The binary64 sweeps keep cutoff 0.01 and resonance 0.8. The following are late
windows at gain 2 and input period 96, using the amplitude-normalized slope
model (which retains the adaptive controller):

| Detector | Smoothing alpha multiplier | Observed k range | Output period-repeat RMS error |
| --- | --- | --- | --- |
| Original two-bandpass power | 1 | 0.185385–0.828097 | 180.454481 |
| Original two-bandpass power | 1/64 | 0.455201–0.484505 | 13.323198 |
| Single-SVF quadrature | 64 | 0.473223–0.479265 | below six-decimal print resolution |

The base alpha is about 0.002174; multipliers are capped at alpha=1. The first
and third rows use window 7 (512 periods per window), while the slower second
row uses window 15 to allow more startup time. The repeat error is the RMS of
`output[n] - output[n-period]`, divided by stimulus peak amplitude. Slower
smoothing reduces the motion but did not establish period locking. The
single-SVF version with faster smoothing looks much better near the tested
peak, with fundamental `-205.573339 + 126.857102i`, total RMS gain 241.564036,
and third-harmonic amplitude 0.247516. Native tanh at input amplitude 1e-7
matches this slope-model case at the printed precision.

The broader checks prevent a general convergence claim. At period 32 the
single-SVF variant still has substantial period-repeat error (75.654709).
At period 384 the output repeats to printed resolution but its total RMS gain
is 206.656551 while its fundamental gain is only about 0.810: periodicity alone
does not mean a nearly sinusoidal response. Gain 1 at period 96 also retains
period-repeat error. The cascaded quadrature experiment did not remove the
large motion in the tested near-peak case. No runtime detector or rate has
been selected from these results.

Reproduce with `make lean-dsp-floor-free-probe FLOOR_FREE_PROBE_ARGS=smoothing`,
`quadrature`, or `quadrature-check`. The diagnostic still advances both original
SVFs even when only one is observed; it is not a performance implementation.
It prints k's mean/range/standard deviation, its 2ω component, and the coherent
mixing correction `coherent(k*delayedStage4) - mean(k)*coherent(delayedStage4)`.
The last quantity is about `-0.082166 - 0.086509i` in the period-96, alpha×64
example, even though the waveform appears periodic.

### What averaging proves

Write `signal[n] = Re(A*z^n)` and a coefficient wobble
`k[n] = center + Re(C*z^(2*n))`. The product's coherent fundamental is

```
center*A + C*conj(A)/2,
```

with the usual DC/Nyquist and third-harmonic alias exclusions. There is also a
third-harmonic component `C*A/2`. Thus a zero-mean coefficient wobble generally
survives coherent integration through its correlation with the signal. We
cannot replace k by its mean to obtain an exact transfer function.

If the actual output approaches a periodic waveform, its long-time coherent
response and RMS can instead be computed from that whole waveform over one
period. The proof handles all observation-window lengths, not just exact
multiples of the period, and discards decaying startup errors. The carrier must
repeat over the chosen period for the coherent theorem; the period can include
subharmonics. This is a sufficient route for rational digital frequencies,
not a theorem that every driven nonlinear system becomes periodic.

| New theorem | Proven scope |
| --- | --- |
| `TPTSVF.Quadrature.outputs_tendsto` | Actual prepared Q=1 SVF BP/LP approach equal-amplitude quadratures at center, with arbitrary histories and input tending to a tone |
| `QuadratureEnvelopeFollower.unequal_quadrature_energy` | Exact mean plus twice-frequency ripple for unequal ideal quadratures |
| `QuadratureEnvelopeFollower.prepared_energy_tendsto` | Actual squared-norm measurement approaches `norm(A)²/2` at center |
| `QuadratureEnvelopeFollower.prepared_power_tendsto` | Actual smoothed detector approaches the same constant for alpha in (0,1], from arbitrary histories |
| `ResponseMeasurement.coherent_feedback_split` | Exact finite-window center/wobble split; no independence assumption |
| `ResponseMeasurement.coherent_modulated_tone` | The wobble's fundamental correction survives infinite-time averaging |
| `ResponseMeasurement.average_of_periodic` | All-window average of a periodic sequence equals its one-period average |
| `ResponseMeasurement.coherent_of_periodic_tracking` | Output tracking a periodic waveform has its one-period coherent response |
| `ResponseMeasurement.meanSquare_of_periodic_tracking`, `rms_of_periodic_tracking` | Companion energy/RMS limit retains all harmonics of that waveform |

The remaining obligations are to prove settling or existence of long-time
averages for the **coupled ladder/controller**, and their small-amplitude limit.
The driven-detector theorem assumes a tone-approaching input; that hypothesis
is not yet proved for the ladder's feedback signal. Numerical repetition and
these conditional averaging theorems do not discharge either obligation.

## Validation boundaries

Native Lean processing trajectories are compared bit for bit with scalar and
multichannel generated C++ at both precisions and widths 1–4, checking all state
fields. Three-lane padding is poisoned. A separate core regression compares
generated tuning, histories, reset and processing directly against the existing
handwritten ladder across modulation, saturation gains, both detector regimes,
Nyquist clamping, and multiple resets. Standalone component tests cover tanh
clamp boundaries and scalar/vector agreement.

These tests validate the lowering and implementation; they do not prove
floating-point accuracy or turn the backend into a verified compiler.

## Desktop performance

A serial comparison of implementation commit `2fa2db5` used one float trio on
an arm64 Mac, AppleClang 17.0.0.17000013, Release `-O3` with LTO,
`-fno-fast-math -ffp-contract=off -funroll-loops`. Each implementation ran nine
times per workload, with alternating order, 32,768 warmup samples and 4,194,304
timed engine samples per run. All audio output hashes agreed.

Median calling-thread CPU times, in **nanoseconds per three-channel engine
sample** (coefficient-only row: nanoseconds per three-channel preparation):

| Workload | Handwritten | Generated | Generated change |
| --- | ---: | ---: | ---: |
| Processing; coefficients already prepared | 58.19 | 57.52 | −1.2%; effectively equal |
| Unchanged settings submitted each sample, plus processing | 58.03 | 66.13 | +14.0% |
| Drive changes each sample, plus processing | 58.18 | 65.92 | +13.3% |
| Cutoff, resonance and drive change each sample, plus processing | 74.47 | 65.93 | −11.5% |
| All coefficients change; preparation only | 23.50 | 18.37 | −21.8% |

The sample recurrence has comparable cost. The current API explains the other
differences: the handwritten setters skip unchanged settings and can refresh
drive-dependent terms alone. The generated `ComputeCoefficients` always prepares
everything, but does the combined update once; the handwritten two-setter path
refreshes feedback terms after both changes. App integration should retain
unchanged-parameter caching and expose a drive-only preparation path. These
results do not measure iPad callback deadlines or justify a processing-speed
claim for the roughly 1% difference.

Reproduce with `make perf-ladder PERF_ARGS="9 4194304"`; the harness is
[ladder.cpp](../../../../private/test/perf/ladder.cpp). Raw local runs and summaries
are in `build/results/naive-reupping-ladder-performance/`.
