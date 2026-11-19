# Conservative Reupping Ladder

[ConservativeReuppingLadder.lean](../ConservativeReuppingLadder.lean) is a
fixed-feedback variant available in the app alongside the handwritten
[Naive Reupping Ladder](../../../../docs/filter-naive-reupping-ladder.md).
Select **Conservative Reupping Ladder** in the voice filter-machine selector.
The original ladder remains the default. Existing saved selector IDs and knob
mappings are preserved; the new selection is appended as ID 2.
Normalized-tanh definitions and proofs are shared; no standalone
saturator or detector processors are exported.

The new core retains the four sequential Euler one-pole updates, normalized
saturation between stages, feedback from the previous unsaturated fourth-stage
state, and final saturated output multiplied by `1+k`. It reuses the existing
Lean `OnePoleZOHLowPass.onePoleStep` and `NormalizedTanh` implementations.
It has no bandpass detectors, envelope followers, or per-sample feedback update.
Its ten numeric state fields comprise four coefficients and six histories.

## Static resonance rule

For drive `g > 0`, define the zero-input saturation slope and loop gain by

```
S(x) = tanh(g*x) / tanh(g)
s = S'(0) = g / tanh(g)
Q = k*s^3
k = resonance * 0.975 * kCritical(alpha) / s^3
```

There are three saturators inside the feedback loop; the fourth is after its
tap. Consequently `Q = resonance * 0.975 * kCritical(alpha)` is independent of
drive. The margin `0.975 = 3.9/4` leaves the maximum resonance strictly below
critical. This is a deliberate static policy: the adaptive ladder can allow
stronger feedback while its detector considers that safe; this variant always
reserves the same fraction of the small-signal stability boundary.

The `resonance` spec field is already normalized to `[0,1]`, not a knob
position. The app adapter retains the existing knob tapers:
`resonance(x) = (49^x - 1)/48` and `gain(x) = 0.25 * 20^x` for `x` in `[0,1]`.
The existing `1+k` output compensation is retained. Drive therefore still
changes gain and saturation even though it cancels from the poles.

## Cutoff and exact digital stability boundary

[ReuppingLadder/Tuning.lean](../ReuppingLadder/Tuning.lean) computes the existing
Euler-stage half-power tuning directly, without an inverse-trig round trip:

```
c = min(requestedCutoff, 0.5)
h = sin(pi*c)
alpha = 2*h*(sqrt(1+h*h)-h)
p = 2*h*(sqrt(1+h*h)+h) = alpha/(1-alpha)
kCritical = (p+2)^2             if p < 2
            (1+2/p)^4          otherwise
```

Cutoff is in cycles per engine sample. Its valid domain is positive, with a
cap at engine Nyquist. The half-power convention belongs to each individual
unit-gain stage; at zero feedback the four-stage response, relative to its DC
gain, is therefore one quarter in magnitude at cutoff (about -12.04 dB).
The stage tuning retains the handwritten Naive ladder's cutoff convention.
A standalone half-power theorem for this tuning is not included in the
inventory below.

The two critical-feedback branches describe different unit-circle crossings
of the delayed digital loop. The high-cutoff branch prevents a pole crossing
at Nyquist. They agree at `p = 2`. The unused branch uses `max(p,2)` in its
denominator, allowing eager vector selection without an unused division by a
tiny `p`.

For nonnegative loop gain `Q`, the stability proof establishes that all roots
of the actual history-mode polynomial

```
(z-(1-alpha))^4 + Q*alpha^4*z^3
```

lie strictly inside the unit circle whenever

```
0 < alpha < 1
Q < ((2-alpha)/alpha)^4
Q*(1-alpha)^2 < (2-alpha)^2
```

The prepared coefficients satisfy both inequalities for every positive cutoff
and drive and every resonance in `[0,1]`. The proof maps the disk to a
half-plane and applies a reusable
[quartic stability lemma](../../Analysis/QuarticStability.lean), proved by
real/imaginary algebra. It does not use sampled roots or assume a stability
test as an axiom.

## The proved small-signal transfer

