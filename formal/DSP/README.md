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
| [NormalizedAllPass](Filters/NormalizedAllPass.lean), [Proofs](Filters/NormalizedAllPass/Proofs.lean) | The comb's one-state normalized lattice: complex transfer, unity magnitude, radial memory loss and finite-horizon energy balance |
| [DelayBuffer](DelayBuffer/Basic.lean), [History](DelayBuffer/History.lean), [Transfer](DelayBuffer/Transfer.lean), [Energy](DelayBuffer/Energy.lean) | Executable circular storage, refinement to a shift register, multi-reader history, channel projection, q^N transfer and weighted storage balance |
| [SchroederAllPass](Filters/SchroederAllPass.lean), [Proofs](Filters/SchroederAllPass/Proofs.lean) | Raw-buffer allpass: arbitrary-input realization, complex mode response, unity magnitude and finite-horizon energy balance |
| [TunedDecayComb](Filters/TunedDecayComb.lean), [Proofs](Filters/TunedDecayComb/Proofs.lean) | Prepared scalar sample core: shared two-reader/one-writer feedback, radial allpass and TPT lowpass, settled/crossfade mode response and return-path energy |
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

The [Conservative Reupping Ladder](Filters/ConservativeReuppingLadder/README.md)
uses four sequential Euler stages, normalized saturation and delayed feedback
with a fixed coefficient `k = resonance * 0.975 * kCritical / slope^3`.
Its proofs connect derivatives of
the actual nonlinear trajectory to an exact digital transfer and establish
strict pole stability throughout the valid tuning range. Its scalar and
multichannel generated cores include coefficient snapshots and a proved
magnitude-squared kernel. The app offers it as a separate shape alongside the
handwritten adaptive ladder, with shared knob mapping and routing. The nonlinear
response is proved to approximate its tangent uniformly over all samples for
sufficiently small bounded inputs. Small sinusoids
settle to a continuous waveform of phase, and their time-first coherent
small-signal response equals the transfer. These are local, fixed-parameter,
exact-real results; global nonlinear attraction and floating-point stability
are outside their scope.

The [Multiband Saturator](Filters/MultibandSaturator/README.md) currently provides
an executable Lean core: the existing compensated LR splitter, per-band gain
and saturation, summation, and master gain/saturation. Its output includes the
audio values needed by external meters, and a separate entry point retains
mono-bass routing. A prepared coefficient bank reuses each crossover's tuning
across split and compensation sections. Time-first small-signal theorems connect
the actual scalar and mono-bass processors to the gain-weighted LR responses.
The common-phase LR factorization gives a proved real magnitude-squared kernel.
Stable-filter tracking and differentiable static-observation lemmas are reusable;
tanh calculus and phase facts are shared with the conservative ladder. Native
scalar/channel and measured-response tests complement the exact-real proofs.
The shared generator emits the four-band core, packed coefficient preparation,
mono-bass routing and power response. The app wrapper owns metering and sampled
UI history around that generated core.

`TPTLadder` is an idealized linear ladder, not the app's nonlinear `NaiveReuppingLadder`. Each
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

## Normalized lattice allpass

`NormalizedAllPass` models the allpass arithmetic in
[`TunedDecayComb::Branch::Process`](../../private/src/TunedDecayComb.hpp).
Its input is the integer-delay read after the comb's `r^N` attenuation. The
single persistent memory is `s`; `y` stores the current output:

```text
memory = r*s
y      = a*x + b*memory
s_next = b*x - a*memory
```

The executable generic step preserves the C++ multiply/add order and specializes
to `Float`. Exact proofs assume `a²+b²=1`; `sqrt_normalized` establishes this
for `b=sqrt(1-a²)` when `|a|≤1`. The stored coefficients stay fixed during a
`Step.states` run. The old stored output is not part of the dynamic memory.

`step_eq_discrete` and `run_eq` connect the actual step to a one-state
realization with `A=-a*r`, `B=b`, `C=b*r`, `D=a`. `discrete_transfer`,
`digital_modes` and `complex_transfer` prove

