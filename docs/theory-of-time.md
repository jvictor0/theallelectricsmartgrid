# Theory of Time

The Theory of Time maps an absolute global phase into six related loops. The core is in `private/src/TheoryOfTimeBase.hpp`; `TheoryOfTime.hpp` advances the input clock, applies its phase-modulation LFO, and emits MIDI clock.

## Tempo and master tuning

The Theory of Time page places Tempo at `(0, 0)` and Master Tune at `(0, 1)`.
Master Tune is a continuous exponential parameter spanning one octave, from
187.5 to 375 Hz: the adjacent suboctaves of the 24 kHz audio Nyquist frequency
that bracket middle C. Its default is middle C (261.6256 Hz). It tunes all nine
voices before Deep Vocoder's pitch selection. With DV bypassed, held notes
follow Master Tune continuously. With DV enabled, tuning moves the selection
center for subsequent notes; the selected external pitch is preserved without
any further master transposition, so it stays in tune with DV's dry signal.
The resulting pitch then feeds portamento and the pitch-tracking source,
filter, and sub oscillator paths.

Tempo spans eight octaves from `Nyquist / 2^21` to `Nyquist / 2^13`
(0.011444091796875–2.9296875 global cycles per second). External clock mode
continues to supply the effective tempo independently of this knob. The first
clock after MIDI Start/Continue resets the phase to the origin even if lingering
voice gates kept the clock moving after Stop.

The tuning visualizer replaces this page's quad analyzer. The outer circle is
one logarithmic octave, with Nyquist and its suboctaves at twelve o'clock and
frequency increasing clockwise. Tempo is yellow, Master Tune is white, and
each unmuted voice appears in its own color using its pitch after DV and portamento.
All three trios are shown regardless of the selected track.

Inside the circle, the upper curved dial centers on tempo and marks simple
fractions of Master Tune alongside the voices. The lower dial centers on
Master Tune and marks simple fractions of tempo, without voice colors. Both
dials span one semitone (−50 to +50 cents). Fractions are reduced ratios with
numerator and denominator at most 16, folded into `[1, 2)`; their octave copies
share a mark, so, for example, `3/2` on the tempo dial lines up with `4/3` on
the tuning dial. Turning either knob scrolls these relations past the centers.
The dials label their fraction marks; frequency values, units, and other text
are omitted. The octave-equivalent 1/1 marks are larger: white for tuning on
the tempo dial, yellow for tempo on the tuning dial.

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

The clock's phase-modulation LFO shares the voice and quad LFO Shape curve: zero (the default) is triangle, 0.5 is sine, and values above 0.5 progressively apply normalized tanh to make the sine square-like. There is no waveform quantization. Skew maps the full knob range to attack fractions 0.1 through 0.9; its midpoint remains symmetric at 0.5. Existing control filtering, loop blending and fractional-Mult lobes are retained. The mixed waveform passes through one low-pass filter at eight times the fastest contributing phasor's expected frequency, as described in [PolyXFader LFOs](polyxfader-lfos.md).

The narrower Skew range applies specifically to the time-warp LFO. Voice and quad LFOs retain their wider Skew range. High Mult can still reverse time; reversals remain an intentional part of the effect.

## Shared integer position

Each sample has one signed `int64_t` position per domain, shared by all loops. The global period in lattice ticks is the LCM of the cycle ratios, with no doubling. A loop's period is the global period divided by its cycle ratio; a period of one tick is valid.

```
globalPeriodTicks = lcm(cycleRatios)
loopPeriodTicks = globalPeriodTicks / cycleRatio
globalTickPosition = floor(globalPhase * globalPeriodTicks)
loopCyclePosition = floorDiv(modulatedGlobalTickPosition, loopPeriodTicks)
```

One gate step is one complete loop cycle. For global cycle ratios 1, 2, and 3, the global period is 6 ticks and the respective loop periods are 6, 3, and 2 ticks. Positions remain absolute in storage. `PhaseUtils::FloorDiv` and `FloorMod` use Euclidean arithmetic for negative positions.

