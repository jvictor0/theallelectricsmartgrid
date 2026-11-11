## 1. Shared history infrastructure

- [x] 1.1 Add atomic inverse storage and a bounded capacity-independent UI read view, with cutoff and wrap tests.
- [x] 1.2 Implement generic warped start/end undo and coherent marker publication; make level-zero scope storage atomic.
- [x] 1.3 Compose ScopeReader axis/value queries through the inverse while preserving unbound readers and existing LOD behavior.

## 2. Consumer integration

- [x] 2.1 Wire voice/quad scopes and modulation overlays to the shared coordinate, preserving transport and backward-trigger rules.
- [x] 2.2 Replace delay positional/min-max envelope storage with a rectified write scope and common dB visualizer.

## 3. Validation and documentation

- [x] 3.1 Add regression coverage for reversals, paired ends, retriggers, fractional markers, restart, overlays, and delay magnitude display.
- [x] 3.2 Run core tests, JUCE visualizer tests, and delay performance smoke checks; fix regressions without weakening assertions.
- [x] 3.3 Update current documentation/specs and validate the OpenSpec change.

## 4. Continuous phase model and implementation

- [x] 4.1 Define the exact-arithmetic phase clock, latest-forward inverse, periodic axis, read heads, and slew contracts in Lean.
- [x] 4.2 Prove the contracts and Veil transition invariants, including transport rebasing and tempo-independent history; audit axioms.
- [x] 4.3 Implement the continuous phase clock and inverse binding, and move delay read heads to phase coordinates.
- [x] 4.4 Publish explicit periodic axes for LFOs and delay; retain event markers for envelopes; use the phase increment for LFO slew.
- [x] 4.5 Associate Veil transitions and mathematical contracts with reviewed C++ source anchors and drift checks.
- [x] 4.6 Test tempo changes, transport transitions, rewind, startup axes, overlays, and phase-dependent slew; run Lean, core, visualizer, and delay smoke checks.
- [x] 4.7 Update the design, specs, and proof boundary documentation to describe the implemented phase model.

## 5. Cursor and time-warp visualizer follow-up

- [x] 5.1 Restore cursors and source-duration comparison using mapped real sample spans.
- [x] 5.2 Move the time-warp view to an explicit unmodulated phase axis, record displacement, and split wrapped paths.
- [x] 5.3 Validate native reconstruction, tempo/reversal/transport, cursor pixels and path breaks; update the Lean correspondence and documentation.

## 6. Compact branch-aware inverse

- [x] 6.1 Model branch boundaries, the forward frontier, relative storage, and inverse error in Lean; state band-limit and capacity hypotheses separately.
- [x] 6.2 Implement the reviewed compact inverse transitions and safe concurrent reads, preserving the agreed memory budget.
- [x] 6.3 Add branch/frontier/relative-storage regressions and rerun the full density correctness sweep, including unsupported inputs.
- [x] 6.4 Update source correspondence, proof audits, documentation and OpenSpec; run the relevant native suites without deployment.
- [ ] 6.5 Rerun the full density performance sweep when an uncontended timing run is authorized. Stopped at the user's request because another agent is running; all partial timings are invalid for comparison.
- [x] 6.6 Formalize stationary-partial paired-analysis invariance, conditional phase-window feature stability, and a phase-confinement criterion for possible boundary compaction; leave runtime storage unchanged.

## 7. Fixed-cell island reassignment

- [x] 7.1 Prove fallback/replacement selection, causal phase-error bounds, exact preimage existence, constant capacity, and Veil reassignment before changing native storage.
- [x] 7.2 Implement forward-only fixed-cell storage and regression coverage for reassignment and partial startup.
- [x] 7.3 Rerun correctness comparisons and relevant native/proof suites; timing remains deferred under 6.5.
- [x] 7.4 Review source correspondence and update contracts, documentation, and OpenSpec.

## 8. Coalesce within-cell turns

- [x] 8.1 Prove the minimum-cut restart and confined-visit composition before native changes.
- [x] 8.2 Implement minimum-cut coalescing, including boundary touches and separate visits, without extra storage.
- [x] 8.3 Rerun regressions, full density correctness, source/proof audits and documentation checks. Timing remains deferred under 6.5.

## 9. Publication and scope cleanup

- [x] 9.1 Review the unmodulated inverse and retain only necessary cycle-crossing consumers.
- [x] 9.2 Compute LFO common periods at UI-state publication, outside sample processing.
- [x] 9.3 Simplify writer-owned marker state and remove unused scope reader APIs and values.
- [x] 9.4 Validate native and visualizer behavior, review proof correspondence, and update documentation. Timing remains deferred under 6.5.

## 10. Scope read concurrency

- [x] 10.1 Prove coherent scalar reads, publication ordering, and retry progress with explicit scheduling assumptions before changing native reads.
- [x] 10.2 Replace bounded inverse retries with a simple busy-wait; bracket bounded cell/frontier updates rather than the entire scatter loop. Preserve missing history as gaps and use one inverse lookup per plotted point.
- [x] 10.3 Add controlled overlap and rendering regressions, run native and proof checks, and refresh source correspondence and documentation. Timing remains deferred under 6.5; no deployment.

