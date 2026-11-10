# Continuous phase, inverse history, and periodic scopes

This is the Lean/Veil model for the delay and scope coordinate change. Run
`make lean-test` to check both this model and the encoder models. Setup uses the
existing `make lean-setup` and `make lean-cache` workflow; all dependencies and
incremental artifacts stay under `build/lean/`.

The checked contract catalogue is [requirements.json](requirements.json).
`make lean-test` generates source links in `build/lean/phase-contracts.md` and
reruns both axiom audits even when compiled proofs are cached. Veil uses
`veil.smt.trust false` with the kernel-checked `grind` backend. The audit rejects
`sorry`, custom axioms, missing contract theorems, and dependencies beyond
`propext`, `Classical.choice`, and `Quot.sound`.

## Coordinates and transitions

Let `p` be the modulated musical master phase, `q` continuous delay phase, `b`
the transport offset, and `t` absolute real engine sample time. All logical
coordinates are in master cycles, independent of the current tempo.

| Transition | Model | C++ correspondence |
| --- | --- | --- |
| Running sample | `q := p + b`, retaining `b` | `PhaseClock::Advance` |
| Restart | `b := q - p`, retaining `q` | `PhaseClock::Resume` |
| Stopped sample | `q := q + nominalIncrement` | `PhaseClock::FreeRun` |
| Forward observation | `F(t) := q`, `G(q) := t` | `TimeWarpInverter::ScatterBufferToInverse` at each crossed grid point |
| Reverse/stationary observation | observe `F`, retain `G` | `TimeWarpInverter::Write` descending/zero-length interval |
| Island reassignment | select a causal historical preimage, retaining F and the clock | `TimeWarpInverter::BeginBranch` |
| Publication | capture real cutoff and cursor | `ScopeWriter::Publish` and `AcceptSamples` |

`TheoryOfTimeBase::Process` chooses one clock transition each engine sample;
`TheoryOfTime::ObservePhase` then records it. The Veil actions are these abstract
operations. `Reachability.lean` explicitly inducts over the generated relational
transition system, including its initializer and every action. It proves clock
coherence and inverse soundness for all reachable states, not only isolated
preservation lemmas. Ghost `F` is proof evidence, not extra runtime storage.

A tempo change changes future phase increments. It does not scale old coordinates
or change the interpretation of `G`. A restart changes only the offset; the next
running sample follows signed musical travel. Genuine warp reversal remains a
reversal. Stopped transport continues at nominal positive phase speed.

## The proved contracts

| Module | Contracts |
| --- | --- |
| [Clock.lean](Clock.lean) | Coherent initialization and transitions; restart continuity; signed travel; nominal stopped travel; constant running offset; tempo changes only travel |
| [History.lean](History.lean) | `F(G(q)) = q` for available entries; forward visits and historical reassignment preserve soundness; rewind retains chosen visits; published cutoff rejects newer replacements; pure signal composition `S(G(q)) = L(q)` given `S = L ∘ F` |
| [Periodic.lean](Periodic.lean) | Cycle bounds; current/previous-cycle selection; preservation of periodic signal phase; read heads stay in one loop strictly behind the writer; restart preserves every fixed read-head configuration |
| [InverseGrid.lean](InverseGrid.lean) | Generic interpolation/frontier/codec bounds and declared spectral range; historical multi-boundary reference lemmas |
| [FixedCell.lean](FixedCell.lean) | Fallback/replacement selection, causal phase bounds, exact preimage existence, island reassignment, confined startup traversal, and constant storage |
| [AnalysisStability.lean](AnalysisStability.lean) | Stationary-partial common-shift invariance, phase-controlled window-feature bounds, and a conditional criterion for coalescing tiny reversals |
| [Concurrency.lean](Concurrency.lean) | Coherent scalar reads under one sequence counter; publication ordering; retry completion under an explicit eventual-quiet-attempt hypothesis |
| [Slew.lean](Slew.lean) | Ideal phase-distance decay is bounded and contractive, holds at zero, depends on absolute travel, and composes under forward constant-input subdivision; forward chain rule |
| [StateMachine.lean](StateMachine.lean), [Reachability.lean](Reachability.lean) | Veil transition obligations and a reachable-state safety theorem |

