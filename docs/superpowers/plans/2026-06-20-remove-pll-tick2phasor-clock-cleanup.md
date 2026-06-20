# Remove PLL Tick2Phasor Clock Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove PLL, tick-to-phasor input, and the clock-mode surface while preserving internal clocking, outgoing phasor-to-tick clock generation, and correct stopped-state loop maintenance.

**Architecture:** `TheoryOfTime` becomes an internal-clock-only timebase with no clock-mode enum or mode member. `Phasor2Tick` moves to its own header for outgoing MIDI clock generation. `TheoryOfTimeBase::ProcessNotRunning` owns stopped-state maintenance, accepting multiplier changes and calling `SetLoopSizes` only when a multiplier changed.

**Tech Stack:** C++17 header-heavy DSP core under `private/src`, doctest tests under `private/test`, CMake test binary `private/test/build/smartgrid_tests`, OpenSpec artifacts under `openspec/changes/remove-pll-tick2phasor-clock-cleanup`.

---

## OpenSpec Requirements Preserved

- `TheoryOfTime::ClockMode` and `TheoryOfTime::Input::m_clockMode` must be removed.
- The timebase advances directly from internal `m_freq`; PLL, external phasor input, and tick-to-phasor input are not selectable product modes.
- Outgoing clock generation remains through `Phasor2Tick` in `private/src/Phasor2Tick.hpp`.
- `ProcessNotRunning(size_t j, Input& input)` runs on running-to-stopped and already-stopped paths.
- `ProcessNotRunning` accepts changed loop multipliers and calls `SetLoopSizes(j)` only when at least one multiplier changed.
- Stopped state clears positions, previous positions, gates, phasors, independent phasors, winding, and independent positions while keeping loop sizes derived from accepted multipliers.
- Nonagon not-running frames still reset `MultiPhasorGate` and `LameJuis`, then the timebase processing loop uses the not-running path.

## File Structure

- Create `private/src/Phasor2Tick.hpp`: Owns outgoing `Phasor2Tick`.
- Delete `private/src/Tick2Phasor.hpp`: Removed input-clock helper after moving `Phasor2Tick`.
- Delete `private/src/PLL.hpp`: Removed PLL helper.
- Modify `private/src/TheoryOfTime.hpp`: Remove old includes/types/branches, include `Phasor2Tick.hpp`, add `ProcessNotRunning`, and simplify `Process`.
- Modify `private/src/TheNonagon.hpp`: Remove `Tick2Phasor.hpp` include and PLL forwarding methods.
- Modify `private/src/TheNonagonSquiggleBoy.hpp`: Remove PLL encoder parameter updates.
- Modify `private/src/ForEachSmartGridOneParam.hpp`: Remove PLL encoder rows.
- Modify `private/test/support/TimeRig.hpp`: Remove clock-mode setup/comment references and add loop-state accessors.
- Modify `private/test/unit/time_rig.cpp`: Add clock-mode absence and stopped multiplier tests.
- Modify `docs/theory-of-time.md` and `docs/nonagon.md`: Update active documentation.
- Modify `openspec/changes/remove-pll-tick2phasor-clock-cleanup/tasks.md`: Check OpenSpec tasks only after reviewed, verified completion.

---

### Task 1: Core Clock Cleanup, `ProcessNotRunning`, and Tests

**Files:**
- Create: `private/src/Phasor2Tick.hpp`
- Delete: `private/src/Tick2Phasor.hpp`
- Delete: `private/src/PLL.hpp`
- Modify: `private/src/TheoryOfTime.hpp`
- Modify: `private/src/TheNonagon.hpp`
- Modify: `private/src/TheNonagonSquiggleBoy.hpp`
- Modify: `private/src/ForEachSmartGridOneParam.hpp`
- Modify: `private/test/support/TimeRig.hpp`
- Modify: `private/test/unit/time_rig.cpp`

- [ ] **Step 1: Write the RED tests and support accessors**

In `private/test/support/TimeRig.hpp`:

- Remove `m_input.m_clockMode = TheoryOfTime::ClockMode::Internal;`.
- Change the comment `Each Process(j) call (ClockMode::Internal) advances` to `Each Process(j) call advances`.
- Add these public methods after `Gate(size_t loop) const`:

