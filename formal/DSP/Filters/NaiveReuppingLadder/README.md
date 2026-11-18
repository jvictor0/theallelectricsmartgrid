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
requirements explicit. RMS gain is the square root of power gain. Asymptotic
RMS is a seminorm on signals with suitable averages (a norm only after
identifying signals that differ by zero asymptotic energy). Coherent measurement
is a complex Fourier projection, not a norm of the full signal.

The proofs establish that decaying startup errors disappear from coherent
measurement and that a sinusoid converges to its expected complex amplitude.
Distinct unit-frequency modes average to zero, with a finite-window bound
`2/(N*|z-1|)` after demodulation. The detector bandpasses provide a complete
calibration: their nested coherent limit equals their existing transfer. The
RMS/power measurements currently have definitions, not analogous calibration
theorems.

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