## 11. Nominal periodic indicator visibility

- [x] 11.1 Use the published period and nominal tempo in periodic visualizers, retaining historical spans for LOD/source selection and event-scope indicators.
- [x] 11.2 Verify indicator pixels across independent nominal tempos, periods, and inverse-history spans; update documentation and source correspondence.
- [x] 11.3 Redeploy the validated change to the paired iPad.

## Initial implementation validation

- `make core-test`: 851 main cases and 36 alternate-rate cases passed; four existing main cases skipped.
- Final test-only addition: rebuilt and ran 38 focused warped-scope, scope, overlay, and delay-head cases; all passed.
- `make visualizers-test`: passed, including magnitude-to-dB rendering.
- Delay performance smoke: all four cases completed with finite output; this is a smoke check, not a timing comparison.
- `openspec validate warped-scope-history --type change --strict` and `git diff --check`: passed.
- Logs retained under `build/results/time-warp-review/warped-scope-*`.

## Continuous phase validation

- `make lean-test`: both source audits and proof builds passed. PhaseTime has 17 contracts and 312 checked theorems, including generated Veil obligations and reachable published-read safety; only standard Lean axioms. Encoder proofs also passed.
- `make core-test`: 866 main cases and 36 alternate-rate cases passed; four existing main cases skipped.
- Focused phase, delay, scope, overlay and slew run: 58 cases passed, including uniform-versus-warped LFO reconstruction.
- `make visualizers-test`: passed, including symmetric dB waveform rendering using an explicit phase axis.
- All four delay performance smoke cases completed with finite output; no timing comparison is claimed.
- Both source correspondence checks, strict OpenSpec validation, and `git diff --check` passed.
- Proof boundary and operation/source mapping are in `formal/PhaseTime/README.md`; generated contract links are in `build/lean/phase-contracts.md`.
- Logs are preserved under `build/results/time-warp-review/phase-*`.

## Cursor and time-warp visualizer validation

- `make core-test`: passed, including unwarped reconstruction through tempo changes and modulated reversal, transport continuity, and explicit fractional positions without start markers.
- `make visualizers-test`: all 78 cases passed, including voice/quad cursor visibility at slow and fast rates and disconnected forward/reverse time-warp wraps.
- `make lean-test`: source correspondence and both proof audits passed; PhaseTime now has 18 contracts and 313 theorems, including displacement reconstruction through the unwarped inverse.
- Strict OpenSpec validation and `git diff --check`: passed. No deployment performed.
- Logs: `build/results/time-warp-review/phase-cursors-*`.

## Compact inverse validation

- `make lean-test`: source audits, both proof builds and both axiom audits passed. PhaseTime has 25 contracts and 381 theorems, including repeated cell-history construction, the sampled inverse phase bound and a continuous band-limited chord bound.
- `make core-test`: 875 main cases and 36 alternate-rate cases passed; four existing main cases skipped.
- `make visualizers-test`: all 78 cases passed.
- Independent oracle/production-trajectory self-test passed. The 120-case correctness sweep completed; this is not a universal correctness signoff.
- Main G payload is 32 MiB; separate unmodulated H is 16 MiB because external MIDI correction can reverse it. Boundary keys remain double; their two time values are relative floats. Main payload arithmetic is checked both statically and in Lean.
- Maximum measured sampled-phase residual was 0.890205 cells, with no measured one-cell violations. Major prior phase-grid branch errors improved, but slow flat branches still show timestamp regressions relative to the old dense grid, including a degree-three Fourier example. Three unfiltered off-grid cases exceeded the 128-boundary per-cell cap and produced unavailable reads. These limitations remain explicit; band limitation does not imply conditioning or local capacity.
- The performance run was interrupted as instructed when the user reported concurrent agent work. All partial timing results are marked invalid; no performance conclusion is drawn.
- Strict OpenSpec validation and `git diff --check` passed. No deployment performed.
- Report, raw correctness data, source snapshots, oracle witnesses and timing invalidation: `build/results/time-warp-review/compact-inverse/`. Native/proof logs: `build/results/time-warp-review/compact-*.log`.

## Fixed-cell validation

- Lean model and Veil reassignment proved before native changes; `make lean-test` subsequently passed both source audits and both proof/axiom audits (27 PhaseTime contracts, 471 theorems).
- `make core-test`: 877 main and 36 alternate-rate cases passed; four existing skips. Strengthened randomized availability assertions then passed in all 10 focused branch cases after an incremental rebuild.
- `make visualizers-test`: all 78 cases passed. Strict OpenSpec validation and diff whitespace checks passed.
- All 120 density correctness cases completed: no raw missing/spurious inverse reads, no measured noncausal returns, and maximum sampled-phase residual 0.890204318 cells. Pooled raw/settled missing counts of 4332/1794 become zero. Four existing analysis-window availability disagreements remain after SafeStart; raw G is available there.
- Fixed fallback/replacement storage remains 32 MiB for G and 16 MiB for H with no reversal-count capacity limit. Latest-forward timestamp agreement is intentionally relaxed. A checked slow-reversal witness selects a valid fallback 51.62 seconds older, with a paired-window phase difference up to 0.02212 master cycles; no inaudibility claim follows.
- Results and source snapshots: `build/results/time-warp-review/fixed-inverse/`. Timing remains deferred under 6.5. No deployment.

