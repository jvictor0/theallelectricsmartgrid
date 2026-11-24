# Lean processors to C++ structs

The delay filters also use generated cores: `SchroederAllPassCoreGeneric`,
`TunedDecayCombCoreGeneric`, `TunedDecayCombTuningGeneric` and
`TunedDecayCombResponseGeneric`. Their existing app wrappers retain control
scheduling and UI interfaces and share `CircularBufferGeneric` storage.
Stereo and quad processing, tuning, response and ring wraparound are covered
by native Lean/C++ comparisons. See the [delay proof and integration
overview](../README.md#delay-filter-codegen-and-app-integration).

[SquiggleBoyQuadReverb.hpp](../generated/SquiggleBoyQuadReverb.hpp) is the owning
192 kHz reverb export. Its constructor and `ComputeCoefficients` take a
`LogicalSpec` with per-channel delay in samples, linear feedback, damping base
in cycles per sample, damping width, and tanh input gain. `Process` takes a quad
audio sample and returns the quad output. The processor retains the delay and
filter histories across coefficient updates.

[QuadReverb.lean](QuadReverb.lean) binds the Lean sample schedule to in-place
storage operations and emits their owning C++ types. The fixed-tap intro model,
the wandering Lean cascade, and the C++ renderer use the same stage order and
tap constants from Lean. Each wandering Schroeder stage owns a spline stream
and its delay ring. A stage shares one smoothly
varying fractional offset across its lanes. The concrete reverb gives each
stage a distinct seed and a peak offset of 128 samples. Every 32 samples, the
stream draws an exponential speed with a mean of one knot per 1,920,000 samples
and applies a 48,000-sample one-pole to the speed. It evaluates the spline at
frame boundaries and linearly interpolates between those values.
See the [framed spline proofs](../SmoothRandomLFO/FramedProofs.lean).
The fixed-tap transfer proofs apply to the separate fixed configuration.

The arithmetic evaluator lowers the mixer, saturation, damping cutoffs, and
cubic interpolation. The delay codegen test compares both reverb
configurations and spline knot crossings against native Lean in float and
double.

The generated filters are under [generated](../generated): the ZOH and TPT
one-poles, TPT highpass and base-width band, `TPTSVF`, `MorphingTPTSVF`, the
three `TPTSVF4Pole` variants, `TPTLadder` and `LinkwitzRileySplitter4BandGeneric`. Each is a
standalone C++17 header with nested `State` and `Input` records, stored
`m_state`, and `void Process(Input& input)`. Compact processors expose current
observations in a separate `Output m_output`; coefficients and recurrence
memories stay in `m_state`. The ZOH one-pole retains `m_state.m_y` because its
output is itself the recurrence memory. Headers have no Lean runtime dependency. Delay and reverb now use `TPTBaseWidthGeneric<float, 4>` through
the app wrapper [TPTBaseWidthFilter.hpp](../../../private/src/TPTBaseWidthFilter.hpp);
the app also uses generated SVFs in the source mixer, both shape machines,
the Naive Reupping Ladder's band-pass envelope followers, and the physical-modeling source.
[TPTSVFFilter.hpp](../../../private/src/TPTSVFFilter.hpp) preserves the app's
`SetCutoffAndResonance` mapping: natural frequency in cycles per engine sample,
clamped to 0.499, and `Q = 0.5 * 11^resonance`. Q is per section in the four-pole
high-pass cascade. Thus zero resonance gives -6.02 dB per section at cutoff
(-12.04 dB for the cascade), not a Butterworth -3 dB convention. Generated coefficient and morph trig now use
`Math4096` instead of `Math`, and arithmetic uses explicit FMA. Physical modeling
uses the generated quarter-circle LP/BP/HP morph with its input clamped to [0, 1].
Snapshots compose power responses; saturation, envelope and sample-rate reduction
remain outside these linear response plots. The separate
[CoupledProcessor.hpp](../generated/CoupledProcessor.hpp) exercises the
`State × Output` form, nested coefficients and two interacting state variables.

The splitter and multiband saturator are exported only as `Generic` headers. The other filters also have a `Generic` header, such as
[TPTSVFGeneric.hpp](../generated/TPTSVFGeneric.hpp), parameterized by
`Number` (`float` or `double`) and `Size`. These include the app's
`MultiChannelNumber.hpp`; add `private/src` to the compiler's include path.

The backend in [Cpp.lean](Cpp.lean) reads an elaborated Lean function and its
record definitions. It does not contain a one-pole arithmetic template.

The response-bearing filter headers also contain a coefficient-only nested
`Snapshot` and static squared-magnitude response methods, alongside its
processing state and `Process`. Scalar and Generic headers have the same
organization. Single-output filters use `MagnitudeSquared`; the SVF provides named methods for its lowpass, bandpass, highpass, notch, and all-pass outputs. The responses
are generated from the same Lean definitions used in their proofs. See the
[response proof walkthrough](../Filters/Responses/README.md) for the transfer
connection, cascade/mixing/feedback composition, tuning contracts and endpoints.

## Sampled UI and coherent snapshots

The production `#generate_cpp_sampled_filter` and `_generic` commands pair a
processing step with `prepareCoefficients` and its response methods. The Lean
preparer returns `State × Snapshot`. C++ `ComputeCoefficients` writes only the
coefficient fields and caches the paired snapshot. `MakeSnapshot()` copies that
record; `Process` does not alter it. Initialize coefficients before capture,
and update them through `ComputeCoefficients` to keep the cache coherent.
Direct writes to `m_state` bypass this contract.

[Capture.lean](../Filters/Capture.lean) composes child preparers for nested
filters. [Capture/Proofs.lean](../Filters/Capture/Proofs.lean) proves, for every
family, that the state projection equals the existing updater, that preparation
commutes with channel projection, and that the real snapshot is the one used by
the cutoff/response theorems. Morph's snapshot theorem assumes its valid blend
range. Thus captured `g` and mix weights come from tuning intermediates, without
reconstructing them from rounded processing coefficients.

The shared [GeneratedFilterUI.hpp](../../../private/src/GeneratedFilterUI.hpp)
adapts the raw record and selected function to `TransferFunctionGeneric`. It
supplies lane metadata and prepared-frequency conversion. It has no virtual
snapshot or complex-response requirement. Evaluation uses the generated filter's
chosen numeric precision; the existing sampler stores float power values.

```cpp
using Filter = OnePoleTPTLowPassGeneric<float, 3>;
Filter filter{};
Filter::UIState ui;
SampledSnapshotWriter<Filter::UIState> writer;
writer.Bind(&ui);
filter.ComputeCoefficients({Filter::Vector::Splat(0.01f)});
filter.PopulateUIStateSample(writer);
```

Call the writer at the normal audio cadence; it preserves the existing host-frame
and random sampling policy. The all-tap SVF exposes `LowPassUIState`,
`HighPassUIState`, `BandPassUIState`, `NotchUIState`, and `AllPassUIState`.
`TPTSVFLowPassGeneric`, `TPTSVFHighPassGeneric`, `TPTSVFBandPassGeneric`,
`TPTSVFNotchGeneric`, and `TPTSVFAllPassGeneric` are separate generated audio
specializations (with scalar counterparts), one header per export. Each saves
only its selected tap and exposes `MagnitudeSquared` and `UIState`. They use
the shared Lean `Processor.mapOutput`, with a simulation proof for each selection;
there is no tap-selection logic in the backend.

`GeneratedFilterView<Filter, &Filter::MagnitudeSquared>` adapts a selected
processor to a composable response `Snapshot`. The app wrappers do this for
their LP/HP/BP filters. A handwritten composite can store these snapshots,
capture children in one callback, and multiply their power responses for a cascade.
Lean nested filters continue composing their own snapshots and kernels directly.
Parallel sums and feedback still need phase during algebraic derivation; their
final generated kernel is squared magnitude, not a product of branch powers.

The sampled UI now carries powers throughout, including order statistics, and
plots `10*log10(power)`. Legacy handwritten complex helpers remain for internal
phase-sensitive composition and reference tests. The app's mastering crossover
uses the generated LR splitter. The old
handwritten crossover and generated `LinkwitzRileyCrossover` have been retired.

`LinkwitzRileySplitter4BandGeneric` uses the separate processing/tuning/snapshot
export. It has static `MakeSnapshot(spec)`, with no `UIState`, sampled publisher,
or emitted response kernel. Consumers such as the multiband saturator own the UI
and compose the splitter snapshot into their response. The splitter's Lean response and
reconstruction proofs remain available for that composition.

## Authoring and generation

Author a pure function of one of these shapes, using named records:

```lean
def step (state : State) (input : Input) : State := ...
def process (state : State) (input : Input) : State × Output := ...
```

An export can run an ordinary `StateM` action and project its result into either
form. The evaluator unfolds the state transitions and preserves reads of the
old value before storing the returned state. A field of type `Vector A n`
with a closed capacity uses indexed storage. `Vector.ofFn` over that field emits
one counted loop and `Vector.get` reads at a bounded index. Small function-valued
`Fin n → A` topologies still use the existing unrolled representation.

`#generate_cpp_effect` compiles a `StateM` action over an exclusive native
storage view. Binds sequence native reads and writes. The emitter validates
that effects use the current storage and that branches retain it. A loop
threads the latest handle through its step. The
[native bindings](NativeBindings.lean) list storage operations and numerical
primitives used by these actions. Calls to other generated procedures require
the target definitions to be exported in the same Lean environment first.
The state and read-only view exports check native index additions, signed
arithmetic, and signed/unsigned conversions against their machine ranges.

For composition, prefer `State × Output`: persistent coefficients and recurrence
memories belong in `State`; current-sample observations belong in `Output`.
The parent keeps child memories, consumes child outputs locally, and returns
only the observations it exposes. The backend unfolds this Lean composition
and removes unused pure temporaries. There is no runtime tap mask or separate
C++ implementation of a child filter. Returning all child taps inside the
parent's state would make their stores observable again.

Existing proved models can retain their richer state representation.
[Processor/Compact.lean](../Processor/Compact.lean) supplies `compact`, which
saves the persistent fields and restores an equivalent model state before its
next step. A simulation proof must show that the omitted observations cannot
affect that step. The existing `Processor.run_simulation` then identifies the
entire output stream, including for arbitrary initial histories and inputs.
This is a Lean representation change, not a backend permission to ignore
returned fields. Arithmetic and FMA order still come from the original step.

The SVF/LR/multiband `Runtime` modules establish this pattern. Restored SVF taps
and splitter leaf observations are dummy zeros that disappear during export;
the recurrence never reads them. Tuning, prepared installation and reset
commute with the state projection. The multiband output retains the exact
before/after saturation values consumed by C++ meters. Differential tests
continue running the original model and compare every retained memory and
exposed observation against generated execution.

The original proof models use the first form, with observations stored as fields.
Production filters export their compact `Runtime.process` adapters. The TPT
one-poles, base-width filter, morph, four-pole cascades and both Lean ladders use
the same pattern in [Filters/Runtime.lean](../Filters/Runtime.lean). Their
`simulation` theorems transfer complete executions through `run_simulation`;
`prepare_save` identifies both tuned memory and the captured response snapshot.
The conservative ladder additionally proves `reset_save`, retaining all four
stage memories and its one-sample feedback semantics. ZOH's output remains
stored once as true history. Float-specialized wrappers choose executable binary64 arithmetic. [GenerateDSP.lean](../../GenerateDSP.lean)
exports them, for example:

```lean
#generate_cpp DSP.TPTSVF.Runtime.processFloat as TPTSVF to "build/lean/generated/TPTSVF.hpp"
```

For multichannel output, export the same generic step directly:

```lean
#generate_cpp_generic DSP.TPTSVF.Runtime.process as TPTSVFGeneric to "build/lean/generated/TPTSVFGeneric.hpp"
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

For a filter with a response, bind the Lean frequency record to the existing
C++ type, then export processing and response together:

```lean
#use_cpp_record DSP.ResponseFrequency as ResponseFrequencyGeneric from "ResponseFrequency.hpp"

#generate_cpp_filter_generic DSP.OnePoleZOHLowPass.step
  response DSP.OnePoleZOHLowPass.Snapshot.magnitudeSquared
  as OnePoleZOHLowPassGeneric to "build/lean/generated/OnePoleZOHLowPassGeneric.hpp"
```

The response has shape `Snapshot R → ResponseFrequency R → R`. The filter
export assembles the two Lean definitions in one C++ struct, with nested
`State`, audio `Input`, and coefficient `Snapshot`. Its `Frequency` alias refers
to `::ResponseFrequencyGeneric<Vector>` from the app's
[ResponseFrequency.hpp](../../../private/src/ResponseFrequency.hpp). There is
no separate response struct or generated frequency copy. `#generate_cpp_filter`
does the same for a scalar specialization.

Filters with several outputs name each method explicitly. They share a single
snapshot and frequency type, which the exporter checks by Lean type identity:

```lean
#generate_cpp_filter_responses_generic DSP.TPTSVF.Runtime.process
  [DSP.TPTSVF.Snapshot.lowPassMagnitudeSquared as LowPassMagnitudeSquared,
   DSP.TPTSVF.Snapshot.bandPassMagnitudeSquared as BandPassMagnitudeSquared,
   DSP.TPTSVF.Snapshot.highPassMagnitudeSquared as HighPassMagnitudeSquared,
   DSP.TPTSVF.Snapshot.notchMagnitudeSquared as NotchMagnitudeSquared]
  as TPTSVFGeneric to "build/lean/generated/TPTSVFGeneric.hpp"
```

The scalar form is `#generate_cpp_filter_responses`. Empty lists, duplicate or
reserved method names and mismatched snapshot/frequency types are rejected.

The common C++ record stores `m_cyclesPerSample`, `m_sinHalf`, and `m_cosHalf`
as vectors, and `m_zInverse` as `MultiChannelComplex`. That existing complex
type owns `m_real` and `m_imag` vectors. The Lean record mirrors this nesting
with `zInverse : ComplexValue R`. Codegen reads those same member paths directly.
The external binding validates the Lean record shape; compiling and comparing
against the actual C++ header checks the representation boundary.

The UI's `ResponseFrequency` alias is the one-lane double-vector form. Typed
snapshots use `ResponseFrequencyGeneric<MultiChannelNumber<double, Size>>`.
The plot caches one-lane prepared frequencies, and `ResponseAll` widens each
frequency once before its snapshot loop. Scalar query adapters do the same
widening for individual queries. Direct vector calls can supply different
frequencies per lane; DC selection in high-pass and SVF responses is per lane.

[ResponseFrequency.lean](../ResponseFrequency.lean) defines `fromHalfAngle` to
build the complex delay from prepared sine and cosine. A functor map converts
precision or selects lanes; `broadcast` shares one frequency across channels.
These are data operations; the exact trigonometric interpretation lives in
[ResponseFrequency/Proofs.lean](../ResponseFrequency/Proofs.lean).

For example, the prepared object below can be passed directly to either the
generated response or a handwritten three-channel snapshot:

```cpp
using Filter = OnePoleZOHLowPassGeneric<double, 3>;
using Vector = Filter::Vector;
const Filter::Snapshot snapshot{Vector(0.1, 0.2, 0.3)};
const auto frequency = Filter::Frequency::Hz(1000, 48000);
const Vector squared = Filter::MagnitudeSquared(snapshot, frequency);
```

The generated method takes const references and returns magnitude squared.
It uses explicit FMA and a safe denominator for zero alpha. Snapshot capture
from the generated processor and its UI sampling adapter remain caller work.
Each filter keeps its own coefficient snapshot; there is no filter typeclass
or inheritance requirement.

The standalone `#generate_cpp_function` and `#generate_cpp_function_generic`
forms remain available for other pure kernels. They accept a named snapshot
and a numeric or registered-record input, returning the same numeric type.
`#generate_cpp_record` can still emit a new data template where no C++ record
already exists; `#use_cpp_record` reuses an existing header without writing it.

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

## Coefficient updates

The sampled filter exports use a tuned form that assembles three source
functions: processing, coefficient preparation, and one or more responses.

```lean
#generate_cpp_tuned_filter_generic DSP.OnePoleTPTLowPass.step
  DSP.OnePoleTPTLowPass.computeCoefficients
  [DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquared as MagnitudeSquared]
  as OnePoleTPTLowPassGeneric to "build/lean/generated/OnePoleTPTLowPassGeneric.hpp"
```

The scalar form is `#generate_cpp_tuned_filter` with specialized Float/Float32
functions. The updater has shape `LogicalSpec R → State R → State R`; its state
identity and target precision must match the processor. The generated method is
`void ComputeCoefficients(const LogicalSpec& spec)`, with `LogicalSpec` nested
alongside `State` and `Snapshot`. It stores only fields whose returned expression
is different from the original field, after capturing all new values. This
preserves simultaneous record-update semantics while avoiding history writes.
The production tuners separately prove that all history fields remain unchanged.

Sampled-filter exports may additionally name `prepared <Lean function>` before
their response list. This generates
`ComputeCoefficientsFromSnapshot(const Snapshot& spec)` in the same filter. The
function must accept that exact response snapshot and the processor state, and
return the matching state/snapshot pair; the generator checks both record
identities and numeric types. It reuses the snapshot declaration and cache,
and still writes only changed state fields. `TPTSVF` uses this for already known
integrator gain `g` and damping `k`, avoiding a conversion to frequency and back.
Its ordinary cutoff/Q preparation delegates to the same Lean function.

`Trig.sin2pi/cos2pi/tanPi` lower to `Math4096::Sin2pi/Cos2pi/TanPi`.
`ExpDecay.expNegTwoPi` lowers to `MultiChannelNumber::Exp()` after multiplication
by the target-precision −2π constant. Scalar exports use the one-lane vector
overloads as well, including for exponential decay. The recognized native Lean
wrappers use libm; **coefficient preparation is deliberately an approximation
boundary**, unlike the bit-identical arithmetic tests for steps and responses.
Generated coefficient tests check direct Math4096/Exp results bit for bit and
measure native-Lean differences separately. No trig identities are asserted for
the lookup table or Float arithmetic.

See the [tuning theorem inventory](../Filters/Tuning/README.md) for parameter
units, validity regions, cutoff/peak proofs, nested reuse, and intentional
convention differences from the handwritten app. Snapshots and frequency-query
preparation retain their existing interfaces.

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
a1, a2 and a3 or a morph's cosine and sine, is now provided by the generated
`ComputeCoefficients` method. Initialize history explicitly before the first call;
the tuner updates coefficients in place and preserves history.

For each call, a state-only `step(state_n, input_n)` returns `state_{n+1}`, and
`process(state_n, input_n)` returns `(state_{n+1}, output_n)`. Projected old values
are captured, and existing expression temporaries are reused. A processor stores
each changed leaf after its last old-value read and the last computation using
its saved result. Complete conditional branches are scheduling boundaries;
stores cannot split an `if`/`else`. Unchanged fields are not rewritten. This
preserves state swaps and outputs that observe old values without keeping an
entire filter tree's results live until the end of the sample. Arithmetic order,
FMA boundaries and all returned state/output fields are unchanged. Coefficient
updaters retain their capture-before-commit order, including when a prepared
snapshot argument aliases the cached snapshot. In the `State × Output` form
the previous `m_output` is not an implicit input to subsequent calls; persistent
history belongs in `State`. A `resetWith` function may return `State × Output`
to clear exposed observations together with histories; the backend checks both
record identities. A state-only reset retains the previous output record.

The ZOH one-pole stores the output at the end of the current held-input
interval. The TPT filters store the current-sample responses of their solved
zero-delay loops, which depend on the current input.

## Supported subset

- Monomorphic, pure functions with exactly two arguments, returning either the
  incoming state type or a `State × Output` pair whose first component is it.
- Pure-function exports of a named snapshot record and numeric or shared-record
  input to a result of the same numeric type, with no state mutation.
- Shared record templates with one number type parameter. Supported fields
  follow the same rules as processor records; duplicate exports and conflicting
  C++ names are rejected.
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
literal numeral), `OfScientific`, `IfLess`, `FMA`, `Trig` and `ExpDecay`, then the two runtime records.
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

