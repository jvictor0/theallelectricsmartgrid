## Context

Theory of Time currently exposes several clock-mode branches even though the product is being prepared for a cleaner MIDI-input implementation. `TheoryOfTime.hpp` owns internal clock advancement, tick-to-phasor input, phasor-to-tick output, and PLL state in one header; `Tick2Phasor.hpp` also contains the outgoing `Phasor2Tick` helper. The current stopped-state handling only recomputes loop sizes on the first running edge, so topology and derived loop state can remain stale while the transport is stopped until the user presses run.

The refactor keeps internal clocking and outgoing MIDI clock generation, removes abandoned input-clock modes, and removes the old `ClockMode`/`m_clockMode` surface entirely.

## Goals / Non-Goals

**Goals:**

- Remove PLL from product source, parameters, tests, and specs.
- Remove tick-to-phasor input support from product source, tests, and specs.
- Preserve outgoing phasor-to-tick clock behavior by moving `Phasor2Tick` into its own `private/src/Phasor2Tick.hpp`.
- Remove `TheoryOfTime::ClockMode` and `TheoryOfTime::Input::m_clockMode`.
- Replace clock-mode cascading conditionals with a single internal-clock path.
- Extract stopped-state loop maintenance into `ProcessNotRunning` and call it whenever `TheoryOfTimeBase::Process` receives a non-running input.
- Ensure stopped loop sizes, loop indexes, gates, independent positions, and related derived values reflect accepted multiplier changes without requiring a start/stop edge.

**Non-Goals:**

- Add MIDI input, external clocking, or new clock modes.
- Change the six-loop topology model, monodromy formula, phase-modulation LFO behavior, or outgoing MIDI clock message format.
- Redesign encoder banks beyond removing PLL-facing parameters and references.

## Decisions

1. Remove the clock enum and mode member.

   Delete `TheoryOfTime::ClockMode` and `TheoryOfTime::Input::m_clockMode`. With PLL, external phasor, and tick-to-phasor input removed, there is only one clock path, so an enum with one value adds indirection without behavior. The alternative was keeping a single-value enum for a future MIDI-input diff, but that would leave a product surface whose only purpose is speculative.

2. Delete PLL rather than leaving a dormant type.

   Remove `private/src/PLL.hpp`, `TheoryOfTime::m_pll`, `PLL::Input`, `ProcessPLLHit`, Nonagon forwarding, SquiggleBoy PLL parameter updates, and PLL encoder rows. The alternative was to keep PLL unused behind no selectable clock mode, but the user explicitly asked for PLL to be completely removed from the product, and leaving it would preserve dead parameters and misleading tests.

3. Remove tick-to-phasor input while preserving phasor-to-tick output.

   Delete the `Tick2Phasor` type and all `m_tick2Phasor` input-clock handling. Move the existing `Phasor2Tick` helper to `private/src/Phasor2Tick.hpp` and include that from Theory of Time. This keeps outgoing clock messages and file ownership clear. The alternative was renaming `Tick2Phasor.hpp`, but a new file avoids carrying the removed input concept forward.

4. Centralize stopped-state processing in `ProcessNotRunning`.

   Move the stopped loop maintenance needed outside `ProcessRunning` into a dedicated `ProcessNotRunning(size_t j, Input& input)`. `TheoryOfTimeBase::Process` calls it both when transitioning from running to stopped and when already stopped. The function checks requested loop multipliers, updates accepted multiplier values, calls `SetLoopSizes(j)` only if at least one multiplier changed, sets loop positions/indexes/gates to the stopped contract, clears phasors/winding, and sets `m_anyChange[j]` only when the observable stopped state changes. Parent-index handling stays out of the stopped recompute trigger unless implementation already accepts it separately. The alternative was duplicating the loop-size code in the non-running branches, but a named helper makes the stopped contract explicit and reduces drift.

5. Keep running-edge priming separate from stopped maintenance.

   `ProcessRunning` should still handle the first running sample by resetting the global phase and priming positions to `loopSize - 1`, so the first advance lands on zero. `ProcessNotRunning` should not consume or simulate time; it prepares the static stopped topology and loop state.

## Risks / Trade-offs

- Removed encoder params may affect saved patches that contain PLL parameter keys -> Treat these as obsolete keys ignored on load; no migration is required because PLL no longer has product behavior.
- Updating stopped loop sizes may make tests that expected all stopped loop sizes to be 1 obsolete -> Update the stopped-state contract to assert topology-derived loop sizes with zeroed positions/gates instead.
- Removing `ClockMode` means the later MIDI-input diff must reintroduce an explicit clock selection surface -> This is intentional; the later change should define the new semantics instead of inheriting stale modes.
- Moving `Phasor2Tick` can create include fallout -> Add the new header where `Phasor2Tick` is used and remove `Tick2Phasor.hpp` includes from files that no longer need it.
