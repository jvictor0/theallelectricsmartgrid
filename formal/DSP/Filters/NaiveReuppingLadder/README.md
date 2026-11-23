# Naive Reupping Ladder implementation

[NaiveReuppingLadder.lean](../NaiveReuppingLadder.lean) models the app's discrete
recurrence. This is a nonlinear state machine, with no transfer-function or
small-signal claim yet. The app still uses the handwritten implementation while
we decide what response the eventual UI snapshot should represent.

The detector-enabled analysis below uses the same delayed fourth stage, two
power detectors, and finite power floor as the sample step. The later sections
record earlier work on floor-free feedback and detector-disabled harmonic
balance. The proof uses real arithmetic and normalized real tanh. The app
evaluates its saturator and coefficient math with finite-precision approximations.

## Periodic response with the production feedback law

Fix the sample period $N>0$ of the stimulus. A real sinusoid can be written
as $u_n=\operatorname{Re}(Az^n)$ with $z^N=1$. Its amplitude and phase are free.
The theorems also accept arbitrary real $N$-sample repeating inputs. A buffer
may contain several sine cycles. Uniqueness is for the specified input phase
and period.

The unknown in the solver is one period of the **unsaturated fourth stage before
each sample**. Given a candidate trace, the dry detector sees the known input
and the feedback detector sees the candidate shifted forward one sample. Each
bandpass in modal coordinates, and then the power smoother, has a forced
recurrence of the form $v_{n+1}=b v_n+u_n$. Its exact periodic initial memory
is the one-period result from zero divided by $1-b^N$. This accounts for the
full power ripple. The controller reads both old powers. Solving the four
audio one-poles in order produces a new candidate. Every fixed point yields
an actual repeating ladder history, and every repeating history gives a fixed
point. [Detector initialization](../BandPassedEnvelopeFollower/Periodic.lean)
and [waveform-to-history equivalence](PeriodicWaveUniqueness.lean).

### A guaranteed resonance range

Let $K$ be prepared base feedback, $D$ the damping scale, $F$ the fixed power
floor, $S=g/\tanh(g)$ the maximum normalized-saturator slope at positive drive
$g$, and $M=1/\tanh(g)$. Let $p_{\min}$ be any nonnegative lower bound on the
dry periodic power. Zero always works. The proof uses a bound $T$ on the peak
sample gain of **one** detector bandpass. If its modal recurrence has pole
$\lambda$ and input coefficient $h$, then $T=2|h|/(1-|\lambda|)$. Both coefficients
come from the prepared Q=1 SVF. A sufficient condition is

$$
L = K S^3\left(1+\frac{2 D T^4 M^2}{p_{\min}+F}\right)<1.
$$

Two cascaded bandpasses contribute $T^2$. Squaring their output gives the
power difference bound $2T^4M$ times the change in fourth-stage amplitude.
The formula includes the feedback change caused by that power error. It
bounds worst-case magnitudes over a whole period and can be very conservative.

For every positive cutoff and drive, the coefficient setter gives a strictly
positive interval of normalized resonance. With $p_{\min}=0$, one can take the
reciprocal of $3.9 S^3(1+2DT^4M^2/F)$ as its upper endpoint because
$K=\min(3.9\,\mathrm{resonance},k_{\mathrm{safe}})$. The interval is uniform in
amplitude, waveform and period. In it a unique repeating history exists.
The waveform solver converges geometrically with factor at most $L$ per
iteration. [Prepared knob range](PreparedPeriodic.lean) and [solver error
bound](PeriodicWaveOrbit.lean).

In the same range, the actual sample stream approaches the periodic output
from every prepared history with nonnegative dry power, including reset. The
fourth-stage error is initially bounded in the long run because the final
one-pole is driven by a bounded saturator. Tracing any such error bound through
the wet detector, the feedback multiplication and all four stages improves
it by a factor of $L$. Iterating that inequality forces the output error to
zero in audio time. The envelope followers may keep rippling. Both long-run
measurements are therefore determined by one period of the computed waveform.
[Sample tracking and measurement from arbitrary history](PeriodicWaveTracking.lean).

### Testing a region at stronger feedback

For a proposed waveform, Lean bounds the wet power over every waveform within
a specified peak-error radius. Subtracting that allowance from the candidate
power gives a certified lower bound $d$ on the controller denominator at each
phase. Actual feedback is then at most $K/d$, and its sensitivity to wet power
is at most $KD/((p_{\min}+F)d^2)$. A small return residual and the resulting
waveform gain below one certify a nearby repeating response, uniqueness within
the radius, and a distance bound. Each premise can be bounded uniformly over
knob intervals. [Waveform region test](PeriodicWaveRegion.lean).

The stock-damping, cold-reset recordings for the four previously repeating
cases A–D illustrate the limitation. Multiplying their recorded mean effective
feedback by the ideal maximum slope cubed gives about 5.6–60.8. The maximum
over sample phases is at least that mean. A test that uses the maximum tanh
slope at all three stages therefore already exceeds one before including detector
sensitivity. These numbers use the earlier production-processor recordings and
the exact-tanh slope. They are a diagnostic for choosing a proof strategy.
A sharper test would propagate the signed differences through one full input
period with slopes bounded at the actual stage amplitudes.

For attraction in audio time at stronger feedback, the one-period return of
the four audio stages and five wet-detector memories can be checked in rescaled
coordinates. A strict residual margin and a uniform contraction in those nine
directions give local uniqueness and attraction. The dry detector converges
from any initial history. A cold start, or a competing starting basin, is
covered if its wet state enters the certified ball once the dry transient is
sufficiently small. Proving the return bound and entry for the high-drive
listening cases remains open. For an attracted trajectory, one period of the
reference output gives its exact long-run coherent fundamental and mean-square
output power. [Physical return and measurement theorems](DetectorAttraction.lean).

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

## Exact harmonic balance with parameterized amplitude

[HarmonicBalance.lean](HarmonicBalance.lean) proves the full Fourier equations
for the **detector-disabled** real ladder (`dampingScale = 0`). This keeps the
four Euler stages, normalized tanh between stages, previous fourth-stage
feedback, and final saturation/compensation exactly as implemented in `step`.
No generated code or app behavior changes here.

A waveform is a continuous function of one or more phases, each measured in
cycles. One phase covers a locked waveform; two phases can describe an input
tone and an independent oscillation. The phase increment per sample is `omega`.
Every integer multi-index `k` is retained, including mixed modes, negative
frequencies, DC, and even harmonics. Existence of such a waveform is not assumed
to follow from the filter settings, and chaotic responses need not admit this
representation.

Write `Xj[k]` for stage j's Fourier coefficient, `U[k]` for the input, and
`N(Xj)[k]` for the Fourier coefficient of the **complete** saturated waveform.
With prepared saturation coefficients this is `tanh(g*xj)/tanh(g)`; the theorem
uses the state's actual cached gain and normalization denominator. Let

```
d[k] = exp(-2*pi*i*(k dot omega))
D[k] = 1 - (1-alpha)*d[k]
L[k] = alpha/D[k]
```

Then the exact equations are

```
X1[k] = L[k] * (U[k] - K*d[k]*X4[k])
X2[k] = L[k] * N(X1)[k]
X3[k] = L[k] * N(X2)[k]
X4[k] = L[k] * N(X3)[k]
Y[k]  = (1+K) * N(X4)[k]
```

The undivided equations `D[k]*Xj[k] = ...` need no coefficient range
assumption. The divided form above is proved valid for `0 < alpha <= 1`.
The feedback delay is the additional `d[k]` in the first equation; there is no
extra delay between each current stage and the next stage's saturator.

The shared [Harmonics](../../Analysis/Harmonics.lean) library defines a stimulus
`A*cos(2*pi*(m dot theta))` with **arbitrary real amplitude A** and proves its
exact coefficients:

```
U[k] = (A/2) * (indicator(k=m) + indicator(k=-m))
```

Initial phase gives the corresponding sine stimulus. The carrier index `m`
may be 3, 5, or 7 in a one-phase description: the input can complete several
cycles during one cycle of the locked response. With two phases, a carrier
index `(1,0)` selects the external input independently of the oscillator phase.
The earlier amplitude 0.25 experiments correspond to approximately -12.04 dBFS
under the peak-amplitude convention; exact -12 dBFS would use `10^(-12/20)`.
The proofs hardcode neither choice.

| Theorem | Contract |
| --- | --- |
| `Harmonics.coeff_injective` | All Fourier coefficients uniquely determine a continuous phase waveform |
| `Harmonics.coeff_delay` | A one-sample delay multiplies each mode by its exact phase factor |
| `Harmonics.stimulus_coeff`, `stimulus_sample` | Arbitrary-amplitude stimulus spectrum and its sampled carrier |
| `Harmonics.synthesize_coeff`, `saturated_from_spectrum` | Absolutely summable coefficients reconstruct the full signal before tanh is applied; saturation is not applied independently to each coefficient |
| `equations_iff_balance` | Full harmonic balance is equivalent to the nonlinear waveform equations |
| `balance_iff_step` | Those equations are equivalent to the actual `step` audio update when the detector is disabled |
| `balance_iff_response` | The familiar per-harmonic one-pole form has a nonzero denominator in the stated alpha range |
| `output_coeff` | The output spectrum includes the fourth saturation and `(1+K)` compensation |
| `balance_trajectory`, `balance_trajectory_output` | Starting on a candidate waveform makes the existing runtime recurrence follow it exactly; detector histories remain free to evolve |
| `Harmonics.phasePower_eq_spectrum` | Parseval's identity for the average over all phases |
| `Harmonics.residual_power`, `omitted_residual_energy` | All coefficient residuals account for the complete squared waveform residual, including modes omitted by a finite solver |
| `Harmonics.coeff_error_le` | A uniform waveform error bounds each coefficient error |

### What this does and does not establish

This establishes the exact equations from which a reduced harmonic-balance or
amplitude/phase model can be derived. It does **not** assert that a nonlinear
root exists, that it is attracting, that reset reaches its basin, or that
retaining a few harmonics is accurate. The trajectory theorem starts **on** the
candidate waveform. A small residual requires a conditioning/stability bound
before it implies a small waveform error.

`phasePower` is explicitly an average over all phases, not automatically the
long-time power of a sampled orbit. Sampling may make distinct multi-indices
share a digital frequency. Time averages and coherent response must respect
those coincidences; they must not silently discard their cross terms. The
existing periodic measurement theorems can be used when a sampled trajectory
is periodic or is proved to approach a periodic trajectory.

The current filter specialization uses exact real tanh, as the other nonlinear
proofs do. This is not a proof that the runtime rational saturator, native
floating-point reference, or Math4096 coefficient approximations are equal to
that model. The reusable Fourier lemmas themselves are independent of the
choice of saturator.