```text
H(z) = (a*z+r)/(z+a*r)
H(q) = (a+r*q)/(1+a*r*q), q=z^-1
```

These transfer statements explicitly exclude singular denominators.
`run_seriesEquation` also connects arbitrary zero-state input/output streams
through the numerator and denominator polynomials. `response_unit` proves
unit squared magnitude on the entire unit circle for `r=1` and `|a|<1`;
`frequency_response_unit` uses the shared `exp(-2πif)` frequency convention.
For `0≤r<1` and `|a|<1`, the section is intentionally damped rather than
unity-magnitude.

`step_energy` proves `y²+s_next²=x²+r²*s²`. Its lossless and contractive
corollaries cover `r=1` and `|r|≤1`. `trajectory_energy` sums this equality
over any finite horizon, including the residual memory and the accumulated
loss `(1-r²)*s²`. It permits different normalized coefficients at every sample,
provided each call carries the previous call's resulting memory. This does
not assert a stationary frequency response under modulation.

The proofs cover exact arithmetic. Native smoke tests compare the Float
lattice to the independent direct input/output recurrence and check energy
accounting with nonzero initial memory and changing coefficients. They do not
prove rounding bounds. The shared-buffer comb model below composes this lattice;
the lattice itself does not export C++ or replace the app's implementation.

## Shared circular delay

`DelayBuffer capacity R` stores a `Vector R capacity` and a bounded write head.
`read` takes `age : Fin capacity`, meaning a delay of `age.val+1` samples;
`ofDelay` converts an explicitly positive, bounded sample count to an age.
This covers capacity one and non-power-of-two capacities. Capacity zero has
no constructible head. Reads are pure; `push` changes one slot and advances
the head. The executable representation does not shift the whole history.
Lean's vector update can update an array in place when it has unique ownership;
the proofs make no allocation or real-time performance guarantee.

`address_mod` and `advance_mod` connect the bounded implementation to the
modular addressing in the C++ rings. The API requires `1 ≤ N ≤ capacity`.
In `AllPassFilter.hpp`, a configured delay of zero happens to address the same
slot as a full-capacity delay; it is not an instantaneous feedthrough. That
physical behavior is represented by `N=capacity`, not a zero-delay Lean reader.

The reusable contracts are:

- `history_push`: the circular update exactly refines `IIR.shift`, for every
  initial buffer, head and sample type.
- `read_trajectory`: from rest, any reader sees the write stream delayed by N.
  Several reads do not advance the buffer or create separate write streams.
- `read_map`, `push_map`, `readChannels_projection`: scalar channel projection
  commutes with storage; `readChannels` supports a different tap per channel.
- `read_series`: the same implementation contributes `q^N` in formal generating
  series. This theorem needs no transform-convergence assumption.
- `history_mode`: a geometric history advances as a delay mode under `q*z=1`.
- `weighted_energy`: for a fixed tap N, set
  `E_r = sum(i=0..N-1, r^(2i)*history[i]^2)`. Then
  `(r^N*read(N))^2 + E_r(next) = written^2 + r^2*E_r(old)`.
  `lossless_energy` specializes to `r=1`. The weighting belongs to the proof;
  the buffer stores raw samples and performs no attenuation.

The history laws remain valid when clients change taps between calls.
The transfer and energy statements use their stated fixed tap and weights;
they do not assert an unchanged storage metric under retuning.

## Schroeder allpass using the shared buffer

The executable `SchroederAllPass.step` matches the two FMA operations in
[`AllPassFilter.hpp`](../../private/src/AllPassFilter.hpp):

```text
d       = read(N)
y       = d - g*x
written = x + g*y
push(written)
```

`trajectory_series` and `realizes` derive its arbitrary-input, zero-state
transfer from `DelayBuffer.read_series`:

```text
H(q) = (q^N - g)/(1 - g*q^N)
```

