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

Small-signal definitions, derivative/transfer correctness, cutoff and nominal
oscillation theorems, and stability-region theorems remain the next work. The
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

The SVF transfer realization and the conditional time-domain convergence theorem
are separate: convergence of arbitrary initial SVF histories to the sinusoidal
response, with amplitude and phase identified by that transfer, remains to be
connected. The amplitude identification is an explicit hypothesis of the
detector-level ripple bound. No nonlinear ladder settling or sideband bound is
claimed by these results.

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
