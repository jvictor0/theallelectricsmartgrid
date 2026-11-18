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