## Fundamental plus third, and a locking-width experiment

[FundamentalThird.lean](HarmonicBalance/FundamentalThird.lean) specializes the
full equations to four stage waveforms of the form

```
xj(theta) = Re(zj1 * exp(i*theta) + zj3 * exp(3*i*theta))
```

Here the `z` values are **peak phasors**, twice the positive Fourier
coefficients. Theta in this display is radians; the Lean phase space uses
cycles. Four stages times two complex phasors gives sixteen real unknowns at a
fixed frequency. For an autonomous oscillation the numerical solver also
solves for frequency and fixes a phase gauge, giving seventeen real unknowns
and equations. The autonomous zero solution is excluded by the nonzero
amplitude seed; finding this branch numerically is not an existence proof.

For each retained harmonic `h = 1, 3`, the projected equations are

```
D[h]*z1h = alpha*(uh - K*d[h]*z4h)
D[h]*z2h = alpha*Nh(z11,z13)
D[h]*z3h = alpha*Nh(z21,z23)
D[h]*z4h = alpha*Nh(z31,z33)
```

`Nh` is **twice the Fourier coefficient of the full normalized tanh applied to
the reconstructed waveform**. No cubic Taylor expansion replaces tanh. The
output also applies the full saturator and retains all its generated
harmonics; it is not limited to harmonics 1 and 3. An input `A*cos(m*theta)` has
`uh=A` when `h=m` and zero otherwise, for positive carrier `m`. A forced
fifth- or seventh-harmonic input needs those modes in a direct waveform solve;
discarding its input mode does not model that forcing. The high-side experiment
below instead treats forcing as a phase perturbation of an autonomous orbit.

| New theorem | Contract |
| --- | --- |
| `Harmonics.coeff_realMode`, `coeff_conj` | Peak-phasor normalization and the conjugate negative modes of a real waveform |
| `project_coeff`, `project_idempotent` | The H1+H3 projection preserves exactly its four retained Fourier coefficients and is a projection |
| `projection_residual_energy` | Exact phase-averaged power in the discarded waveform modes |
| `residual_coeff` | A stage's waveform defect transforms to its exact harmonic-balance defect |
| `projected_iff_residual`, `projected_tail` | Enforcing the two positive complex equations kills all four retained residual modes; the remaining defect energy is exactly the omitted tail |
| `full_implies_reduced`, `projected_phasors` | The finite equations come from the full recurrence and have the peak-phasor form above |
| `Harmonics.realMode_correlation` | Averaging a real sensitivity against a sinusoid extracts its corresponding complex Fourier coefficient, including phase |
| `PhaseLocking.invariant_phase_adjoint` | Differentiating a supplied smooth local invariant phase gives the discrete adjoint equation |
| `PhaseLocking.phase_kick_derivative` | Given that phase derivative, a sample's input perturbation changes phase to first order by `Z(next)*B` |
| `PhaseLocking.equilibrium_iff`, `harmonic_equilibrium_iff` | Exact fixed-point existence criterion **within the scalar averaged phase model**, with input amplitude left as a parameter |

### From an orbit to a locking width

The numerical reduction first solves the autonomous harmonic-balance equations.
It then differentiates the actual four-stage discrete map, including the
feedback delay, and solves the periodic adjoint equations

```
Z(theta) = J(theta)^T Z(theta+Omega)
Z(theta) dot q'(theta) = 1
Gamma(theta) = Z(theta+Omega) dot B(theta)
```

`J` is the state Jacobian, `B` the derivative with respect to the input sample,
and `Gamma` the phase kick per unit input. The supplied-phase theorems justify
these derivative identities conditionally. They do not construct an
asymptotic phase or establish that the numerical orbit is attracting.
The numerical adjoint uses least singular vectors of a Fourier discretization;
its defect and variation in `Z dot q'` are recorded, not silently treated as zero.

Near input angular frequency `omega = m*Omega`, sinusoidal averaging gives a
phase mismatch equation of the form

```
Delta psi = m*Omega - omega + m*A*abs(Gamma_m)*sin(psi + offset)
```

The correlation identity is exact as a phase integral. Replacing the actual
sampled phase dynamics by that averaged equation is an approximation, still
requiring an averaging remainder bound. `Gamma_m` here is the complex Fourier
coefficient, **not** its doubled peak phasor. For nonnegative amplitude the
predicted input-frequency half-width is

```
W_m = sampleRate/(2*pi) * m * A * abs(Gamma_m)
```

Fixed points of this scalar equation exist exactly when
`abs(m*Omega-omega) <= m*abs(A)*abs(Gamma_m)`. The equality case need not be
attracting. This criterion alone does not prove that the original ladder
locks, that it reaches this orbit from reset, or that its output amplitude
matches the autonomous waveform under finite forcing.