The shared [ResponseFrequency/Proofs.lean](../ResponseFrequency/Proofs.lean)
defines `ofCyclesPerSample f` using exact real sine and cosine, proves the
half-angle identity, and proves that the prepared delay equals `exp(-2πif)`.
`prepared.Represents f` is the proposition that the real record equals that
exact preparation. This proposition is separate from the data; it is neither
stored in generated C++ nor asserted for floating-point approximations.

For the ZOH response, `Snapshot.magnitudeSquared_eq_normSq` in
[Response/Proofs.lean](../Filters/OnePoleZOHLowPass/Response/Proofs.lean) proves
that the real interpretation of the exported kernel equals `Complex.normSq`
of the shared digital transfer when given `ofCyclesPerSample frequency`, for
nonnegative alpha. The companion theorem `_of_prepared` accepts any real
record with a `Represents` proof. The channel lemmas in
[Channels.lean](../Filters/OnePoleZOHLowPass/Channels.lean) establish pointwise
evaluation and lift that identity to independently parameterized real channels,
at either a broadcast frequency or individually prepared frequencies.
Generation reads `Snapshot.magnitudeSquared` itself; it does not read or
reconstruct a formula from the proof term.

The backend is **not a formally verified compiler**, and these theorems do not
prove IEEE rounding behavior or the compiled C++ executable. The code generator,
Lean compiler/runtime used for native reference execution, C++ compiler and
floating-point environment remain part of the trusted implementation boundary.

