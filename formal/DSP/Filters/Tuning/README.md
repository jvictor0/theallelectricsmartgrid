# Logical coefficient tuning and theorem inventory

[Tuning.lean](../Tuning.lean) defines `LogicalSpec R` and
`computeCoefficients : LogicalSpec R → State R → State R` for every generated
filter. The generated C++ equivalent is a nested `LogicalSpec` and
`void ComputeCoefficients(const LogicalSpec& spec)`. Only coefficient members of
`m_state` are stored. Integrator memories, previous samples, and stored audio
outputs remain untouched; tuning does not reset the filter.

All frequencies are cycles/sample. There is no sample-rate argument, silent
clamping, normalized UI resonance knob, or overall-slope cutoff compensation.
Convert Hz by dividing by the sample rate before constructing the spec.
Initialize history explicitly, for example with `Filter filter{};`, before the
first update/process call. Retuning a running filter preserves its history.

```cpp
using Filter = TPTSVFGeneric<float, 2>;
using Vector = Filter::Vector;
Filter filter{};
Filter::LogicalSpec spec{Vector(0.01f, 0.02f), Vector(0.70710678f, 2.0f)};
filter.ComputeCoefficients(spec);
```

The two channels have different natural frequencies and Q. Base-width, morph,
four-pole, and splitter tuners call the existing section tuners in Lean; codegen
inlines these calls. Composite `LogicalSpec`s retain this nesting. For example,
a four-pole spec contains `m_stage1` and `m_stage2`, each with `m_cutoff` and `m_q`.

## Meaning of the parameters

Let `fc` be cutoff, `g = tan(π fc)`, `k = 1/Q`, and
`t = (tan(π f)/g)²`. Gains below are **power** (`|H|²`), not magnitude.

| Filter | LogicalSpec | Cutoff / peak convention | Frozen stability region |
| --- | --- | --- | --- |
| ZOH low-pass | `cutoff` | Matched analog pole rate: `α = 1−exp(−2π fc)`. Its exact digital cutoff power is `α²/(α²+4(1−α)sin²(π fc))`, **not generally ½**. Maximum gain is 1 at DC. | Positive cutoff gives pole `exp(−2π fc)` in `(0,1)`; the public Valid predicate uses `0<fc<½`. |
| TPT low-pass / high-pass | `cutoff` | Power ½ (−3.0103 dB) at fc. | `0<fc<½`. |
| TPT base-width | `highPass.cutoff`, `lowPass.cutoff` | These are the two section cutoffs. At either edge, total power is half the other section's power. Shared peak algebra gives `tan²(π fpeak)=gHP*gLP`. No ordering is needed for stability. | Both cutoffs in `(0,½)`. |
| TPT SVF | `cutoff`, `q` | Natural frequency: LP/BP/HP power `Q²` and notch zero at fc. BP peaks at fc. For `Q>1/√2`, LP peaks at `t=1−1/(2Q²)` and HP at its reciprocal; both peak powers are `1/[k²(1−k²/4)]`. Otherwise LP peaks at DC, HP is bounded by its Nyquist gain. | `0<fc<½`, `Q>0`. |
| Morphing SVF | `svf`, `blend` | Existing adjacent equal-power LP→BP→HP blend on `[0,1]`; cutoff power stays `Q²`. Arbitrary morph peak location is not claimed. | Valid inner SVF; blend in `[0,1]` for the stated morph semantics. |
| Four-pole LP/BP/HP | `stage1`, `stage2` | Independent section specs. Identical sections have cutoff power `Q⁴` and inherit their section's peak through the shared cascade bound. Arbitrary unequal stages need not peak at either cutoff. | Both sections valid. |
| Ideal TPT ladder | `cutoff`, `feedback` | Four equal natural-frequency stages. Cutoff power `1/(4−feedback)²`; with zero feedback this is `1/16` (−12.0412 dB). No DC-gain compensation. Peak at DC for `0≤feedback≤⅓`; above that, a unique peak below fc solves `(1+t)³+feedback*(t−3)=0`, `0<t<1`. | `0<fc<½`, `0≤feedback<4`. At 4, a pole reaches the imaginary axis at `p=i g`. |
| Linkwitz–Riley splitter | One cutoff per internal node | LR4 nodes use `Q=cos(π/4)=1/√2`. Sibling all-pass compensation gives unit-magnitude reconstruction for any positive band count. Four-band processing, tuning and snapshot capture are emitted. | `0<fc<½` at every node. |

