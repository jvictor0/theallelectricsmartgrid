# Proved response kernels for the generated TPT filters

Every generated TPT filter now has a coefficient `Snapshot` and static squared
magnitude methods in its processor header. Single-output exports use
`MagnitudeSquared`; `TPTSVF` exposes `LowPassMagnitudeSquared`,
`BandPassMagnitudeSquared`, `HighPassMagnitudeSquared`, and
`NotchMagnitudeSquared`. `LinkwitzRileyCrossover` exposes the lowpass and
highpass methods. All use the existing C++ `ResponseFrequencyGeneric`.
The three four-pole processor variants each use `MagnitudeSquared` for their
selected output. `CoupledProcessor` remains a codegen fixture, not a filter
with one of these response specifications.

## What a snapshot means

Snapshots describe frozen coefficients. They omit integrator state and stored
audio outputs. The stream realization theorems describe a filter starting at
rest with those coefficients, not a time-varying filter's transient trajectory.

The TPT snapshots retain the original integrator gains `g`. The processors
store solved coefficients such as `G = g/(1+g)` or the SVF's `a1`, `a2`, `a3`.
The caller must retain the gains used for tuning and capture them with the
matching damping and mixing coefficients. Reconstructing gains by dividing
rounded solved coefficients is unnecessary. `Realizes.lean` states these tuning
relations explicitly; it does not accept an arbitrary unrelated snapshot.

The morph snapshot stores the three weights calculated from the processor's
blend, sine and cosine. The `realizes_snapshot` theorem uses the existing
`stateWeights` definition. Calculate/capture those weights once, outside the
frequency loop. The generated response supports any real weights, including
negative weights; it preserves their relative phase.

Snapshot capture, thread-safe sampling and the app's UI adapter remain caller
work. This change generates and proves response arithmetic; it does not replace
the app's audio processors or connect generated snapshots to its plotter.

## How the proof connects to the code

There are four steps, all checked by Lean:

1. Existing `realizes` theorems connect each executable processor step to its
   rational stream transfer. `Realizes.lean` reuses them to express that transfer
   using the new snapshot's named transfer function.
2. `DSP.Response.halfAngle_coordinates` connects the digital delay `q` to the
   prepared sine/cosine coordinates. Homogeneity lets the same named transfer
   use these coordinates without division by a trigonometric value.
3. `Proofs.lean` proves that the executable magnitude kernel, interpreted over
   real numbers, equals the squared complex norm of that transfer.
4. `Channels.lean` proves that selecting a lane commutes with the executable
   kernel. It applies the scalar correctness theorem independently to every
   lane, including different prepared frequencies in different lanes.

Codegen consumes those same executable Lean kernels. Proofs are checked before
release but are erased, not translated into C++. The backend is tested, not a
verified compiler. Native Lean and generated C++ are compared bit for bit with
`Float`/`Float32` and SIMD widths 1, 2, 3 and 4. A separate complex calculation
checks physical frequencies, endpoint behavior, mixing and unequal stages.
The real proofs are not IEEE rounding-error bounds or proofs about the app's
approximate trigonometric preparation.

## The shared frequency lemma

Write `s = sin(pi*f)`, `c = cos(pi*f)` and `q = exp(-2*pi*i*f)`, where `f` is in
cycles per sample. Lean proves there is a nonzero complex `t` such that

```
1 - q = t * (i*s)
1 + q = t * c
```

In fact `t = 2*(c - i*s)`, which cannot vanish because `s*s + c*c = 1`.
Each section's numerator and denominator have the same degree in the pair
`(a,b)`. Multiplying both coordinates by `t` therefore cancels from the ratio.
We can evaluate the digital transfer using `(a,b)=(i*s,c)`, including Nyquist
where `c=0`. There is no `tan(pi*f)` in the executable kernels.

This is algebra on finite rational expressions. It does not claim that an
arbitrary infinite Laurent series can be evaluated on the unit circle.

## Composition: cascades

If two stages have transfers `H1` and `H2`, the existing stream theorem
`Bilinear.Realizes.cascade` proves that feeding stage one's output into stage
two gives `H2*H1`. It uses the actual processor step and its projections onto
the two stage states.

For plotting, the new `DSP.Response.cascade` lemma says

```
m1 = normSq(H1)  and  m2 = normSq(H2)
  implies
m1*m2 = normSq(H1*H2)
```

Consequently the base-width response is the highpass squared magnitude times
the lowpass squared magnitude. Each four-pole SVF response multiplies the two
selected stage responses. Each crossover branch does the same with its own
second stage. Their gains and dampings may differ; the proof never assumes
identical stages. Squaring one stage's response is only the equal-stage case.
No square roots are needed until the UI wants ordinary magnitude.

## Composition: mixing

For an SVF let `u=g*c` and `D=(u*u-s*s)^2+(k*u*s)^2`. Its low, band and high
complex numerators over the common denominator are `u*u`, `i*u*s`, and
`-s*s`. A weighted sum therefore has squared magnitude

```
((low*u*u - high*s*s)^2 + (band*u*s)^2) / D
```

The same kernel gives all four basic SVF outputs, notch and the morph. The
low/high cancellation is essential: adding their separate squared magnitudes
would lose the cross term. The shared `DSP.Response.mix` lemma explicitly
states that cross term. The SVF proof combines the complex numerator before
taking its norm; the morph's existing stream proof connects those weights to
the processor's actual mixed output.

## Composition: feedback

The ladder is a cascade inside a feedback loop. If its open-loop transfer is
`P`, the closed-loop transfer is `P/(1+k*P)`. A magnitude-only cascade cannot
close that loop because the denominator depends on the phase of `P`.

The generated kernel multiplies the four complex stage denominators using
real/imaginary pairs, adds the feedback term to the real part, then computes
one squared norm. With `ui=gi*c`, its numerator is `n=u1*u2*u3*u4`, and its
complex denominator is

```
(u1+i*s)*(u2+i*s)*(u3+i*s)*(u4+i*s) + k*n
```

This supports unequal stage gains. `TPTLadder.realizes_snapshot` reuses the
existing solved-feedback proof, including its loop well-posedness condition.
The magnitude proof expands only this final finite complex expression.

## Domain and endpoints

Use finite positive stage gains and positive SVF dampings for the ordinary
lowpass/highpass/SVF frequency plots. DC and Nyquist are included. The ladder
also requires its closed-loop response denominator to be nonzero at the
frequency being plotted; these response proofs do not add a stability theorem.

Lean's exact division is total (`x/0=0`). The norm identities are consequently
algebraic identities even at singular coefficients. The runtime helper guards
a zero numerator to avoid `0/0`, but deliberately leaves a nonzero numerator
at a pole as division by zero. Do not interpret the algebraic zero at a
simultaneous numerator/denominator zero as a proved canceled or limiting
frequency response. For example, a zero-gain TPT highpass at DC is outside the
positive-gain plotting contract. Floating-point overflow and underflow remain
outside the real-number proofs.