The scalar differential test executes the actual Lean source functions and
compares every generated state/output field bit for bit. It covers:

- ZOH and TPT one-pole trajectories with changing inputs, endpoint/near-one coefficients,
  signed-zero initial states and subnormal values.
- Trajectories of every TPT composite from random coefficients, integrator states
  and stored responses, including both morph halves and ladders with feedback.
- The nonlinear Conservative Reupping Ladder, including normalized saturation,
  nonzero histories, fixed feedback and coefficient-preserving reset.
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
It also contains 4,240 native Lean ZOH response cases, executed in both precisions
and packed into 8,834 pure vector calls at widths 1, 2, 3 and 4. Every result is
compared bit for bit with both scalar and generic generated C++. Coverage
includes mixed zero/positive alpha lanes, alpha one and its predecessor, DC,
Nyquist, signed zero, subnormal inputs and binary32 squared coefficients, and
random independent coefficients and prepared sine values. The compiler corpus
uses arbitrary data in the other frequency fields; a second kernel reads all
five numeric leaves, including both components of `MultiChannelComplex`. C++
assertions check that both kernels and the handwritten ZOH response use the
same shared frequency type at double precision. Three-channel padding is poisoned. The native response
smoke tests separately use trigonometric preparation and compare against complex
denominator components. Core tests pass one prepared object to handwritten and
generated ZOH responses, and compare independent vector-frequency lanes with
scalar response queries, including mixed DC and Nyquist endpoints. Other exporter fixtures cover unused arguments and
direct returns, and rejection checks cover unsupported argument/result shapes,
mixed number types, external operations, invalid shared records and conflicting names.