```cpp
    int LoopSize(size_t loop) const
    {
        assert(loop < x_numLoops);
        return m_tot.m_loops[loop].m_loopSize[CurrentUBlockIndex()];
    }

    int Position(size_t loop) const
    {
        assert(loop < x_numLoops);
        return m_tot.m_loops[loop].m_position[CurrentUBlockIndex()];
    }

    int PrevPosition(size_t loop) const
    {
        assert(loop < x_numLoops);
        return m_tot.m_loops[loop].m_prevPosition[CurrentUBlockIndex()];
    }
```

In `private/test/unit/time_rig.cpp`, add:

```cpp
#include <type_traits>
```

Inside the anonymous namespace, after `TraceMasterPhasor`, add:

```cpp
template <typename T, typename = void>
struct HasClockMode : std::false_type
{
};

template <typename T>
struct HasClockMode<T, std::void_t<decltype(std::declval<T&>().m_clockMode)>> : std::true_type
{
};
```

Add this test after `TimeRig: construction is SampleTimer-coherent and starts stopped`:

```cpp
DOCTEST_TEST_CASE("TimeRig: TheoryOfTime input has no clock mode")
{
    DOCTEST_CHECK(HasClockMode<TheoryOfTime::Input>::value == false);
}
```

Add this test after `TimeRig: not running -> phasors hold at zero`:

```cpp
DOCTEST_TEST_CASE("TimeRig: stopped multiplier changes recompute loop sizes before first run")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;

    rig.SetMultiplier(4, 3);
    rig.AdvanceControlFrame();

    DOCTEST_CHECK(rig.IsRunning() == false);
    DOCTEST_CHECK(rig.LoopSize(4) == 2);
    DOCTEST_CHECK(rig.LoopSize(TimeRig::x_masterLoop) == 6);
    DOCTEST_CHECK(rig.Position(4) == 0);
    DOCTEST_CHECK(rig.PrevPosition(4) == 0);
    DOCTEST_CHECK(rig.Gate(4) == false);
    DOCTEST_CHECK(rig.Phasor(4) == doctest::Approx(0.0));
    DOCTEST_CHECK(rig.DirectPhasor(4) == doctest::Approx(0.0));
}
```

In `TimeRig: stopping a running rig halts the phasor`, insert after `rig.SetMasterPeriodSamples(128.0);`:

```cpp
    rig.SetMultiplier(4, 3);
```

After `DOCTEST_CHECK(rig.IsRunning() == false);`, add:

```cpp
    DOCTEST_CHECK(rig.LoopSize(4) == 2);
    DOCTEST_CHECK(rig.LoopSize(TimeRig::x_masterLoop) == 6);
    DOCTEST_CHECK(rig.Position(4) == 0);
    DOCTEST_CHECK(rig.PrevPosition(4) == 0);
    DOCTEST_CHECK(rig.Gate(4) == false);
```

- [ ] **Step 2: Verify RED**

Run:

```bash
cmake --build private/test/build -j 8
private/test/build/smartgrid_tests --test-case="TimeRig:*"
```

Expected before implementation: compile or focused tests fail because `m_clockMode` still exists and stopped multipliers do not recompute before first run.

- [ ] **Step 3: Move `Phasor2Tick` to its own header**

Create `private/src/Phasor2Tick.hpp` containing the existing `Phasor2Tick` type from `private/src/Tick2Phasor.hpp`, with these includes:

```cpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
```

Keep the existing constants, constructor, `Process(double phase)`, and `UpdateDivisions(double freq)` behavior unchanged.

- [ ] **Step 4: Remove old clock source members and branches**

In `private/src/TheoryOfTime.hpp`:

- Replace `#include "Tick2Phasor.hpp"` with `#include "Phasor2Tick.hpp"`.
- Remove `#include "PLL.hpp"`.
- Remove `Tick2Phasor m_tick2Phasor;`.
- Remove `PLL m_pll;`.
- Remove the entire `enum class ClockMode` block.
- Remove `m_clockMode(ClockMode::Internal)` from `TheoryOfTime::Input`'s initializer list.
- Remove `ClockMode m_clockMode;`.
- Remove `Tick2Phasor::Input m_tick2PhasorInput;`.
- Remove `PLL::Input m_pllInput;`.
- Remove both `ProcessPLLHit` overloads.
- Replace the old `if (input.m_clockMode == ...)` cascade at the start of `TheoryOfTime::Process` with:

