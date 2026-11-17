# Lean processors to C++ structs

The generated filters are under [generated](../generated): the ZOH and TPT
one-poles, TPT highpass and base-width band, `TPTSVF`, `MorphingTPTSVF`, the
three `TPTSVF4Pole` variants, `TPTLadder` and `LinkwitzRileyCrossover`. Each is a
standalone C++17 header with nested `State` and `Input` records, stored
`m_state`, and `void Process(Input& input)`. Filters are state-only: every
response they expose is a field of `m_state`. Headers have no Lean runtime
dependency and are not integrated into the audio app. The separate
[CoupledProcessor.hpp](../generated/CoupledProcessor.hpp) exercises the
`State × Output` form, nested coefficients and two interacting state variables.

Each also has a `Generic` header, such as
[TPTSVFGeneric.hpp](../generated/TPTSVFGeneric.hpp), parameterized by
`Number` (`float` or `double`) and `Size`. These include the app's
`MultiChannelNumber.hpp`; add `private/src` to the compiler's include path.

The backend in [Cpp.lean](Cpp.lean) reads an elaborated Lean function and its
record definitions. It does not contain a one-pole arithmetic template.

## Authoring and generation

Author a pure function of one of these shapes, using named records:

```lean
def step (state : State) (input : Input) : State := ...
def process (state : State) (input : Input) : State × Output := ...
```

Filters use the first, state-only form, so proofs are statements about stored
fields. Each generic filter `step` has a `stepFloat` specialization that chooses
executable binary64 arithmetic. [GenerateDSP.lean](../../GenerateDSP.lean)
exports them, for example:

```lean
#generate_cpp DSP.TPTSVF.stepFloat as TPTSVF to "build/lean/generated/TPTSVF.hpp"
```

For multichannel output, export the same generic step directly:

```lean
#generate_cpp_generic DSP.TPTSVF.step as TPTSVFGeneric to "build/lean/generated/TPTSVFGeneric.hpp"
```

This interprets the step's arithmetic parameter `R` as
`MultiChannelNumber<Number, Size>`. Its arithmetic instances become vector
operations, literals become `Vector::Splat`, and `IfLess.ifLess` becomes
`Vector::IfLess`. All coefficients, state and output fields of type `R` are
per channel. Fixed `Fin n → R` state dimensions remain `std::array<Vector, n>`;
the state dimension and the channel count are independent.

The generic export uses the same recurrence as `stepFloat` and the lifted
proofs. It takes no proof arguments and retains the scalar exporter's tested
compiler boundary.

Run from the repository root:

```sh
make lean-dsp-generate
make lean-dsp-test
```

`lean-dsp-generate` regenerates and updates the reviewable headers under
`formal/DSP/generated/`. `lean-dsp-codegen` generates only the build-tree copies.
`lean-dsp-test` checks the Lean proofs and axiom audit, generates and compiles
the C++ tests, compares native execution, and checks that the reviewable headers
match current generation. It does not silently update a stale header.

All incremental compiler output and test results stay under `build/lean/`.
Use `CXX=clang++` (or another C++17 compiler) to select the native test compiler.

## Generated interface

Fields acquire an `m_` prefix unless they already have it. C++ identifiers must
be ASCII and cannot contain double underscores. The enclosing processor name
must start with an uppercase letter and cannot be `State`, `Input` or `Output`.
Generic exports also reserve `Number`, `Size` and `Vector`.

The generated processor is an aggregate. The caller supplies the initial state,
including coefficients and stored responses; the backend does not invent an
initialization or reset policy. For example:

```cpp
OnePoleZOHLowPass processor{{0.5, 0.0}};
OnePoleZOHLowPass::Input input{1.0};
processor.Process(input);
```

A three-channel instance, with independent coefficients, is:

```cpp
using Filter = OnePoleZOHLowPassGeneric<float, 3>;
using Vector = Filter::Vector;
Filter processor{{Vector(0.1f, 0.2f, 0.3f), Vector()}};
Filter::Input input{Vector(1.0f, 2.0f, 3.0f)};
processor.Process(input);
```

In the scalar example, the stored output `m_state.m_y` is `0.5`; the coefficient stays
`0.5`. The coefficient `alpha` is the input weight, ideally `1 - exp(-T/tau)`,
as in the app's `OPLowPassFilter`. Coefficient preparation, such as an SVF's
a1, a2 and a3 or a morph's cosine and sine, remains the caller's responsibility.

For each call, a state-only `step(state_n, input_n)` returns `state_{n+1}`, and
`process(state_n, input_n)` returns `(state_{n+1}, output_n)`. All returned
scalar fields are captured before any stored record is modified. This preserves
state swaps and outputs that observe old values. In the `State × Output` form
the previous `m_output` is not an implicit input to subsequent calls; persistent
history belongs in `State`.

The ZOH one-pole stores the output at the end of the current held-input
interval. The TPT filters store the current-sample responses of their solved
zero-delay loops, which depend on the current input.

## Supported subset

- Monomorphic, pure functions with exactly two arguments, returning either the
  incoming state type or a `State × Output` pair whose first component is it.
