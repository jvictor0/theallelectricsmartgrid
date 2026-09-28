# Sequencer UI state

`TheNonagonInternal::UIState` now inherits `TheNonagonUIState`. The audio-side
`TheNonagonSmartGrid::PopulateUIState` publishes the timebase, harmonic engine,
and index-arp inputs during the existing frame update. The new query and cache
helpers let one UI-side consumer evaluate a forward sequence from fixed settings.

## Published data

| Publisher | UI state | Contents |
| --- | --- | --- |
| `TheoryOfTimeBase` | `TheoryOfTimeBaseUIState` | Live absolute modulated phase and tick, six accepted loop periods and six loop rhythms: size, reset selection, all 16 gate slots |
| `HarmonicSheaf::Sheaf` | `HarmonicSheaf::SheafUIState` | All 64 sections, including three high counts and three total counts per section |
| `HarmonicSheaf::Evaluator` | `HarmonicSheaf::EvaluatorUIState` | Three pitch coefficients |
| `LameJuisInternal` | `HarmonicSheaf::UIState` | Shared sheaf and evaluator plus nine chooser configurations: lens, base strategy, main strategy |
| `NonagonIndexArp` | `NonagonIndexArp::UIState` | Clock/reset per trio and resolved per-voice arp settings |

Resolved arp settings include min/max after zone stacking and overlap, offset,
note and page intervals, inversion, retro, cycle, rhythm length, and all eight
rhythm slots. The UI does not need to repeat the voice-zone calculation.

The harmonic publisher repeats each lane's accepted lens and strategies for its
three voices. A lens bit of one means read; zero means co-mute. LameJuis also
publishes `m_currentTimeSlice`, `m_currentPitchSection`, and `m_dimensions` for
live indicators. `TheoryOfTime::UIState` retains `m_timeYModAmount`, and Nonagon
retains its current gate, mute, pitch, and voice-cycle display fields.

`TheoryOfTimeBaseUIState::m_globalPhase` publishes the continuous absolute
modulated phase in global cycles; `m_globalTickPosition` publishes its integer
lattice tick. Both use sample zero alongside the accepted periods and are live
indicators excluded from `Changed()` and `Snapshot()`, so playback does not
invalidate the sequence cache. The Melody slot derives its cache position,
displayed loop, and fractional playhead position from one phase reading and the
accepted global period. It shares the scopes' active-trio and voice selection.

## Ownership and refresh

Each component has atomically published fields and ordinary `m_snapshot` fields
owned by the consumer. `Changed()` compares the two; `Snapshot()` copies the
published fields. Queries use the copied fields until the next snapshot, even
when the producer publishes another configuration. Denominator-only changes
invalidate a harmonic snapshot because they change timbre coefficients.

This is field-by-field atomic publication, not a transaction spanning all fields
or components. The intended use freezes the values observed during a refresh;
it does not reproduce when pending parameter edits become audible. Use one
consumer thread for snapshots and caches. The consumer must wait for a complete
initial publication with positive periods before querying; a fresh timebase UI
state has zero published periods.

The live-indicator accessors `GetSection` and `GetLensForVoice` read the published
atomics directly. `Choose` uses the copied harmonic state. The current sheaf
visualizer uses the former accessors; it does not drive the sequence cache yet.

## Shared evaluation

`GetTimePoint(globalTickPosition, clockLoop, resetLoop)` returns the signed tick,
six rhythm bits, clock loop-cycle position, and `m_resetCycleCount`: the number
of clock cycles per valid reset, or zero for an unbounded clock. It reuses
`TheoryOfTimeBase::GetLoopCyclePosition`, `GetResetCycleCount`, and
`TheoryOfTimeRhythm::GateAt`.
Clock selection -1 returns clock position zero while still evaluating the bits.

Reset ancestry is inferred from period divisibility. For clock period `Lc` and
reset period `Lr`, a reset is valid when `Lr % Lc == 0`, independent of explicit
parent links. Thus ratio links `1 → 2`, `1 → 3`, and `2 → 6` imply `3 → 6`.
Equal-period loops reset each other to zero. Publishing accepted periods is
sufficient for this calculation; the UI does not need the parent tree.

