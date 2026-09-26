# Theory of Time

The Theory of Time maps an absolute global phase into six related loops. The core is in `private/src/TheoryOfTimeBase.hpp`; `TheoryOfTime.hpp` advances the input clock, applies its phase-modulation LFO, and emits MIDI clock.

## Coordinates and domains

Phase is a `double` measured in absolute cycles. It can be negative or larger than one. There is no winding counter or reconstruction from wrapped samples. The two domains are:

- `PhaseDomain::Unmodulated`: the input clock phase.
- `PhaseDomain::Modulated`: input phase plus the current phase offset.

Loop 5 is the global loop (`x_globalLoop`). Each child selects a higher-index parent and a positive integer parent multiplier. Its **cycle ratio** is the product of those multipliers along the path to the global loop. For either domain:

```
loopPhase = globalPhase * cycleRatio
```

This is a direct function of the current phase and accepted topology, including after reverse motion, seeks, or jumps across many cycles. Periodic outputs reduce phase with `phase - floor(phase)` only where they need a circle coordinate. Sample playback applies speed before this reduction; LFO waveform evaluation reduces its input before applying its periodic shaping multiplier.

## Time-warp modulation controls

The clock's phase-modulation LFO uses a continuous sine-to-triangle Shape blend over the full normalized knob range. Shape zero is sinusoidal, Shape one is triangular, and intermediate values blend them without quantization. Skew maps the full knob range to attack fractions 0.1 through 0.9; its midpoint remains symmetric at 0.5. Existing control filtering, loop blending, output slew and fractional-Mult lobes are retained.

These restrictions apply specifically to the time-warp LFO. Voice and quad LFOs retain their existing Shape and Skew behavior. High Mult can still reverse time; reversals remain an intentional part of the effect.

## Shared integer position

Each sample has one signed `int64_t` position per domain, shared by all loops. The global period in lattice ticks is the LCM of the cycle ratios, with no doubling. A loop's period is the global period divided by its cycle ratio; a period of one tick is valid.

```
globalPeriodTicks = lcm(cycleRatios)
loopPeriodTicks = globalPeriodTicks / cycleRatio
position = floor(globalPhase * globalPeriodTicks)
absoluteStep = floorDiv(modulatedPosition, loopPeriodTicks)
```

One gate step is one complete loop cycle. For global cycle ratios 1, 2, and 3, the global period is 6 ticks and the respective loop periods are 6, 3, and 2 ticks. Positions remain absolute in storage. `PhaseUtils::FloorDiv` and `FloorMod` use Euclidean arithmetic for negative positions.

`GetGateStepIndex(loop, sample, resetLoop)` returns the signed absolute step when reset is -1 or is not an ancestor. For a selected ancestor or the loop itself:

```
stepsPerReset = resetPeriodTicks / loopPeriodTicks
step = floorMod(absoluteStep, stepsPerReset)
```

A self reset therefore always returns zero. At position -1 with loop period 8, the absolute index is -1; with ancestor period 24 the reset-relative index is 2. The index remains signed 64-bit through rhythm lookup and the sequencer's motive calculations, reducing only to a bounded slot or output.

## Loop rhythms and tick events

Every loop has a `TheoryOfTimeRhythm`: up to 16 gate values, an active size, and an optional reset loop. On that loop's modulated cycle crossing, its gate becomes `rhythm.gate[floorMod(step, rhythm.size)]`. The default is size 2 with `[true, false]`: one complete cycle on, then one complete cycle off. This loop rhythm supplies the LameJuis time bit; each voice's index-arp rhythm is a separate pattern.

`CrossedCycleBoundary` reports a change in the floor-divided cycle index in the requested domain, in either direction. `AnyTick(loop)` aggregates the loop's modulated crossings over the microblock. A tick remains an event when neighboring rhythm values are equal, a seek skips several steps, or self reset leaves the selected index at zero. Consumers receive at most one event per sample, without synthesized intermediate steps.