Passing these checks is implementation evidence, not a universal compiler proof
or a floating-point error bound.

## Circular storage and delay preparation

```lean
#generate_cpp_delay_buffer CircularBufferGeneric to "build/lean/generated/CircularBufferGeneric.hpp"
#generate_cpp_generic DSP.DelayKernels.Runtime.Comb.process as TunedDecayCombCoreGeneric
  to "build/lean/generated/TunedDecayCombCoreGeneric.hpp"
#generate_cpp_preparation_generic DSP.DelayKernels.Runtime.prepareCoefficients
  as TunedDecayCombTuningGeneric to "build/lean/generated/TunedDecayCombTuningGeneric.hpp"
```

The comb and Schroeder cores use the compact `State × Output` convention.
Their runtime simulations preserve the original rich model's output streams;
native differential checks still execute that original model as the oracle.
The standalone preparation command reuses the filter-bank preparation emitter:
it takes one scalar or record input, returns a record, and exports a stateless
`static Prepared Prepare(const Input&)` (a vector argument for arithmetic scalar inputs).
The comb wrapper calls this API when it accepts new parameters.

The buffer command exports a fixed-capacity storage component. It specializes
the actual `DelayBuffer.address` and `advance` definitions with a template
capacity, erases their `Fin` proofs and emits checked native indices. Reads
leave the head unchanged; `Push` performs one array write and advances once.
For power-of-two capacities no greater than half the native unsigned maximum,
the buffer-specific renderer emits the modular forms proved by `address_mod`
and `advance_mod`. `modulo_intermediates_bounded` establishes that their sums
fit the native index and their subtractions do not saturate. Constant modulo
then compiles to masking. All other capacities retain the conditional lowering.
The array-read/write/clear lowering is an explicit tested compiler boundary.
It does not unroll or copy the whole delay line for each sample. Capacity must
be positive; `ReadAge` accepts `[0,Capacity)` and `ReadDelay` accepts
`[1,Capacity]`. `ReadDelays` gathers independent channel taps.