- Named records, nested records and fixed `Fin n` vectors/matrices, with
  `Float`, `Float32` or `Bool` leaves. Vectors become `std::array` and dimensions
  are capped at 256 per axis. Inherited records, dependent elements and vectors
  of records are rejected.
- Safe helper definitions, generic arithmetic instantiated with the built-in
  floating-point operations, record construction/update/projection, pairs and
  local bindings and statically specialized structural recursion over closed
  natural numbers. Each value binding is emitted once and shared; function-valued
  bindings and helpers are specialized before emission.
- Floating-point addition, subtraction, multiplication, division, negation, explicit `FMA`,
  `<`, `<=` and Boolean equality tests; `if` expressions over supported Boolean
  conditions using the standard Boolean/float decision functions. Each branch's
  operations stay inside that branch in C++.
- Finite numeric literals. Hexadecimal literals preserve the exact bits selected
  by Lean's literal conversion; zero and one use their exact decimal spellings.

Generic export accepts one leading arithmetic type parameter `R : Type`,
followed by instances of `Add`, `Sub`, `Mul`, `Div`, `Neg`, `OfNat` (with a
literal numeral), `OfScientific`, `IfLess` and `FMA`, then the state and input.
Other generic parameters and operations are rejected. Shared `Bool` fields
can still control whole-vector branches; channel-dependent selection uses
`IfLess`, with no conversion to a shared Boolean. Literals are converted by
Lean separately at both precisions and the C++ template selects the matching
bits. A generic literal must be finite in both precisions.

Unknown external functions, alternative `implemented_by` implementations,
unsafe/partial functions, recursion with a runtime major argument, dynamic
allocation, general pattern matching, integer fields, and unsupported types or
operations fail generation. There is no fallback to hand-translated arithmetic.
The exporter reads the number-type specialization explicitly; it never silently
replaces `ℝ` with floating point.

Compile generated code with strict floating-point arithmetic and implicit contraction
disabled (`-fno-fast-math -ffp-contract=off` in the test harness). Operation order
is preserved; the backend does not apply real-number algebraic rewrites or
fuse ordinary multiply/add expressions. Explicit `FMA.fma a b c` in the source
maps to `std::fma(a, b, c)` for scalar export and
`Vector::FusedMultiplyAdd(a, b, c)` for generic export. Native Lean calls `fma`
or `fmaf`, so these operations round once even with implicit contraction disabled.
The exact-arithmetic instance in `DSP.FMA.Algebra` interprets FMA as `a*b+c`;
`LawfulFMA` records this law for proofs, with no such instance for floating point.
The ZOH, TPT, SVF, morph and ladder steps and state-space/IIR accumulations
use explicit FMA; composite filters inherit it. These outputs may differ in
low bits from the earlier separate multiply/add versions. The native comparisons use the host's normal
floating-point environment. Different rounding modes, flush-to-zero settings,
compiler options or target platforms require their own validation.

## Evidence and proof boundary

`DSP.OnePoleZOHLowPass.step_matches_interval` proves that the real specialization of
the exported step returns the analog endpoint under the stated ODE and
continuity assumptions. The existing `analog_matches_digital` proves sampled
equivalence for the recurrence across all intervals. The TPT filters' `realizes`
theorems state their transfers from rest. These results share the same generic
arithmetic and state update as each `stepFloat`.

The backend is **not a formally verified compiler**, and these theorems do not
prove IEEE rounding behavior or the compiled C++ executable. The code generator,
Lean compiler/runtime used for native reference execution, C++ compiler and
floating-point environment remain part of the trusted implementation boundary.

The scalar differential test executes the actual Lean source functions and
compares every generated state/output field bit for bit over 32,268 cases. It covers:

- ZOH and TPT one-pole trajectories with changing inputs, endpoint/near-one coefficients,
  signed-zero initial states and subnormal values.
- Trajectories of every TPT composite from random coefficients, integrator states
  and stored responses, including both morph halves and ladders with feedback.
- A distinct processor that swaps state fields, branches, and emits old-state
  values, so assignment order and independent outputs are exercised.
- A binary32 processor using division, negation, comparisons and decimal
  constants, with both conditional branches exercised.
- A coupled two-state processor using generic state-space arithmetic, nested
  coefficient records, fixed vectors/matrices and nonzero direct feedthrough.
- Explicit FMA at both precisions, checked against known bit patterns for
  cancellation, intermediate overflow, subnormals, signed zero and infinity.
- Rejection of unsupported field types, external operations, unsafe definitions,
  alternate literal/conditional implementations and mismatched returned state types,
  in both result forms.

The multichannel test packs independently generated native Lean trajectories
into `float` and `double` vectors of widths 1, 2, 3 and 4. C++ keeps its own
state between samples, and every logical field/channel is compared bit for
bit. Three-channel padding is poisoned with NaN and excluded from the logical
fields. Coverage includes every filter, the coupled state-space processor,
mixed morph halves, state swaps with old-state outputs, selection at equality
and NaN, decimal rounding, and the explicit FMA cases. Unsupported generic ordering, external literal
implementations, returned-state mismatches and reserved names are rejected.

The input/reference corpus is retained at `build/lean/codegen-test/reference.txt`.
Passing these checks is implementation evidence, not a universal compiler proof
or a floating-point error bound.