Write `q` for a unit delay, so frequency evaluation uses `q = exp(-i*omega)`.
The tangent recurrence has the exact transfer

```
P(q) = alpha / (1-(1-alpha)*q)
H(q) = (1+k)*s^4*P(q)^4 / (1+Q*q*P(q)^4)
     = (1+k)*s^4*alpha^4 / ((1-(1-alpha)*q)^4 + Q*alpha^4*q)
```

The extra `q` in the feedback term is the explicit one-sample delay. The fourth
saturator contributes to the numerator's `s^4`, but not to the loop's `s^3`.
This is a discrete recurrence calculation; no analog ladder substitution is
used.

The connection to the nonlinear implementation is proved, not postulated:
start from reset, drive the actual `step` with `epsilon*u[n]`, and differentiate
its output with respect to epsilon at zero. At each finite sample this
derivative is exactly the output of `linearStep`. Its formal stream is
`H * inputStream` for every input sequence (`derivative_stream`). The prepared
transfer's denominator is also proved nonzero everywhere on the unit circle.

The stronger **time-first nonlinear result is now proved** in
[Convergence.lean](Convergence.lean). For the actual prepared processor, started
from reset and driven by `epsilon * cos(omega*n)`, define its settled complex
amplitude by

```
C(epsilon) = lim N→∞ (2/N) * sum[n=0..N-1] y(epsilon,n) * exp(-i*omega*n)
lim epsilon→0+ C(epsilon)/epsilon = H(exp(-i*omega))
```

`prepared_hasSmallSignalResponse` establishes exactly this nested limit using
the existing `ResponseMeasurement.HasSmallSignalResponse` definition. The
inner time limit is proved to exist for every sufficiently small positive
amplitude; it is not a hypothesis. The measurement retains phase and is a
coherent Fourier projection. DC and Nyquist are excluded because its factor-two
real-sinusoid calibration is different there. Both rational and irrational
digital frequencies are covered.

## How the nonlinear convergence proof works

1. The existing pole theorem is connected to the spectrum of the actual
   four-history linear map. Gelfand's formula supplies a power `N` with
   `||A^N|| < 1`. The proof then constructs the equivalent state norm
   `p(x) = sum[j=0..N-1] ||A^j*x||`, in which each linear update contracts.
   This construction is entirely proof-side; it adds no runtime work.
2. The actual nonlinear update is continuously differentiable. Its derivative
   at silence is exactly the same `linearUpdate`. In a small enough neighborhood,
   nonlinear corrections fit inside the linear contraction margin. A sufficiently
   small bounded drive keeps the states in this neighborhood for all time.
3. [Uniform.lean](Uniform.lean) proves a single amplitude threshold works for
   **all samples and all input sequences bounded by one**. For any `eta > 0`
   there is a `delta > 0` such that, whenever `|epsilon| < delta`,
   `|y(epsilon,n) - epsilon*yLinear(n)| <= eta*|epsilon|` for every `n`.
   This includes the final output saturator. The error cannot grow relative to
   the input merely because the experiment runs longer.
4. Extending a sinusoidal input farther into the past gives a Cauchy sequence of
   state functions, uniformly over input phase. Its limit is continuous, and
   the actual zero-state output approaches its corresponding phase waveform.
   Fourier-polynomial density proves that every such continuous waveform has
   a coherent long-time average, even when no integer sample period exists.
5. [Sinusoid.lean](Sinusoid.lean) proves time-domain settling of the tangent
   recurrence to the exact `H` sinusoid. Uniform waveform error bounds pass
   through averaging, so the nonlinear settled gain tends to that same `H`.

The new generic tools in `Analysis/` and `ResponseMeasurement/` are reusable
lemmas. The ladder-specific files supply the actual state projection,
derivative, spectrum, and transfer connections. They do not replace the
processor with a separate recurrence or assume its convergence.

The result is local around silence, with fixed valid tuning and reset initial
histories. The amplitude threshold may depend on tuning and requested accuracy;
the proof supplies existence, not a numerical threshold or an explicit cubic
error constant. A finite-amplitude settled waveform may contain harmonics.
The exported nested-limit theorem uses coherent gain; a separate RMS-gain
theorem is not added here. Global attraction from arbitrary loud states,
parameter modulation, and floating-point dynamics remain outside this result.

