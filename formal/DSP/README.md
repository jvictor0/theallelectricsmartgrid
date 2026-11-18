# Verified LTI filters

A reusable Lean 4.32.0 library connecting linear ODEs, analog transfer functions,
exact zero-order-hold discretization, digital transfer functions and executable
IIR processors. Mathematical proofs use exact real arithmetic (complex numbers
for analog frequencies). Floating-point approximation is trusted, as agreed for
this exercise. Nothing is integrated into the audio application.

The design follows the [updated shared conversation](https://chatgpt.com/share/6afa972b-c5ac-83e8-856d-b0bd084760a5?ogimg=plain).
The original scalar proof from the [earlier conversation](https://chatgpt.com/share/6afa6c9d-1ee4-83e8-a24d-dfc9fe9507d9?ogimg=plain)
is retained as independent regression evidence in `OnePoleZOHLowPass.Reference`.
Public ZOH lowpass correctness now follows the generic state-space/ZOH machinery.
`OnePoleTPTLowPass` is the trapezoidal counterpart, related to its analog
prototype through the bilinear transform. The trapezoidal (TPT) filters compose:
highpass, base-width band, state-variable filter, morphing SVF, four-pole SVF
cascades, an idealized ladder and a general Linkwitz–Riley splitter are each proved to realize
the bilinear image of their analog prototype.

## Pipeline: analog prototype to C++

Every TPT filter passes through the same stages. `TPTSVF` as an example:

1. **Analog prototype.** The transfer is a ratio of polynomials in s.
   [TPTSVF/Proofs](Filters/TPTSVF/Proofs.lean) defines `denominator g k` =
   s²+kgs+g² and `lowPassNumerator`, `bandPassNumerator`, `highPassNumerator`
   and `notchNumerator`.
2. **Generic recurrence.** [TPTSVF](Filters/TPTSVF.lean) defines
   `step : TPTSVF R → Sample R → TPTSVF R` once, for any arithmetic `R`. It is
   state-only: lowpass, bandpass, highpass and notch are stored fields.
3. **Proof over a field.** `realizes_lowPass` (and the bandpass, highpass and
   notch versions) proves that, from `AtRest` with `Tuned` coefficients, the
   stored field realizes `analog (lowPassNumerator g) (denominator g k)`: the
   prototype with s replaced by the bilinear σ. `streams` derives the analog
   SVF's integrator equations from the recurrence using the shared `integrator`
   lemma. Composites reuse these theorems: `MorphingTPTSVF`, `TPTSVF4Pole` and
   `LinkwitzRileySplitter` never re-analyze the SVF recurrence.
4. **Float specialization.** `stepFloat` is the same `step` at `R = Float`;
   the proved and exported functions share one definition.
5. **Export.** [GenerateDSP.lean](../GenerateDSP.lean) runs
   `#generate_cpp DSP.TPTSVF.stepFloat as TPTSVF`, and the
   [Cpp](Codegen/Cpp.lean) backend translates the elaborated function to
   [TPTSVF.hpp](generated/TPTSVF.hpp). `make lean-dsp-generate` refreshes the
   reviewable copy.
6. **Differential check.** `make lean-dsp-test` runs the Lean `stepFloat` and
   the compiled header side by side, requires bit-identical results, and audits
   the theorems' axioms.

The proofs cover the exact-arithmetic interpretation of the shared recurrence.
Floating-point specialization and C++ export are tested; floating-point error
and compiler correctness are outside the theorems.

ZOH and TPT discretize the same analog prototype differently. ZOH solves the
ODE exactly for an input held constant across each sample, so its samples equal
the analog trajectory at every sample instant (`zoh_sampledState`): it is exact
in the time domain. That exactness does not compose. The signal between two
stages is not piecewise constant, so the ZOH of a cascade or feedback loop is
not built from the ZOHs of its parts. TPT substitutes the bilinear
σ = (1-q)/(1+q) for s. Substitution is a ring homomorphism, so TPT is
functorial: sums, cascades and feedback of analog prototypes map to the same
connections of their realizations, and each composite's theorem is assembled
from its parts'. In exchange, TPT is exact in frequency, up to warping, rather
than sample-exact in time.

## Representations and files

| Files | Definitions and principal results |
| --- | --- |
| [Signals](Signals.lean), [Filter](Filter.lean) | Signal sampling and the independent analog/digital `Implements` contract |
| [Processor](Processor.lean), [Run](Processor/Run.lean), [Simulation](Processor/Simulation.lean) | `State → Input → State × Output`, stream execution, uniqueness and simulation |
| [Step](Step.lean) | State-only `State → Input → State` filters, the shared `Sample` input, trajectories, component projection and invariants |
| [FMA](FMA.lean), [FMA.Algebra](FMA/Algebra.lean) | Explicit fused multiply-add for `Float`, `Float32` and channels; exact multiply-add interpretation for the proofs |
| [IfLess](IfLess.lean) | The per-channel select `a < b ? x : y` for `Float`, `Float32` and channels, and its lawful scalar form |
| [Transfer.Bilinear](Transfer/Bilinear.lean) | Streams as Laurent series, the bilinear homomorphism, `Realizes`, the trapezoidal integrator and composition lemmas |
| [StateSpace.Basic](StateSpace/Basic.lean) | Fixed-dimensional A/B/C/D, ordered executable arithmetic, `advance`, `observe`, `processor`, `processorNext` |
| [StateSpace.Digital](StateSpace/Digital.lean) | Matrix interpretation, digital realization uniqueness, `observeNext_equiv` |
| [ODE.LinearFlow](ODE/LinearFlow.lean), [StateSpace.Analog](StateSpace/Analog.lean) | Matrix-exponential flow, input integral, actual held trajectories, interval existence/uniqueness |
| [Transfer.Basic](Transfer/Basic.lean), [Analog](Transfer/Analog.lean), [Digital](Transfer/Digital.lean) | Resolvent domain, unique transfer modes, differential and discrete trajectory correspondence |
| [Transfer.Rational](Transfer/Rational.lean) | Determinant/adjugate polynomial presentation; `transfer_eq_rationalEvaluation` |
| [Transfer.Realization](Transfer/Realization.lean), [ODE.Companion](ODE/Companion.lean) | Proper monic rational coefficients, companion state realization, arbitrary-input scalar derivative-jet correspondence and `ode_hasTransfer` |
| [Discretization.ZOH](Discretization/ZOH.lean) | `zoh_interval`, `zoh_sampledState`, `heldRealization`, `heldRealization_unique` |
| [IIR.Basic](IIR/Basic.lean), [Proofs](IIR/Proofs.lean) | Executable histories and recurrence, `history_invariant`, `iir_run_iff_recurrence` |
| [Transfer.IIR](Transfer/IIR.lean), [IIR.Realization](IIR/Realization.lean) | Formal generating series; normalized causal polynomial realization and `realize_series` |
| [Transfer.Delay](Transfer/Delay.lean) | State-space delay polynomials, `run_seriesEquation`, `toIIR_correct` for every input stream |
| [Discretization.Pipeline](Discretization/Pipeline.lean) | Complete analog → ZOH → rational → IIR output correspondence, at sample starts or hold endpoints |
| [OnePoleZOHLowPass](Filters/OnePoleZOHLowPass.lean), [Proofs](Filters/OnePoleZOHLowPass/Proofs.lean), [Transfer](Filters/OnePoleZOHLowPass/Transfer.lean) | ZOH lowpass specialization of the generic update, sampling theorem and rational transfer |
| [OnePoleTPTLowPass](Filters/OnePoleTPTLowPass.lean), [Proofs](Filters/OnePoleTPTLowPass/Proofs.lean) | Trapezoidal lowpass: state-space realization, trapezoidal rule, bilinear transfer, IIR realization and `realizes` |
| [OnePoleTPTHighPass](Filters/OnePoleTPTHighPass.lean), [TPTBaseWidth](Filters/TPTBaseWidth.lean) | Highpass as input minus a stored lowpass; highpass into lowpass band |
| [TPTSVF](Filters/TPTSVF.lean), [MorphingTPTSVF](Filters/MorphingTPTSVF.lean), [TPTSVF4Pole](Filters/TPTSVF4Pole.lean) | The app's linear SVF with stored lowpass/bandpass/highpass/notch/all-pass, its equal-power morph and two-stage cascades |
| [TPTLadder](Filters/TPTLadder.lean), [LinkwitzRileySplitter](Filters/LinkwitzRileySplitter/README.md) | Idealized zero-delay four-pole ladder; general LR4 tree with all-pass phase compensation and flat reconstruction |
| [Transfer.Channels](Transfer/Channels.lean), [Filters/Channels](Filters/Channels.lean) | Pointwise multi-channel filters: channel projection, naturality and per-channel `Realizes` for every TPT filter |
| [Coupled](Filters/Coupled.lean) | Two-state executable example with nested coefficients, cross-coupling and direct feedthrough |
| [Codegen](Codegen/README.md), [Audit](Audit.lean) | C++ export, native differential checks and transitive axiom checks |

Each filter's runtime definition is beside its `Proofs` module.
`import DSP` brings in the theory. Import a runtime module such as
`DSP.Filters.TPTSVF`, `DSP.StateSpace.Basic` or `DSP.IIR.Basic` to use only
Lean's executable core, without Mathlib. Runtime definitions require arithmetic
operations rather than field laws, so they accept `Float` and `Float32` without
asserting false algebraic identities about floating-point arithmetic. `FMA`
follows the same arithmetic-operation pattern as `IfLess`: native `fma`/`fmaf`
for floating point, pointwise for channels, and exact `a*b+c` in the proofs.
`LawfulFMA` is available only for the exact interpretation. Scalar and channel
proofs continue to cover the shared steps; runtime tests check the fused
rounding. Explicit FMA can change low bits compared with separate operations.

## State space, transfer and ODEs

The common coefficient record is `StateSpace n R`:

```
continuous: q'(t) = A q(t) + B u(t),  y(t) = C q(t) + D u(t)
discrete:   q[k+1] = A q[k] + B u[k], y[k] = C q[k] + D u[k]
```

The interpretations are separate. A discrete `advance` is executable, and
`advance_eq` proves that its ascending FMA accumulations equal matrix
arithmetic over exact rings. `snapshot` evaluates every new coordinate before
returning the state; histories do the same on shift. This prevents function-backed
vectors from retaining deferred computations across samples. The initial state
is always explicit.

At regular frequencies, `transfer z = D + C (zI-A)⁻¹ B`. `RegularAt` requires
`det(zI-A) ≠ 0`; no theorem silently evaluates an inverse at a pole. Analog
transfer is characterized through actual `exp(s t)` trajectories, digital
transfer through actual geometric trajectories. This is an exponential-mode
semantics, not a formalization of the Laplace transform. The separate formal
series theorem below establishes arbitrary-input digital behavior.

`RationalCoefficients n K` stores a monic denominator of degree n+1 and a proper
numerator, including direct feedthrough d:

```
A(s) = s^(n+1) + Σ a[i] s^i
B(s) = d s^(n+1) + Σ b[i] s^i
```

The companion realization has a derivative chain, final row `-a`, final input
coefficient 1, output row `b-d*a`, and direct feedthrough d. `JetSolution`
expresses the scalar equations **A(D)v = u, y = B(D)v**, using a common auxiliary
signal v and its derivatives. `stateSolution_iff_jet` and
`jet_to_stateSolution` prove correspondence with actual companion ODE solutions
for arbitrary inputs. The jet makes the required differentiability explicit;
we do not assume derivatives of a discontinuous held input exist. This API takes
normalized coefficient arrays; it is not a symbolic parser for arbitrary ODEs.

Generic zero-dimensional state space also represents static gains. Equal
rational transfer does not identify hidden state, initial conditions or internal
stability. The state-space object is retained through discretization.

## Exact sampling and output timing

For hold duration T≥0, ZOH constructs

```
Ad = exp(T A)
Bd = ∫[0,T] exp(r A) B dr
Cd = C, Dd = D
```

No inverse of A is needed, so singular systems are included. A held realization
consists of continuous local trajectories on [0,T], joined at their endpoints.
The ODE holds on each open interval; derivatives are not required at input jumps.
`heldRealization` constructs this object, uniqueness identifies every such
solution, and `zoh_sampledState` proves all samples equal processor execution for
arbitrary input streams and arbitrary initial states.

Standard observation reports C q[k]+D u[k]. Updated-state observation reports
C q[k+1]+D u[k]; `observeNext` realizes it as a standard processor with
C'=CA and D'=CB+D. The endpoint still uses **the input held on that interval**.
With nonzero D, that can differ from the right-side value after the next input
jump. Both conventions have end-to-end theorems in `Discretization.Pipeline`.

## Digital rational functions and IIR realization

Digital polynomials use **q=z⁻¹**, the delay variable:

```
M(q) = I-qA
Q(q) = det M(q)
P(q) = D Q(q) + q C adj(M(q)) B
```

`delayDenominator_constant` proves Q(0)=1. `run_seriesEquation` proves
Q(q)Y(q)=P(q)U(q) for arbitrary input sequences with zero initial state. Formal
power series need no analytic convergence assumption. The adjugate proof avoids
dividing by a determinant.

`IIR.ofPolynomials p q` normalizes coefficients by q(0); its realization theorems
require q(0)≠0. The executable processor stores input/output histories and uses

```
y[k] = b0*u[k] + Σ b[i]*u[k-i-1] - Σ a[i]*y[k-i-1]
```

Negative-time samples are zero. `history_invariant` connects the actual stored
state to those delays; recurrence uniqueness gives both directions of
`realize_series`. `StateSpace.toIIR_correct` then proves that the synthesized
IIR and original state-space processor have identical outputs for every input
stream from zero state. Nonzero state/history requires a separate state mapping;
we do not discard its natural response.

## Generated ZOH lowpass

[OnePoleZOHLowPass.hpp](generated/OnePoleZOHLowPass.hpp) uses the app's
`OPLowPassFilter` convention, where alpha weights the new input:

```
alpha = 1 - exp(-T/τ)
yNext = fma(alpha, x, (1-alpha)*y)
```

This matches the app's recurrence, including its explicit fused multiply-add.
`OnePoleZOHLowPass.onePoleStep` delegates to `StateSpace.advance` in dimension one;
`step` stores the updated output `y` as the state.
`zoh_advance_eq` identifies that operation with generic exact discretization;
`exported_zoh_step` relates the exported processor's real interpretation to the
analog endpoint. The analog transfer is 1/(1+τs); the updated-state digital delay
transfer is alpha/(1-(1-alpha)q). `lowpass_iir_equivalent` establishes its
direct-form IIR realization. The original scalar calculus proof remains in
`Reference` and is not a premise of these public ZOH proofs.

The coefficient-only `Snapshot R` captures the processing step's `alpha`.
[Response](Filters/OnePoleZOHLowPass/Response.lean) names the shared rational
formula `transfer alpha q = alpha / (1 - (1 - alpha) * q)`. The existing
`realizes` theorem now uses that definition with the formal one-sample delay.
[Response.Proofs](Filters/OnePoleZOHLowPass/Response/Proofs.lean) specializes it
to complex `q = exp(-2πif)`, with `f` in cycles per sample, and proves

```
|H(f)|² = alpha² / (alpha² + 4(1-alpha) sin²(πf)).
```

`Snapshot.magnitudeSquared` is the executable real-arithmetic kernel. It accepts
the prepared `sin(πf)` value and uses explicit FMA. At zero alpha it selects a
denominator of one before division, giving the zero response from rest even at
DC. The raw denominator is proved positive for `0 < alpha ≤ 1`.
`magnitudeSquared_eq_normSq` connects this kernel to the complex transfer;
`magnitude_eq_norm` proves that taking its square root gives magnitude.
`magnitudeSquared_channels` lifts the identity to independently captured channel
coefficients at a shared frequency. These are exact-real identities; native
Float/Float32 checks cover the implementation, not a universal rounding bound.
The complex definition is proof-only. Response C++ emission is a subsequent step.

## Generated TPT lowpass

[OnePoleTPTLowPass.hpp](generated/OnePoleTPTLowPass.hpp) stores the solved
zero-delay-feedback coefficient G = g/(1+g), the trapezoidal integrator state s
and the most recent output y:

```
delta = x - s
y = fma(delta, G, s)
sNext = fma(delta, G, y)
```

The stored output observes the current input. `exported_real_step` and `run_eq` prove
that this operation order equals the standard-observation state space
A = 1-2G, B = 2G, C = 1-G, D = G for every input stream and initial state.
`trapezoidal` proves y[n+1] = y[n] + g((x[n+1]-y[n+1]) + (x[n]-y[n])), the
trapezoidal rule for y' = ω(x-y) with g = ωT/2.

The bilinear variable is normalized: the physical prototype ωc/(s+ωc) at
s = c(z-1)/(z+1) equals the prototype g/(s+g) at (z-1)/(z+1), with g = ωc/c.
Plain trapezoidal integration has c = 2/T; prewarping the cutoff gives
g = tan(ωcT/2). `bilinear_transfer` proves that the digital transfer at z equals
the prototype's at (z-1)/(z+1), away from z = -1 and the pole; `bilinear_modes`
states the same result for geometric digital and exponential analog modes over ℂ.
`iir_equivalent` proves that the executable processor, started at rest, realizes
G(1+q)/(1-(1-2G)q). Unlike ZOH, bilinear correspondence is exact in frequency,
with warping, rather than sample-exact in time.

## State-only filters and composition

The TPT filters are state-only: `step : State → Sample R → State`, and every
observable response is a stored field. Specifications are then statements about
fields. A stream u is the power series Σ u[n] qⁿ in the delay q, embedded in
the field of Laurent series so transfers can be divided. The bilinear map `bil`
substitutes s := σ = (1-q)/(1+q) into analog polynomials; it is a ring
homomorphism, so products, sums and feedback of analog prototypes map to the
same connections of their realizations. `analog N M` is bil N / bil M, and

```
Realizes step initial field H  :=  ∀ u, Σ field(state[n+1]) qⁿ = H · Σ u[n] qⁿ
```

The coefficients are fields of `initial`, so each theorem states which
coefficients it assumes and that the integrators start empty. Every zero-delay
loop is well posed exactly when its analog denominator is nonzero at s = 1
(`bil_ne_zero`): 1+g, 1+kg+g², or ∏(1+gᵢ)+k∏gᵢ. The coefficient hypotheses imply
this except for the ladder, which assumes 1 + k G₁G₂G₃G₄ ≠ 0.

All proofs reuse a few lemmas:

- `integrator`: from a zero state, icₙ₊₁ = 2vₙ - icₙ and vₙ = icₙ + g wₙ give
  σ V = g W. Each trapezoidal integrator in every filter discharges this.
- `Realizes.project`: a component that receives the composite's input keeps
  its own specification inside the composite.
- `Realizes.cascade` (and `driven`): a component driven by another stored field
  multiplies that field's transfer by its own.
- `Realizes.add`, `sub`, `mul`, `complement`, `congr`: stored fields that are
  sums, coefficient multiples or input differences of realized fields.

| Filter | Stored output | Realized analog transfer |
| --- | --- | --- |
| `OnePoleTPTLowPass` | `y` | g/(s+g) |
| `OnePoleTPTHighPass` | `y` (input minus stored lowpass) | s/(s+g) |
| `TPTBaseWidth` | `lowPass.y` | g₂s/((s+g₂)(s+g₁)) |
| `TPTSVF` | `lowPass`, `bandPass`, `highPass`, `notch` | g², gs, s², s²+g² over D = s²+kgs+g² |
| `MorphingTPTSVF` | `output` | (w_low g² + w_band gs + w_high s²)/D |
| `TPTSVF4Pole` | `stage2.highPass`, `lowPass`, `bandPass` | product of the two stages' responses |
| `TPTLadder` | `stage4.y` | g₁g₂g₃g₄/(∏(s+gᵢ) + k g₁g₂g₃g₄) |
| `LinkwitzRileySplitter` | `bands i` | split responses along the path, compensated by sibling all-pass factors |

`TPTSVF` uses the app's former handwritten SVF recurrence with explicit FMA and
a1 = 1/(1+g(g+k)), a2 = g a1, a3 = g a2; `streams` derives the analog SVF's two
integrator equations and summing junction from the stored responses, and the
four transfers are solved from them. The notch is stored as lowpass plus
highpass, so its theorem is the sum of theirs.

`MorphingTPTSVF` stores the clamped `blend` and its `cosine` and `sine`, computed
by the caller. Like the app's `GetMorphedOutput`, it mixes both the
lowpass/bandpass and the bandpass/highpass halves, using FMA for each sum,
then selects with `IfLess 0.5 blend`, the app's `Vector::IfLess`. `IfLess` is a
small arithmetic operation: an ordinary branch on `Float`, and pointwise on
channels, so every channel picks its own half. `LawfulIfLess` exposes a scalar
selection's decision as a `Bool`; `ℝ` decides by its order. The
morph is part of the analog model: its numerator is the weighted sum of the
three SVF numerators. `realizes_blend` states the transfer for a blend
parameter with the app's clamp and quarter-turn phase; `blendWeights_power`
proves the morph is equal power, and `numerator_endpoints` that blends 0, 1/2
and 1 are exactly lowpass, bandpass and highpass.

`TPTSVF4Pole.stepHighPass` is the app's former handwritten four-pole HP; the lowpass
and bandpass variants feed and read the corresponding response. Stages may have
different gains and dampings.

`TPTLadder` is an idealized linear ladder, not the app's nonlinear ladder. Each
TPT stage is affine in its input, y = S + G x, so the global loop e = x - k y₄ is
solved in closed form: S is the cascade's response to zero input, and
e = (x - k S)/(1 + k G₁G₂G₃G₄). `loop_solved` proves the stored `drive` satisfies
the loop exactly, and `realizes` composes the four stages with the loop. Stages
may have different gains; `realizes_equal` gives g⁴/((s+g)⁴ + k g⁴).

[LinkwitzRileySplitter](Filters/LinkwitzRileySplitter/README.md) uses shared
SVF lowpass and all-pass taps in a general balanced tree. Each node has true
LR4 branches, and sibling all-pass compensation makes the sum have unit
magnitude for any positive band count. Its proofs cover actual stored band
outputs, logical tuning, snapshot capture and independent channels. Only the
four-band processing/tuning/snapshot specialization is emitted.

These are transfer statements from rest. Responses from a nonzero state,
parameter changes between samples and floating-point error are outside these
theorems.

Logical coefficient preparation is generated for the executable filters.
Matrix exponentials, integrals
and symbolic polynomial manipulation are mathematical construction APIs, not
floating-point coefficient routines emitted into `Process`. The code generator
specializes a chosen executable arithmetic processor with fixed dimension.

## Multi-channel filters

The app's `MultiChannelNumber` is the product ring Kⁿ with pointwise
arithmetic. Each filter record is generic in its arithmetic, so `TPTSVF (Fin n → K)`
is an n-channel SVF with per-channel coefficients and the same `step`. Kⁿ has
zero divisors and is not a field, so it does not fit `Bilinear` directly.
Instead, [Transfer.Channels](Transfer/Channels.lean) lifts scalar theorems:

- Every filter record derives `Functor`; `channel i` reads every scalar at i.
- `Natural multi scalar`: stepping n channels and reading channel i equals
  stepping channel i alone.
- `Channels.realizes`: if a step is natural and each channel's scalar filter
  realizes H i, then channel i of the stored field realizes H i for channel i of
  every input.

[Filters/Channels](Filters/Channels.lean) instantiates this for every TPT
filter. Lean has no parametricity theorem, so naturality is checked once per
step, but each check is `rfl`. Each `realizes_*_channels` theorem is a one-line
application of the scalar theorem, with per-channel hypotheses and transfers
such as g_i²/(s² + k_i g_i s + g_i²). Every coefficient is per channel,
including the morph's blend: `realizes_blend_channels` gives each channel the
transfer of its own blend. Decimal literals such as the morph's 0.5 are the
same in every channel, like `Vector::Splat`.

[OnePoleZOHLowPass/Channels](Filters/OnePoleZOHLowPass/Channels.lean) lifts the
ZOH lowpass too. `states_channel` identifies each channel of the actual vector
state trajectory with the scalar step, and `realizes_channels` gives the
digital transfer α_i/(1 - (1 - α_i)q) from rest. This uses the shared stream
semantics without a bilinear substitution. `step_matches_interval_channels`
and `analog_matches_digital_channels` reuse the scalar ZOH correctness proofs:
with α_i = 1 - exp(-T/τ_i), every channel agrees exactly with its held-input
analog solution at the interval endpoint and at every sample time. Channels
share the sample period T but may have different time constants, arbitrary
initial states and independently changing input streams.

These theorems cover the exact model over `Fin n → K`. The
`#generate_cpp_generic` target interprets the same generic `step` with
`MultiChannelNumber<Number, Size>` arithmetic, producing `Generic` headers for
the ZOH and TPT filters and the coupled state-space example. Numeric fields
and temporaries are vectors, literals are splats, and the morph uses
`Vector::IfLess`; explicit FMA uses `Vector::FusedMultiplyAdd`. As with scalar export, the shared recurrence connects the
proofs and code; the exporter is tested rather than verified. See the
[codegen guide](Codegen/README.md) for the interface and supported operations.

## Build and evidence

```
make lean-setup       # initial local toolchain setup
make lean-cache      # initial Mathlib cache
make lean-dsp-generate
make lean-dsp-test
```

The DSP target builds the theory with warnings treated as errors, audits all
transitive DSP declarations for unexpected axioms/admissions, runs native
Float/Float32 checks, generates C++ and compares native Lean/C++ cases bit
for bit. It also checks multichannel trajectories against native Lean at both
precisions and widths 1, 2, 3 and 4. The coupled example exercises every stored
coefficient, two interacting state variables, nonzero feedthrough and
simultaneous state updates.

The exact model is proved in Lean. The exporter and C++ compiler are tested,
not formally verified; floating-point approximation is trusted. The app uses
the generated filter adapters described above; the independent encoder smoother
is outside this DSP implementation.

## Logical coefficient tuning

All generated filters now include nested `LogicalSpec` records and in-place
`ComputeCoefficients` methods. See the [per-filter tuning theorem inventory](Filters/Tuning/README.md)
for cycles/sample conventions, cutoff and peak results, frozen stability regions,
Math4096 lowering, and intentional differences from the handwritten app.