The periodic axis is explicit:

```
a = origin + floor((now - origin) / period) * period
candidate = a + x * period                  (0 <= x < 1)
q = candidate if candidate <= now else candidate - period
value = S(G(q))
```

Thus `now - period < q <= now`. The first partial cycle has exactly the same
width as a later one; missing history stays unavailable. The delay uses origin
zero and period one. An LFO uses the current musical-to-continuous offset and
the common period of its participating loops (`1 / gcd(cycleRatios)`). Fractional
LFO multipliers reshape lobes inside that period and do not shorten it.
`ComputeLogicalPeriods` evaluates this metadata during audio-thread
`PopulateUIState`, before scope publication, using the latest processed weights
and current cycle ratios. Sample processing still calculates the fastest active
ratio for slew; it no longer aggregates unused top events or computes GCDs.
Envelope/event scopes retain their existing reversible start/end markers.
Modulation overlays copy the same periodic axis and query the same inverse.

The writer's marker count is ordinary audio-thread state. It equals the sum of
the former published writer count and pending count at every event transition;
`Publish` copies it to the independent atomic snapshot. Deriving open-start
status from the latest end flag preserves the rewind/reopen behavior without a
cached flag. These are implementation simplifications, not changes to the
mathematical publication or periodic-axis contracts. Theory of Time loop
crossings now store only their boolean result: remaining consumers use no
fractional offset. Oscillator/sample-source scope offsets remain interpolated.

The time-warp visualizer instantiates the same clock/inverse/periodic machinery
on unwarped phase `U`, with inverse `H`. It records displacement
`D(t) = F(t) - (U(t) - origin)`, then draws `q - origin + D(H(q))` modulo its
period. `warp_scope_composition` proves that the unwrapped value is `F(H(q))`
when the musical origin is constant over the queried history. Thus the warp
remains visible. Transport-origin changes have the same boundary as the LFO
composition hypothesis below. The separate inverse holds two master cycles
(16 MiB including branch storage); external MIDI corrections can reverse even
this clock. The plotted period is at most one. Finite retention remains outside
the proof. Path breaks and cursor visibility have native pixel regression tests.

Delay read heads project `writePhase * speed - loopPeriod * bufferFraction`,
then wrap into `[writePhase - loopPeriod, writePhase)`. The general proof allows
a nonnegative phase gap; production uses zero. Complete analysis-window and
interpolation-support safety is enforced by the grain manager **after** mapping
through `G` into real host samples. No tempo-dependent hop length shifts the
phase coordinate. Changing speed/delay/loop settings can still move a read head;
the continuity theorem holds those settings fixed.

## Implementation boundary

These are mathematical proofs with reviewed source correspondence, just like
the encoder model. They are not generated C++, C++ semantics proofs, or machine
code verification. [traceability.json](traceability.json) names unique source
anchors and pins the complete covered files. `check_phase_spec.py` rejects source
drift or unreferenced anchors. Review the mathematical correspondence before
updating hashes; a changed hash alone is not evidence of equivalence.

The clock/history/Veil model retains an ideal pointwise inverse. Its reassignment
transition may select any causal historical preimage without changing F or the
clock. It no longer requires globally latest-forward selection.
[FixedCell.lean](FixedCell.lean) proves that the selected traversal contains an
exact preimage and that its linear timestamp approximation is causal with sampled
phase residual at most one cell. An induction proves these properties survive
repeated certified cell construction and replacement.

Each cell preserves the complete endpoints of its first traversal as a fallback,
plus at most one replacement traversal. A new visit after leaving the cell replaces
that single replacement. Below its cut, old islands revert to the fallback; above it,
reads use the new traversal. No interpolation crosses between their timestamps.
Both fallback endpoints belong to the cell: rewriting the next cell's entry
cannot distort the fallback. Forward entry through the left edge starts a new
fallback. Reverse observations do not rewrite G.

