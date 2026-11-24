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

## Compose stateful processors with StateM

Describe processing, initialization, and tuning as ordinary `StateM` actions.
The owner declares its child state and buffer capacities in Lean. Pass input
and current-sample results as values, and update persistent state in place
through the common emitter.

Use [`DSP.focus`](../../../formal/DSP/StateFocus.lean) to run a child action on
one field of its parent. Supply the field projection and the matching record
update. The emitter validates both and borrows the field by reference. Focus
on small fields when reading values from a record that owns native storage.
Use fixed products for statically sized children with different capacities.
Use the shared native storage bindings for buffer reads, writes, and gathers,
with the caller choosing the addresses and their order in Lean.

## Use exact mathematics by default

For numerical theorems, use the real numbers (`ℝ`) or complex numbers (`ℂ`) as appropriate, unless the task specifies a different numerical model. Interpret transcendental operations using their exact mathematical definitions, such as `Real.sin`, `Real.exp`, and `Real.tanh`. Keep `Float` and `Float32` specializations available for execution and testing where needed. Apply exact algebraic laws only in interpretations that satisfy them.

## Prove bounds for generated hot paths

Prove the preconditions for direct native indexing and arithmetic in Lean.
Use bounded types such as `Fin` for array indices and carry range proofs for
signed and unsigned conversions, intermediate sums and differences, and loop
bounds. Connect those proofs to the actual exports and their entry contracts.
Lower the proved operations directly and erase the proof arguments. If the
emitter cannot consume the proof, treat that as a codegen gap. Report any
remaining generated checks as an interim limitation.

Trust established upstream contracts. A native producer can supply an input
range before it has a Lean model. State the range as a hypothesis on the values
the export receives, identify which producer guarantees it, and use a debug
assertion at the handoff when useful. Keep release-path checks and clamps rare.
Use them for exceptional inputs with specified handling or for deliberate
algorithm semantics. Separate intentional modular arithmetic from indices
whose mathematical values must be preserved.

Prove conservative ranges over exact reals for finite numerical paths. State
the native assumptions that carry the bounds through rounding, including
finite inputs, headroom for intermediate results and narrowing conversions,
and denominators that stay nonzero after rounding. Track missing values or
nonfinite sentinels through their representation contract.

## Prove initialization and erase the proof

For fixed-capacity local scalar scratch, use
[Scratch.ofFn](../../../formal/DSP/Codegen/Scratch.lean) to fill a prefix.
Prove that the count fits the capacity. Give each `Scratch.get` a bounded
index and a proof that the slot lies in the initialized prefix.

Keep initialization evidence in `Prop`. Lower the scratch to local native
storage with one loop over the active prefix and lower proved reads directly
to indexing. Check the generated code for implicit zero-fills, copies of the
whole buffer, and runtime initialization tracking. Keep the buffer local so
its unwritten suffix stays unread.

## Allow approximations in generated code

Code generation may lower exact transcendental operations to lookup tables, rational approximations, or other numerical implementations of those operations. This difference is intentional. For example, prove a recurrence using exact sine or tanh and let the generator emit a sine-table lookup or a rational tanh approximation for the corresponding operation.

Preserve the numerical evaluation order and explicit `FMA.fma` in the Lean source when lowering ordinary arithmetic. The C++ build uses `-fno-fast-math -ffp-contract=off`. Put intentional numerical substitutions at named operations such as `Trig` and `Tanh` so the exact and executable interpretations share the surrounding algorithm. See the [codegen arithmetic contract](../../../formal/DSP/Codegen/README.md#supported-subset).

## Report what the proof covers

State what Lean checked, including material hypotheses about inputs and state. A theorem about the shared definition covers its exact interpretation.

Trace the exported Lean definition through the common emitter, the allowlisted native operations, and the app wrapper. Identify the contract for each native operation the claim depends on and the evidence that supports that contract.

Routine proof work ends at the exact interpretation. Address floating-point behavior or approximation error only when explicitly requested or when a specific requirement depends on it. Support claims about rounding, approximation error, preservation of a property by an approximation, or correctness of the generated executable with evidence specific to the behavior being claimed. When reporting numerical comparisons, identify them as tests and state what they cover.

## Regenerate and validate DSP changes

Edit the Lean definition, the common emitter, or a named allowlisted lowering. When emitted C++ changes, run `make lean-dsp-generate` and review the resulting changes in `formal/DSP/generated/`. Use `make lean-dsp-test` to build the proofs, run the DSP axiom audit and native comparisons, and check that the checked-in headers are current. Use the existing comparison for the operation being tested. Shared executable arithmetic can be compared bit for bit, while approximating lowerings can be checked against their intended implementation, such as `Math4096` for tuning. See the [Lean build targets](../../../docs/building.md#lean-specifications).
