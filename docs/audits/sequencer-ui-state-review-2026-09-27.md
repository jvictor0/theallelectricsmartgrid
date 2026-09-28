# Sequencer UI state review — 27 September 2026

Reviewed the worktree changes against `9800575`, including the new timebase and
Nonagon UI-state headers. The initial review changed documentation and tests only;
production fixes were subsequently authorized and are recorded below. The
findings describe the original defects. Inferred reset ancestry is intentional:
explicit ratio links `1 → 2`, `1 → 3`, and `2 → 6` imply `3 → 6` because the
periods divide. The live and snapshot arithmetic pass that case.

## Findings

1. **Negative cache windows can exclude their seed.**
   `TheNonagonUIState::Sequence::GetDesiredPositionRange` uses C++ remainder to
   compute the containing cycle. For tick -1 and period 1024 it returns
   `[0, 1024)` instead of `[-1024, 0)`. `PreProcess(-1)` seeds -1, then `Process`
   can remove that sole point and read `front()` on an empty deque. Smaller
   periods also select the wrong complete cycles. Use floor-based cycle
   selection. The regression checks signed ranges at all three cache scales.

2. **Touching half-open cache windows are not reseeded.**
   `TheNonagonUIState::PreProcess` tests strict separation. A completed
   `[-32, 64)` cache followed by `PreProcess(96)` requests `[64, 160)`, but keeps
   the old cache. `Process(96, voice)` then trims it to empty and accesses the
   empty deque. Treat equality at either boundary as disjoint. The regression
   requires a new seed before calling the unsafe trim path.

3. **Leading arp rests disagree with forward live playback.**
   `IndexArp::UIState::GetChoiceValue` maps the current coordinates even when the
   rhythm slot is disabled. With rhythm `[false, true]`, range `[0, 1]`, note
   interval 0.25, and page interval 0.125, moving from position 1 to position 2
   holds choice 0 in the live arp but returns 0.875 in the UI. Forward lookup
   needs the previous enabled note and its motive, including across a motive
   boundary. Share that interpretation with the live path.

4. **A disabled arp clock can produce a different choice from its reset state.**
   `TheNonagonUIState::GetVoicePoint` sends clock position zero through the
   rhythm-aware UI lookup even when clock selection is -1. If slot zero is a
   rest and the note interval is 0.25, the fixture's live reset choice is 0 but
   the preview returns 0.75. Disabled-clock evaluation needs the live reset
   note/motive choice independently of the rhythm at position zero.

5. **The reset picker hides inferred ancestors.**
   `TheoryOfTimeRhythmResetCell::IsEnabled` still calls the explicit-parent-tree
   `IsAncestorOf`. In the `1 → 2`, `1 → 3`, `2 → 6` example, the pad selecting
   ratio 3 as ratio 6's reset is dark and pressing it leaves the reset at -1,
   although live and snapshot queries accept it. The picker must use accepted
   period divisibility, retaining its separate self-selection restriction.

## Coverage and validation

Added 17 `SequencerUI:` cases across `harmonic_sheaf_ui_state.cpp`,
`index_arp_ui_state.cpp`, `nonagon_ui_state.cpp`, `theory_of_time_ui_state.cpp`,
and `theory_of_time_rhythm_ui.cpp`. They cover publication through Nonagon,
snapshot isolation, denominator-only changes, all nine voices, two-stage
harmonic selection, resolved arp zones, signed/wide coordinates, all 16 rhythm
slots, pending versus accepted topology, inferred resets, cache invalidation,
bounded growth, and the five regressions above. Existing reset/reference-model
tests now follow the intended divisibility rule.

The initial build succeeded. The initial focused run had **12 passing and 5 failing cases**
(327 of 338 assertions pass). All five failures correspond to the findings;
production fixes were outside the requested scope.