```cpp
        if (input.m_running)
        {
            input.m_phasor += input.m_freq;
            input.m_phasorTop = 1.0f <= input.m_phasor;
            input.m_phasor = input.m_phasor - floor(input.m_phasor);
            m_masterLoopSamples = 1.0 / input.m_freq;
        }
        else
        {
            input.m_phasor = 0;
            m_masterLoopSamples = 1.0 / input.m_freq;
        }
```

Delete the `Tick2Phasor` post-branch.

- [ ] **Step 5: Implement `ProcessNotRunning`**

In `private/src/TheoryOfTime.hpp`, add to `TheoryOfTimeBase` after `SetLoopSizes(size_t j)`:

```cpp
    void SetStoppedLoopState(size_t j)
    {
        m_positionIndependent[j] = 0;
        m_prevPositionIndependent[j] = 0;

        for (int i = 0; i < x_numLoops; ++i)
        {
            TimeLoop& loop = m_loops[i];
            loop.m_gate[j] = false;
            loop.m_gateChanged[j] = false;
            loop.m_top[j] = false;
            loop.m_topIndependent[j] = false;
            loop.m_position[j] = 0;
            loop.m_prevPosition[j] = 0;
            loop.m_phasor[j] = 0;
            loop.m_phasorIndependent[j] = 0;
            loop.m_globalWinding[j] = 0;
            loop.m_ascending[j] = false;
            loop.m_externalLoopMult[j] = GetMasterLoop()->m_loopSize[j] / std::max(1, loop.m_loopSize[j]);
        }
    }

    void ProcessNotRunning(size_t j, Input& input)
    {
        bool multChanged = false;
        for (int i = x_numLoops - 2; i >= 0; --i)
        {
            if (m_loops[i].m_parentMult[j] != input.m_input[i].m_parentMult)
            {
                m_loops[i].m_parentMult[j] = input.m_input[i].m_parentMult;
                multChanged = true;
            }
        }

        if (multChanged)
        {
            SetLoopSizes(j);
        }

        m_anyChange[j] = m_running || multChanged;
        m_running = false;
        m_globalPhase.Reset(0);
        SetStoppedLoopState(j);
    }
```

Then replace the non-running branches in `TheoryOfTimeBase::Process(size_t j, Input& input)` with:

```cpp
        else
        {
            ProcessNotRunning(j, input);
        }
```

Keep the first-running-sample priming in `ProcessRunning` intact.

- [ ] **Step 6: Remove Nonagon and product PLL hooks**

In `private/src/TheNonagon.hpp`:

- Remove `#include "Tick2Phasor.hpp"`.
- Remove `TheNonagonInternal::ProcessPLLHit`.
- Remove `TheNonagonSmartGrid::ProcessPLLHit`.

In `private/src/TheNonagonSquiggleBoy.hpp`, remove the four `m_pllInput` update lines in `SetNonagonInputs`.

In `private/src/ForEachSmartGridOneParam.hpp`, remove rows for:

```cpp
PLLPhaseLearn
PLLFreqLearn
PLLPhaseApply
PLLFreqApply
```

- [ ] **Step 7: Delete removed helper headers**

Delete:

```bash
private/src/Tick2Phasor.hpp
private/src/PLL.hpp
```

- [ ] **Step 8: Verify GREEN**

Run:

```bash
cmake --build private/test/build -j 8
private/test/build/smartgrid_tests --test-case="TimeRig:*"
```

Expected: build succeeds and all `TimeRig:*` tests pass.

---

### Task 2: Active Documentation and Reference Cleanup

**Files:**
- Modify: `docs/theory-of-time.md`
- Modify: `docs/nonagon.md`

- [ ] **Step 1: Update Theory of Time clock-source docs**

In `docs/theory-of-time.md`, replace:

```md
- **True phase** comes from the chosen clock source and is stored in the input phasor (`input.m_phasor`). It is advanced each control frame (e.g. internal tempo, external clock, PLL, or Tick2Phasor).
```

with:

```md
- **True phase** comes from the internal clock frequency and is stored in the input phasor (`input.m_phasor`). It is advanced each control frame from `input.m_freq`; PLL, external phasor input, and tick-to-phasor input are not product clock modes.
- **Outgoing MIDI clock** is generated in the opposite direction by `Phasor2Tick` (`private/src/Phasor2Tick.hpp`), which observes the independent master phasor and emits clock ticks through the message-out buffer while the timebase is running.
```

- [ ] **Step 2: Document stopped-state loop maintenance**

In `docs/theory-of-time.md`, after the `Loop size and LCM` section, add:

```md
When the transport is not running, `ProcessNotRunning()` keeps the stopped loop state deterministic. It accepts changed loop multipliers, calls `SetLoopSizes()` only when at least one multiplier changed, clears positions, gates, phasors, independent phasors, and winding, and leaves loop sizes derived from the accepted multipliers. This lets stopped multiplier edits be visible before the first run without advancing time.
```

- [ ] **Step 3: Update Nonagon process docs**

In `docs/nonagon.md`, under step 5, add:

```md
   - When the user transport is stopped and no voice gate keeps the timebase alive, those calls take the timebase's `ProcessNotRunning()` path, so stopped loop sizes and related variables stay current while Multi-Phasor Gate and LameJuis are reset.
```

- [ ] **Step 4: Verify docs/reference cleanup**

Run:

```bash
rg -n 'PLL|Tick2Phasor|ClockMode|m_clockMode|ProcessPLLHit|m_tick2Phasor' private/src private/test docs openspec/specs --glob '!openspec/changes/archive/**'
```

Expected: no active product references remain, except active OpenSpec prose describing removals and any generated build metadata under ignored build directories.

---

### Task 3: Full Verification and OpenSpec Task Synchronization

**Files:**
- Modify: `openspec/changes/remove-pll-tick2phasor-clock-cleanup/tasks.md`

- [ ] **Step 1: Configure and build tests**

Run:

```bash
cmake -S private/test -B private/test/build
cmake --build private/test/build -j 8
```

Expected: `smartgrid_tests` builds successfully.

- [ ] **Step 2: Run focused timebase and topology tests**

Run:

```bash
private/test/build/smartgrid_tests --test-case="TimeRig:*"
private/test/build/smartgrid_tests --test-case="sys_theoryoftime_topology:*"
private/test/build/smartgrid_tests --test-case="TheNonagonSquiggleBoyInternal constructs and processes samples"
```

Expected: all selected tests pass.

- [ ] **Step 3: Run full standalone test suite**

Run:

```bash
private/test/build/smartgrid_tests
```

Expected: all tests pass.

- [ ] **Step 4: Run final active-reference search**

Run:

```bash
rg -n 'PLL|Tick2Phasor|ClockMode|m_clockMode|ProcessPLLHit|m_tick2Phasor' private/src private/test docs openspec/specs --glob '!openspec/changes/archive/**'
```

Expected: no active product references remain, except active OpenSpec prose saying these names were removed and ignored build metadata if present.

- [ ] **Step 5: Mark OpenSpec tasks complete**

In `openspec/changes/remove-pll-tick2phasor-clock-cleanup/tasks.md`, change every checkbox from `- [ ]` to `- [x]` only after Tasks 1 through 3 pass implementation review and verification.

- [ ] **Step 6: Verify OpenSpec status**

Run:

```bash
openspec status --change remove-pll-tick2phasor-clock-cleanup
```

Expected: OpenSpec reports all artifacts complete.

---

## Self-Review

Spec coverage:

- Clock mode removal is covered by Task 1 tests and implementation.
- PLL and tick-to-phasor removal is covered by Task 1 implementation and Task 3 search verification.
- `Phasor2Tick.hpp` ownership is covered by Task 1 and Task 2 docs.
- `ProcessNotRunning` stopped multiplier behavior is covered by Task 1 tests and implementation.
- Nonagon stopped pipeline documentation is covered by Task 2.
- OpenSpec task synchronization is covered by Task 3.

Placeholder scan:

- No `TBD`, `TODO`, `implement later`, or unspecified edge handling remains.

Type consistency:

- The plan consistently uses `TheoryOfTimeBase::ProcessNotRunning(size_t j, Input& input)`, `SetLoopSizes(j)`, `Phasor2Tick`, `m_parentMult`, and removal of `ClockMode`/`m_clockMode`.
