# Encoder state-machine specification

This directory specifies the encoder runtime in **Lean 4.32.0 and Veil**, with
73 EARS entries and kernel-checked proofs. Observable control behavior is specified
independently of evaluation strategy. Supporting models describe the prepared
evaluator, editable recursive encoder state, and committed `HEAD` at
`0a0d206529be45b6ff3ac82824c65ebe73b49003`. Production C++ is
not generated from this model and has not been formally verified by it.

## Run

From the repository root:

```sh
make lean-setup
make lean-cache  # optional: download the pinned Mathlib proof cache
make lean-test
```

Setup installs Elan and the pinned Lean toolchain under `build/lean/elan`, without
changing shell profiles or a global Lean installation. Verification uses the
committed `lake-manifest.json`; it builds incrementally under `build/lean/`.
The first build downloads dependencies. `lean-cache` avoids compiling the
Mathlib dependencies used for real exponentiation and arithmetic tactics; its
cache stays under `build/lean/mathlib-cache`. Veil is built locally. It needs Git, curl,
Python 3, a C/C++ toolchain, and Node/npm (upstream recommends Node 24+).
Lean verification is independent of app/audio builds. Do not run concurrent Lake
builds against this worktree's output directory.

`make lean-test` checks source correspondence, builds every proof, then reruns
the axiom/contract audit even when build artifacts are cached. It also writes a
[readable EARS index](../../build/lean/contracts.md) with current C++ line links.
The versioned contract source is [requirements.json](requirements.json). The
[HEAD comparison](../../build/lean/head-comparison.md) classifies every contract;
its versioned source, pinned revision and C++ references are in [head.json](head.json).

For direct Lake commands, use `scripts/lean.sh build Encoder.Core` or
`scripts/lean.sh env lean path/to/file.lean`. The wrapper resolves the checkout
from its own location, including when invoked from another working directory.

## Model structure