`IndexArp::GetCoordinates` supplies rhythm slot, motive position, and enabled-note
ordinal. `Input::GetChoiceValue` supplies folding, inversion, retro, and range
mapping. Live processing and the UI reuse those functions. The arp's
`GetForwardCoordinates` finds the preceding enabled slot during forward rests,
including across a reset boundary. Its search is bounded to one rhythm length
without a reset, or at most two rhythm lengths when a reset joins partial
motives. No enabled slot reachable under the frozen settings means the reset
note and motive, both zero. Historical choices from before a parameter edit are
not reconstructed. A disabled clock also uses the reset choice, including the
shared offset, inversion, and range mapping.

Live lane selection and `HarmonicSheaf::UIState::Choose` call the same two-stage
`SectionChooser::Choose`. The base strategy's value is added to the choice value
before running the main strategy. `GetVoicePoint` composes the time point, arp
choice, and harmonic selection without advancing audio processing.

`VoicePoint::m_pitch` is the raw LameJuis pitch and section. It precedes unison
routing and octave/spread, and does not represent trigger suppression, mute
decisions, note duration, or trigger-time output latching. The snapshot does not
include the time-warp LFO settings/history; current absolute phase is published
separately as a live indicator. Its caller
supplies positions in the modulated global lattice; a view in unmodulated phase
needs a separate warp mapping.

## Sequence cache

After valid publication, call `PreProcess(position)` once per consumer update,
then `Process(position, voice)` for the voices being prepared. The first call
refreshes changed snapshots and seeds the caches. Each per-voice call adds at
most 128 points at each end. A centered 1024-tick window fills in four calls
at a fixed position; a whole-cycle window can take up to eight. This work runs
on the consumer's thread, outside the audio callback. Sequences contain one point
per integer tick, with half-open bounds; consumers must check availability before
`GetPoint`.

The desired range fits at most 1024 points. It prefers three complete global
cycles centered on the containing cycle, then one complete cycle with spare
capacity on either side, then a centered 1024-tick window for larger periods.
Whole-cycle selection uses mathematical floor for signed positions. A global
cycle is a display window: arp motives and loop rhythms may span several cycles.

Changes to the copied configuration clear all nine caches. Disjoint or touching
windows are reseeded before trimming. Negative cycle starts use floor arithmetic,
and equality at either half-open boundary counts as disjoint. Regression tests
cover negative windows and reseeding from either direction.

## Tests and implementation status

`private/test/unit/*_ui_state.cpp` covers publication, snapshot isolation,
invalidation, shared pitch math, signed coordinates, inferred resets, and cache
boundaries. `theory_of_time_rhythm_ui.cpp` additionally covers the inferred reset
pad and selection of distinct equal-period loops. The picker and time queries
share the core reset-divisibility calculation.
The Melody slot in Panning & Sequencing consumes these helpers through
`SequencerMelodyVisualizerComponent`. Its pitch axis spans the displayed voices'
arp bounds without sheaf-driven padding. In ClosestModOne, candidate pitch
classes repeat in every visible octave. In Percentile, candidates keep their
original register and gain upper-octave copies through the integer part of the
largest displayed arp maximum.

Percentile choices and axis bounds use a per-time-slice pitch mapping: interpolate
between the shared chooser's rank positions `i/N`, preserving duplicate sections,
hold the last rank until the next integer, and add the whole percentile as an
octave shift. The axis covers the mapped ranges over the displayed slices,
including extrema at octave jumps. The dashed curve is the interpolated raw arp
choice; the solid voice line remains the discrete result of the full chooser.
The arp bounds determine the axis range without separate boundary curves. Read
gates use muted gray for high and black for low, with subdued outlines.

The choice curve holds for the first half of each global tick and eases with a
cosine to the following tick's displayed choice over the second half. Curves
connect only within the same forward motive occurrence, following held notes
across rests and distinguishing repeated motives after a reset. Each motive is
dashed separately to preserve the gap. Faint vertical rules mark changes in the
visible read-bit gates; co-muted gates do not add separators, and tightly packed
changes fade out to avoid a dense grid. Horizontal grid lines mark whole octaves
within the displayed pitch range.

Native drawing and range regressions live in `private/test/juce-visualizers`:

```sh
cmake -S private/test/juce-visualizers -B /tmp/smartgrid-visualizer-tests
cmake --build /tmp/smartgrid-visualizer-tests -j 6
ctest --test-dir /tmp/smartgrid-visualizer-tests --output-on-failure
```

See [Nonagon](nonagon.md), [LameJuis](lamejuis.md), and
[Theory of Time](theory-of-time.md) for the live processing contracts.
