## Why

The verified one-pole currently has bespoke calculus and recurrence proofs. A reusable finite-dimensional LTI foundation should connect ODEs, analog and digital transfers, and executable processors so subsequent filters reuse those proofs.

## What Changes

- Add analog/digital state-space semantics, processor runs and simulations.
- Prove transfer-mode correspondence and exact zero-order-hold sampling over exact numbers.
- Add rational transfer and difference-equation/IIR realization machinery with explicit initialization and output timing.
- Extend C++ generation to fixed-size structured state and state-only steps.
- Add compositional bilinear (TPT) realization proofs and a TPT filter family: highpass, base-width band, SVF, morphing SVF, four-pole SVF variants, idealized ladder and the app's crossover.
- **BREAKING** Rename the standalone formal example and generated artifact to `OnePoleZOHLowPass` and derive it through the reusable machinery.
- Trust floating-point approximation for this exercise; retain native execution checks. Do not integrate the generated code into the app.

## Capabilities

### New Capabilities
- `verified-lti`: Reusable Lean LTI models, transfer and discretization proofs, executable IIR realizations, compositional TPT filters, and generated C++ examples.

### Modified Capabilities

None.

## Impact

Changes are confined to `formal/DSP`, its Lean/codegen drivers, DSP validation scripts and Make targets, and documentation. Reuse pinned Mathlib and the worktree-local build. No application DSP sources or dependencies change.