`GetLoopCyclePosition(loop, resetLoop)` infers reset ancestry from the accepted periods. A reset is valid when its period is an integer multiple of the clock period, even if it is on another explicit parent path. Reset -1 or a non-divisible reset leaves the absolute position. For a valid reset:

```
cyclesPerReset = resetPeriodTicks / loopPeriodTicks
loopCyclePosition = floorMod(absoluteLoopCyclePosition, cyclesPerReset)
```

A self reset or a reset to another equal-period loop therefore always returns zero. For cycle-ratio links `1 → 2`, `1 → 3`, and `2 → 6`, the `3 → 6` relationship is implied: the ratio-3 loop can reset the ratio-6 loop without reparenting. At global tick position -1 with loop period 8, the absolute loop-cycle position is -1; with ancestor period 24 the reset-relative position is 2. The position remains signed 64-bit through rhythm lookup and the sequencer's motive calculations, reducing only to a bounded slot or output.

## Loop rhythms and tick events

Every loop has a `TheoryOfTimeRhythm`: up to 16 gate values, an active size, and an optional reset loop. On that loop's modulated cycle crossing, its gate becomes `rhythm.gate[floorMod(loopCyclePosition, rhythm.size)]`. The default is size 2 with `[true, false]`: one complete cycle on, then one complete cycle off. This loop rhythm supplies the LameJuis time bit; each voice's index-arp rhythm is a separate pattern.

`CrossedCycleBoundary` reports a change in the floor-divided cycle index in the requested domain, in either direction. `AnyTick(loop)` reports the loop's modulated crossing on the current sample. A tick remains an event when neighboring rhythm values are equal, a seek skips several steps, or self reset leaves the selected index at zero. Consumers receive at most one event per sample, without synthesized intermediate steps.

Gate, size, and reset edits become audible only at the edited loop's next modulated tick. Between its ticks the gate holds, including when a faster loop ticks. A rhythm edit alone does not raise `m_anyChange`. Startup explicitly marks every loop as crossed and evaluates all rhythms; stopping forces all gates false.

## Topology edits and processing order

A running topology edit accepts the requested parent and multiplier together only when both the current and requested parents cross a modulated cycle boundary on the same sample. All eligibility checks use the topology before any edits on that sample. Stopped edits and startup accept the requested topology immediately.

For a running sample, processing derives positions, computes crossings, accepts eligible topology edits, remaps positions if the lattice changed, and finally evaluates gates for the loops whose crossing flags are set. Remapping preserves the events that admitted the edit and raises `m_anyChange`; the remap itself is not elapsed travel. Gate lookup uses the accepted topology and its reset ancestry. A stored reset whose period no longer divides in this way is ignored; it becomes effective again when the period relationship returns, at the loop's next tick. Explicit parent-path changes alone do not invalidate a compatible reset.

Changing topology recomputes cycle ratios and lattice periods. Absolute child phase can change by whole cycles at an aligned edit. Periodic outputs agree at the mathematical boundary; coordinates remain direct functions of the new topology. Queries apply the current sample's accepted topology to its global phase, so a child coordinate moves to its new mapping on the accepting sample without passing through intermediate values.

Topology acceptance uses modulated boundaries. The phase-modulation LFO uses unmodulated phase, so those boundaries can differ under phase modulation; that existing distinction is preserved.

## Rhythm controller and patch state

Wrld.Bldr's TheoryOfTimeRhythm mode pairs a left rhythm page with a right reset page. Select it with aux pad `(1, 1)` in the normal grid-mode selector view. The left page has six loop columns and eight step rows: press a pad to toggle its gate; Shift-press row `j` to set size `j + 1`. The reset-page contract follows inferred ancestry: every other loop with a divisible accepted period is eligible, including equal-period loops. Pressing the selected reset again clears it; self and non-divisible resets are disabled. A reset made invalid by a period change remains stored. The picker and time queries share the core period-divisibility calculation.

The engine supports 16 rhythm slots. The grid edits slots 0 through 7 and sizes 1 through 8; StateSaver persists the size and reset plus gate slots 0 through 7. Persistence does not clamp an imported size to eight, and engine slots 8 through 15 have no saved gate entries. StateSaver keys are `TheoryOfTimeRhythm` (loop, step), `TheoryOfTimeRhythmSize` (loop), and `TheoryOfTimeRhythmReset` (loop). Loading a patch with missing rhythm keys preserves the current registered values, following the ordinary StateSaver policy; a fresh instance starts with the default rhythm. Controller mode ordinals are runtime state and require no patch migration.