## Minimum-cut coalescing validation

- The restart/minimum and confined-interval composition proofs passed before native edits. Final `make lean-test` passed: 28 contracts, 483 PhaseTime theorems, both source and axiom audits.
- `make core-test`: 880 main and 36 alternate-rate cases passed, four existing skips. All 13 focused inverse cases and 78 visualizer cases passed.
- All 120 density correctness cases completed with no raw/settled inverse gaps or spurious mappings, no measured noncausal returns, and maximum sampled-phase residual 0.890204318 cells. The four existing real-time SafeStart boundary disagreements remain.
- The discussed slow-reversal witness stays on the recent visit: 4.677 ms from the latest-forward timestamp and 0.7494 cells maximum paired-window phase difference. Separate visits still permit older branch selection; these diagnostics are not a general audibility result.
- Storage remains 32 MiB for G and 16 MiB for H. No timing benchmarks, band-limit filter, or deployment. Results: `build/results/time-warp-review/coalesced-inverse/`.

## Publication and scope cleanup validation

- Retained H for the time-warp view's unmodulated phase-to-history mapping through tempo edits and MIDI corrections; its 16 MiB storage is unchanged.
- Moved common-period GCDs and periodic-axis updates to UI-state publication. Removed unused LFO/global top events, Theory of Time fractional crossing offsets, and the resulting dead SampleTop helpers.
- Writer markers now share one plain state layout and use one count; open-start status is derived. Removed unused reader cycle arguments, cached endpoint values, holder conveniences and unused overloads.
- `make core-test`: 882 main cases and 36 alternate-rate cases passed; four existing skips. The focused publication, rewind and scope run passed all 31 cases.
- `make visualizers-test`: all 78 cases passed. `make lean-test`: both source audits and both proof/axiom audits passed (28 PhaseTime contracts, 483 theorems). Reviewed correspondence now covers the period-publication callers.
- Strict OpenSpec validation and diff whitespace checks passed. No timing benchmarks or deployment. Logs: `build/results/time-warp-review/cleanup-*`.

## Scope read concurrency validation

- Added and checked `Concurrency.lean` before changing native reads. It proves scalar snapshot coherence, publication ordering (including an earlier captured end), and retry completion given an eventual quiet attempt. Native 64-bit revisions must not wrap during one attempt; scheduler timing and C++ compilation remain explicit boundaries.
- Inverse readers now busy-wait on an odd/changed counter. The writer commits bounded branch/seed, crossed-cell/frontier, and final-frontier transitions; the scatter loop and interpolation arithmetic are outside these sections. G/H payloads remain 32/16 MiB. Scope metadata keeps its separate bounded capture and previous-frame fallback.
- Cutoffs now apply to the interpolated timestamp. Missing history propagates as gaps through scope, weighted-modulator, delay and warp renderers. Warp points use one inverse lookup; the cursor reuses its sampled value.
- Controlled overlap waits for writer completion without producing zero; a concurrent forward scatter retains all old queried points. New pixel tests cover LFO, envelope, delay and warp gaps. Updated prior expectations that rejected a readable interpolated time or silently omitted a missing weighted source; a real zero-valued source still produces the expected curve.
- `make core-test`: 886 main cases and 36 alternate-rate cases passed; four existing main skips. `make visualizers-test`: all 80 cases passed. `make lean-test`: both source and axiom audits passed, with 31 PhaseTime contracts and 520 theorems. Strict OpenSpec validation and diff whitespace checks passed.
- No timing benchmarks or deployment. Final logs are `build/results/time-warp-review/ui-concurrency-core-final.log`, `ui-concurrency-visualizers-verified.log`, and `ui-concurrency-lean-complete.log`, with full native test logs alongside them.

## Nominal periodic indicator validation

- Periodic voice/quad LFO and time-warp indicators use `axisPeriod / tempoHz > 0.1` entirely in the visualizers. Existing published values supply both inputs; DSP, inverse storage, publication, LODs and source-span selection are unchanged.
- Pixel tests vary nominal tempos (1, 5 and 20 Hz), displayed periods (1 and 0.5 master cycles), and inverse-history durations independently. The exact 0.1-second threshold remains hidden. Time-warp cursors also hide for a fast nominal tempo despite longer recorded history.
- All 80 visualizer cases passed. `make lean-test` passed both source and axiom audits (31 PhaseTime contracts, 520 theorems); the cursor-policy edit leaves the mathematical composition unchanged. Strict OpenSpec validation and diff checks passed.
- Release iOS build succeeded, and the upgrade was installed and launched over Wi-Fi on the paired iPad. Logs: `build/results/time-warp-review/nominal-cursor-visualizers.log`, `nominal-cursor-lean.log`, and `nominal-cursor-ipad-deploy.log`.