During a replacement visit confined to the closed cell, new forward turns keep
the minimum cut and its original timestamp; equal minima retain that earlier time.
Only a deeper minimum changes the start. The forward frontier still advances.
`restart_certified` joins the prior traversal, reverse bridge, and fresh forward
interval, proving causality and the one-cell phase bound for the coalesced piece.
Boundary touches do not end the visit: the live endpoint reopens after a touch.
Strict forward exit beyond the right edge permits a fresh cut on return; leaving
the left edge causes forward grid entry to reset the cell. Native detection uses
the existing forward frontier, adding no storage. These lifecycle checks remain
reviewed correspondence. The existing Veil historical-reassignment action covers
selection of valid historical preimages for coalesced as well as replaced pieces.

Before the first complete traversal, the entry and forward frontier bound an
interval confined to the cell, possibly containing internal reversals. This is
a phase-error guarantee, not exact latest-branch interpolation. Crossing back
through the left edge retires that entry on the next forward crossing. A single
fractional startup-entry marker handles the lowest partially observed cell.
`confined_join` and `affine_phase_confined` provide the sample-interval construction
lemmas. Certification of the native traversal lifecycle remains reviewed source
correspondence, not a proof of the C++ loop or floating-point execution.

Raw circular storage may retain old entries ahead of the writer; published views
exclude those coordinates and delay heads are constructed strictly behind it.
The model does not require eagerly erasing inaccessible entries.

Production retains four master cycles at 2^18 cells/cycle. The main payload is
32 MiB: three double arrays (fallback left time, fallback right time, and exact
replacement phase) plus two float arrays (replacement start/end time deltas from
the fallback left time). The separate unmodulated H uses 16 MiB. Exact phase keys
avoid rounding a discontinuity onto the wrong side of a query. There is no
boundary pool, per-cell list, allocation, or exhaustion caused by reversal count.
Lookups choose at most two pieces. Write work still scales with crossed grid edges.
The former pooled implementation and its comparison results are retained under
`build/results/time-warp-review/compact-inverse/`; they are not the current algorithm.

The spectral model is an explicitly declared finite sum of phase-domain sines
(with arbitrary offsets), angular frequencies bounded by B, and amplitude budget
A. It proves an upper forward slope bound 1+B*A. The current warp generator is
NOT claimed to satisfy that contract, and no filtering is added in this change.
The sampled phase-error proof uses cell confinement, not a claim that a band
limit separates roots. On an independently identified monotone branch, time error
has a local duration bound; a uniform width/speed bound requires a
separate strictly positive lower speed. The single-sine zero-speed theorem shows
why band limitation does not provide that hypothesis. Lipschitz logical signals
inherit the phase residual bound through S=L∘F.

Float encoding is modeled by explicit endpoint perturbation bounds on the same
selected branch, not by a proof of IEEE arithmetic. The timestamp delta theorem
is independent of absolute uptime. Native tests and the density benchmark compare
actual float storage against a full sampled-trajectory reference. For a continuous Fourier warp W(u)=u+wave(u), the chord theorem bounds the
observation error by (1+B*A)*step/2, where step is the unmodulated-phase distance
between engine observations. Adding this to the grid residual bounds continuous
phase error when u is interpolated within that interval. Neither the proof nor
the measurements establish exact continuous reconstruction or selection of a
hidden continuous branch between engine observations. Finite retention and the
reviewed C++ correspondence remain implementation boundaries. The host audio
ring independently retains 2^24 host samples.

## Concurrent reads

`Concurrency.lean` models atomic scalar accesses in one sequentially consistent
order. A single writer increments a counter before and after changing shared
fields. Even means committed; odd means being changed. Every field read occurs
between two counter loads. An unchanged even counter proves all the loaded fields,
and the computation using them, equal one committed snapshot. Natural-number
revisions assume the native 64-bit counter does not wrap during one read attempt.

`TimeWarpInverter::ReadInverse` implements exactly that loop, busy-waiting on odd
or changed revisions without an attempt limit. The audio thread never waits for
readers and calls its own lookups outside write sections. Each section covers a
bounded branch/seed transition, one crossed-grid boundary, or the final frontier
pair. In a grid crossing, the old cell's endpoint, the new cell's entry and the
frontier commit together. Interpolation arithmetic and the scatter loop lie
outside these sections, so an intermediate committed state represents a completed
prefix of the forward traversal. Storage remains 32 MiB for G and 16 MiB for H.