| File | State/behavior | Main proof |
| --- | --- | --- |
| [OnePole.lean](OnePole.lean), [Settling.lean](Settling.lean) | Fixed coefficient, contraction, snap, finite completion, masked execution and early retirement | `run_lands`, `trajectory`, `early_retirement_preserves_values` |
| [Core.lean](Core.lean) | Eight current/target scene values; separate linear/one-pole policies; recording requests; four output lanes; host-frame history and oversample publication | `ramp_finishes`, `compute_valid`, `same_host_preserves_history` |
| [Motion.lean](Motion.lean) | Ramp step bounds, settled refresh equivalence, blend sensitivity and single-fader motion | `ramp_step_between`, `settled_refresh_preserves_ramp_values`, `blend_motion_bound` |
| [Behavior.lean](Behavior.lean) | Veil host-frame behavior with current inputs/ramp values and eager publication, independent of evaluation path | Generated preservation theorems for input freshness, evaluated output and published cache |
| [StateMachine.lean](StateMachine.lean) | Veil actions for target setting, recorded edits, reset, compute, output writes and publication | Generated initialization/preservation theorems for `ramp_bounds` and `published_cache` |
| [Reachability.lean](Reachability.lean) | Arbitrary action sequences in **Veil's generated transition relation** | `EncoderRuntime.reachable_safe` |
| [Evaluation.lean](Evaluation.lean) | Ordered signed modulation sums, gestures, depth caching and compensated scene edits | `mix_bounded`, endpoint/gesture/compensation theorems |
| [Control.lean](Control.lean) | Dependency predicates, ancestor invalidation, prepared eligibility, deferred synchronization and host gating | `prepared_requires_settled`, `synchronization_preserves_live_and_history` |
| [Prepared.lean](Prepared.lean) | Veil protocol for live/evaluated snapshots, materialized tree state, revisions and fallback/prepared transitions | Generated theorems for snapshot coherence and schedule validity |
| [Schedule.lean](Schedule.lean) | Child-before-parent graph evaluation, with arbitrary nesting and pure operations | `prepared_equals_recursive` |
| [Crossfade.lean](Crossfade.lean) | Scene-manager change flags and ordinary distinct-scene increment arithmetic | `no_blend_change_is_lost`, `distinct_scene_increment` |
| [Edits.lean](Edits.lean) | Ordinary/Shift combine, sign and zero handling, scene shares, recursive descendant visits and gesture edit allocation | `shift_preserves_sign`, `shift_edits_every_descendant`, `actual_edit_partition` |
| [Curve.lean](Curve.lean) | Ideal real exponential law and a separate exact-arithmetic model of the SIMD polynomial | Ideal bounds/monotone magnitude/half-depth; polynomial zero/oddness/saturation |
| [Head.lean](Head.lean) | HEAD recursive evaluation and its original publication loop | `prepared_publication_equals_head` |
| [HeadStateMachine.lean](HeadStateMachine.lean), [HeadReachability.lean](HeadReachability.lean) | Separate HEAD Veil runtime using shared leaf operations | `EncoderHeadRuntime.reachable_safe` |
| [HeadProtocol.lean](HeadProtocol.lean) | HEAD eager recursive protocol | Output/snapshot coherence and eager materialization invariants |
| [Reset.lean](Reset.lean) | Scoped recorded reset writes, ramp coherence, finite completion and forced evaluation | `reset_finishes`, `reset_is_dirty` |
| [ResetTree.lean](ResetTree.lean) | Recursive depth neutralization, gesture deactivation, scene preservation and bank/full-owner scope | `reset_descendant`, `reset_gesture_weight_zero`, `full_reset_reaches_hidden` |
| [ResetEvaluation.lean](ResetEvaluation.lean) | Reset tree evaluation, semantic reclamation and publication in both versions | `reset_tree_returns_default`, `both_publish_reset_default` |
| [GestureLifecycle.lean](GestureLifecycle.lean) | Optional allocation, ordered scene capture/activation, flags, recording, toggle, deactivation and a ramp counterexample | `capture_blend_continuity`, `changed_capture_finishes`, `capture_stalled_counterexample` |
| [GestureTree.lean](GestureTree.lean) | Keyed gesture deletion through nested modulators; selected Shift-press; owner/bank scope | `clear_every_descendant`, `shift_press_keeps_descendants`, `clear_is_dirty` |
| [GestureMachine.lean](GestureMachine.lean), [HeadGestureMachine.lean](HeadGestureMachine.lean) | Separate current/HEAD Veil lifecycle machines with parent/gesture interleaving | All action-preservation theorems for bounds, parent coherence and absent slots |
| [PatchLoad.lean](PatchLoad.lean) | Recursive serialized-slot replacement/reuse, current/target preservation, saved-domain comparison and an explicit old-loader counterexample | `old_polynomial_reload_counterexample`, `reload_preserves_state`, `reload_preserves_output_trajectory` |
| [Audit.lean](Audit.lean) | Existence of every contract theorem and transitive axiom inspection | Build fails for missing proofs or unapproved axioms |

The runtime model exposes small atomic operations rather than treating the whole
audio callback as one transition. `turn` is one scene-target write after edit routing/weighting (including the
zero-delta no-op); it is not the full hardware input handler. `compute` models the ramp-state part of C++
`Compute`; `set_output` models its write; `publish` models cache publication.
The numerical and graph contracts justify evaluation separately. Publication's
`published` flag and recording-request count are ghost state, not proposed C++
fields. `published` marks completion of a cache write; intermediate states are
not claimed to be DSP-observable.

The prepared protocol models a connected, named root. Its `snapshot` contains
the relevant scene/target/source/amplitude/gesture inputs, `evaluate` computes a
tree image, and `rootOf` projects its root output. These are universally
quantified pure functions, not axioms asserting their desired result. An edit
or input change may occur before the next host evaluation. Synchronization uses
the **last evaluated** snapshot, while preserving the live one. The protocol's
force counter abstracts the eligibility of the active subtree; actual recursive
eligibility is a separate C++ correspondence obligation.

The behavioral machine has no prepared/fallback branch, schedule revision or
layout constraint. Each host action advances the ramp, evaluates current inputs
and publishes the oversample cache. One action corresponds to one 48 kHz host
frame; it does not introduce a slower control rate. Its pure evaluation kernel
is abstract: numerical and dependency-graph equations remain separate proofs.
The native implementation may skip work when the result would be unchanged.
This machine is a behavioral reference, not an automatically established
refinement of the C++ evaluator. It selects the current one-pole policy whose motion/convergence
contracts are proved separately. It does not itself claim continuity across discrete patch or scene changes;
`PatchLoad.lean` separately covers identical settled patch reloads.