The phase vocoder uses the common state exporter for `ResynthesisFrame`,
`AnalyzePVDR`, and `SynthesizeSpectrum`. The exact model uses a cyclic sample
grid, one-sided spectra, and mathlib's finite DFT with the existing forward
normalization and inverse grid expansion. The app's storage views expose
packed transforms, heap operations, phase draws, coefficient access and
numerical lookups. Lean orders the transform calls, bin visits, phase updates,
pitch scatter, and window application. Native comparisons check the spectral
step with the same phase draws, and compare the full grain with the staged
transform and window calls. The fixed-floor experiments select a Lean
classifier in the same generated PVDR traversal.

| Export | Native storage and numerical operations |
| --- | --- |
| `AnalyzePVDR32/64` | Read coefficient data and graph links, store gates and visits, push and pop heap entries, draw a random phase |
| `SynthesizeSpectrum32/64` | Read controls and visits, read and store phases and coefficients, clear the spectrum, evaluate polar coefficients and pitch ratios, round bin positions |
| `ResynthesisFrame32/64` | FFT and IFFT, coefficient magnitude and argument, slew coefficient approximation, Hann coefficients, sample and state field reads and writes, per-field fills |

The frame's `AnalyzeGraph` and `Synthesize` bindings forward to the other
generated procedures. The common emitter handles standard arithmetic,
comparisons, numeric casts, square root, floor and the unison exponential.
The packed FFT, IFFT and numerical leaf implementations use the app's
[storage adapters](../../../private/src/Resynthesis.hpp).