## Current sample

`TheoryOfTimeBase` holds one `Sample`, `m_sample`, containing its phases, positions, accepted loop topology, gates, crossing events, running flag, and change flag. `Process` advances it in place by exactly one engine sample; crossings compare against the phases it held before the call. Queries take no sample index and read the current sample: `GetLoop`, `GetPhase`, `GetGlobalTickPosition`, `GetPeriodTicks`, `GetCycleRatio`, `CrossedCycleBoundary`, `GetLoopCyclePosition`, and `GetResetCycleCount`. `AnyChange()` reports position motion, startup, stop, or an accepted topology edit on the current sample; `AnyTick(loop)` reports the loop's modulated crossing on the current sample. The static `GetLoopCyclePosition(sample, loop, resetLoop)`, `GetLoopCyclePositionFromTicks`, and `GetResetCycleCount(loopPeriodTicks, resetPeriodTicks)` evaluate the same arithmetic from a given sample or from tick periods.

`TheoryOfTime::Process` runs once per engine sample. It advances the unmodulated phase by `m_freq` (or sets it to zero when stopped), evaluates the phase-modulation LFO from the previous sample's phases, advances the base sample, emits transport start and stop messages, emits a MIDI clock message on any sample where `Phasor2Tick` sees a division crossing, and writes the mono scope.

When stopped, musical phases and positions are zero and gates and crossing flags are false. The stop transition and any accepted stopped topology edit raise the change flag for that sample. Accepted topology remains available for the next start.

## Consumers

- [Sequencer UI State](sequencer-ui-state.md) publishes accepted periods and all loop rhythm slots for signed tick queries outside the audio thread. Reset eligibility is inferred from period divisibility, so these queries do not need a copied parent tree.
- Playback uses absolute modulated loop phase, applies speed and window length, then wraps into its output window.
- PolyXFader selects an explicit phase domain and evaluates a periodic waveform in double precision before narrowing to float. Voice LFOs use modulated phase; the clock's own modulation LFO uses unmodulated phase.
- AHD captures a source/global cycle ratio and envelope period at trigger and follows only absolute modulated global phase during the note.
- Recording timestamps and outgoing MIDI clock use unmodulated global phase. Recording does not inherit phase-offset excursions.
- Delay heads use continuous master phase. `PhaseClock` follows musical phase plus a constant offset while running, advances at nominal speed while stopped, and rebases only the offset on restart. Tempo changes do not reinterpret retained phase history. Read heads wrap within one selected loop behind the writer.
- TheoryOfTime records that phase into its fixed-grid inverse G, mapping to absolute engine samples. G retains four master cycles at 2^18 cells per cycle, with a 32 MiB payload with two fallback endpoints and one replacement per cell. The last forward endpoint closes the partial cell. LFO/delay scopes publish explicit periods and compose reads through G; envelope scopes retain reversible event markers. See the [inverse model](../formal/PhaseTime/README.md) for phase-error bounds and island reassignment and separate spectral/conditioning assumptions.
- The time-warp visualizer uses a second `PhaseClock` and inverse for unmodulated phase. Its explicit period is the warp LFO's common loop period. The mono scope records the warp displacement, and the UI adds the unwarped musical coordinate before wrapping vertically and breaking the path. This inverse uses two master cycles at the same resolution (16 MiB including branch storage); external MIDI corrections can reverse even this clock. The scope needs at most one cycle under normal forward unmodulated travel. It uses the same clock transport transitions and accepted publication as other periodic scopes.
- `GetModulatedGlobalPhaseIncrement()` exposes signed phase travel for LFO slew; `GetUnmodulatedGlobalPhaseIncrement()` supplies nominal travel, including while stopped.

See [PolyXFader LFOs](polyxfader-lfos.md), [AHD envelopes](ahd-envelopes.md), [Multi-Phasor Gate](multi-phasor-gate.md), and [Controller Integrations](ui-controller-integrations.md).
