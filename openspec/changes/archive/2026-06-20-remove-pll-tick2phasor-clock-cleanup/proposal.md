## Why

The clock path still carries abandoned PLL and tick-to-phasor code that makes the Theory of Time harder to reason about before MIDI input is added. Cleaning this now removes the old clock-mode surface entirely and fixes stopped transport bookkeeping so loop topology state is valid whenever the engine is not running.

## What Changes

- **BREAKING** Remove PLL support from the product, including the PLL type, clock-mode branch, clock-hit forwarding, encoder parameters, and any remaining public product hooks.
- **BREAKING** Remove tick-to-phasor clock input support from the product.
- **BREAKING** Remove the `ClockMode` enum and `TheoryOfTime::Input::m_clockMode` entirely.
- Move phasor-to-tick MIDI clock generation into a new `Phasor2Tick.hpp` file and leave Theory of Time using that type for outgoing clock messages.
- Replace the cascading clock-mode `if`/`else if` handling in Theory of Time with a direct internal-clock path.
- Extract the stopped-state loop maintenance from `ProcessRunning` into `ProcessNotRunning`, call it from the existing non-running flow, ensure it also runs while the timebase remains stopped, and only call `SetLoopSizes` from that path when one of the loop multipliers changed.
- Update specs so the observable contract describes internal clock only, removed PLL and tick-to-phasor behavior, outgoing phasor-to-tick clock generation, and stopped-state loop-size/index maintenance.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `phasor-timebase`: Removes PLL and tick-to-phasor input modes, removes `ClockMode`/`m_clockMode`, keeps outgoing phasor-to-tick clock generation, and changes stopped-state processing so loop sizes and related loop state are maintained whenever the timebase is not running.
- `nonagon-sequencer`: Clarifies that the non-running pipeline uses the timebase's not-running processing before resetting the downstream sequencer/gate state.

## Impact

- Affected source: `private/src/TheoryOfTime.hpp`, `private/src/Tick2Phasor.hpp`, new `private/src/Phasor2Tick.hpp`, `private/src/PLL.hpp`, `private/src/TheNonagon.hpp`, `private/src/TheNonagonSquiggleBoy.hpp`, `private/src/ForEachSmartGridOneParam.hpp`, and tests/support that reference removed clock modes or PLL parameters.
- Affected docs/specs: `openspec/specs/phasor-timebase/spec.md`, `openspec/specs/nonagon-sequencer/spec.md`, and any product docs that still describe PLL or tick-to-phasor as active clock sources.
- Tests should cover internal-clock behavior, stopped-state loop maintenance before first run and after stop, outgoing phasor-to-tick clock messages, and absence of PLL/tick-to-phasor references from product code.