The general processor exporter still rejects runtime-sized arrays and
unbounded Nat fields. Its fixed topology vectors retain their 256-element
limit. The buffer-specific renderer binds the bounded cursor values and
capacity explicitly, allowing natural addition, saturating subtraction and
proof-dependent comparisons in those definitions.

`DelayMath` adds `exp2`, `floor`, `phaseTurns` and `decay` to preparation.
`phaseTurns(y,x)` lowers to `Math::ATan2(y,x)/(2*pi)`; `Math::ATan2` delegates to
`std::atan2` for each lane. `decay(x)` means `exp(-log(1000)*x)` and uses the
existing vector exponential. The app tunes the comb in double precision,
which uses libm for exponentiation. Float `Exp2` requires inputs in
`[-126, 127]`. Native real instances keep exact pi, complex argument, floor
and logarithms. The double-precision reference uses libm.

The delay tests retain their native corpus at
`build/lean/codegen-test/delay-reference.txt`. Processing and ring state match
bit for bit at stereo/quad widths. The tests use double precision for the comb
and both precisions for Schroeder all-pass. Tuning validates normalization and
bounded delays as well as reconstructed peak gain and loop phase. This distinguishes
the sensitivity of fractional-delay coefficients from the accuracy of the
assembled response.

The comb adapter stores its generated core persistently inside each branch.
Coefficient installation also updates the native delay taps and preserves the
live memory. Processing calls that core directly; snapshot capture reads its
coefficients. This adapter change and the buffer-specific modular lowering do
not extend the general processor exporter.

