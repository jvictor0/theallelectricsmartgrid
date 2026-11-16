## 1. State and execution

- [x] 1.1 Add generic processor runs and simulation proofs.
- [x] 1.2 Add ordered state-space arithmetic, real matrix interpretation, digital semantics and observation adapters.
- [x] 1.3 Add analog trajectory semantics and existence/uniqueness foundations.

## 2. Transfers and discretization

- [x] 2.1 Define regular transfer domains and prove analog/digital modal correspondence.
- [x] 2.2 Connect resolvents to rational transfer presentations.
- [x] 2.3 Construct held-input solutions and prove generic ZOH interval and stream correspondence.

## 3. Realizations

- [x] 3.1 Add normalized rational/delay coefficients and executable IIR histories with recurrence proofs.
- [x] 3.2 Prove zero-history transfer/series correspondence and rational realization contracts.
- [x] 3.3 Connect linear differential equations and analog rational realizations.

## 4. Lowpass export and validation

- [x] 4.1 Derive and rename OnePoleZOHLowPass through the generic pipeline, preserving output timing.
- [x] 4.2 Extend generation for fixed structured state and exercise a coupled multi-state processor.
- [x] 4.3 Regenerate the lowpass header, update documentation, expand proof audit and run DSP validation.
- [x] 4.4 Rename the ZOH example to OnePoleZOHLowPass and adopt the app's input-weight alpha convention.
- [x] 4.5 Add OnePoleTPTLowPass with state-space, trapezoidal-rule, bilinear-transfer and IIR proofs, generation and native checks.

## 5. State-only TPT filters

- [x] 5.1 Generate state-only `State → Input → State` functions; share value bindings in emitted C++.
- [x] 5.2 Convert both one-poles to state-only steps with stored outputs.
- [x] 5.3 Add Laurent-series stream semantics, the bilinear homomorphism, `Realizes`, the integrator lemma and composition lemmas.
- [x] 5.4 Add the TPT highpass, base-width band, SVF, morphing SVF, four-pole SVF variants, idealized ladder and crossover, each with realization proofs.
- [x] 5.5 Generate all filters, extend the bitwise differential test, smoke checks, audit and documentation.

## Validation

- `make lean-dsp-generate`: all twelve reviewable headers regenerated.
- `make lean-dsp-test`: passed; 489 audited theorems, 32,256 native Lean/C++ bitwise comparisons, native filter smoke checks and unsupported-export rejection checks.
- `openspec validate reusable-verified-lti --strict`: passed.
- `git diff --check`: passed.
- Existing unrelated failure: `scripts/check_phase_spec.py` reports source-fingerprint drift for `private/src/Filter.hpp`; that application file and its specification fingerprint were not changed.