`mode_transfer` evaluates the actual junction on geometric histories over
any field, including complex numbers, with an explicit nonzero denominator.
`response_unit` proves unit squared magnitude on the unit circle for real
`|g|<1`, reusing the normalized allpass's unit-circle theorem at `q^N`.

This realization's stored samples are not normalized lattice states. With
raw energy `E = sum(history[i]^2, i < N)`, `step_energy` proves

```text
(1-g^2)*y^2 + E(next) = (1-g^2)*x^2 + E(old).
```

For `|g|<1`, storage `S=E/(1-g^2)` is nonnegative.
`step_energy_normalized` gives `y^2+S(next)=x^2+S(old)`;
`trajectory_energy` telescopes it over any finite horizon, including arbitrary
initial storage and the residual tail. Gain and tap are fixed in that theorem.

## Tuned decay comb using the shared buffer

`TunedDecayComb.branchStep` models one prepared scalar channel's radial lattice,
feedback excitation, radial TPT integrators, lowpass gain and bypass clearing.
`step` accepts either `none` for a settled sample or `some mix` for a crossfade.
It reads both branches from the old buffer, computes the mixed output, and
pushes that output once. The settled path leaves the unused second branch alone.
The C++ coefficient tuner and transition scheduler are outside this kernel.

`step_buffer` and `read_trajectory` prove the shared-output history contract
even for arbitrary per-sample coefficients, taps and mixes. `read_series`
uses the very same buffer transfer theorem as Schroeder.

For frozen coefficients, `branch_mode`, `settled_mode` and `mixed_mode` derive
the response of the actual update under geometric memory/history relations.
They apply over complex numbers, explicitly require nonzero denominators, and
assume prepared contracts `a^2+b^2=1` and `h*(1+k*g+g^2)=1`. Put

```text
A(q) = (a+r*q)/(1+a*r*q)
L(q) = gain*g^2*(1+r*q)^2 /
       [(1+k*g+g^2) + 2*(g^2-1)*r*q + (1-k*g+g^2)*(r*q)^2]
R(q) = L(q)*sign*A(q)*delayGain*q^N
```

Bypass uses `L=1`. The settled response is `L/(1-R)`. The frozen crossfade
response is

```text
[(1-m)*L0 + m*L1] / [1 - ((1-m)*R0 + m*R1)].
```

This denominator is one shared feedback loop, not a mixture of two independent
closed-loop comb responses. `radial_return` substitutes the tuner contract
`delayGain=r^N` to combine the delay factors into `(r*q)^N`.
`Realization.lean` also proves `realizes_settled` and `realizes_mixed` for every
input stream from rest. These reuse the buffer's formal-series delay law and
eliminate all three branch memories. The stream denominators are proved
nonzero from their constant coefficients; this result needs no analytic
convergence assumption.

`return_energy` combines the buffer's weighted balance with
`NormalizedAllPass.step_energy`. `step_return_energy` applies it to the actual
mixed write: return-output energy plus lattice/buffer storage after the call
equals written-output energy plus `r^2` times prior lattice/buffer storage.
This proves the delay-plus-lattice return path's balance. The complete frozen
comb's pole and T60 results use the separate analysis below.

`Tests/DelayFilters.lean` compares native rings against an append-only history,
including capacities 1, 3, 8, 4096 and 16384 and repeated wraparound. It compares
Schroeder against an independent direct recurrence and checks accumulated
energy. For the comb it compares both branch factors against independent
direct recurrences, exercises settled and changing crossfade mixes, checks
bypass clearing and the return-path energy balance. Additional checks cover
controller request coalescing, copying, promotion and reset; three channels
with independent taps and bypass choices; and the complex/magnitude response
against Fourier sums of actual impulse samples, including DC and Nyquist.
These are numerical checks, not floating-point error proofs. Both scalar
kernels execute with `Float`; Schroeder also has a `Float32` smoke case.

### Comb tuning, stability and linearity