The progress theorem assumes an eventual attempt fits between writer sections.
It proves no nanosecond bound and cannot guarantee scheduling fairness. A paused
writer can leave a reader waiting. This concurrency model is not a proof of C++
compilation or hardware behavior; sequential consistency and lock-free scalar
atomics are the implementation correspondence.

Publication happens after G and scope samples are written. Acceptance captures
an exclusive sample end; the model preserves `accepted <= published <= completed`.
This orders completed writes without pinning circular slots against later reuse.
Scope metadata retains its separate bounded snapshot attempts and previous-frame
fallback. Its counter covers the metadata copy, not the inverse's short sections.

A coherent inverse lookup can still be absent: history may be unwritten, expired,
ahead of the captured phase, or replaced with a timestamp after the accepted scope
end. Absence is the lookup result, not a failed concurrency attempt, and is not
retried. The real-time cutoff applies to the interpolated timestamp; a newer right
endpoint may locate an older readable timestamp. Renderers preserve absence as a
path gap. They never substitute zero displacement or drop an unavailable weighted
source's contribution. The time-warp renderer uses one inverse lookup per point
and reuses its cursor value. UI views are not frozen inverse snapshots.

The LFO is a pure function for the composition theorem. Its hypothesis is
explicit: the recorded signal must be `S = L ∘ F` over the queried history.
Parameter changes, sample-and-hold state, and transport changes to the LFO's
musical origin can violate that hypothesis. Existing real-time FIR LODs are
retained: the proof does not claim those filters commute with nonlinear warping
or that all LODs reconstruct the identical unsmoothed signal.

The slew model gives the ideal decay `exp(-k * abs(deltaPhase))`. Production uses
`k = 2*pi*8*mult*maxCycleRatio`, a stable approximate coefficient for small steps,
and `expm1` for large steps. The continuous forward chain rule explains tempo
invariance; discrete sampled varying inputs still incur integration error.
Reversal uses absolute travelled distance; it does not undo the filter state.
The warp-producing oscillator uses nominal unmodulated increments to avoid a
self-dependent derivative. Stopped LFO slew uses nominal increments so it can
settle while the musical phase is stationary.

Native regressions exercise phase clocks, transport, all delay speeds, inverse
replacement/cutoffs, periodic startup and rewind, shared overlay coordinates,
phase slew, actual grain playback, and symmetric dB rendering. Those tests
complement the proof boundary rather than extending its claims.

## Timestamp error versus analysis error

The density comparison measures timestamp accuracy, not audibility. Both analysis
windows are shifted by the same inverse error: current audio is captured at
`t + k`, previous audio at `t + k - H`. Their separation remains H. For an ideal
resolved stationary partial with phase `omega*t + initial`, the measured phase
advance is `omega*H`, independent of t. The Lean model proves that a common shift
preserves this advance, the wrapped bin-frequency estimate, relative phases of
bins sharing that carrier, and a non-triggered leader update from equal prior
synthesis state. It permits any deterministic wrap function; invariance is not
a claim that the frequency estimator is free of aliasing.

For varying features the useful hypothesis concerns the full analysis support:
`epsilon = sup_r |F(t+r) - F(u+r)|` for offsets covering `[-H, N-1]`, rather than
only `|F(t)-F(u)|`. Given a logical feature A with Lipschitz gain K, a nonnegative
normalized weighted average of `A(F(t+r))` changes by at most `K*epsilon`.
This covers ideal averaged envelope or frequency controls. The model does not
identify a real FFT magnitude with such an average: overlapping partials,
leakage, transients, external input and evolving feedback require separate
analysis. PVDR gating, leader assignments and triggered phase resets are also
outside the stationary-update guarantee. No end-to-end audibility theorem is
claimed for the full delay.

The same module gives a weaker condition under which internal branch records
could be coalesced: if every intervening time stays within a phase band of width
w, interpolating between the outer timestamps retains a phase residual at most
w even without monotonicity. This intentionally relaxes exact latest-forward
branch identity. Merely finding several boundary keys in one cell is not enough:
the trajectory could have left that cell between them. For analysis stability,
the confinement condition must also extend over both shifted analysis windows.
The fixed-cell implementation uses confinement for unfinished first traversals,
and selects a preserved complete fallback when reassigning older islands. It does
not merge arbitrary timestamps across excursions outside the cell.