E08 (force-zero no-op), E11 (mandatory full endpoint-refresh countdown), E23
(specific ancestor force counters), E24 (ramp/revision fallback) and E36
(prepared eligibility) are retired as requirements. Their historical lemmas
remain available as descriptions of the old implementation. E03, E50, E54, E55
and E69 no longer require a particular force duration or recursive evaluation
path. Other implementation-model lemmas are supporting evidence, not a mandate
to preserve that model when changing the implementation. E71–E76 add motion,
freshness, publication and settled-refresh guarantees. The accepted gesture
capture counterexample E70 remains recorded.

## What the proofs establish

The [EARS manifest](requirements.json) maps each requirement to actual theorem
names and source anchors. It covers target isolation, recording placement,
read-only computation, normalized state, finite ramp completion, scene blending,
signed modulation bounds, gesture endpoints, amplitude/gesture invalidation,
snapshot synchronization, host gating, current-input evaluation at host cadence,
output history, publication, evaluation-order equivalence, Shift-turn arithmetic
and recursive traversal, gesture edit allocation, depth-curve properties, reset-to-default, gesture activation/capture and selective deletion. E70 is a proved counterexample to unconditional activation convergence, not a desired-behavior guarantee.

Veil checks every initializer and action against each invariant and exports the
proof terms with `#gen_theorems`. `reachable_safe` also proves runtime safety by
induction on Veil's own reachability relation. The ramp completion theorem is a
bounded progress statement: processing advances for the specified number of
samples, with no intervening edits. It is not an unconditional liveness claim
about a callback that might stop running.

The schedule theorem quantifies over arbitrary graph length and expression
nesting. Each node's references have type `Fin index`, so only previously
computed nodes can be read. The proof preserves each expression's operation
order and supports arbitrary pure unary/binary operations. It does **not**
assume the final outputs are equal. It also does not prove that the C++
`PrepareNode` builder always supplies such a well-formed graph.

## Numeric and implementation boundary

- Runtime numbers are exact rationals. `Curve.ideal` uses Mathlib real numbers
  to prove properties of the intended exponential law. `Curve.approximate`
  separately models nearest-integer range reduction, the actual binary32
  polynomial coefficients, normal exponent scaling and `CopySign` in exact
  arithmetic. The proofs **do not** establish IEEE-754 rounding, polynomial
  error bounds, approximation monotonicity, exact unit endpoint magnitude,
  NaNs/infinities, denormals or SIMD instruction selection. The two curves are
  not equated. Schedule equivalence allows any pure curve; signed mixing stays
  bounded for any finite effective depths if base/source values are normalized.
- Target-setting safety requires a normalized target and initialization requires
  a normalized default. C++ `SetTarget` does not clamp arbitrary input; this is
  a caller/input-validation precondition, not a proved JSON-parser property.
- Host clocks/counters are unbounded naturals; overflow is excluded. Valid DSP
  reads have positive oversampling ratio and `slot < ratio`. Four lanes are
  modeled; a trio uses its first three lanes. Cache functions are total in Lean,
  but out-of-range slots have no C++ correspondence.
- The control model assumes correct active dependency sets, complete ancestor
  paths, immutable snapshots of the last evaluation, and no concurrent mutation
  during an action. These are obligations on the C++ integration. In particular,
  source references do not prove the lifetime of pointers or that each snapshot
  captures exactly the intended input version.
- Initialization, edits, compute and publication are modeled separately from
  allocator ownership, UI atomic synchronization, MIDI hardware mapping,
  recording serialization/filtering, and JSON/schema migration. Persisted
  target selection and decoded recursive loading are modeled; byte-for-byte
  serialization and JSON parsing are not.
- The prepared protocol abstracts away skipped evaluations that would produce
  the same result. It does not estimate runtime or prove audio deadlines.

This is a formal core specification alongside the broader
[encoder EARS spec](../../openspec/specs/encoder-parameter-system/spec.md) and
[scene-state spec](../../openspec/specs/scene-state-management/spec.md).
Their catalogue/layout, discrete state registry, UI presentation, physical storage
reclamation and complete input routing are not claimed as proved here. Gesture
activation/deletion now have explicit models; the complete `Increment` routing
composition, allocator behavior and compact active-index arrays remain outside
these proofs. Existing C++ tests remain necessary for those boundaries and for
validating model/implementation correspondence.

## Patch-load coverage