## Fixed topology processors with snapshot capture

The [Conservative Reupping Ladder](../Filters/ConservativeReuppingLadder/README.md)
adds its proved small-signal response using the same sampled-filter export as
linear filters, with an optional generated reset:

```lean
#generate_cpp_sampled_filter_generic DSP.ConservativeReuppingLadder.step
  DSP.ConservativeReuppingLadder.prepareCoefficients
  resetWith DSP.ConservativeReuppingLadder.reset
  [DSP.ConservativeReuppingLadder.Snapshot.magnitudeSquared as MagnitudeSquared]
  as ConservativeReuppingLadderCoreGeneric to "build/lean/generated/ConservativeReuppingLadderCoreGeneric.hpp"
```

`resetWith` checks the complete state type and preserves the captured response.
The Lean reset must preserve coefficients, as proved for this ladder. The kernel
is proved equal to the power of the actual processor's time-first small-signal
transfer. `Tanh` lowers to clamped cubic rational arithmetic with explicit FMA;
the real instance is exact tanh, with no rational/transcendental equality claim.
`SquareRoot` lowers to vector `Sqrt` or scalar `std::sqrt`. Native source and
independent complex-response comparisons cover both precisions and widths 1–4.

The splitter uses a processing/tuning/capture export without a response method:

```lean
#generate_cpp_tuned_snapshot_generic DSP.LinkwitzRileySplitter.step4Band
  DSP.LinkwitzRileySplitter.computeCoefficients4Band DSP.LinkwitzRileySplitter.makeSnapshot4Band
  as LinkwitzRileySplitter4BandGeneric to "build/lean/generated/LinkwitzRileySplitter4BandGeneric.hpp"
```

