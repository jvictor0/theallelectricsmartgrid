# verified-lti Specification

## Purpose
Defines the formal Lean DSP library under `formal/DSP`: exact LTI state-space, ZOH and IIR semantics, the composable bilinear (TPT) transfer calculus for state-only filters, and C++ generation of those filters, checked bit for bit against native Lean execution. Nothing here is integrated into the audio application.

## Requirements
### Requirement: Reusable exact LTI semantics
The library SHALL define finite-dimensional SISO analog and digital state-space models, trajectory semantics, initialization, processor execution, and reusable correctness proofs over exact numbers.

#### Scenario: Run a coupled system
- **WHEN** a finite state-space model and initial state are provided
- **THEN** its real processor run satisfies its independently stated recurrence, with explicit old-state or next-state observation.

### Requirement: Transfer correspondence and realization
The library SHALL connect actual exponential/geometric modes to rational transfer functions on explicit regular domains and provide realization machinery for proper analog and causal digital rational data. Initial-state behaviour SHALL remain separate from transfer equality.

#### Scenario: Evaluate a regular mode
- **WHEN** the resolvent is invertible at a mode
- **THEN** the generic transfer theorem identifies its unique gain.

#### Scenario: Realize an IIR transfer
- **WHEN** normalized causal delay coefficients are supplied
- **THEN** the executable realization satisfies its difference equation and zero-history transfer relation.

### Requirement: Exact held-input discretization
The library SHALL construct exact ZOH dynamics with exp(TA) and integrated input gain, prove held-interval solutions and sampled correspondence, and support singular A.

#### Scenario: Sample a held trajectory
- **WHEN** T is positive and inputs are held per interval
- **THEN** analog states and digital states agree at every sampling instant for matching initial states.

### Requirement: Derived lowpass and standalone export
The independent example SHALL be named OnePoleZOHLowPass, use the app's input-weight coefficient convention, derive its correctness through the reusable models, and export a state-only step as nested State/Input records with stored state and Process(Input&). Floating-point approximation SHALL be trusted for this exercise.

#### Scenario: Generate and validate the lowpass
- **WHEN** the DSP generation and test targets run
- **THEN** the reviewable OnePoleZOHLowPass header matches generation, its native results match Lean, and the proof audit rejects admissions or nonstandard axioms.

### Requirement: Trapezoidal lowpass
OnePoleTPTLowPass SHALL execute the solved zero-delay-feedback trapezoidal update, store its output in its state, prove its real interpretation equals a standard-observation state space for every input and initial state, prove the trapezoidal recurrence, and prove its digital transfer equals the normalized analog prototype at the bilinear frequency on explicit regular domains.

#### Scenario: Generate and validate the TPT lowpass
- **WHEN** the DSP generation and test targets run
- **THEN** the reviewable OnePoleTPTLowPass header matches generation and its native results match Lean bit for bit.

### Requirement: State-only processors
Code generation SHALL accept State → Input → State functions, emitting no Output record, alongside State × Output functions. Filters SHALL store every exposed response in their state, so their specifications are statements about state fields.

#### Scenario: Reject a mismatched state-only result
- **WHEN** a two-argument function returns a type other than its incoming state or a pair whose first component is that state
- **THEN** generation fails with a returned-state-type error.

### Requirement: Compositional bilinear realization
The library SHALL interpret causal streams as Laurent series in the delay, map analog polynomials through the bilinear substitution s := (1-q)/(1+q) as a ring homomorphism, and define when a stored field of a state-only filter realizes a transfer from a given initial state. It SHALL provide the trapezoidal integrator lemma and composition lemmas for components receiving the composite input, components driven by stored fields, and sums, differences and coefficient multiples of realized fields.

#### Scenario: Compose filters
- **WHEN** a composite filter's stored responses are cascades or combinations of realized components
- **THEN** its realized transfer is the corresponding product or combination of the component transfers, without re-deriving component proofs.

### Requirement: TPT filter family
The library SHALL provide state-only TPT filters with generated C++ and realization proofs from rest: a one-pole highpass, a base-width band, a state-variable filter in the app's operation order with stored lowpass, bandpass, highpass and notch, a morphing SVF in the app's operation order whose morph weights are part of its analog transfer, two-stage SVF lowpass/bandpass/highpass cascades, an idealized four-pole ladder with an exactly solved zero-delay global loop, and the app's three-section crossover. Coefficient formulas SHALL remain the caller's responsibility, stated as hypotheses.

#### Scenario: Validate the TPT family
- **WHEN** the DSP generation and test targets run
- **THEN** every reviewable header matches generation, native Lean and C++ trajectories match bit for bit, and the proof audit requires each filter's realization theorem.