[PatchLoad.lean](PatchLoad.lean) models decoded patch loading as an explicit
recursive transition. Slots retain their serialized indices, including holes;
these are not positions in the compact active lists. The historical
`replaceDepth` transition initializes a replacement at neutral and then writes
saved targets. `old_reload_counterexample` proves the resulting 1 → 0.3
excursion for an endpoint-exact depth curve; `old_polynomial_reload_counterexample`
proves an excursion from above 0.9 to 0.3 using the existing exact-rational model
of the production exponential polynomial. These evaluate before the first ramp
advance. The C++ regression measures the published first-frame excursion
(approximately 1 → 0.30008) and checks every oversample slot thereafter.

The fixed `load` transition reuses matching cells recursively, creates missing
cells at their constructor neutral, and removes absent patch slots.
`reload_preserves_state` proves that loading saved targets preserves the entire
logical tree and every ramp state, except for restarting force-refresh counters.
`reload_preserves_output_trajectory` proves that an identical settled reload
preserves observations at every following host frame under unchanged inputs.
Its evaluator is any pure function of observed current scene values, activation
flags and fixed external inputs; observation equality is proved, not assumed.
The C++ tests additionally cover prepared-to-recursive transitions, nested
modulation and gestures, endpoint/center blends, pending ramps, topology edits,
and all published engine-sample cache entries.

The saved-domain guard has a separate `encoded_self_load` theorem: it does not
require encoding and decoding to be exact inverses. This corresponds to avoiding
a spurious target change when a bipolar float is saved and decoded with rounding.
The recursive model uses normalized exact arithmetic; `normalized_decode_agrees`
connects its target writes to this guard in that domain.

The tree model describes logical slots before garbage collection. Neutral,
childless depths and inactive gestures may be reclaimed; their lack of audio
contribution is covered by the existing reset/evaluation silence lemmas, rather
than claiming pointer identity for reclaimed cells. Constructor neutral values
are determined by slot kind and parent polarity, represented as node metadata.
JSON parsing, binary32 numerical accuracy, active-list ordering, allocator
behavior, output-history storage and C++ refinement remain test/review boundaries.
Changing topology or activation flags in a different patch is not covered by an
identical-patch continuity guarantee. Matching changed targets still ramp from
current values; newly added paths ramp from neutral; removed paths disappear.

Earlier bounds/convergence/publication proofs did not catch this defect because
they omitted the patch replacement transition. Lean checks the stated model;
the source-hash audit detects reviewed C++ drift but does not extract a model
from C++. E77 now records the load contract, its old-loader counterexample, and
the fixed-transition proofs.

## HEAD comparison and coverage

The baseline is the committed `HEAD` selected for this task, not `main` and not
whatever `HEAD` happens to point to later. Verification reads Git objects at the
pinned revision and puts reviewable copies in `build/lean/head-source/`; it does
not check out or build the baseline C++.

Both versions satisfy policy-parameterized leaf contracts. Historical machines
accept only linear timing; current machines accept only `currentTiming`. Recording,
modulation, gesture and interpolation operations remain shared. The audit checks
the remaining unchanged C++ bodies for exact textual equality; the deliberately
changed filter and compute bodies have separately reviewed correspondence. HEAD additionally has its own Veil runtime and eager
root protocol, and its publication loop is modeled independently. We prove
publication equality and combine it with arbitrary-depth schedule equivalence.
That comparison assumes the same valid pure graph, relevant inputs and starting
output/history. It is not a full C++ observational-equivalence proof covering
allocator state or all interleavings.

See the generated table for the machine-audited classification of every entry.
E20 (prepared leaf cache), E25 (deferred synchronization) and E35
(synchronize/rebuild) describe mechanisms absent from HEAD. HEAD instead proves
that its derived tree remains materialized at each modeled boundary. The new
host behavioral contracts have an eager-protocol analogue in HEAD; this is not
a new end-to-end C++ equivalence proof. E76 compares the short and historical
full refresh trajectories under explicit settled-state assumptions.