For a fixed external control schedule, the sample kernels are linear in the
audio input and numerical memories in exact arithmetic. Fixed coefficients,
taps and mix give a time-invariant input/output filter; a changing externally
specified schedule gives a linear time-varying filter. The coefficient tuner
is not linear in the controls. A fixed nonzero initial memory adds an
initial-condition response, making the map from input alone affine.
`Linearity.lean` proves branch, complete-step and arbitrary-schedule trajectory
superposition, using logical buffer histories rather than adding ring indices.
`Controller.tick_superposition` includes state copying and promotion.
Floating-point arithmetic does not satisfy exact superposition.

`Tuning.lean` defines the exact real logical tuner. Valid controls have positive
sample rate, feedback sign in `{-1,0,1}`, damping at least one octave, frequency
at most one quarter-cycle/sample, and a period fitting the chosen capacity.
It proves positive bounded integer delay, fractional delay in `(0,2)`,
`|a|<1`, `a^2+b^2=1`, the TPT normalization equation, `0<r<1`, and
`delayGain=r^N`. `Phase.lean` proves that the undamped prototype has unit lowpass
gain at the fundamental and half power at the clamped damping frequency. Its
integer delay and fractional lattice compensate the lowpass phase exactly,
including the half-cycle convention for negative feedback. Zero feedback
selects the dry response.

`Stability.lean` proves the peak-normalized lowpass and lattice bounds on the
closed unit disk. The integer delay gives a strict return-loop bound in the
radially scaled open disk. Consequently every root of the settled closed
denominator has `|z|≤r`. A frozen crossfade in `[0,1]` has a nonzero loop
denominator outside the larger branch radius as well. The tuned fundamental
attains radius `r`, and `prepared_fundamental_uncancelled` excludes numerator
cancellation there. The radius law `r^(sampleRate*T60)=1/1000` connects this
to a -60 dB modal amplitude bound, including an integer-sample bound after T60.

These are frozen modal guarantees. They do not assert a monotone envelope for
arbitrary sums of modes, passivity of the whole resonant comb, or a uniform
decay bound under unrestricted retuning. A changing controller is linear for
external controls, but linearity alone does not establish stability. A stronger
retuning guarantee would require a common storage bound or additional
constraints on the control schedule.

`Controller.lean` models accepted coefficient requests. Its proofs cover
latest-request coalescing, copying the current branch at fade start, preserving
history, one shared write, mix bounds, promotion and finite completion, and
reset to rest. Raw UI parameter comparison/clamping remains outside this
accepted-request boundary. `DelayChannelsProofs.lean` proves complete-step and
trajectory projection for both filters with independent channel taps and a
shared ring head, then lifts each scalar arbitrary-input transfer theorem to
the actual channel trajectory. These projection identities also hold for floating-point
operations because they require no ring laws.

`Response.lean` provides a captured coefficient/mix snapshot and executable
complex-component and squared-magnitude kernels. `ResponseProofs.lean` proves
that these equal the same frozen response realized by arbitrary input streams.
Pending targets do not affect the captured response until their fade starts.
Snapshots describe frozen coefficients at capture time, not the transfer of
an entire time-varying transition. The reference response uses a power of the
prepared complex unit delay; the generated response uses its proved equivalent
phase expression, avoiding work proportional to the delay length.

### Delay filter codegen and app integration

`DelayKernels.lean` supplies the executable, channel-wise arithmetic used by
the generated Schroeder and comb branch cores, comb coefficient preparation
and complex response. `DelayKernelsProofs.lean` connects those kernels to the
buffer-backed model, including the numeric bypass selector, exact logical
tuner and integer-delay phase identity. The numeric coefficient delay is an
integer-valued sample count; the wrapper converts it to a bounded index.

`DelayKernels/Runtime.lean` applies `Processor.compact` to these original
models. The comb keeps its three recurrence memories and returns `y` in
`Output`; the Schroeder arithmetic core keeps only gain and returns `y` and
`written`. The ring holds the actual delay memory separately. Simulation and
`run_eq` theorems establish identical complete output streams, for the generic
arithmetic including native floats, regardless of the old observation values.
The original rich models remain the native differential-test oracle.
The tuner exports pure `Prepare(input)` returning `Prepared`, with a definitional
proof that preparation is independent of previous coefficients.