Capture takes one logical specification and returns a coefficient record.
The generated `MakeSnapshot` is static. The exporter compares complete state
and specification types, including fixed topology parameters, so equal layouts
or a shared record name cannot hide incompatible signatures. Literal Nat
comparisons used for static band selection are evaluated during specialization;
external, unsafe, or dynamic topology definitions remain rejected. Recursive
record types reduce to fixed nested records. No runtime vector of records is
needed. Per-band transfers remain proof definitions for a later consumer.

## Coupled channels and packed prepared banks

The four-band multiband saturator uses the same sampled-filter backend. Its
`State × Output` result includes a fixed array of saturation-observation records;
record arrays are emitted as `std::array` with a named nested element struct.
The shared `ChannelMean` arithmetic operation lowers to `MeanBroadcast`, with a
function-channel Lean interpretation and native differential tests. All other
arithmetic remains pointwise. Static natural equality and constant Boolean
conditions are folded only through the known primitive definitions.

A generic sampled filter may add:

```lean
preparedBank DSP.MultibandSaturator.install4Band
prepareWith DSP.LinkwitzRileySplitter.prepareSection
```

This emits a `PreparedSpec` and `InstallPrepared` method that preserve histories
and update the cached response snapshot. The installer is checked against the
same state and snapshot types as normal tuning. It takes its prepared record by
value, making its inputs independent of the destination during installation.
The pure installer is always inlined so this value does not become an aggregate
copy across a function-call boundary.
Callers can construct a local bank for the compiler to eliminate without
retaining a second expanded coefficient bank in the processor.
`Preparation<Width>::Prepare`
is a nested pure factory accepting a numeric or record input and returning a
`Prepared` record. Its arithmetic width is independent of the audio width, so
three cutoffs can share a Math4096 call before their fields are broadcast into
the audio bank. The factory uses the existing expression evaluator, including
external-operation checks and FMA/trig lowering.

The evaluator tracks closed literal arithmetic at both float and double
precision. A cosine whose phase is exactly an eighth-cycle multiple at the
target precision uses `Math4096::CosEighth`, a constexpr entry shared with the
runtime table. This lets C++ fold the LR damping and its reciprocal, retaining
the table's float rounding even for double arithmetic. Other constant phases
and all runtime phases keep the normal Math4096 lookup; the generator does not
substitute a higher-precision transcendental result or approximate nearby phases.

Unused pure expression temporaries are removed after result projection. This
avoids computing or retaining the unused side of a compound result such as the
pair of LR weights. Mutable branch joins, persistent stores and output stores
are retained. The normal codegen suite also checks the multiband core against
native Lean, including every stored field, all meter observations and the power
response.