Gate, size, and reset edits become audible only at the edited loop's next modulated tick. Between its ticks the gate holds, including when a faster loop ticks. A rhythm edit alone does not raise `m_anyChange`. Startup explicitly marks every loop as crossed and evaluates all rhythms; stopping forces all gates false.

## Topology edits and processing order

A running topology edit accepts the requested parent and multiplier together only when both the current and requested parents cross a modulated cycle boundary on the same sample. All eligibility checks use the topology before any edits on that sample. Stopped edits and startup accept the requested topology immediately.

For a running sample, processing derives positions, computes crossings, accepts eligible topology edits, remaps positions if the lattice changed, and finally evaluates gates for the loops whose crossing flags are set. Remapping preserves the events that admitted the edit and raises `m_anyChange`; the remap itself is not elapsed travel. Gate lookup uses the accepted topology and its reset ancestry. A stored reset that ceases to be an ancestor is ignored; it becomes effective again when that ancestry returns, at the loop's next tick.

Changing topology recomputes cycle ratios and lattice periods. Absolute child phase can change by whole cycles at an aligned edit. Periodic outputs agree at the mathematical boundary; coordinates remain direct functions of the new topology. Fractional sample queries interpolate global phase first, then apply the topology of their interval. They do not interpolate between differently mapped child coordinates.

Topology acceptance uses modulated boundaries. The phase-modulation LFO uses unmodulated phase, so those boundaries can differ under phase modulation; that existing distinction is preserved.

## Rhythm controller and patch state

Wrld.Bldr's TheoryOfTimeRhythm mode pairs a left rhythm page with a right reset page. Select it with aux pad `(1, 1)` in the normal grid-mode selector view. The left page has six loop columns and eight step rows: press a pad to toggle its gate; Shift-press row `j` to set size `j + 1`. The right page selects an ancestor reset for each loop; pressing the selected ancestor again clears it. Self and non-ancestor pads are disabled. A reset made invalid by reparenting remains stored but its pad is hidden while invalid.

The engine supports 16 rhythm slots. The grid edits slots 0 through 7 and sizes 1 through 8; StateSaver persists the size and reset plus gate slots 0 through 7. Persistence does not clamp an imported size to eight, and engine slots 8 through 15 have no saved gate entries. StateSaver keys are `TheoryOfTimeRhythm` (loop, step), `TheoryOfTimeRhythmSize` (loop), and `TheoryOfTimeRhythmReset` (loop). Loading a patch with missing rhythm keys preserves the current registered values, following the ordinary StateSaver policy; a fresh instance starts with the default rhythm. Controller mode ordinals are runtime state and require no patch migration.

## Microblock snapshots

Each microblock has eight audio samples and one lookahead sample. `RolloverMicroblockBuffer` copies slot 8 to slot 0, then processing fills slots 1 through 8. Every slot contains its phases, positions, accepted loop topology, gates, and events. Queries take an explicit sample index; `GetPhase` also accepts fractional positions through 8 inclusive.

When stopped, all phases and positions are zero and gates and crossing flags are false. Completing the stopped block also clears slot 0 while preserving the stop/topology-change notification. Accepted topology remains available for the next start.

## Consumers

- Playback uses absolute modulated loop phase, applies speed and window length, then wraps into its output window.
- PolyXFader selects an explicit phase domain and evaluates a periodic waveform in double precision before narrowing to float. Voice LFOs use modulated phase; the clock's own modulation LFO uses unmodulated phase.
- AHD captures a source/global cycle ratio and envelope period at trigger and follows only absolute modulated global phase during the note.
- Recording timestamps and outgoing MIDI clock use unmodulated global phase. Recording does not inherit phase-offset excursions.
- Delay heads use absolute sample coordinates, retaining their transport/tempo glue and wrapping only to the buffer's read window. Scopes and UI indicators explicitly reduce their displayed phase.

See [PolyXFader LFOs](polyxfader-lfos.md), [AHD envelopes](ahd-envelopes.md), [Multi-Phasor Gate](multi-phasor-gate.md), and [Controller Integrations](ui-controller-integrations.md).