| Behavior | Coverage and remaining boundary |
| --- | --- |
| Negative modulation | Full and partial inverted-source equations; mixed signed-route output bounds; source-amplitude invalidation |
| Modulation curve | Ideal zero/endpoints/half-depth, monotone magnitude, oddness and bounds; actual-coefficient exact-arithmetic zero, oddness and saturation; floating-point error remains unproved |
| Shift-turn | Dispatch exceptions; sign preservation; inward/outward magnitude; zero crossing and neutral absorption; every existing descendant and missing-path preservation; root target preservation |
| Gesture lifecycle/edits | Capture, scene isolation, flag recording, toggle and selective deletion; changed-target capture convergence; counterexample for unchanged-target capture. Weighted edit algebra is proved, but full multi-gesture edit routing/output bounds remain open |
| Scene edits | Target isolation, blend shares, compensated ordinary edits, independent Shift scene shares; not a verified scene/UI registry |
| Nested modulation | Arbitrary finite expression graphs and descendant trees; correctness of C++ graph/dependency construction remains a correspondence obligation |
| Reset-to-default | Scoped defaults, recursive neutralization, gesture deactivation, forced evaluation, ramp convergence and publication; semantic reclamation rather than allocator/pointer verification |
| Runtime | Both Veil runtimes have reachable-state safety; protocol invariants are proved for every action; no concurrency or deadline claim |
| Persistence/UI/hardware | Target selection for saves/copies only; not JSON bytes, patch migration, MIDI routing, catalogue layout or pointer lifetime |

## Reset-to-default proofs

E48–E57 add explicit `reset` actions to **both** generated Veil runtimes and both
evaluation protocols. The numeric reset uses the same ascending eight-scene
`SetAndRecordValue` loop as `SetValue`; it preserves current values immediately,
changes selected targets, and forces the full update countdown. A shared
`resetTree` applies the configured default to the root, normalized neutral `1/2`
to every allocated modulator descendant, and scoped neutral targets/deactivation
to each node's linked gestures. Existing cells are modeled independently of the
affecting-source mask.

The stronger `Reset.Ready` invariant proves that zero remaining ramp means
current values equal targets, and age zero implies a live ramp. Both runtimes
establish it initially and preserve it through all their actions, including
reset. This makes `reset_finishes` cover settled cells, unchanged default targets,
and resets midway through an existing ramp. After at most `timing.maximum`
advancing host steps without intervening target edits, the reset values reach
their targets. The historical bound is **2400 host samples**; the current conservative bound is
**15001 host samples**, with exact early settlement permitted.

`reset_tree_returns_default` proves the combined result by induction on an
arbitrary finite nested tree, using the actual signed mixing and gesture
formulas. It assumes coherent base/modulator cells, normalized defaults, a fixed
valid reset scene blend, and processing of the required ramps. It imposes **no
coherence assumption on stored gesture values**: scoped deactivation makes their
effective weights zero. Sources and amplitudes may have arbitrary finite values
at evaluation. The implemented polynomial model's proved zero-at-zero property
is sufficient; ideal exponential accuracy is not needed.

`both_publish_reset_default` then proves the external default, including bipolar
conversion, at the final valid oversample slot in both publication models. Earlier
slots in that host frame may still interpolate from the previous output. Reset
itself does not overwrite output history or pretend that ramping has completed.
The protocol proofs show that the next host pass uses the post-reset live
snapshot even when inputs are unchanged. The older prepared model chooses its
recursive path, but that choice is not a behavioral requirement. The protocol's
snapshot argument is the concrete reset result
at this boundary, not an independent proof of the target-write formula.

Full-owner reset covers named roots even when hidden. Bank reset covers only
entries placed in that bank, and leaves other entries unchanged. As elsewhere,
correct construction of the C++ ownership/placement registry is a correspondence
obligation, not a property of a modeled allocator.

The logical tree can retain inactive/neutral nodes that C++ reclaims. E57 proves
that an everywhere-inactive gesture and a settled neutral child without surviving
children/gestures contribute zero, and that erasing those zero contributions
preserves ordered evaluation. Coherence rules out a hidden pending target on a
settled collected modulator. These are semantic removal proofs; the exact
in-place compaction algorithm, pointer lifetimes, and retained bytes of a deleted
inactive gesture are outside the model. Scene preservation concerns logical
scoped writes and surviving/audible state, not a promise to retain garbage.

## Code references and maintenance

Every modeled component group carries an `@cpp key:` comment; shared and
baseline operations also carry `@head key:` references.
[traceability.json](traceability.json) resolves the key to unique C++ symbols
and records SHA-256 hashes of the reviewed source files. The source audit rejects
missing/ambiguous symbols, unknown model anchors, and changed source files.
Whole-file hashes deliberately make unrelated changes require review too.
They are a drift alarm, not a semantic extraction or equivalence proof.