The initial full suite had **503 passing and 7 failing cases** (90,494,257 of
90,494,270 assertions pass). In addition to the five new regressions, the two
startup-silence checks at `sys_startup_stability.cpp:95` and `:272` fail with
peak 0.000657712 and a frozen clock. The identical failures were previously
reproduced on original source `5ec7071` and are documented in the
[effects review](effects-repairs-2026-09-15.md#test-changes-and-validation).
That initial suite was not green.

```sh
cmake -S private/test -B /tmp/nonagon-uistate-review-build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build /tmp/nonagon-uistate-review-build -j 6
/tmp/nonagon-uistate-review-build/smartgrid_tests --test-case='SequencerUI:*'
ctest --test-dir /tmp/nonagon-uistate-review-build --output-on-failure
```

See [Sequencer UI state](../sequencer-ui-state.md) for ownership, shared math,
cache lifecycle, and the current raw-pitch and modulated-coordinate boundaries.
The active `absolute-time-coordinates` change and the three main sequencer/time
specs now include these contracts and the intentional reset inference.

## Authorized fixes

- Cache cycle starts use `FloorMod`, and touching half-open windows reseed in
  either direction before trimming.
- `TheoryOfTimeBase::GetResetCycleCount` owns the divisibility check and returns
  the reset span, or zero when no valid reset applies. Live time queries,
  snapshot time points, and the reset picker share it. The picker continues to
  exclude self-selection and now permits inferred and equal-period resets.
- `IndexArp::GetForwardCoordinates` uses the existing coordinate decomposition
  to find the previous enabled slot. The time point supplies the reset span so
  the lookup wraps correctly when a reset truncates a motive. The search is
  bounded to at most two rhythm lengths; this covers rests joined across a
  reset. If no enabled slot is reachable, the frozen preview uses reset note
  and motive zero. Live event processing is unchanged.
- A disabled clock uses the reset note and motive through the existing
  choice-value mapping, preserving offsets and the other mapping parameters.

The expanded coverage has 20 `SequencerUI:` cases. It includes signed forward
rests, wholly inactive rhythms, truncated motives across clock resets, both
directions of touching cache windows, a nonzero disabled-clock offset, and
equal-period reset pads. The additional rest cases were observed failing before
their fixes. The combined focused ToT, LameJuis, absolute-time, and UI-state run
passes **63 cases and 5,578 assertions**.

Independent read-only review found no further code issues. Its bounded-lookup
check matched exhaustive search across all rhythm patterns of lengths 1–8,
reset spans 1–64, and every wrapped position: 1,060,800 combinations.

After the fixes, the full suite passes **511 of 513 cases and 90,494,331 of
90,494,333 assertions**. Only the same two startup-silence failures remain;
all 20 UI-state cases pass. The build, strict validation of the active change
and all three affected main specs, and `git diff --check` pass. The five added
UI-state requirements match between main and delta specs. Production changes
are confined to `IndexArp.hpp`, `TheNonagonUIState.hpp`, `TheoryOfTimeBase.hpp`,
`TheoryOfTimeBaseUIState.hpp`, and `TheoryOfTimeSmartGrid.hpp`.

Validation logs: `/tmp/nonagon-uistate-fix-focused.log` and
`/tmp/nonagon-uistate-fix-full.log`.

## Naming audit and checkpoint

A subsequent read-only naming audit found no broken callers, wrong overloads,
swapped loop/reset arguments, voice-indexing errors, or persistence-key changes.
The requested cleanup renames the lane-local chooser argument to `channelIndex`
and `GetNumActiveNotes` to `GetNumEnabledStoredSlots`. The latter still counts
all eight stored rhythm flags, including flags beyond the active length;
its calculation is unchanged. Current spec scenarios now consistently use
clock position, rhythm-slot index, and motive position. OpenSpec rename targets
match the corresponding requirement titles in both the delta and main specs.

The naming cleanup rebuilds successfully and passes the same **63 focused cases
and 5,578 assertions**. Strict spec validation and `git diff --check` pass. The
full suite was not repeated for these identifier and prose changes; its latest
result is the 511/513 run above. Logs:
`/tmp/nonagon-uistate-checkpoint-build.log` and
`/tmp/nonagon-uistate-checkpoint-tests.log`.