The method of computing phase/amplitude responses from harmonic balance also
appears in [Nicks, Allen and Coombes, Physical Review E 110, L012202
(2024)](https://doi.org/10.1103/PhysRevE.110.L012202). That paper treats delay
differential equations; the formulas here use our discrete sample map instead.

### Numerical findings, not proof certificates

The reproducible probe is
[analyze_ladder_harmonic_reduction.py](../../../../scripts/analyze_ladder_harmonic_reduction.py).
It tests exact tanh and the clamped rational approximation separately, in
binary64. Settings are 192 kHz internal sample rate, 1 kHz cutoff, detector
disabled, and three drive/resonance pairs: `(100%,75%)`, `(75%,75%)`,
`(50%,100%)`. The feedback guard is inactive at these settings. Jacobians are
checked independently by central finite differences. Autonomous roots start
from first-harmonic balance, not from measured oscillation frequencies.

At maximum drive and 75% resonance:

| Model / retained stage harmonics | Oscillation Hz | Largest normalized stage defect | Predicted half-widths for m=3,5,7 at A=0.25, Hz |
| --- | ---: | ---: | --- |
| Tanh / 1,3 | 1122.628 | 0.2394 | 116.11, 75.25, 40.67 |
| Tanh / through 7 | 1127.674 | 0.0504 | 119.66, 77.57, 42.84 |
| Tanh / through 31 | 1127.291 | 0.0000125 | 119.40, 77.46, 42.79 |
| Rational / 1,3 | 1128.565 | 0.2489 | 118.88, 77.69, 39.61 |
| Rational / through 7 | 1134.218 | 0.0506 | 122.82, 80.28, 41.88 |
| Rational / through 31 | 1133.858 | 0.0000145 | 122.58, 80.16, 41.84 |

“Normalized stage defect” is `rms(recurrence residual) / (alpha*rms(stage))`,
evaluated on a separate 16,384-point phase grid. It is **not** a waveform-error
bound. All retained-equation residuals were below `1e-7` in the solver's
alpha-scaled units. The adjoint retained odd harmonics through 31 for this
comparison. H1+H3 is a useful mechanism model but its roughly five-hertz
frequency error moves the seventh peak by roughly 35 Hz, comparable to its
width. Through the seventh harmonic is a better candidate for a cheap
preparation calculation; it is not yet a certified production choice.

Direct forced simulations used one second of warmup followed by two seconds
of measurement. At both `A=0.125` and `A=0.25` (approximately -18.06 and -12.04
dBFS peak), all 60 coarse inside/outside checks agreed: offsets of `0` and
`+/-0.8 W_m` locked numerically, whereas `+/-1.2 W_m` slipped. A locked test
means the last second's input phase at positive stage-four crossings spans
less than 0.01 radian; this is a finite-time diagnostic. The largest span among
these inside tests was 0.00103 radian; the smallest outside span was 81.24 radians.

Five further bisections on each side at `A=0.25` gave the following rational
model boundaries. These are numerical brackets from that diagnostic, not
interval-certified bounds:

| Input harmonic | Predicted center +/- half-width, Hz | Measured lower boundary bracket, Hz | Measured upper boundary bracket, Hz |
| --- | --- | --- | --- |
| 3 | 3401.574 +/- 122.579 | 3278.995–3280.527 | 3522.621–3524.153 |
| 5 | 5669.290 +/- 80.160 | 5587.126–5588.128 | 5745.442–5746.444 |
| 7 | 7937.006 +/- 41.842 | 7893.072–7893.596 | 7975.710–7976.233 |

The two models' predicted edges differ from their measured brackets by less
than 4.1 Hz in these tests. This validates a useful prediction at the tested
settings, not a bound across knob ranges. The measured asymmetry is consistent
with corrections beyond the first-order phase model.

Run from the worktree root with NumPy and SciPy installed:

```
python3 scripts/analyze_ladder_harmonic_reduction.py
python3 scripts/analyze_ladder_harmonic_reduction.py --check-locking
python3 scripts/analyze_ladder_harmonic_reduction.py --check-edges
```

Results and raw diagnostics are in `build/results/fundamental-third/`.
`reduction.json` holds orbit coefficients, output harmonic powers and adjoint
diagnostics; `locking.json` and `edges.json` hold the forced checks.

This provides high-side peak centers and approximate locking widths. It does
not yet give the entire UI response: coherent peak heights under forcing,
unlocked skirts, and total-power changes need amplitude information. The
low-side peaks near `Omega/3` require nonlinear/higher-order input coupling;
they do not follow from this first-order single-tone phase equation. The
remaining proof work is orbit existence/attraction with explicit error bounds,
followed by bounds for phase reduction and averaging. None of those assumptions
are hidden in the new projection theorems. No app DSP, codegen, or UI behavior
has changed in this experiment.

## Power response: derived phase curve and a stop before UI integration

The next investigation derived the power curve of the stationary averaged
phase model and checked its approximation error over a wider set of controls.
It found a substantial limitation, so **UI integration is deferred**. Nothing
in the Lean/audio snapshot mechanism, audio-thread sampling, codegen, or app
runtime was changed. Future preparation remains entirely on the UI side, with
frequency-independent snapshot preparation and persistent target-frequency
preparation; it must be budgeted for a fresh batch of snapshots every paint.

### The phase-only curve

Let `r = abs(inputFrequency - m*oscillatorFrequency)/halfWidth`, with positive
half-width. The squared coherent moment of the model is

```
C2(r) = 1                                      when r <= 1
C2(r) = 1/(r + sqrt(r*r - 1))^2                when r > 1
predictedCoherentOutputPower = freeOscillatorHarmonicPower[m] * C2(r)
```

The second expression avoids subtracting nearly equal numbers far from a
peak. Inside a locking band the phase is fixed, so its coherent moment has unit
magnitude. Outside the band, the phase spends longer where its angular speed
is lower. Uniformly averaging that phase would incorrectly predict zero
coherence. The normalized inverse-velocity density instead gives the displayed
nonzero skirt.

[PhasePower.lean](../../Analysis/PhasePower.lean) identifies that density with a
Poisson kernel, and uses the Poisson integral formula to calculate its first
complex moment exactly. This is a **continuous averaged phase-flow result**.
It is not the exact invariant measure of the discrete phase map, nor a proof
that a ladder trajectory converges to it. A continuous slow-flow approximation
needs a bound in addition to the harmonic truncation, amplitude reduction and
fast-phase averaging bounds.

The rigid-waveform model leaves total all-phase power unchanged under phase
rotation. Thus its leading total-power prediction is the free oscillator's
power. That is a property of this approximation, not a general property of the
driven ladder. Both output powers can be divided by `A*A/2` for input-relative
power gain when `A > 0`, away from the DC/Nyquist normalization exceptions.

| New theorem | Exact contract |
| --- | --- |
| `velocity_pos`, `density_pos`, `constant_flux` | Positive running-phase velocity and normalized-inverse-velocity form, for the specified parameter range |
| `density_eq_poisson`, `density_normalized`, `running_mean` | The stationary phase density is a normalized Poisson kernel with the stated first complex moment |
| `running_parameters`, `coherence_at_ratio`, `power_at_ratio` | The closed-form running coherence and its squared magnitude in normalized detuning coordinates |
| `locked_balance`, `locked_norm_sq` | Locked equilibrium moment satisfies the phase equation and has unit norm |
| `coherent_power_factor` | The coherent moment scales a harmonic's power by its squared norm |
| `rotated_total_power` | Phase rotation preserves all-phase total power; no identification with nonuniform sampled time averaging is asserted |
| `edge_conditioning`, `edge_not_lipschitz` | The phase-only power curve has no finite Lipschitz constant at a locking edge |
| `complex_power_error` | An established complex-amplitude error `epsilon` implies power error at most `epsilon*(2*abs(reference)+epsilon)` |
| `coherent_power_sum` | Adding oscillator and driven amplitudes includes a phase-dependent cross term |

For the conditioning theorem, parameterize the running coherent magnitude by
`0 < c < 1`. Then

```
r = (1+c*c)/(2*c)
r-1 = (1-c)^2/(2*c)
(1-C2(r))/(r-1) = 2*c*(1+c)/(1-c)
```

The last ratio grows without bound at the locking edge. The formal theorem
quantifies over every proposed finite Lipschitz constant and supplies a
counterexample. This does not rule out useful error bounds: bounds away from
the edge, a weaker modulus of continuity, or an explicit uncertain transition
region remain possible. It does rule out using one uniform linear sensitivity
bound across a peak boundary.

### The missing driven response matters

The probe [analyze_ladder_power_response.py](../../../../scripts/analyze_ladder_power_response.py)
compares this curve with the original detector-disabled four-stage sample
map. It reuses the autonomous solutions without fitting any curve parameters.
Across 132 finite-time checks (both tanh and rational, three control settings,
two amplitudes), the leading total-power prediction differs by at most 0.066
dB. At maximum drive and 75% resonance, the coherent prediction is within
0.958 dB in the previously tested windows. At weaker harmonics, the coherent
error becomes much larger.

Longer checks use two seconds of warmup and ten seconds of measurement, with
both ordinary and Hann-weighted coherent projections. At 75% drive, 75%
resonance, `A=0.25`, the rational seventh-harmonic lower skirt at 7346.392 Hz
has predicted power `0.0000639952` versus Hann-measured `0.000174418`, an
underestimate of 4.35 dB. The ordinary projection gives a 4.20 dB error. This
is not explained by short-window leakage alone.

A second check uses exactly repeating input tables and measures whole output
periods, after four input-table periods of warmup. At 50% drive and full
resonance the rational case uses an input period of 374,284 samples containing
14,000 input cycles (2,000 cycles of the locked oscillator). Input frequency is
7181.712283 Hz, near the predicted seventh-harmonic center. Two table periods
are measured. The phase-only prediction inside the band is `1.85287e-8` for
every amplitude:

| Peak input amplitude | Measured coherent output power | Prediction error |
| --- | ---: | ---: |
| 0.0625 | 1.02321e-8 | +2.58 dB |
| 0.125 | 4.41396e-9 | +6.23 dB |
| 0.25 | 1.84422e-10 | +20.02 dB |
| 0.5 | 2.13518e-8 | -0.62 dB |

All four states repeat across the measurement window to within `2.9e-13` in
binary64; for `A=0.25` they repeat exactly in that numerical run. Hann and
ordinary power agree to about eleven relative decimal digits in that case.
The corresponding tanh test, also measured over whole repeating periods,
has a 6.39 dB error at `A=0.25`. The 20 dB example is a very weak harmonic in
absolute power; the moderate-drive skirt above shows a larger visible signal
with a material error too. These are numerical checks, not interval proofs.

The amplitude-dependent dip and phase change are consistent with interference
between the oscillator harmonic and the driven response. A rigid phase-only
model cannot represent this behavior. In general the relevant power is

```
abs(oscillatorPhasor * coherentMoment + drivenPhasor)^2
```

and not a sum of their individual powers. Deriving that driven phasor, its
coupling back into phase/frequency, and its error bounds is additional work.
The interference identity is proved; this decomposition for the actual ladder
has not yet been proved. Adding a fitted peak correction would not discharge
that obligation.

The proposed next mathematical step is therefore an amplitude-and-phase
response about the oscillating waveform, including the nonresonant driven
response and its cross term. Orbit existence/attraction, a residual-to-waveform
error bound, and the discrete-to-averaged-flow bound remain unproved. Step three
does not proceed on the strength of the earlier locking-width matches.

Reproduce after the harmonic-reduction probe has produced its input data:

```
python3 scripts/analyze_ladder_power_response.py
python3 scripts/analyze_ladder_power_response.py --refine
python3 scripts/analyze_ladder_power_response.py --periodic
```

The numeric records are `build/results/ladder-power-response/power.json`,
`refine.json`, and `periodic.json`. No timing/performance claim is made by these
mathematical validation runs.

## Driven amplitude and phase correction

The follow-up derives the missing waveform response rather than fitting a peak
correction. It explains the cancellation above, but still does **not** establish
a certified response for the nonlinear ladder. UI integration remains deferred;
the audio processor, generated code, snapshot capture and sampling are untouched.

### What is expanded

Write the autonomous stage waveform as `q0(theta)` with angular increment
`Omega0`. Around harmonic `m`, express the input as `A*cos(m*theta - phi)`.
The fixed-phase projected harmonic-balance equations determine expansions

```
q(theta,phi) = q0(theta) + A*(qc(theta)*cos(phi) + qs(theta)*sin(phi))
              + A^2*(qcc(theta)*cos(phi)^2 + qss(theta)*sin(phi)^2
                     + qcs(theta)*cos(phi)*sin(phi)) + remainder
Omega(phi) = Omega0 + A*(nu_c*cos(phi) + nu_s*sin(phi))
             + A^2*(nu_cc*cos(phi)^2 + nu_ss*sin(phi)^2
                    + nu_cs*cos(phi)*sin(phi)) + remainder
```

The same bordered derivative matrix solves the cosine/sine responses and the
quadratic corrections. Its added column is the derivative with respect to
`Omega`; its added row fixes the otherwise arbitrary oscillator phase. The
phase convention is zero sine coefficient for the first harmonic of stage four.
The quadratic right-hand sides come from the saturation curvature and the
frequency-dependent delay, including the delayed feedback path. They are not
regressions against measured powers. Input amplitude remains a parameter.

This is a derivative about the **nonzero oscillating waveform**, with each
saturator slope evaluated at its changing stage value. It is not the unstable
zero-input linearization. The first-order output coefficient after rotation
into the input phase is

```
Y0*mu1 + A/2*((Yc - i*Ys)*mu2 + Yc + i*Ys)
mu_n = average(exp(i*n*phi))
```

Thus the driven response is not just a constant added power: its complex
amplitude interferes with the oscillator harmonic. A quadratic output observer
also uses `mu3` and `conjugate(mu1)`; its squared magnitude for total power uses
moments through order four. The probe retains all these cross terms using a
Gram matrix of the waveform corrections. Squaring the quadratic observer does
not establish fourth-order accuracy of the underlying waveform.

For the first-order continuous phase equation, all positive moments are
`mu_n = mu1^n`. The quadratic phase drift has a constant and second harmonics,
so that Poisson-kernel shortcut no longer applies. The probe finds a stable
phase root when present, or integrates normalized inverse speed when the drift
has no root. The test cases have a single stable branch. This exploratory root
search is not a certified root count or a production boundary algorithm.

### Moving phase and the discrete map

A running relative phase changes between successive samples. The previous
waveform must use both the previous oscillator phase and the previous relative
phase. With scaled detuning `d = (inputOmega/m - Omega0)/A`, the second-order
backward invariance equation has the additional source

```
m*(nu_1(phi)-d)*historyOperator(dq_1/dphi)
historyOperator(v) = (1-alpha)*v, with -alpha*K*v4 added to component one
```

The numerical probe derives these transport corrections and separately retains
the `v'*v/2` modified-flow term for an implicit backward phase step. This is
still a **continuous averaged-flow approximation** of a discrete map. It is
not an exact invariant density of that map. The transport and modified-flow
expansions are currently numerical/algebraic derivations, not end-to-end Lean
proofs of the reduced dynamics.

Independent residual evaluations substitute the corrected waveform into the
discrete equations with both phases moving. For the six model/control cases
and harmonics 3, 5, 7, halving `A` from 0.01 to 0.005 to 0.0025 reduces the
projected quadratic residual by factors between 7.94 and 8.04. Frozen-phase
quadratic residuals also scale cubically. The probe asserts this scaling and
checks the saturation curvature against finite differences of its first
derivative. These checks exclude a sign/factor mistake in the tested expansion;
they do not bound omitted Fourier modes or prove an attracting invariant set.

### New formal results

| Module / theorem | Exact contract |
| --- | --- |
| `NormalizedTanhDerivatives.hasDerivAt_apply` (in namespace `DSP.NormalizedTanh`) | Derivative of the existing real normalized saturator at every input |
| `hasDerivAt_slope`, `hasDerivAt_curvature` | Its second and third derivatives |
| `contDiff_apply`, `deriv_apply`, `deriv_slope`, `deriv_curvature` | Smoothness and derivative-function identities |
| `thirdDerivative_bound` | Uniform bound `abs(S'''(x)) <= 2*abs(g)^3/abs(tanhGain)` |
| `quadratic_remainder` | `abs(S(x+h)-S(x)-S'(x)*h-S''(x)*h^2/2) <= abs(g)^3*abs(h)^3/(3*abs(tanhGain))` |
| `HarmonicBalance.step_firstVariation` | Chain rule through the actual existing ladder step, at arbitrary histories, with detector disabled and coefficients held fixed; current-stage slopes and old-stage feedback are preserved |
| `PhasePower.running_moment_pow` | Every positive Fourier moment of the first-order running-phase density is the corresponding power of its first moment |
| `driven_observer`, `quadratic_observer` | Exact complex identities for the rotated first- and second-order output observers on the unit circle |
| `PhasePowerBounds.inverse_speed_error` (in namespace `DSP.PhasePower`) | If both positive speeds are at least `margin`, an error `epsilon` changes their inverses by at most `epsilon/margin^2` |
| `normalized_moment_error` | Numerator and total-weight errors at most `epsilon` imply normalized-moment error at most `2*epsilon/mass`, assuming positive weights and a reference numerator norm no larger than its total weight |

The derivative and remainder proofs concern exact real tanh, with the existing
cached denominator. They do not identify rational saturation or float arithmetic
with tanh. Both nonlinearities are examined separately by the numerical probe.
The inverse-speed and normalization bounds take their error estimates and
positive margins as hypotheses; they are not claims that these have already
been established for every ladder snapshot.

### Numerical results and the remaining edge error

At 50% drive, full resonance, the earlier whole-period rational tests give:

| Peak input amplitude | Measured coherent power | Quadratic fixed-phase prediction | Power error |
| --- | ---: | ---: | ---: |
| 0.0625 | `1.023209e-8` | `1.023237e-8` | +0.00012 dB |
| 0.125 | `4.413960e-9` | `4.415469e-9` | +0.00148 dB |
| 0.25 | `1.844223e-10` | `1.842494e-10` | -0.00407 dB |
| 0.5 | `2.135181e-8` | `2.115253e-8` | -0.04072 dB |

The previous phase-only miss at `A=0.25` was +20.02 dB. The quadratic tanh
prediction at the corresponding test improves from +6.39 dB to +0.00598 dB.
The moving-phase version differs slightly, as expected from omitted higher
orders: -0.01053 dB for that rational cancellation and +0.00496 dB for tanh.
Across all eight whole-period cases both quadratic versions stay within
0.041 dB. These are binary64 numerical agreements, not certified bounds.

At maximum drive, the moving-phase version stays within 0.255 dB in the 60
previous coarse measurement windows. The coarse weak-harmonic windows still
show large coherent discrepancies and are not accepted as accurate stationary
measurements; the longer/Hann and whole-period checks above were introduced to
address that leakage. No blanket accuracy claim over all coarse cases is made.

At 75% drive/resonance, the rational lower-skirt test still has a 1.057 dB
coherent error: the quadratic model calls it locked while the measured phase
slowly slips. A fuller forced H31 solve, using analytic Jacobians and no
amplitude expansion, locates the local periodic-branch turning point:

```
quadratic predicted lower edge:  7346.384311 Hz
measured test frequency:         7346.391736 Hz
full projected lower edge:       7346.413697 Hz
```

The approximately 0.0294 Hz higher-order edge shift is enough to put the test
on opposite sides of the boundary. This agrees with the previously proved
failure of a uniform Lipschitz bound at locking edges. The full projected
turning point is also a numerical result, not a proof of attraction or a
validated exact-ladder locking boundary. It diagnoses why an explicit boundary
uncertainty region is necessary even when waveform errors are small.

### What remains before a UI evaluator

The local tanh Taylor bound and exact sample-step derivative are now available.
They still need to be combined with a validated orbit and transverse inverse/
stability estimates to turn residuals into waveform errors. The discrete
phase-reduction and averaging errors need bounds too. Finally, coherent and
total phase averages must be related to sampled long-time measurements for the
chosen attracting response. A finite matrix condition number or a small
projected residual establishes none of these on its own.

Away from an edge, the new inverse-speed/normalization bounds provide the error
propagation once the dynamical estimates are supplied. At an edge, use a
validated uncertainty region rather than silently selecting a locked/running
branch. Multiple attractors, loss of the assumed torus, and untested control
settings remain outside these numerical observations.

The intended UI preparation split is unchanged: snapshot-only orbit and response
work, persistent control-independent frequency-grid work, and evaluation using
both. The current probe uses H31 (129 unknowns in each bordered system) as a
reference and is not a cost claim for a production UI approximation. It shares
the mathematical derivative operator across the response right-hand sides;
choosing and certifying a cheaper order remains future work. No assumption of
unchanged-snapshot caching is introduced.

After producing the reduction, power, refined and periodic data above, run:

```
OPENBLAS_NUM_THREADS=1 python3 scripts/analyze_ladder_driven_response.py
```

The complete numerical record is
`build/results/ladder-driven-response/response.json`. It includes both quadratic
variants, projected residual scaling, derivative checks, measured comparisons
and the full projected branch-edge diagnostic. Proof checks are included in the
normal DSP root and axiom audit.

## Local existence and attraction: first periodic enclosure

The next step separates two different objects. A locked response to a repeating
sampled input can be an attracting periodic orbit of the discrete ladder. A
free-running or drifting response requires an invariant curve/torus and its
internal phase dynamics. The latter has a phase direction that need not contract.
Treating all four directions as contracting would discard the behavior we want.

The new numerical probe `scripts/analyze_ladder_attraction.py` checks the six
existing tanh/rational control settings. For each approximate free oscillator,
it advances the actual sample map and its derivative for roughly one cycle,
then projects away the tangent direction. At 128 sampled starting phases, the
largest transverse operator norm is 0.013–0.205 across these cases. This is a
promising normal-contraction diagnostic, **not** an enclosure for an invariant
curve: the reference waveform is approximate and only finitely many phases are
checked. In particular, the H31 high-drive orbit has a roughly `1e-6` block defect;
its small normal norm does not make that defect disappear.

The same probe solves an exactly repeating forced sample block. Ordinary
Euclidean operator norms can exceed one because transient expansion depends on
the direction of the perturbation. A fixed change of coordinates, selected from
a discrete Lyapunov equation, exposes contraction. The change of coordinates is
only a proof device; it does not change the audio implementation or its state.

### The proved criterion

Let `F` be a complete input-period map in those coordinates, `c` a candidate
state and `B(c,r)` a closed ball. Supply uniform bounds

```
||DF(x)|| <= q < 1            for every x in B(c,r)
||F(c)-c|| <= epsilon
epsilon + q*r <= r
```

Then `F` maps the entire ball into itself and is a contraction there. The new
Lean results prove an exact fixed point in the ball, uniqueness within that
ball, attraction from every point in it, a geometric convergence rate, and an
error bound `distance(c, fixedPoint) <= epsilon/(1-q)`. This does not assert a
unique attractor globally or convergence from arbitrary initial histories.

| Module / theorem | Exact contract |
| --- | --- |
| `AttractingBlock.mapsTo_ball` | Residual and uniform Lipschitz bounds make the closed ball invariant |
| `exists_attracting_fixedPoint` | Local existence, uniqueness, attraction and candidate error bound, using the complete-space contraction theorem |
| `iterate_error` | Block-boundary perturbations decay at most geometrically with factor `q` |
| `lipschitz_of_derivative` | A derivative norm enclosure over the whole convex ball supplies the Lipschitz bound |
| `Step.states_add`, `states_period_shift`, `states_blocks` | Iterating a block map equals running the original periodically driven sample processor |
| `states_periodic_of_fixed` | A block fixed point produces an exact periodic sample trajectory |
| `states_phase_tendsto`, `exists_attracting_periodicOrbit` | Attraction holds at every fixed sample phase, not just at block boundaries |
| `states_continuous` | Continuous sample steps give continuous finite prefixes |
| `tendsto_of_residue_classes`, `states_observer_tracking` | Finitely many phase limits combine into all-sample tracking of a continuous observation |
| `HarmonicBalance.audioStep_projection`, `states_audioHistory` | The four-history proof map is exactly the projection of the existing ladder sample run with damping disabled |
| `step_sameAudioParameters`, `audioStep_continuous`, `audioPrefix_continuous` | Coefficients remain fixed, and the projected map/prefixes are continuous |

The proof-side `audioStep` invokes the existing `step`; it is not another DSP
implementation. The projection theorem permits the full processor's unused
detectors to continue advancing while proving that they have no effect on audio
histories when damping is disabled. Existing `coherent_of_periodic_tracking` and
`meanSquare_of_periodic_tracking` now have the needed kind of all-sample tracking
hypothesis available from this contraction route. No fast-phase averaging or
harmonic truncation is required to define the exact periodic measurements.

### A concrete interval check

`scripts/check_ladder_periodic_enclosure.py` checks the whole neighborhood using
outward-rounded 70-digit Decimal intervals. Sin, cos and exp use explicit Taylor
remainders; pi is enclosed using Machin's arctangent identity and alternating
series bounds. Square roots use correctly rounded Decimal roots widened by one
representable value. The coordinate transform's inverse is computed with exact
rational arithmetic before conversion to intervals. Input and coefficient
rounding errors are enclosed throughout the sample block.

The fixture is exact real tanh with the excess-energy detector disabled:

```
gain = 5
alpha = 2*sin(pi/192)*(sqrt(1+sin(pi/192)^2)-sin(pi/192))
baseFeedback = (39/10)*(7*sqrt(7)-1)/48
tanhGain = tanh(5)
input[n] = cos(2*pi*3*n/170)/4
```

This corresponds to the explored 1 kHz cutoff, full drive, 75% resonance and
peak input amplitude 0.25 at 192 kHz engine rate. Input frequency is about
3388.235 Hz; the periodic response block contains three input cycles. The
fixture fixes `baseFeedback` explicitly. It is not a theorem about every
setting of the app's coefficient setter, approximation or safety cap.

| Checked quantity | Outward bound |
| --- | ---: |
| Period | 170 samples |
| Ball radius in the specified coordinates | `0.001` |
| Derivative norm throughout the ball | `< 0.877415` |
| Candidate residual norm | `< 2.146e-60` |
| Image radius | `< 0.000877415` |

Thus the rounded budgets `q = 0.9`, `epsilon = 1e-58`, `r = 0.001` satisfy the
criterion with room to spare. The script bounds the transformed derivative's
Frobenius norm, which bounds its Euclidean operator norm; it does not infer a
uniform derivative bound from eigenvalues at the candidate. Exact-rational
arithmetic checks, square-root inequalities and independent higher-precision
exp/tanh evaluations exercise the interval implementation.

**Trust boundary of this checker:** these particular Euclidean-norm bounds
are checked by the external Python script. They are not imported as Lean axioms.
The separate kernel-checked certificate below uses different coordinates and
more generous budgets, and closes a theorem for the same physical fixture.
The Python check remains an independent numerical cross-check; arithmetic tests
alone do not prove an interval implementation correct.

### Remaining scope

This is a local periodic-case result. It supplies a workable route to existence
and attraction without assuming either from a small harmonic-balance residual.
It does not yet cover the drifting skirts, all control/frequency values, the
basin reached from reset, or rational/float runtime behavior. Those need their
own enclosures or a parameter-region argument. The free oscillator's normally
attracting invariant curve and a justified internal phase/averaging model remain
separate obligations. A periodic fixture does not establish them by continuity.

Reproduce after the harmonic-reduction data exists:

```
OPENBLAS_NUM_THREADS=1 python3 scripts/analyze_ladder_attraction.py
OPENBLAS_NUM_THREADS=1 python3 scripts/check_ladder_periodic_enclosure.py
```

Results are `build/results/ladder-attraction/diagnostics.json` and
`periodic-enclosure.json`. This remains proof/exploration work; the app, audio
thread, generated DSP, snapshot system and UI are unchanged.


## Locked response: kernel certificate and error bounds

`HarmonicBalance/Certificates/LockedResponse.lean` connects a concrete numerical
certificate to the existing `step`, including its delayed feedback and final
output saturator. `LockedParameters.Parameters` specifies the exact tanh,
detector-disabled fixture above. These physical parameter equalities are the
only model premises; contraction, trapping, residual, input bounds and output
bounds are proved, rather than supplied as numerical assumptions.

The certificate uses a rational change of coordinates and the infinity norm:

| Quantity | Certified budget |
| --- | ---: |
| Block length | 170 samples |
| Initial coordinate-ball radius | `1e-4` |
| Whole-ball contraction factor | `3/5` |
| Center residual | `1e-12` |
| Exact fixed point's distance from the center | at most `2.5e-12` |
| Ball replayed for the response enclosure | `1e-8` |
| Coherent output power | `[0.35523868, 0.35523877]` |
| Total output mean-square power | `[4.97991252, 4.97991274]` |

Coherent power is the squared magnitude of the peak fundamental phasor divided
by two. Total power includes every harmonic present in the exact periodic
output. Both are limits of measurements over arbitrary window lengths for all
initial audio histories in the certified local basin. We observe history after
sample `n` has advanced the state, matching the processor's output convention.

The frozen quadratic prediction, rounded to fixed rational reference values,
is `0.35523845801455806` coherent power and `4.979911677333437` total power.
The absolute errors are respectively at most **`3.2e-7`** and **`1.1e-6`**.
These bounds compare the proposed numbers directly against exact sample-orbit
powers. They include neglected harmonics and evaluation errors at this fixture;
they do not certify the numerical program that produced the proposed numbers,
or give a parameter-uniform error bound for the quadratic approximation.
With input peak amplitude `1/4`, normalized squared gains multiply powers and
these absolute error bounds by 32.

### Proof and certificate organization

| Component | Result |
| --- | --- |
| `ResponseMeasurement/ErrorBounds` | Sample error to phasor, coherent-power, total-power and normalized-gain error |
| `ResponseMeasurement/IntervalBounds` | Real sine/cosine and output boxes enclose the existing complex coherent measurement and total mean square |
| `Analysis/LockedResponse` | Generic local contraction plus observation bounds gives existence, attraction, all-window limits and response errors |
| `Analysis/IntervalExp`, `IntervalTanh`, `IntervalArithmetic` | Sound rounded interval evaluation, proposed tanh bounds, square-root/inverse checks, secant and matrix bounds |
| `HarmonicBalance/Enclosure`, `EnclosureTrace` | Checked boxes replay the actual audio step and exact differences between pairs of histories |
| `HarmonicBalance/AffineEnclosure` | Checked rational-coordinate traces imply a uniform block contraction and center residual bound |
| `LockedData`, `LockedCheck00` through `LockedCheck16`, `LockedChecks` | Proposed rational data and kernel-checked finite sample/matrix certificates |
| `LockedParameters`, `LockedCoordinates` | Exact trigonometric input/coefficients, coordinate inverse, initial-ball containment, contraction and residual budgets |
| `LockedOrbit.exists_attracting` | An actual fixed point of the sample block exists and attracts the stated local basin |
| `LockedMeasurements.approximation_error` | Both reported power errors satisfy the bounds above |
| `LockedResponse.exists_response`, `exists_full_response` | The closed response theorem, including measurements of the full existing processor run |

The interval implementation imports minimal core modules from pinned LeanCert
revision `5c055002ac8681dbd61d4c7dab11c1450146be60` and uses mathlib's proved
transcendental bounds. Exact rational checks use `decide +kernel`; neither
`native_decide` nor Python results are proof assumptions. The usual DSP axiom
audit traverses the final theorem's dependencies.

`scripts/generate_ladder_interval_witness.py` is an **untrusted witness
producer**. It emits checked-in rational data and small certificate modules.
Changing a proposed value requires the corresponding kernel check to succeed.
The emitter encodes intervals by lower endpoint and nonnegative rational width;
this keeps interval validity structural and avoids thousands of expensive
arithmetic proof elaborations. Individual sample lemmas avoid expensive
combined reduction of transcendental checks over a finite quantifier.

A normal Lean build needs neither Python nor ignored experiment results. To
regenerate proposals, first run the existing attraction/periodic-enclosure
experiments, then run the witness producer. The `--emit-only` option reuses the
witness JSON under `build/results/ladder-locked-error/`. Build the certificate
modules in small batches when regenerating on a memory-constrained machine.

This is a local theorem at one locked fixture. It does **not** establish the
basin reached from reset, the entire locked frequency/control region, or the
rational/float runtime's approximation error. Audio, codegen, snapshots and UI
execution are unchanged.

## Drifting response: existence-and-attraction attempt

The drifting case needs an attracting **curve**, rather than a fixed point of
the input-period block. The state approaches that curve while its phase keeps
moving. Phase is allowed to affect amplitude, and amplitude is allowed to
affect the next phase; treating this as an uncoupled rotation would omit an
actual term of the ladder's dynamics.

`Analysis/AttractingGraph.lean` proves a reusable graph-transform criterion.
In phase/transverse coordinates `(theta, r)`, let `F` update phase and `G`
update the transverse state. The certificate supplies bounds over an entire
tube: transverse trapping, bijective phase maps on admissible graphs, phase
separation `A > 0`, transverse-to-phase sensitivity `b`, and phase/transverse
sensitivities `c, d` of `G`. For graphs with Lipschitz bound `L`, the conditions
are

```
(c + d*L)/A <= L
q = d + (c + d*L)*b/A < 1.
```

The proven conclusions are existence of an invariant graph, uniqueness among
the admissible graphs, a residual-to-graph-distance bound `defect/(1-q)`, and
geometric attraction to that graph at the trajectory's **current phase**.
`Certificate.exists_invariant`, `invariant_unique`, `invariant_error`,
`iterate_error` and `attracts` carry these statements. Phase convergence to a
single point is not a premise or a conclusion.

`scripts/analyze_ladder_drift_attraction.py` tests the actual tanh recurrence
at the same drive, resonance and input amplitude, with a 54-sample input
period (about 3555.56 Hz) and a 162-sample block. A candidate graph is represented
by a Fourier profile in a polar/PCA chart. Phase-map derivatives include the
complete coordinate chain rule and are cross-checked by finite differences.
The tube probes use offset phase points and transverse center/axis/corner
points, followed by direct attraction tests.

At both 1024- and 2048-point profile resolutions, the sampled graph-transform
contraction is approximately **0.18724**, with transformed slope approximately
`0.00483 < L=0.01`, in a tube of radius `0.001`. The graph-iteration defect falls
from about `5.69e-8` at the coarse resolution to `3.83e-9` at the finer one.
The sampled image tube radius is about `0.00018053 < 0.001`. The observed
relative phase keeps slipping rather than settling to the locked response.

**Status:** the abstract existence-and-attraction theorem is proved in Lean.
The first script has sampled double-precision checks. The subsequent work below
has whole-cell interval **proposals**, but still no closed Lean certificate for
this concrete curve. The remaining connection includes chart semantics, phase
invertibility, coverage of the tube and verified replay. An invariant curve
alone does not prove irrational rotation, exclude longer-period locking, or
justify a particular time-averaging density.

Reproduce the drift diagnostics with:

```
OPENBLAS_NUM_THREADS=1 python3 scripts/analyze_ladder_drift_attraction.py
```

Results and their explicit trust status are in
`build/results/ladder-drift-attraction/diagnostics.json`.

## Discrete measurement certificates

The new measurement argument works with the **actual discrete block map**,
including phase/amplitude coupling. It does not assume the inverse-speed
density of the approximate continuous phase flow.

For a block observable `O`, a proposed mean `m`, block map `F`, and bounded
correction `V`, check on an invariant domain that

```
norm (O(x) - m - (V(x) - V(F(x)))) <= epsilon
norm (V(x)) <= M.
```

Summing along the actual orbit cancels all interior correction values. After
`N` blocks, the measurement error is at most `epsilon + 2*M/N`. The observable
can be the complex coherent phasor or total power. Coherent phasors are averaged
**before** taking their squared magnitude; averaging block powers would lose
phase cancellation and measure something else.

This gives useful finite-window bounds without settling whether the phase
rotation number is rational. It does **not** prove exact convergence from one
small residual. The new convergence theorems require arbitrarily accurate
bounded correction certificates; the correction bound may grow as accuracy
increases. Such a family has not been constructed for the concrete ladder.

### New theorem inventory

| Component | Proven conclusion, conditional where stated |
| --- | --- |
| `Analysis/Coboundary.finite_bound`, `eventually_bound` | Uniform correction residual gives a finite-window error and an eventual error envelope |
| `tendsto_of_certificates`, `exists_limit_of_certificates` | Arbitrarily accurate certificates imply an exact limit, with fixed or varying proposed means |
| `limit_error` | Any established limit lies within a supplied residual budget |
| `orbit_bound` | Uniform invariant-domain checks apply to actual iterates |
| `perturbation_bound`, `parameter_region_bound` | Uniform map/observable perturbations give explicit errors over a parameter region |
| `Analysis/AttractingGraph/Measurements.tube_residual`, `average_bound` | Center-curve corrections extend across a trapped tube, including transverse changes to the next phase |
| `Analysis/CoboundaryBlocks` | Exact block/sample regrouping, incomplete-window bounds, and all-window convergence when block convergence is proved |
| `ResponseMeasurement/Coboundary` | Correction certificates bound coherent and total powers of an actual sample processor, observing after each input step |
| `HarmonicBalance/SecondVariation.step_mixedVariation` | Exact second variation of the existing delayed-feedback step, including the final output saturator |
| `Analysis/IntervalDerivatives.mem_curvature` | Checked rational tanh boxes enclose the real saturation curvature |
| `HarmonicBalance/SecondEnclosure.mixedRanges_sound` | The same witness engine propagates mixed-derivative boxes through the actual sample recurrence |

The parameter-region theorem retains four distinct contributions: reference
residual, observable perturbation, potential-Lipschitz times map perturbation,
and error of the proposed UI prediction. It needs uniform trapping and bounds
for every parameter in the region. There is no concrete certified control or
frequency patch yet. In particular, the periodic-input block theorem cannot
cover arbitrary input frequencies just by changing a coefficient: those need
an input-phase state or another justified treatment.

### Concrete drifting proposals

`analyze_ladder_drift_averages.py` truncates the candidate coordinate profile to
80 Fourier modes and measures its defect afresh. A least-squares fit proposes
64-harmonic corrections for coherent real/imaginary components and total power.
The sampled correction residuals are about `2.96e-7`, `2.93e-7`, and `1.34e-9`.
These sampled values alone are not uniform error bounds.

`propose_ladder_drift_tube.py` then propagates value, first-derivative and
second-derivative intervals across phase cells and transverse boxes. Centered
Taylor bounds preserve cancellations that ordinary interval Jacobian products
lose. The proposed 16,384-cell cover of a radius `2e-6` tube gives:

| Quantity | Unverified proposal |
| --- | ---: |
| Graph slope budget `L` | `0.04` |
| Minimum graph phase separation `A` | `0.38533` |
| Transformed graph slope bound | `0.03247` |
| Graph contraction `q` | `0.22973` |
| Maximum image transverse radius | `6.868e-7 < 2e-6` |
| Coherent phasor center | `-0.3074016431 - 0.0093112238 i` |
| Coherent power center | `0.04729123454` |
| Total power center | `4.97012270731` |
| Long-window coherent-power error envelope | `3.794e-6` |
| Long-window total-power error envelope | `7.378e-7` |

The finite-window envelope also includes the endpoint correction `2*M/N`
(separately for phasor components and total power) and an incomplete-block
term if needed. These numbers are powers, before dividing by input power
`(1/4)^2/2`.

**Trust boundary:** ordinary operations are widened with `nextafter`, while
transcendental values use padded binary64 evaluations. These proposals still
need replay against the exact chart data below, with verified physical
coefficient rounding and phase-domain bounds. The Python program is not a verified interval library;
none of these proposed bounds is imported as a Lean fact. Independent finite
differences check the full coordinate-map Hessian to about `6.5e-9` and the
measurement-residual Hessian to about `9.5e-7`; these checks test the formulas,
not the soundness of interval enclosures. The formal second-variation engine
above verifies the sample recurrence. The subsequent polar-chart results below
also verify the coordinate identities and derivatives. The later exact-data
and centered-Taylor results validate the chart and checker semantics; the
concrete whole-tube replay remains to be certified.

The distinction matters for the cheap harmonic-balance approximation:

| Proposed response at this fixture | Coherent power | Total power |
| --- | ---: | ---: |
| Discrete-map correction fit | `0.04729123` | `4.97012271` |
| Frozen quadratic phase model | `0.05469182` | `4.96891559` |
| Moving quadratic phase model | `0.05040561` | `4.96921968` |

Thus the frozen coherent prediction differs by roughly `0.63 dB`, and the
moving prediction by `0.28 dB`, even though total power is close. The tiny
locked-fixture approximation error is not a uniform accuracy claim for the
drifting region.

Reproduce after the previous drift diagnostics:

```
OPENBLAS_NUM_THREADS=1 python3 scripts/analyze_ladder_drift_averages.py
OPENBLAS_NUM_THREADS=1 python3 scripts/propose_ladder_drift_tube.py --check-hessian --measurements
OPENBLAS_NUM_THREADS=1 python3 scripts/propose_ladder_drift_tube.py --cells 16384 --radius 0.000002 --measurements
```

The remaining concrete obligations are: certify uniform dynamics on the exact
chart in Lean; replay the correction residual bounds; then cover useful
parameter regions and budget the cheap approximation's error. Exact
long-time convergence requires the additional argument stated above. The app,
audio implementation, generated DSP and snapshot interfaces are unchanged.

## Exact polar chart and derivative proofs

`Analysis/PolarTube` now supplies the mathematical coordinate system used by
the drift proposal. The state is an exact invertible linear transformation of

```
((P0(theta)+r0)*cos(theta), (P0(theta)+r0)*sin(theta),
 P1(theta)+r1, P2(theta)+r2).
```

Phase is `Real.Angle`, the circle of real angles modulo `2*pi`. The jump in a
real-valued atan2 output at the negative axis therefore does not create a jump
in the phase state. A strictly positive radial margin makes the coordinates
injective throughout the tube. The profile need not already be invariant.

| Module | Proven result |
| --- | --- |
| `PolarTube` | Encode/decode identities, injection on the positive-radius domain, continuity, and exact commutation of coordinate iterations and observations with the original state map |
| `PolarTube/Decode` | First and mixed second derivatives of the initial curved chart, including phase/radius cross terms and profile curvature |
| `PolarTube/Phase` | Smooth local real phase lifts with a rotated branch cut, agreement with the global circle phase, and the phase-gradient/Hessian formulas |
| `PolarTube/Radius` | First and mixed second derivatives of polar radius, including agreement with the proposed real Hessian matrix |
| `PolarTube/Encode` | A local real lift of the entire output chart agrees with the global chart; its first and mixed second derivatives retain both polar and profile curvature |
| `PolarTube/Lift` | A global phase lift agreeing with the sample map, with exactly one turn of lift per input turn, continuous wherever the alignment domain check holds |

The lift result adds an explicit check that was missing from the proposal:
positive local phase derivatives alone do not rule out a map that winds around
the circle twice. For updated transformed head `z`, the proposal now checks

```
z.real*cos(theta) + z.imaginary*sin(theta) > 0
```

throughout the tube. This keeps the relative phase away from its branch cut.
The global lift is `theta + arg(z / exp(i*theta))`; Lean proves that it agrees
with the global phase and shifts by exactly `2*pi` under `theta -> theta+2*pi`.
At the 16,384-cell, radius `2e-6` fixture, the proposed alignment lower bound is
about **1.23757**, so this additional check passes comfortably. The previous
contraction and measurement proposals are unchanged. The alignment number has
the same unverified numerical status as those other proposals.

These are exact general chart theorems with explicit domain and profile
derivative hypotheses. The following additions discharge the abstract phase
invertibility obligation and instantiate the candidate chart. Uniform dynamic
bounds for that chart still remain before a closed concrete drift theorem.

## Phase invertibility, exact drift data, and centered Taylor checks

The graph certificate no longer needs an unexplained phase bijection along
every candidate graph. `Analysis/CircleLift` proves that a continuous real
lift with degree one and a positive lower separation bound induces a circle
bijection. The lower bound holds in the quotient metric, including across the
atan2 seam. `Analysis/AttractingGraph/Circle` applies this to every admissible
Lipschitz graph: if the fixed-fiber phase lower slope is `a`, the fiber coupling
is at most `b`, and graph slope is at most `L`, then its phase separation is
at least `A` whenever

```
0 < A,    A + b*L <= a.
```

This uses finite differences; it does not assume the Lipschitz graph is
differentiable. The remaining transverse bounds, trapping, cone condition and
contraction condition are still explicit certificate inputs.
`PolarTube/Graph` connects this construction to the actual chart phase lift
and proves that certificate iterations decode to iterations of the original
state map whenever the certificate's step agrees on the tube.

`Certificates/DriftChartData` now fixes the proposal's 80-harmonic profile and
64-harmonic correction potentials as exact rational coefficients. The producer
combines both positive and negative Fourier coefficients; it does not silently
assume perfect conjugate symmetry of the floating-point fit. It also computes
the inverse of the decimal-valued coordinate matrix over the rationals.
`Certificates/DriftChart` checks both matrix products in Lean's kernel and
constructs an exact linear equivalence. No rounded numerical inverse is used
as an equality.

The following concrete chart facts are proved:

| Fact | Certified bound or identity |
| --- | --- |
| Fourier profile | Continuous, with the specified first and second derivatives |
| Radial profile | `P0(theta) >= 11/10` for every phase |
| Tube chart | Positive radius and exact encode/decode for transverse norm at most `2e-6` |
| Correction potentials, real/imaginary/power | Absolute bounds `(3, 3, 1)` |
| Correction derivatives | Absolute bounds `(10, 10, 2)` |

The correction bounds are endpoint/regularity facts, **not** certified
coboundary residuals or certified means. Their constants are conservative
rational budgets. `Analysis/TrigPolynomial` supplies the shared exact finite
Fourier representation, derivative identities and coefficient-sum bounds;
`TrigPolynomial/Enclosure` supplies rational interval evaluation with proved
trigonometric remainders. Derivative profiles use the same enclosure routine.

`Analysis/CenteredTaylor` proves the whole-segment remainder estimate used by
the tube proposal. With coordinate offsets bounded by `w_i` and uniform
Hessian-entry magnitudes bounded by `H_ij`, the remainder after the center's
linear term is bounded by

```
(1/2) * sum_i sum_j H_ij * w_i * w_j.
```

The separate widths preserve the thin transverse tube instead of replacing
every width by the much larger phase-cell width. `Analysis/IntervalTaylor`
provides kernel-checkable rational containment tests and proves their meaning.
It covers both first-order cell enclosures (also applicable to Jacobian rows)
and second-order centered enclosures. Hypotheses require derivatives and
Hessian bounds throughout each cell; finite samples do not discharge them.

To reproduce the exact proposal definitions after the drift diagnostics:

```
python3 scripts/generate_ladder_drift_witness.py
```

Normal Lean builds consume the saved definitions without Python or numerical
diagnostic files. The additions below compose the sample/chart derivative
recurrences and implement centered block replay. The full rational witness for
the exact chart and physical coefficients is still outstanding. The binary64
tube and measurement numbers above remain proposals until that replay succeeds.
Then parameter regions and approximation budgets remain; exact time-average
convergence is still a separate obligation. Audio and UI code are unchanged.

## Block replay and a concrete Fourier cell

The derivative and centered-value engines now compose over an arbitrary finite
block of the actual detector-disabled ladder. In particular they apply to the
162-sample drifting proposal without changing its feedback delay or omitting
its output saturator.

| Module | Proven result |
| --- | --- |
| `HarmonicBalance/BlockVariation` | A derivative frame carries the exact value, two first directions and their mixed variation through every sample; its value equals the original audio projection, and observation derivatives include the final saturator |
| `HarmonicBalance/CoordinateVariation` | The complete curved-input-chart → linear inverse → ladder block → linear transform → curved-output-chart chain has the claimed first and mixed second derivatives; its value is the actual coordinate block map |
| `HarmonicBalance/DerivativeEnclosure` | Rational first/mixed derivative checks compose across the block, provided the value boxes have been established |
| `HarmonicBalance/CenteredEnclosure` | Centered value checks establish those value boxes stage by stage and compose across the block; an exact secant matrix tracks differences from the checked center orbit |

The centered checker avoids a circular assumption: it encloses the first new
stage, validates that stage's tanh secant, encloses the next stage, and continues.
It does not assume the next whole-state box is correct in order to prove that
same box. The initial secant matrix can be obtained from derivatives along a
curved chart segment. Each output row may use its own mean-value point; the
proof does not assert a generally false single-point vector mean-value theorem.

The coordinate derivative theorem still requires output branch safety. The
matrix-entry enclosures for the initial/final charts, the domain checks, and
the complete concrete dynamic and measurement witnesses must still be supplied.

A direct monolithic Taylor replay of all 80 Fourier harmonics proved too slow
in the first local experiment and was stopped. The replacement witness format
shares two checked base-angle transcendental evaluations and validates rounded
higher-harmonic boxes using exact angle-addition identities.
`Analysis/IntervalTrig` uses twenty-decimal proved pi bounds for range reduction
and a Lipschitz width bound around the center. `TrigPolynomial/HarmonicEnclosure`
proves the rounded harmonic recurrence and its whole-cell Fourier enclosure.
No numerical transcendental value or rounding decision is trusted.

`Certificates/DriftProfileCell` now closes the concrete profile-preparation
proof for every phase in `[2.4998, 2.5002]`, using the exact saved candidate
coefficients. All three components and their first two derivatives are enclosed:

| Component | Profile | First derivative | Second derivative |
| --- | --- | --- | --- |
| Radius | `[1.320226, 1.320591]` | `[0.055354, 0.057397]` | `[0.770197, 0.787225]` |
| Normal coordinate 1 | `[0.426539, 0.430686]` | `[2.361770, 2.381495]` | `[0.232325, 0.374093]` |
| Normal coordinate 2 | `[-0.894348, -0.889702]` | `[2.808232, 2.831018]` | `[7.889472, 8.058986]` |

This concrete module checked in about nine seconds locally. It validates the
profile witness format and a whole phase interval, **not** trapping of a ladder
orbit in that cell or attraction around the full circle. The size and cost of
the full dynamic witness have not yet been established.

Reproduce this proposed preparation data, then validate it:

```
python3 scripts/generate_ladder_drift_profile_cell.py
scripts/lean.sh build DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftProfileCell
```

The next section extends that preparation through an entire ladder block.
The full output-chart bounds, whole-circle coverage and measurement residuals
remain necessary for the selected fixture's attraction and bounded response
errors. Those bounds would not by themselves prove exact long-time convergence,
uniform accuracy across controls and frequencies, or accuracy of the eventual
cheap UI approximation.

## Checked drifting block and attraction in physical coordinates

`Analysis/PolarTube/Attraction` now proves the final geometric implication of
a valid graph certificate in the original four-history state space. It gives
a continuous, injective invariant circle and the bound

```
distance(state_n, circle(current_phase_n))
  ≤ norm(inverse_linear_chart) * q^n * initial_transverse_error
```

Here the state and transverse norms are maximum-component norms, and `q < 1`
is the graph certificate's contraction rate. The comparison point follows the
actual current phase. This permits persistent phase motion; it does not claim
convergence to a fixed phase, shadowing of one prescribed circle orbit, or an
irrational rotation number. The **valid graph certificate remains an input**
to this theorem. It is not yet supplied for the concrete drifting fixture.

The concrete replay now covers every initial state with phase in
`[2.4998, 2.5002]` and each of the three transverse coordinates in
`[-0.000002, 0.000002]`. It runs all 162 samples of the actual tanh recurrence,
with the original one-sample feedback delay, gain 5, detector disabled, and
input `cos(n*pi/27)/4`. Physical coefficient enclosures are reused from the
locked fixture. Reduction of the input index modulo 54 is proved equal to
that sinusoid for every natural sample index.

| Module | Checked result |
| --- | --- |
| `DriftProfileCell` | Tight center-point profile bounds as well as the existing whole-cell profile and first two derivative bounds |
| `PolarTube/DecodeEnclosure`, `DriftBlockInitial` | Actual curved initial states and Jacobians lie in their boxes; a row-wise mean-value argument initializes the exact secant matrix |
| `DriftBlockCheck00`–`16`, `DriftBlockChecks` | All 162 centered state, saturation and matrix checks are discharged with kernel computation |
| `DriftBlockCell` | Every intermediate state and output observation is enclosed; the result also applies to the full processor's projected histories and output, including its final saturator |
| `DerivativeEnclosure.first_run`, `DriftBlockDerivatives` | The same matrix trace bounds the true first derivatives in the phase and three transverse directions, using separately established whole-state membership |
| `DriftBlockGeometry` | The output head has alignment greater than 1 with the input phase everywhere in this cell; it stays nonzero and the relative phase lift avoids its branch cut |

The derivative theorem does not identify a finite-difference secant matrix
with a Jacobian. It independently propagates the exact derivative recurrence
through the already checked boxes. The initial profile curvature is retained
because the decode is differentiated at every point in the curved input cell.

Reproduce and check the complete witness:

```
python3 scripts/generate_ladder_drift_profile_cell.py
python3 scripts/generate_ladder_drift_block_cell.py
scripts/lean.sh build DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockGeometry
scripts/lean.sh build DSP
```

Both producers are untrusted. Exact rational containment and analytic
transcendental bounds establish the generated data's meaning; binary floating
point proposals are not imported as facts. Regeneration reproduces the saved
witness byte for byte. The data file for this one block is about 541 KB; the
ten-sample kernel-check modules took roughly 31–38 seconds each locally, with
four modules built together. A full-circle certificate will need attention to
proof size and checking cost as well as the mathematical bounds.

The output-chart and centered trapping obligations for this input cell are
now closed as described below. **Remaining:** prove trapping and contraction
uniformly around the entire circle, assemble those bounds into the graph
certificate, and replay the measurement residuals. The concrete result does
not prove that the proposed drifting circle attracts. Audio, generated DSP,
snapshot publication and UI code are unchanged.

## Checked complete coordinate map for one drifting cell

`Certificates/DriftCellMap.actual_transverse_image` now proves that every
initial point in the phase interval `[2.4998, 2.5002]`, with each transverse
coordinate bounded by `2e-6`, maps after 162 actual samples to transverse
coordinates bounded by **`1.4e-6`**. The only hypotheses are the physical
fixture parameters and membership in that input cell. There is no assumed
Jacobian, numerical remainder, output profile, or trapping bound. The output
phase is generally in a different cell, so this theorem alone cannot be
iterated to infer attraction.

| Module | Result |
| --- | --- |
| `PolarTube/PhaseEnclosure` | A positive dot product and bounded cross product certify a real output-phase window; a proposed numerical `atan2` only selects its anchor |
| `DriftOutputCell` | Checked center and whole-cell phase windows, radii, reciprocals, Fourier profile and derivative bounds, branch safety, and center transverse defect below `1e-8` |
| `PolarTube/DecodeHessianEnclosure` | Exact initial phase/radial Hessian, including the Fourier-profile curvature and phase/radius cross terms |
| `DriftSecondInitial`, `DriftSecondCheck00`–`16`, `DriftSecondCell` | All 162 whole-cell Hessian and tighter center-only Jacobian checks, connected to actual derivative frames |
| `PolarTube/EncodeEnclosure`, `DriftSecondOutput` | First and mixed second derivatives of the complete curved output chart, including its polar and Fourier curvature |
| `CoordinatePartials` | The frames differentiate one common coordinate map and its Jacobian entries, rather than unrelated surrogate functions |
| `Analysis/CoordinateBounds` | Coordinatewise mean-value bounds over a rectangular cell, retaining each coordinate's own width |
| `DriftOutputBounds`, `DriftCellMap` | Center Jacobian plus uniform Hessian bounds give a sharp whole-cell Jacobian; that Jacobian and the center defect prove the actual transverse image bound |

The center output phase is certified within `3.5743532e-15` radians of the
rational anchor `2.2426155314739495`; the whole output phase is within
`0.0004723851900745565095` radians of that anchor. These are checked enclosures,
not claims of exact binary floating-point arithmetic. The rounded whole-cell
Jacobian bounds give transverse image estimates approximately
`[1.4242e-7, 6.1521e-7, 1.30794e-6]`, including the `1e-8` center-defect allowance;
the theorem uses the simpler common upper bound `1.4e-6`.

Reproduce the additional witnesses and replay:

```
python3 scripts/generate_ladder_drift_output_cell.py
python3 scripts/generate_ladder_drift_second_cell.py
python3 scripts/generate_ladder_drift_output_bounds.py
scripts/lean.sh build DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftCellMap
scripts/lean.sh build DSP
```

The current witness format is suitable for establishing the proof chain, but
not for copying blindly across the proposed 16,384-cell grid. The additional
second-order trace is about 1.28 MB for this one cell, on top of the 541 KB
centered-state trace. Repeating these traces would require about 30 GB of
source before compiled artifacts. The next certificate implementation needs
compact, checked recurrence evaluation or sharper/adaptive coverage. Merely
coarsening the existing grid has failed the proposed contraction bounds; no
coarser coverage or measurement residual has been certified here.

## Response approximation goal from reset

The chosen target is now a fixed-amplitude measurement, not a small-signal
limit and not a claim about every initial state. For prepared coefficients,
positive peak amplitude `A`, and frequency `0 < f < 1/2` cycles/sample:

1. Reset the exact-real tanh ladder, with its excess-energy detector disabled.
2. Apply `x[n] = A*cos(2*pi*f*n)` with input phase zero.
3. Observe the output after processing sample n, including the final saturator.
4. Measure coherent peak phasor `C_N = (2/N)*sum y[n]*exp(-i*2*pi*f*n)`
   and total power `P_N = (1/N)*sum y[n]^2`.

[ResponseGoal.lean](ResponseGoal.lean) defines this trajectory by calling the
existing `step` and `reset`. It changes no production behavior. The statement
`Meets` requires that, for every positive slack, **every sufficiently long**
measurement obeys

```
|C_N - C_hat| <= coherentError + slack
|P_N - P_hat| <= powerError + slack
```

This permits persistent jitter and does not require either measurement to
converge. It also permits a finite startup transient. The predictions are raw
phasor/power quantities; the corresponding plotted squared gains are
`|C_hat|^2/A^2` and `2*P_hat/A^2`. Amplitude remains a parameter. Zero amplitude,
DC and Nyquist are excluded from this input-power normalization.

### Explicit finite-spectrum curve

[ResponseMeasurement/FiniteSpectrum.lean](../../ResponseMeasurement/FiniteSpectrum.lean)
uses a finite real signal

```
y_hat[n] = sum_j Re(a_j * z_j^n),   |z_j| = 1.
```

For distinct, nonconjugate carriers away from DC/Nyquist, its exact long-time
curve is

```
C_hat = a_j at the input frequency (zero if that frequency is absent)
P_hat = (1/2) * sum_j |a_j|^2.
```

The general definitions retain all digital-frequency coincidences and cross
terms. Equal or conjugate frequencies cannot simply have their powers added;
their phasors interfere. The proofs sum geometric sequences directly, so they
do not identify a uniform phase integral with a time average by assumption.
Input harmonics, independent oscillator tones and mixed frequencies all fit
the same formula. Near-coincident frequencies can need long measurement windows.

[ResponseApproximation.lean](HarmonicBalance/ResponseApproximation.lean) connects
these sequences to finite Fourier waveforms on the existing HB phase torus.
Initial phases are included in the peak phasors. An on-orbit trajectory theorem
also connects the measurement back to the original processor, with the correct
one-sample indexing.

This is an explicit curve **given proposed spectral coefficients and
frequencies**. It is not yet a closed formula producing those coefficients from
cutoff, drive, resonance, input amplitude and frequency. The existing finite HB
solvers can propose them; their roots and branch choices still need validation.
The earlier first-order phase-reduction formula remains an unvalidated cheaper
candidate for the full discrete ladder.

### Proved comparison with full harmonic balance

[Residual.lean](HarmonicBalance/Residual.lean) defines all four waveform-equation
residuals, with the original delayed fourth-stage feedback. Zero residual is
proved equivalent to full HB, including **all** modes. A solved projection's
retained residual alone is insufficient.

Let `J` be an injective bounded linear preconditioner and `q < 1` bound the
Lipschitz constant of the equation-solver map `x -> x-J*R(x)` in a radius-r ball
around the candidate. Let `d` bound `|J*R(candidate)|`, with `d+q*r <= r`.
[ResidualValidation.lean](../../Analysis/ResidualValidation.lean) then proves a
nearby exact solution and

```
stateError <= d/(1-q).
```

It also proves that the derivative condition `||I-J*DR(x)|| <= q` throughout
that ball supplies the required solver bound. This is a root-solver condition,
not physical attraction. For a drifting solution, the free oscillator phase
prevents blindly assuming such a contraction: an augmented, phase-fixed
equation formulation is still required.

If `tail` bounds the difference between the candidate's **final saturated
output** and the finite output spectrum, set

```
L = |k+1| * |gain| / |tanhGain|
e = L*d/(1-q) + tail
B = sum_j |a_j|.
```

The proved comparison with a nearby exact HB response is

```
coherentError = 2*e
powerError = e*(2*B+e).
```

The error in the histories is propagated through the actual final normalized
tanh. Its generated harmonics are not discarded: `tail` is a separate required
bound. These are **conditional, parameterized estimates**, not a certified
numerical tolerance for our high-drive curves. The inverse/derivative, full
residual and output-tail hypotheses have not been discharged across those
parameters. A very large band would be rigorous but unhelpful; usefulness still
needs to be demonstrated.

### Connecting the curve to reset without proving convergence

`ResponseGoal.meets_of_coboundaries` proves a direct route to the selected goal.
For each observable (demodulated output or squared output), find a bounded
correction `V_n` such that

```
|observable_n - prediction - (V_n - V_(n+1))| <= error
|V_n| <= M.
```

The correction telescopes. The finite-window error is at most
`error + 2*M/N`, and hence satisfies the long-run goal. The correction may
oscillate forever. This uses the existing discrete averaging theorem and does
not assume an invariant density, attraction, or existence of a time-average
limit. Establishing the residual inequality and boundedness along the reset
trajectory remains the hard part; this theorem does not assume them proved.

The remaining tasks are therefore concrete: select the coefficient/branch
family (including a phase condition and oscillator frequency for drift),
certify useful HB comparison budgets, and establish a reset-trajectory
averaging certificate or another reset-to-response bound. The large one-cell
drifting witness above is not a substitute for these tasks. No new numerical
witness grid, audio processing, code generation or UI machinery is added here.

## Concrete driven curve and constructed error bounds

[DrivenApproximation.lean](HarmonicBalance/DrivenApproximation.lean) now defines
the actual first-order curve explored by `analyze_ladder_driven_response.py`.
`Prepared.response` takes input amplitude and frequency after preparing the
autonomous HB orbit, the two bordered forced first variations, and their output
phasors and power inner products. No measured response is used to fit them.
The structure alone does not certify those prepared values or the HB branch.

For harmonic m, free oscillator frequency Omega, frequency sensitivities
`nuC, nuS`, and input angular frequency omega, define

```
rho = sqrt(nuC^2 + nuS^2)
r = (omega/m - Omega)/(A*rho)
orientation = (nuC + i*nuS)/rho

M = orientation * (r + i*sqrt(1-r^2))                  |r| <= 1
M = orientation * sign(r)/(abs(r)+sqrt(r^2-1))         |r| > 1
```

The intended domain requires `m > 0`, `A > 0`, and `rho > 0`. This is a local
driven-orbit approximation around the prepared harmonic resonance, not a
validated formula for every drive setting or every input frequency. The curve
is explicit once prepared; it does not solve a new HB system at each frequency:

```
C_hat = y0*M + A/2*((yc-i*ys)*M^2 + yc+i*ys)

P_hat = P0 + 2*A*(crossC*Re(M) + crossS*Im(M))
      + A^2/2*(Gcc+Gss + (Gcc-Gss)*Re(M^2) + 2*Gcs*Im(M^2)).
```

Here y0 is the autonomous output phasor at harmonic m; yc and ys come from its
two driven waveform corrections. P0, crossC/crossS and Gcc/Gss/Gcs are the
corresponding waveform power inner products, including the final saturator.
The two observables are quadratic complex polynomials in the mismatch phase
(take the real part for power). Their identities are proved, including the
factor two on the cosine/sine power cross term.

Using M^2 here does **not** assert that an actual trajectory's second moment is
the square of its first moment. The constructed correction below bounds the
whole quadratic observer against this proposed value directly.

### A correction we can write down, not an assumed certificate

For the positive running branch, write `c = 1/(r+sqrt(r^2-1))`, so `0<c<1`.
In canonical phase coordinates the proposed moment is `M0=-i*c` and the flow is

```
W(z) = i*(1+c^2)*z + c*(z^2-1),       |z|=1.
```

For an observable `Q(z)=q0+q1*z+q2*z^2`, the finite correction is

```
V_N(z) = -i * sum(l=0..N-1) (i*c)^l *
         ((q1+q2*M0)*z^(l+1)/(l+1) + q2*z^(l+2)/(l+2)).
```

`PhasePower.Discrete.correction_identity` proves exactly

```
V_N'(z)*W(z) = (Q(z)-Q(M0))*(1-(i*c*z)^N).
```

Thus its uncancelled phase residual has the explicit bound
`T_N=(|q1|*(1+c)+|q2|*(1+c^2))*c^N`. Let B0, B1 and B2 be the finite
coefficient-sum bounds on V, V' and V'' defined by `size0/size1/size2`.
For the sampled phase recurrence

```
theta[n+1] = theta[n] + h*(1+c^2+2*c*sin(theta[n]))
```

the proved long-run error is

```
E_N = T_N + (B1+B2)*h*(1+c)^4/2.
```

The finite-window bound additionally contains `2*B0/(h*sampleCount)`.
This holds for **every initial phase**. It requires neither convergence of
the measured average nor irrational rotation. There is no interval witness
grid or fitted correction. For a perturbed phase update with error at most eta
radians per sample, the explicit replacement is

```
D = h*(1+c)^2 + eta
E_N(eta) = T_N + B1*(eta+D^2/2)/h + B2*D^2/(2*h).
```

`physical_running_curve` proves the change back to the phase convention of the
prepared HB sensitivities. The locked formula is defined and its algebra is
proved; a general locked-basin/perturbation theorem is not added in this pass.

### Fast carriers and full HB waveform error remain in the budget

A changing fast-carrier amplitude need not average to zero. The constructed
correction in `ResponseMeasurement/SlowModulation.lean` proves that a term
`a[n]*lambda^n`, with `|lambda|=1` and `lambda != 1`, has long-run average at
most `variation(a)/|1-lambda|` in norm. Its finite-window boundary also decays
as 1/sampleCount. `modulated_curve` combines all these terms with the slow
correction above. Aliases onto digital DC belong in the slow observer;
near-resonant terms have a larger, explicitly visible error cost.

[DrivenResidual.lean](HarmonicBalance/DrivenResidual.lean) bounds the proposed
waveform itself against the **unprojected** HB equations. It retains the base
orbit defect, the linear solve defect, and an explicit quadratic remainder in
A. The nonlinear remainder uses the proved global tanh curvature bound.
`FirstOrderTaylor.delayed_remainder` accounts for the frequency shift of both
the base orbit and its first variation. `moving_delayed_remainder` also retains
the change of mismatch phase between samples: its extra error is bounded by
`|A|*L_phase*|phaseChange|`. Dropping this term would incorrectly use a frozen
phase for a drifting response.

The input-stage theorem includes both delayed histories; the nonlinear-stage
theorem applies to each of the following three stages. `output_remainder`
propagates a certified distance to the full HB fourth stage through the final
saturator and its first-order expansion. These are parameterized estimates;
their base/linear defects and derivative bounds must concern the whole phase
domain, including omitted modes. The existing inverse/conditioning theorem is
still needed to turn a small full HB residual into a nearby full solution.

### Numerical diagnostic and remaining proof

`scripts/analyze_ladder_phase_corrector.py` evaluates the analytic budgets
using the existing exact-tanh HB proposals (drive 100%, resonance 75%, A=0.25,
harmonic 3). These are floating evaluations of the proved formulas, **not**
kernel-certified physical-ladder response intervals:

| Running ratio r | Predicted power | Phase-sampling power allowance | Coherent-phasor allowance |
| --- | ---: | ---: | ---: |
| 1.01 | 4.9540 | 0.02338 | 0.6165 |
| 1.20 | 4.9646 | 0.0015205 | 0.04015 |
| 2.00 | 4.9732 | 0.0005667 | 0.01499 |

The allowances exclude HB-to-phase error, waveform error, fast-carrier error
and reset selection. They show that the phase-sampling contribution can be
small for total power, while the coherent bound deteriorates near locking.
Twenty-one diagnostic phase trajectories (seven ratios, three initial phases)
satisfied the finite-window bounds. These runs check the formula implementation;
the parameterized Lean theorem supplies the proof.

**Still open:** validate the HB preparation and its full residual/conditioning,
bound the actual phase-model and observer errors in a useful parameter region,
and prove the reset trajectory enters and stays in that region. The new results
do not claim those obligations are discharged. They replace the previously
unspecified averaging correction by an explicit one, and expose the remaining
errors as quantities to bound rather than a requirement to prove convergence.
