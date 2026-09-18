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

Each sample has one signed `int64_t` position per domain, shared by all loops. The global period in lattice ticks is twice the LCM of the cycle ratios. A loop's period is the global period divided by its cycle ratio. Consequently every loop has an integral half-period.

```
position = floor(globalPhase * globalPeriodTicks)
gate = floorMod(position, loopPeriodTicks) < loopPeriodTicks / 2
```

Positions are never reduced modulo a period in storage. `PhaseUtils::FloorDiv` and `FloorMod` implement Euclidean arithmetic for negative positions. Gates and indices agree on the same lattice, avoiding independently rounded loop boundaries.

`GetGateStepIndex(loop, sample, resetLoop)` replaces the recursive monodromy calculation. Without a reset it is `floorDiv(position, loopPeriodTicks / 2)`. For a selected ancestor or the loop itself, it reduces that index modulo the number of half-periods in the reset period. A non-ancestor reset is ignored. The sequencer carries the signed 64-bit index through its rhythm and motive calculations, reducing only for bounded outputs.

## Topology edits and events

A running edit accepts the requested parent and multiplier together only when both the current and requested parent cross a modulated cycle boundary on the same sample. All eligibility checks use the topology before any edits on that sample. Stopped edits and initial startup accept the requested topology immediately.

Changing topology recomputes cycle ratios and lattice periods. Absolute child phase can change by whole cycles at an aligned edit: periodic outputs agree at the mathematical boundary, while the coordinate remains a direct function of the new topology. Fractional samples interpolate global phase first, then apply the topology of their interval. They do not interpolate between two differently mapped child coordinates.

`CrossedCycleBoundary` reports a change in the floor-divided cycle index for the requested domain, in either direction. `m_gateStepChanged` similarly reports a changed half-cycle index, including multi-step seeks whose final gate happens to equal its initial state. Startup explicitly raises boundary events. A topology edit retains the old boundary events that admitted it and raises `m_anyChange`; remapping the lattice does not count as elapsed travel.

Topology acceptance uses modulated boundaries. The phase-modulation LFO uses unmodulated phase, so those boundaries can differ under phase modulation; that existing distinction is preserved.

## Microblock snapshots

Each microblock has eight audio samples and one lookahead sample. `RolloverMicroblockBuffer` copies slot 8 to slot 0, then processing fills slots 1 through 8. Every slot contains its phases, positions, accepted loop topology, gates, and events. Queries take an explicit sample index; `GetPhase` also accepts fractional positions through 8 inclusive.

When stopped, all phases and positions are zero and gates and crossing flags are false. Completing the stopped block also clears slot 0 while preserving the stop/topology-change notification. Accepted topology remains available for the next start.

## Consumers

- Playback uses absolute modulated loop phase, applies speed and window length, then wraps into its output window.
- PolyXFader selects an explicit phase domain and evaluates a periodic waveform in double precision before narrowing to float. Voice LFOs use modulated phase; the clock's own modulation LFO uses unmodulated phase.
- AHD captures a source/global cycle ratio and envelope period at trigger and follows only absolute modulated global phase during the note.
- Recording timestamps and outgoing MIDI clock use unmodulated global phase. Recording does not inherit phase-offset excursions.
- Delay heads use absolute sample coordinates, retaining their transport/tempo glue and wrapping only to the buffer's read window. Scopes and UI indicators explicitly reduce their displayed phase.

See [PolyXFader LFOs](polyxfader-lfos.md), [AHD envelopes](ahd-envelopes.md), [Multi-Phasor Gate](multi-phasor-gate.md), and the mathematical source in `docs/tex/TheoryOfTime.tex`.