## Theorem inventory

| File | Main theorems and their scope |
| --- | --- |
| [ResponseProofs](ResponseProofs.lean) | `prepared_ui_smallSignal`: the response whose power is plotted is the actual nonlinear processor's time-first small-signal limit |
| [ResponseProofs](ResponseProofs.lean) | `Snapshot.magnitudeSquared_eq_normSq`, `snapshot_response`, `prepared_magnitudeSquared_eq_transfer`: the normalized executable power kernel equals the squared norm of the existing delayed transfer |
| [ResponseProofs](ResponseProofs.lean) | `prepareCoefficients_state`, `prepareCoefficients_snapshot`, `magnitudeSquared_channel`, `snapshot_channel`: joint preparation retains the original audio updater, captures its coefficients, and commutes with lane selection |
| [Convergence](Convergence.lean) | `prepared_hasSmallSignalResponse`: the actual nonlinear processor's time-first coherent small-signal response equals `transfer`, under tuning and frequency conditions only |
| [Convergence](Convergence.lean) | `prepared_phase_response`, `prepared_settled_response`: sufficiently small sinusoids approach a continuous waveform of phase and have a long-time coherent average |
| [Uniform](Uniform.lean) | `prepared_uniform_output`: nonlinear versus tangent output error is `o(epsilon)` uniformly over every sample and every input bounded by one |
| [Sinusoid](Sinusoid.lean) | `prepared_linear_tracks`: the actual tangent output approaches the sinusoid with complex amplitude `transfer`, with its feedback delay retained |
| [StateSpace](StateSpace.lean) | `stateUpdate_hasStrictFDerivAt`, `prepared_spectrum_lt_one`, `preparedGauge`: the actual four-history update is smoothly linearized by `linearUpdate`, has strict spectral stability, and admits a contracting state norm |
| [Linearization](Linearization.lean) | `trajectory_hasDerivAt_zero`, `output_hasDerivAt_zero`: derivatives of all four actual nonlinear stage histories and output equal the tangent recurrence, at every finite sample from reset |
| [Linearization](Linearization.lean) | `derivative_stream`: the actual output's samplewise amplitude derivative realizes the stated transfer for arbitrary input streams |
| [Transfer](Transfer.lean) | `realizes_linear`: exact formal-series input/output transfer of `linearStep` from rest |
| [Transfer](Transfer.lean) | `characteristic_of_mode`: every nonzero homogeneous mode of the four actual histories satisfies the quartic; internal modes are included even if an input/output cancellation could hide them |
| [Stability](Stability.lean) | `roots_lt_one`: both explicit feedback inequalities imply strict unit-disk roots; `denominator_ne_zero_of_roots` connects them to frequency evaluation |
| [Contracts](Contracts.lean) | `prepared_slope_pos`, `prepared_loopGain`: the chosen normalizer has positive zero slope and the setter cancels its cube exactly |
| [Contracts](Contracts.lean) | `prepared_roots_lt_one`, `prepared_denominator_ne_zero`: end-to-end pole and denominator guarantees for the actual setter over its full valid real domain |
| [Contracts](Contracts.lean) | Step preserves coefficients and reads the old fourth stage; preparation preserves history and is idempotent; step/preparation commute with lane selection; reset clears history and preserves coefficients |
| [TuningProofs](../ReuppingLadder/TuningProofs.lean) | `prepareTuning_properties`, `critical_bounds`: positive stable stage tuning and both critical-feedback bounds |
| [QuarticStability](../../Analysis/QuarticStability.lean) | `re_neg_of_monic`, `re_neg`: reusable real-coefficient quartic Hurwitz criterion |
| [SchurDecay](../../Analysis/SchurDecay.lean), [StabilityGauge](../../Analysis/StabilityGauge.lean) | Strict spectral stability gives a contracting power and an equivalent contracting state norm |
| [LocalDynamics](../../Analysis/LocalDynamics.lean) | `exists_localContraction`, `orbit_bound`, `orbit_distance`, `uniform_linearization`, `uniform_observation`: reusable local stability, forgetting, and uniform differentiation of forced recurrences |
| [ForcedResponse](../../Analysis/ForcedResponse.lean) | `pullback_converges`, `observation_tracks_pullback`: continuous compact-phase drives have a limiting phase response tracked by the actual trajectory |
| [Measurement/Circle](../../ResponseMeasurement/Circle.lean) | `circle_average_exists`, `circle_coherent_exists`: averaging continuous phase waveforms works for arbitrary digital phase increments |
| [Measurement/Uniform](../../ResponseMeasurement/Uniform.lean) | `smallSignal_of_uniform_linearization`: existing inner limits and uniform waveform approximation imply the time-first small-signal response |