The encoder layout and depth-batching optimization retains these definitions and
contracts. `EncoderMatrix::UpdateDepths` groups four independent simple-route
positions into the existing lane-wise depth curve after a bank-wide blend-change
notification; scalar scene blending, route accumulation order, graph evaluation,
fallback guards, synchronization and publication equations are unchanged. The slot-index
arrays use signed bytes, with compile-time bounds for both catalogues and the
existing `-1` sentinel. Member placement and cell alignment change storage only.
The source audit still checks the unchanged shared C++ bodies against the pinned
revision. The context constructor is no longer in the literal-body equality list:
its initializer expressions and body are unchanged, but the initializer list is
reordered to match member placement. E01, E58 and E63 record that correspondence
review. Differential tests cover batch sizes and tails, alongside the existing
edit, amplitude, gesture, scene, reset and output-history cases. This review does
not extend the model's floating-point or C++ verification claims.

Publication also captures the output-buffer pointers once per host frame, and
the root's output index and polarity before writing cache slots. The bank
allocates these buffers at construction; processing does not resize them. Each
slot retains the same timestamp guard, interpolation expression and bipolar
mapping, including ratio-one processing. This changes memory accesses only;
the publication model and its contracts are unchanged.

Manual stored scene controls now use a 20 ms time-constant one-pole at 48 kHz.
The coefficient is the exact binary64 value of `-expm1(-1/(.020*48000))`;
Lean treats that coefficient and all subsequent arithmetic as exact rationals.
The filter snaps when the next residual is at most `1e-6` normalized. This is
20 ms to 63.2%, about 92 ms to 99%, and about 277 ms for a full-scale move to
reach the snap threshold. This is not a fixed 20 ms arrival deadline.

`OnePole.run_lands` proves exact completion by 15001 frames using a conservative
rational bound. `Settling.trajectory` matches the budgeted runtime to literal
pole steps before the final budget step; `budget_landing_matches_pole` proves
the final forced target agrees with the pole's own convergence. The budget is
ghost scheduling state for shared proofs, not required CPU work: `tick_matches_pole`
relates moving flags to pole values, and `early_retirement_preserves_values`
justifies skipping any remaining budget after exact settlement. Retargeting
preserves current values; the pole coefficient does not depend on edit cadence.

Native code keeps double state only for moving scenes, publishes float values,
and clears each moving bit at exact settlement. A subtree retires after its
children and its own scenes settle, publishing the last value before rebuilding
its dependency mask. Settled endpoint changes require one refresh. Native tests
must cover IEEE rounding, final publication, nested propagation, patch/capture
state, and cached execution: these integration details are not a C++ refinement
proof. Source modulation, live gesture weights, scene blending, modulation sums
and output-history interpolation are not fed through this pole.

Publication remains eager. `GetValue` still reads the published cache, and no
work is transferred into production `SetEncoderParameters`. The maintained
consumer benchmark calls that function at every engine sample as an additional
performance guard. Neither the Lean models nor a source hash prove execution cost.

After changing covered C++, review the affected definitions, requirements and
proofs before updating the recorded hashes. Do not refresh fingerprints merely
to make verification green. Add a contract to `requirements.json` when adding
a behavioral promise; its theorem must exist in the imported Lean environment.

All Veil modules explicitly disable trusted SMT results and use Lean's `grind`
solver. The audit checks all declarations in `Encoder`, `EncoderRuntime`, and
`EncoderPrepared`, `EncoderBehavior`, `EncoderHeadRuntime` and `EncoderHeadProtocol`, including
generated Veil verification conditions and both manifests' named theorems. Only
`propext`, `Classical.choice`, and `Quot.sound` are permitted transitively;
admitted proofs, custom axioms and native-evaluation trust axioms fail the audit.

