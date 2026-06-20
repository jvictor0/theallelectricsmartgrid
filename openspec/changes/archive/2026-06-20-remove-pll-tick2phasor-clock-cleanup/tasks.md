## 1. Clock Source Cleanup

- [x] 1.1 Move `Phasor2Tick` from `private/src/Tick2Phasor.hpp` into a new `private/src/Phasor2Tick.hpp` file and update includes.
- [x] 1.2 Remove `Tick2Phasor` input-clock state, input fields, processing, and reset handling from `TheoryOfTime`.
- [x] 1.3 Remove `PLL` support from `TheoryOfTime`, including `m_pll`, `PLL::Input`, `ProcessPLLHit`, and PLL clock processing.
- [x] 1.4 Remove Nonagon PLL forwarding methods and any call sites that expose PLL hits as product behavior.
- [x] 1.5 Remove `private/src/PLL.hpp` and `private/src/Tick2Phasor.hpp` once no product code includes them.

## 2. Clock Mode Simplification

- [x] 2.1 Remove `TheoryOfTime::ClockMode`.
- [x] 2.2 Remove `TheoryOfTime::Input::m_clockMode`.
- [x] 2.3 Replace the cascading clock-mode `if`/`else if` handling in `TheoryOfTime::Process` with the direct internal-clock path.
- [x] 2.4 Remove tests/support code that assigns or asserts clock-mode values.

## 3. Stopped-State Processing

- [x] 3.1 Extract stopped-state loop maintenance into `TheoryOfTimeBase::ProcessNotRunning(size_t j, Input& input)`.
- [x] 3.2 Move stopped multiplier application into `ProcessNotRunning`.
- [x] 3.3 Call `ProcessNotRunning` both on the running-to-stopped transition and while already stopped.
- [x] 3.4 Preserve running-edge priming in `ProcessRunning` so start still resets global phase and primes loop positions to `loopSize - 1`.
- [x] 3.5 Ensure `ProcessNotRunning` calls `SetLoopSizes` only when one or more accepted loop multipliers changed.
- [x] 3.6 Ensure stopped state clears positions, previous positions, gates, phasors, independent phasors, winding, and independent positions while keeping loop sizes derived from accepted multipliers.

## 4. Product Parameter and Documentation Cleanup

- [x] 4.1 Remove PLL encoder parameters from `private/src/ForEachSmartGridOneParam.hpp`.
- [x] 4.2 Remove SquiggleBoy PLL parameter update code from `SetNonagonInputs`.
- [x] 4.3 Update product docs that mention PLL or tick-to-phasor as active clock sources.
- [x] 4.4 Keep outgoing phasor-to-tick clock documentation aligned with the new `Phasor2Tick.hpp` ownership.

## 5. Verification

- [x] 5.1 Update unit/support tests for removal of `ClockMode` and `m_clockMode`.
- [x] 5.2 Add or update tests proving stopped multiplier changes recompute loop sizes before first run.
- [x] 5.3 Add or update tests proving stopping clears loop motion while preserving topology-derived loop sizes.
- [x] 5.4 Run the relevant Theory of Time, Nonagon, and system test targets.
- [x] 5.5 Search product source, tests, docs, and specs to verify no PLL or tick-to-phasor product references remain except archived OpenSpec changes.
