## Context

The existing independent Lean one-pole proves exact sampling under held inputs and exports a pure `State → Input → State × Output` function to standalone C++. Its mathematical and executable arithmetic are shared. The accepted design generalizes through finite-dimensional SISO LTI state space.

## Goals / Non-Goals

**Goals:** reusable state-space trajectories, transfer modes, ZOH and digital/IIR realization proofs; a migrated `OnePoleZOHLowPass`; generated C++ retaining updated-state output; explicit initial conditions and pole domains.

**Non-Goals:** application integration, arbitrary nonlinear ODEs, inverse discretization, floating-point error bounds, a formally verified C++ compiler, or a full Laplace transform library. Floating-point approximation is explicitly trusted by the user.

## Decisions

- Keep executable arithmetic operation-only and ordered. Prove its real interpretation equals matrix arithmetic, without giving Float unlawful field instances.
- Use state-space coefficients A/B/C/D as the common representation; distinguish continuous dynamics from discrete transitions through their interpretations.
- Define actual analog and digital trajectories separately from their executable realizations. Define transfer modes through trajectories, with an explicit resolvent domain; provide rational presentations for construction and coefficient extraction.
- Exact ZOH uses exp(TA) and the integral of exp(rA)B. It must support singular A and arbitrary initial states. Prove one interval, then the whole stream. Construct solutions as well as proving uniqueness.
- Standard observation reads old state. An explicit next-state adapter has C'=CA and D'=CB+D. Preserve the old lowpass Process convention through this adapter; distinguish endpoint observation before a new held input from right-continuous sampling when D is nonzero.
- Transfer realization preserves zero-state input/output behaviour, not hidden modes or arbitrary initial state. Direct-form IIR uses normalized delay coefficients and explicit history. Formal power series express recurrence semantics without convergence hypotheses.
- Specialize fixed dimensions for C++ generation; support structured finite state, preserving arithmetic evaluation and simultaneous state updates.
- Keep the prior scalar calculus proof as independent regression evidence during migration, then make public lowpass correctness follow generic machinery.
- Filters are state-only steps that store every exposed response. A specification says which stored field realizes which transfer from rest, so composites state results about their components' fields.
- Trapezoidal filters relate to analog prototypes through the bilinear substitution, a ring homomorphism into Laurent series in the delay. Series, parallel and feedback connections therefore compose; ZOH does not, and remains a one-pole example.
- Linear zero-delay loops are solved in closed form; well-posedness is the analog denominator being nonzero at s = 1. Coefficient formulas are hypotheses, not derived.

## Risks / Trade-offs

- Matrix analysis APIs require substantial adaptation in pinned Mathlib → build each reusable lemma incrementally and audit transitive axioms.
- Rational cancellation hides internal modes → retain state-space and initialization contracts; do not infer internal stability from a reduced transfer alone.
- Equivalent real expressions can round differently → keep ordered arithmetic and compare native Lean with generated C++; trust approximation to exact arithmetic.
- Generic dimensions can expose codegen gaps → exercise a coupled two-state processor and nonzero direct feedthrough as well as the scalar lowpass.

## Migration Plan

Build the generic modules beside the existing example, prove its specialization agrees, rename drivers/tests/generated header to OnePoleZOHLowPass, then run DSP proof and native C++ checks. Leave application sources untouched.

## Open Questions

None requiring user input. Concrete API choices may be refined as the pinned library dictates, preserving the contracts above.