Veil is pinned to
[`e737e7b8fb4b61bdee837800ab6b97ee894e28c3`](https://github.com/verse-lab/veil/tree/e737e7b8fb4b61bdee837800ab6b97ee894e28c3),
the upstream Veil 2.0 prerelease. All transitive revisions are locked by Lake.

Mathlib is pinned to `v4.32.0` (manifest revision
`81a5d257c8e410db227a6665ed08f64fea08e997`). It supplies real exponentiation
and proof-producing arithmetic tactics; it does not add an encoder-specific axiom.

## Gesture lifecycle and deletion proofs

E58–E70 model the logical optional gesture slots and their operations in both
implementations. The reviewed C++ bodies are identical at pinned HEAD. Separate
Veil machines also check initialization and every lifecycle action while parent
edits/processing interleave with gesture allocation, activation, toggle, clearing
and processing. Parent cells retain `Reset.Ready`; gesture cells retain `Valid`.
An unallocated slot has only an untouched ghost payload. The generated lifecycle
proofs establish inductive initialization/preservation obligations, while the
older `reachable_ready` theorems apply to the separate leaf runtimes.

Scene operations reproduce the code's **right-then-left** visit order. There is
no deduplication: an interior blend with both decks selecting the same scene
visits it twice. Enabling captures it once and requests two activation records;
raw `ToggleActive` flips it twice, leaving it unchanged. The raw toggle helper
has no capture or recording effect, and this model does not assert that a
hardware handler currently calls it. Activation recording counts represent
requests to the logger, not emitted serialized events.

An inactive scene captures the parent's current value and target separately.
Already-active scenes keep their cell, and inactive/unvisited scenes retain
their previous current values, targets and flags. If all contributing scenes
were inactive, the newly captured gesture's immediate banked value equals the
parent's banked value. This does **not** claim continuity when adding a weight
among arbitrary existing gestures, nor that capture follows the parent's
modulated output. A changed capture target starts/continues a valid ramp and
reaches the copied target within the maximum number of advancing host frames,
provided the gesture was coherent beforehand and there are no further edits.

**E70 records an accepted defect in both versions.** The user has explicitly
accepted it for now; keep the counterexample and leave production behavior
unchanged. Starting with
a settled unipolar gesture at zero, let the parent start ramping from one toward
zero. Activation copies current value one and target zero. Since the gesture's
old target is already zero, `SetTarget` returns without starting a ramp. The
gesture now has current one, target zero and zero remaining ramp samples.
`capture_stalled_counterexample` proves that processing any number of host frames
leaves that current value at one. The example uses the normal 96/2400-sample timing.
`clamped_edit_does_not_repair_capture` proves that a subsequent downward edit
clamped to the same zero target also fails to re-arm the ramp. A later changed
target can alter this state. This is a kernel-checked model counterexample with
reviewed source correspondence, not an end-to-end C++ or audible reproduction.
Gesture `Ready` is not silently assumed.

E60 also provides `capture_finishes` and the actual activation wrapper
`activating_finishes` with one extra condition, `CaptureCanSettle`:

> If the gesture target already equals the parent target and no gesture ramp is
> running, the parent current value must already equal that target.

This permits changed targets, an existing ramp, or an already-settled capture.
`capture_finishes_iff` proves that the condition is **necessary and sufficient**
under the existing normalized-parent/coherent-gesture assumptions, so it excludes
exactly the failing case for this per-scene capture. Completion still assumes
advancing processing for the maximum host-frame bound with no intervening edits.
`accepted_counterexample_excluded` proves the retained counterexample violates
this condition. These shared operations and proofs apply to both implementations.

Recursive `ClearGesture(id)` clears that ID at every allocated modulator node,
retaining the other IDs, base/route state and topology. Selected-gesture
Shift-press clears only local linked gestures; descendant gestures remain intact.
The no-selection Shift-press branch is the existing reset-to-default operation.
The broader encoder and scene-state specifications describe that same
configured-default reset; this branch does not preserve the previous base target.
Owner-level deletion reaches named roots regardless of visibility, whereas bank
local deletion touches only placed entries. Placement/ownership construction,
allocation/index-array maintenance and subsequent garbage collection remain
integration boundaries.

The deletion protocols prove a corrective host evaluation even when deletion
empties the dependency mask and all inputs remain unchanged. The older prepared
model uses recursive fallback; HEAD eagerly materializes the then-live snapshot.
The observable requirement does not prescribe the evaluation path.
As with reset, the snapshot is supplied by the concrete modeled deletion at the
protocol boundary: these are not proofs of the C++ dependency-mask builder.

Remaining priorities include full multi-gesture bounds/edit response, complete
`IncrementInternal` routing/clamp composition, recursive scene copy/patch loading,
actual prepared-graph and dependency-mask construction, recording payload/replay,
and hardware/UI routing. Theorems do not yet cover all encoder behavior.

The source audit also reviewed the earlier UI-only additions in `9d6a7fb`: visible
encoder revision/spread updates and bound matrix readers. They observe existing
outputs and do not change leaf control arithmetic or publication. Their previously
stale file fingerprints are updated with this review.