The standard LR4 convention is described in
[Linkwitz's filter notes](https://www.linkwitzlab.com/filters.htm).
The numerical dB values above are explanatory conversions, not Lean log theorems.

## Theorem inventory and proof connection

Every filter namespace has these three generic theorems in
[History.lean](History.lean):

- `computeCoefficients_preserves_history`: every non-coefficient state field is unchanged.
- `computeCoefficients_idempotent`: repeated updates with the same spec have no further effect.
- `computeCoefficients_channel`: selecting a channel after tuning equals tuning that channel independently.

These quantify over the arithmetic type, so the history/naturality statements
also apply to executable Float instances. Transcendental response/stability
claims are stated over **ℝ/ℂ**.

| Filter | Named tuning/response results |
| --- | --- |
| ZOH | `computeCoefficients_bounds`, `computeCoefficients_stable`, `cutoff_gain`, `peak_dc`; the existing `Snapshot.magnitudeSquared_dc` supplies attainment at DC. |
| TPT LP / HP | `computeCoefficients_tuned`, `cutoff_half_power`, `computeCoefficients_stable`. |
| Base-width | `computeCoefficients_tuned`, `cutoff_edges`, `computeCoefficients_stable`; shared `Tuning.baseWidth_peak` is the normalized peak bound. |
| SVF | `computeCoefficients_tuned`, `weighted_cutoff`, `cutoff_gains`, `Snapshot.normalized`, `digital_normalized`, `computeCoefficients_hurwitz`, `computeCoefficients_stable`. |
| Morph | `computeCoefficients_tuned`, `computeCoefficients_weights`, `cutoff_gain`, `computeCoefficients_stable`; weights agree with the previously proved `blendWeights`. |
| Four-pole | `computeCoefficients_tuned`, `equal_cutoff_gains`, `computeCoefficients_stable`; response products and stream cascade realizations are reused. |
| Ladder | `computeCoefficients_ready` (requires original integrators at rest), `normalized`, `digital_normalized`, `cutoff_gain`, `computeCoefficients_stable`. |
| LR4 | `computeCoefficients_tuned`, `cutoff_quarter_power`, `butterworth_sum_allpass`, `sum_allpass`, `computeCoefficients_stable`. |

Shared results in [Peaks.lean](Peaks.lean) and [Digital.lean](Digital.lean):
`svf_low_peak`, `svf_low_peak_dc`, `svf_band_peak`, `svf_high_reciprocal`,
`svf_high_peak`, `svf_high_peak_nyquist`, `svf_peak_power`, `cascade_peak`,
`baseWidth_peak`, `ladderDen_pos`, `ladder_peak_exists`, `ladder_peak`,
`ladder_peak_unique`, and `ladder_peak_dc`.
`frequencyAtRatio_warp` converts the normalized squared frequency into
`atan(g*sqrt(t))/π` cycles/sample; `frequencyAtRatio_below_cutoff` proves that
`0<t<1` maps strictly below fc. `digital_normalized` ties the SVF and ladder
polynomials to the actual optimized magnitude kernels at interior digital
frequencies, rather than asserting peaks of unrelated formulas.

The coefficient equalities in [Proofs.lean](Proofs.lean) and
[Stability.lean](Stability.lean) establish the existing `Tuned`/`Ready` hypotheses
used by [Responses/Realizes.lean](../Responses/Realizes.lean). Thus the chain is:

`LogicalSpec → computeCoefficients → Tuned/Ready → stream transfer → exact complex response → optimized |H|²`.

The transfer-from-rest hypotheses remain explicit. Tuning a nonzero history
preserves the initial-condition response; it does not retroactively start the
filter at rest. `LogicalSpec.snapshot` in the proof modules specifies the frozen
response coefficients. Snapshot capture/storage in the UI is still caller-owned;
this change does not add a sampling scheduler or reconstruct g from rounded a2/a1.

[Stability/Bilinear.lean](../../Stability/Bilinear.lean) proves that roots in the
continuous left half-plane map strictly inside the digital unit disk, then
proves the one-pole, SVF, and equal-stage ladder denominator regions. Cascade
stability follows by factoring a product zero, with no repeated root solving.
These are sufficient **frozen-pole** stability regions, not a theorem about
arbitrary time-varying coefficient modulation, nonlinear saturation, or all
possible negative feedback values. The ladder boundary theorem is
`Stability.ladder_boundary`; no asymptotic peak-limit theorem is claimed.

## Differences from the handwritten app

- ZOH keeps the matched-pole convention. No app cutoff clamp or tiny-alpha
  numerical approximation is imported into the generic mathematical tuner.
- SVFs accept Q directly instead of the app's exponentially mapped resonance
  control. Four-pole stages can have independent specs.
- Base-width accepts both physical section cutoffs rather than a base×width UI
  parameterization. There is no hidden overall −3 dB reinterpretation.
- The generated ladder already describes an **ideal linear TPT feedback loop**.
  The app's ladder uses a nonlinear/delayed-feedback ZOH architecture, retunes
  the complete cascade to −3 dB, and maps a UI resonance control. This tuner uses
  stage natural frequency and raw loop feedback, with neither that retuning nor
  saturation/gain compensation. It is not a drop-in model of the app ladder.
- The handwritten `LinkwitzRileyCrossover.hpp` uses unequal Butterworth-4
  dampings. The new tuner deliberately uses standard LR4 equal Butterworth-2
  sections within a phase-compensated splitter tree. The old Lean crossover
  model has been removed; the production app itself is unchanged.
- Morph retains the app's adjacent equal-power weighting on `[0,1]`, but the
  tuner requires the range rather than clamping invalid input.

## Transcendentals and runtime validation

[Transcendental.lean](../../Transcendental.lean) provides `Trig` (`sin2pi`,
`cos2pi`, `tanPi`) and `ExpDecay` (`expNegTwoPi`). Real instances are mathematical
sin/cos/tan/exp. Function-space instances lift each operation pointwise.
Native Float/Float32 instances call Lean's transcendental functions.

The C++ backend explicitly lowers trig primitives to `Math4096::Sin2pi`,
`Math4096::Cos2pi`, and `Math4096::TanPi`. Exponential decay uses
`MultiChannelNumber::Exp()` (internally Exp2), including a one-lane vector in
scalar exports. Scalar trig likewise uses one-lane vector overloads. This
preserves scalar/Generic agreement and avoids silently narrowing double inputs
to the app's scalar-float lookup overloads. SVF coefficient denominators use
the same explicit FMA abstraction as processing.

Neither Math4096 nor IEEE arithmetic satisfies the exact real trig identities.
The real proofs do not establish floating-point error bounds or certify every
representable input in the open real validity region. In particular, the
existing float cosine table is not exactly zero at quadrantal angles, and
phase quantization/cancellation matter near DC/Nyquist. Extremely small ZOH
cutoffs can round `1-exp(...)` to zero. There is no new runtime clamp/guard;
callers must choose an appropriate numerical operating range.

`make lean-dsp-test` checks every coefficient in all 11 exports, float/double,
widths 1–4, against an independent direct-Math4096/Exp oracle, and compares
against native Lean with an explicit approximation tolerance. It checks scalar
agreement, idempotence, preserved history bits (including NaNs and signed zero),
and the exact set of coefficient stores. It also checks the poles of the rounded
SVF state matrix at the sampled valid specs. The deterministic sweep uses
cutoffs `[0.0001,0.49]`, Q `[0.25,20]`, blend `[0,1]`, and ladder feedback
`[0,3.95]`. This is regression coverage, not a proof over that whole box.
The proof audit rejects admissions and extra axioms transitively.

The initial validation run passed 11,748 coefficient batches and the 917-theorem
DSP axiom audit. Maximum measured `|C++−Lean|/(1+|Lean|)` over coefficient fields
was `3.613e−7` for float and `2.375e−7` for double; these measurements are not
formal worst-case bounds. The regression tolerance is `2e−5` on this metric.