## Generated interface and numeric boundary

Generated headers are
[ConservativeReuppingLadderCore.hpp](../../generated/ConservativeReuppingLadderCore.hpp)
and
[ConservativeReuppingLadderCoreGeneric.hpp](../../generated/ConservativeReuppingLadderCoreGeneric.hpp).
The generic core supports float/double multichannel numbers without lane loops.
Initialize with `{}` and prepare coefficients before processing:

```cpp
using Core = ConservativeReuppingLadderCoreGeneric<float, 3>;
using Vector = Core::Vector;
Core filter{};
filter.ComputeCoefficients({Vector::Splat(0.125f),
    Vector::Splat(0.7f), Vector::Splat(2.0f)});
Core::Input input{Vector::Splat(0.1f)};
filter.Process(input);
auto output = filter.m_output.m_x;
filter.Reset();
```

The exported `Runtime.process` projects the existing proved step onto persistent
coefficients and the four Euler stage memories. Its output is separate; the
recorded drive is a local intermediate. `Runtime.simulation` preserves the
complete output stream, including the previous-fourth-stage feedback delay.
`Runtime.prepare_save` and `Runtime.reset_save` connect tuning and reset to the
same original model.

`ComputeCoefficients` updates only coefficients; `Reset` clears histories and
the exposed output while keeping tuning. The same generated struct contains `Snapshot`, static `MagnitudeSquared`,
`MakeSnapshot`, `UIState`, and `PopulateUIStateSample`. Coefficients and the UI
snapshot are prepared together; `Reset` leaves the response snapshot intact.

The snapshot stores `(1-alpha)/alpha`, the saturation slope, and feedback. The
kernel divides the transfer numerator and denominator by `alpha^4`, avoiding
underflow from squaring tiny cutoff coefficients. It uses prepared half-angle
sine for the real denominator component, preserving accuracy near DC. No square
root or transcendental call runs per snapshot/frequency evaluation.

The handwritten outer shape shares its controls and routing with the original:
LP ladder → sample-rate reducer → generated four-pole SVF HP. Its snapshot embeds
the generated child snapshots and multiplies their powers, using the existing
sampled history and log-power UI. This is the small-signal LP/HP curve; the
sample-rate reducer and finite-amplitude saturation do not have an LTI transfer.
The old ladder's approximate plot omits saturation slopes, so its graph can
differ from the conservative graph even at identical knob positions.

Proofs use exact real tanh and sine. Generated processing uses the existing
clamped cubic rational tanh, explicit one-pole FMAs, and the chosen saturation
implementation's cached denominator. Generated preparation uses one
`Math4096::Sin2pi` call and vector square root. Rational-approximation derivative
proofs, approximation error, floating-point stability, and compiler correctness
are outside the real-number theorems.

`make lean-dsp-test` validates native Lean against scalar C++ and vector C++ at
widths 1, 2, 3, and 4 in both precisions; processing agrees bit for bit. Tuning
has independent Math4096 checks, native-transcendental error checks, history
preservation, idempotence, and scalar/vector agreement. The proof audit checks
the named theorems and permits only standard Lean axioms. This adds no measured
performance claim: removing the detectors reduces the work structurally, but
the new shape has not yet been auditioned against the adaptive app ladder.
Core tests compare tiny-sinusoid complex gain and power with the generated UI
response and independently evaluate the delayed transfer over the tuning range.