`CircularBufferGeneric<Number,Size,Capacity>` owns interleaved fixed storage and
one head. Its generated address and advance calculations come from the Lean
definitions. Power-of-two capacities at most half the native unsigned maximum
use the equivalent modular expressions from `address_mod` and `advance_mod`;
`modulo_intermediates_bounded` proves that their intermediate sums and
subtractions fit without overflow or saturation. The C++ compiler reduces the
constant power-of-two remainder to a mask. Other capacities retain the general
conditional arithmetic. The storage lowering implements pure reads, one in-place write
per push and zero-fill on clear. That translation is tested rather than
verified. The app's `AllPassFilterGeneric` and `TunedDecayCombGeneric` use it
with their existing capacities, calling generated arithmetic cores through
their existing APIs. The comb wrapper retains parameter comparison, request
coalescing, crossfade scheduling and UI sampling. These adapter operations are
tested against scalar lanes; the controller's exact model is proved separately.
Stereo and quad aliases are provided alongside the existing mono/trio uses.

Each comb branch owns a persistent generated core. `SetCoefficients` installs
the arithmetic coefficients and native integer taps together while preserving
the filter memories; `Process` updates those memories in place. Snapshot
capture reads coefficients from that same core. Crossfade copy/promotion and
reset operate on the owning branch, with no reference members or per-sample
temporary core.

Coefficient preparation follows the existing transcendental abstraction:
proofs use exact real/complex functions, and generated trig lowers to
`Math4096::Sin2pi`, `Math4096::Cos2pi` and `Math4096::TanPi`, including one-lane
vector overloads for scalar exports. Exponentials, floor and square root use
`MultiChannelNumber` operations. `Math::ATan2(y,x)` wraps `std::atan2(y,x)`,
with a vector overload applying it per lane. `DelayMath` keeps phase in turns
and the exact log(1000) decay law in its mathematical interpretation. The app's
double-precision tuner uses libm for exponentiation, including tiny delay gains.

`test_dsp_delay_codegen.py` checks the stereo and quad comb in double precision
and the Schroeder all-pass at both precisions.
Buffer reads/head updates and processing results match native Lean bit for
bit. The storage checks include every head/age pair for small power-of-two
and non-power-of-two capacities, plus repeated wraps at production capacities.
Tuning and response comparisons allow the documented transcendental
approximation error. Fractional-allpass coefficients need a frequency-scaled
tolerance because subtracting a long integer delay amplifies phase error;
independent checks also reconstruct unity peak gain and complete fundamental
loop phase. Core tests exercise independent taps, zero-delay compatibility
(zero denotes a full buffer in the legacy allpass API), queued transitions,
bypass, reset and snapshots. Exact proofs do not make lookup tables or IEEE
arithmetic exact.

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

## Smooth random LFO

`SmoothRandomLFOGeneric<float, 4> lfo(48000.0, 20.0, seed)` creates four
independently timed lanes with a mean drawn phase velocity of 0.05 Hz. `Process()`
returns one vector of samples. `SetExponentialRate(sampleRateHz, lambda)` takes
the exponential rate for velocity in cycles per second, so the mean of a draw
is `1 / lambda`. Segment lengths are the reciprocals of the per-sample draws.
Their bounds are one sample and `2^48` samples. Rate changes apply when the LFO
draws a new span for its two-span lookahead.

Uniform `[0, 1)` draws are control values of a nonuniform cubic B-spline.
The generated evaluator uses three vector FMAs per sample. The generated
coefficient preparation runs only when a lane installs a span. `contDiff_join`
proves that adjacent cubics join with continuous value, velocity and acceleration
in sample time for positive spans of different lengths. The runtime preserves
fractional-sample overshoot across each knot. See the [Lean kernel](SmoothRandomLFO.lean),
[C² proof](SmoothRandomLFO/Proofs.lean), and [runtime](../../private/src/SmoothRandomLFO.hpp).

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
