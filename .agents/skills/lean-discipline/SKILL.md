---
name: lean-discipline
description: Apply the project's Lean proof discipline when writing, reviewing, or reporting proofs about code generated from Lean, including exact arithmetic and numerical approximations in code generation.
---

# Lean discipline

## Prove the code that feeds generation

Start at [GenerateDSP.lean](../../../formal/GenerateDSP.lean) and trace the exported Lean definitions relevant to the claim, including processing, coefficient preparation, installation of prepared coefficients, reset, and snapshot or response functions where applicable. State implementation claims about those definitions under the exact arithmetic interpretation. Share the algorithm between the proof and the export, for example by parameterizing its numerical operations so both interpretations use the same state updates, branches, and outputs.

Use separate mathematical specifications when they help state a property. If a proof works on a different state representation or algorithm, prove the connection to the actual export source with an equality, refinement, or simulation that covers the behavior being claimed.

For compact `State × Output` exports, use the existing `Processor.compact` and `Processor.run_simulation` pattern to connect retained state and the output stream to the model. For a claim that includes coefficient preparation or another state update, show that the update agrees through the same state projection and returns the same snapshot where applicable. See [TPTSVF.Runtime](../../../formal/DSP/Filters/TPTSVF/Runtime.lean).

## Keep algorithms in Lean

Derive every exported algorithm body from Lean through the common C++ emitter.
Loop bounds and traversal order, branches, scheduling, selection, and persistent
state updates all belong in the Lean definition. Custom C++ algorithm emitters,
literal C++ algorithm bodies, and native helper calls that hide the algorithm
are prohibited.

Use one shared allowlist for native bindings. Give each binding a named Lean
operation with stated semantics and a C++ lowering. Mathematical functions can
use native implementations or numerical approximations, such as a packed FFT
for the finite DFT or a rational approximation for tanh. Keep other native
bindings to representation and primitive data-structure operations, such as
array reads and writes and heap push and pop. The Lean caller owns traversal,
ordering, and the decision to perform each operation.

When the common emitter cannot express a required operation, extend its
general lowering support. Treat custom algorithm emitters in existing exports
as migration work. Tests compare the behavior of allowed lowerings and cannot
substitute for deriving the algorithm body from Lean.

## Use exact mathematics by default

For numerical theorems, use the real numbers (`ℝ`) or complex numbers (`ℂ`) as appropriate, unless the task specifies a different numerical model. Interpret transcendental operations using their exact mathematical definitions, such as `Real.sin`, `Real.exp`, and `Real.tanh`. Keep `Float` and `Float32` specializations available for execution and testing where needed. Apply exact algebraic laws only in interpretations that satisfy them.

## Allow approximations in generated code

Code generation may lower exact transcendental operations to lookup tables, rational approximations, or other numerical implementations of those operations. This difference is intentional. For example, prove a recurrence using exact sine or tanh and let the generator emit a sine-table lookup or a rational tanh approximation for the corresponding operation.

Preserve the numerical evaluation order and explicit `FMA.fma` in the Lean source when lowering ordinary arithmetic. The C++ build uses `-fno-fast-math -ffp-contract=off`. Put intentional numerical substitutions at named operations such as `Trig` and `Tanh` so the exact and executable interpretations share the surrounding algorithm. See the [codegen arithmetic contract](../../../formal/DSP/Codegen/README.md#supported-subset).

## Report what the proof covers

State what Lean checked, including material hypotheses about inputs and state. A theorem about the shared definition covers its exact interpretation.

Trace the exported Lean definition through the common emitter, the allowlisted native operations, and the app wrapper. Identify the contract for each native operation the claim depends on and the evidence that supports that contract.

Routine proof work ends at the exact interpretation. Address floating-point behavior or approximation error only when explicitly requested or when a specific requirement depends on it. Support claims about rounding, approximation error, preservation of a property by an approximation, or correctness of the generated executable with evidence specific to the behavior being claimed. When reporting numerical comparisons, identify them as tests and state what they cover.

## Regenerate and validate DSP changes

Edit the Lean definition, the common emitter, or a named allowlisted lowering. When emitted C++ changes, run `make lean-dsp-generate` and review the resulting changes in `formal/DSP/generated/`. Use `make lean-dsp-test` to build the proofs, run the DSP axiom audit and native comparisons, and check that the checked-in headers are current. Use the existing comparison for the operation being tested. Shared executable arithmetic can be compared bit for bit, while approximating lowerings can be checked against their intended implementation, such as `Math4096` for tuning. See the [Lean build targets](../../../docs/building.md#lean-specifications).
