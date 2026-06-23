# MIDI Clock Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build reliable external MIDI clock sync by routing realtime MIDI through the existing message bus, owning synchronization inside `TheNonagonSquiggleBoyInternal`, and exposing internal/external clock configuration plus external sync loop selection.

**Architecture:** Preserve the user-provided `ExternalClockSync` math as a focused helper, then feed it per-sample clock tick status from timestamp-gated `MessageIn` realtime events. `TheNonagonSquiggleBoyInternal` remains the only owner of external clock policy and supplies only an effective frequency to Theory of Time, while JUCE MIDI I/O handles timestamping and fixed-latency output scheduling at the boundary.

**Tech Stack:** C++17 headers under `private/src`, doctest tests under `private/test`, JUCE app code under `JUCE/SmartGridOne/Source`, OpenSpec tasks under `openspec/changes/add-midi-clock-sync`.

---

## File Map

- Create `private/src/ExternalClockSync.hpp`: synchronizer helper ported from `/Users/joyo/theallelectricsmartgrid/private/src/ExternalClockSync.hpp`, with initialization and zero-division guards.
- Modify `private/src/BasicMidi.hpp`: add realtime continue support, realtime helper predicates, route-aware constructors, and one-byte size behavior.
- Modify `private/src/MessageIn.hpp`: add typed realtime modes and helper predicates.
- Modify `private/src/MidiToMessageIn.hpp`: add a fixed realtime route, initialize routes, convert clock/start/continue/stop to `MessageIn`.
- Modify `private/src/MessageInBus.hpp`: preserve timestamp-gated realtime events and avoid queueing `NoMessage`.
- Modify `private/src/TheNonagonSquiggleBoy.hpp`: own clock mode, synchronizer, per-sample tick status, realtime transport handling, external tempo handoff, and loop mapping.
- Modify `private/src/TheNonagonSquiggleBoyQuadLaunchpadTwister.hpp` and `private/src/TheNonagonSquiggleBoyWrldBldr.hpp`: configure the fixed realtime route and forward realtime messages to the internal integration.
- Modify `private/src/ForEachSmartGridOneParam.hpp`: add a six-position Theory of Time external sync loop selector.
- Modify `JUCE/SmartGridOne/Source/MidiHandlers.hpp`: forward supported one-byte realtime messages using the fixed realtime route.
- Modify `private/src/MessageInBus.hpp`: queue all incoming `MessageIn` events with `timestamp + 20000us` while leaving outgoing MIDI sender behavior unchanged.
- Modify `JUCE/SmartGridOne/Source/Configuration.hpp`, `ConfigPage.hpp`, `MainComponent.h`, `MainComponent.cpp`, and `NonagonWrapper.hpp`: store, display, persist, and apply clock mode.
- Create tests in `private/test/unit`: `external_clock_sync.cpp`, `midi_realtime.cpp`, and parameter/config tests where the existing test harness supports them.
- Create or extend tests in `private/test/system`: system coverage for run-state realtime events, one-sample clock tick consumption, and external tempo source behavior.
- Update `openspec/changes/add-midi-clock-sync/tasks.md` only after corresponding code is implemented, reviewed, and verified.

## Task 1: Core Realtime MIDI and Message Bus

**Files:**
- Modify: `private/src/BasicMidi.hpp`
- Modify: `private/src/MessageIn.hpp`
- Modify: `private/src/MidiToMessageIn.hpp`
- Modify: `private/src/MessageInBus.hpp`
- Test: `private/test/unit/midi_realtime.cpp`

- [ ] **Step 1: Write failing tests for realtime MIDI primitives and bus routing**

Create `private/test/unit/midi_realtime.cpp` with tests that assert:

```cpp
#include "doctest.h"
#include "BasicMidi.hpp"
#include "MessageInBus.hpp"
#include "MidiToMessageIn.hpp"

using namespace SmartGrid;

TEST_CASE("BasicMidi preserves supported one byte realtime statuses")
{
    BasicMidi clock = BasicMidi::Realtime(2000000, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusClock);
    BasicMidi start = BasicMidi::Realtime(2000001, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusTransportStart);
    BasicMidi cont = BasicMidi::Realtime(2000002, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusTransportContinue);
    BasicMidi stop = BasicMidi::Realtime(2000003, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusTransportStop);

    CHECK(clock.Size() == 1);
    CHECK(start.Size() == 1);
    CHECK(cont.Size() == 1);
    CHECK(stop.Size() == 1);
    CHECK(clock.m_routeId == MidiToMessageIn::x_realtimeRouteId);
    CHECK(cont.m_msg[0] == BasicMidi::x_statusTransportContinue);
    CHECK(BasicMidi::IsSupportedRealtimeStatus(BasicMidi::x_statusClock));
    CHECK_FALSE(BasicMidi::IsSupportedRealtimeStatus(0xFE));
}

TEST_CASE("Realtime route converts clock start continue stop into timestamp gated MessageIn")
{
    MessageInBus bus;
    bus.m_midiToMessageIn.SetRouteType(MidiToMessageIn::x_realtimeRouteId, MidiToMessageIn::RouteType::Realtime);

    bus.Push(BasicMidi::Realtime(2000, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusClock));
    MessageIn early = bus.Pop(1999);
    CHECK(early.NoMessage());

    MessageIn clock = bus.Pop(2000);
    CHECK(clock.m_mode == MessageIn::Mode::MidiClock);
    CHECK(clock.m_timestamp == 2000);

    bus.Push(BasicMidi::Realtime(2001, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusTransportStart));
    bus.Push(BasicMidi::Realtime(2002, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusTransportContinue));
    bus.Push(BasicMidi::Realtime(2003, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusTransportStop));

    CHECK(bus.Pop(2001).m_mode == MessageIn::Mode::MidiStart);
    CHECK(bus.Pop(2002).m_mode == MessageIn::Mode::MidiContinue);
    CHECK(bus.Pop(2003).m_mode == MessageIn::Mode::MidiStop);
}

TEST_CASE("Unsupported realtime statuses become no message and are not queued")
{
    MessageInBus bus;
    bus.m_midiToMessageIn.SetRouteType(MidiToMessageIn::x_realtimeRouteId, MidiToMessageIn::RouteType::Realtime);

    bus.Push(BasicMidi::Realtime(3000, MidiToMessageIn::x_realtimeRouteId, 0xFE));

    CHECK(bus.Pop(3000).NoMessage());
}
```

- [ ] **Step 2: Run the focused test and confirm it fails**

Run: `cmake --build private/build --target tests && private/build/tests --test-case="*Realtime*"`

Expected before implementation: compile failure for missing `BasicMidi::Realtime`, `x_statusTransportContinue`, `RouteType::Realtime`, `MessageIn::Mode::MidiClock`, and related helpers.

- [ ] **Step 3: Implement realtime MIDI primitives and conversion**

Add these public surfaces:

```cpp
static constexpr uint8_t x_statusTransportContinue = 0xFB;

static BasicMidi TransportContinue(size_t timestamp)
{
    return BasicMidi(timestamp, -1, x_statusTransportContinue, 0, 0);
}

static BasicMidi Realtime(size_t timestamp, int routeId, uint8_t status)
{
    return BasicMidi(timestamp, routeId, status, 0, 0);
}

static bool IsSupportedRealtimeStatus(uint8_t status)
{
    return status == x_statusClock
        || status == x_statusTransportStart
        || status == x_statusTransportContinue
        || status == x_statusTransportStop;
}
```

Add `MessageIn::Mode::MidiClock`, `MidiStart`, `MidiContinue`, and `MidiStop`, plus `IsRealtime()` and `IsMidiClock()` helpers. Add `MidiToMessageIn::RouteType::None` and `Realtime`, initialize all routes to `None`, add `static constexpr int x_realtimeRouteId = 15`, and convert supported realtime statuses to the new modes. Update `MessageInBus::Push(BasicMidi)` so converted `NoMessage` values are not queued.

- [ ] **Step 4: Run the focused tests and confirm they pass**

Run: `cmake --build private/build --target tests && private/build/tests --test-case="*Realtime*"`

Expected after implementation: all tests in `midi_realtime.cpp` pass.

## Task 2: External Clock Synchronizer Helper

**Files:**
- Create: `private/src/ExternalClockSync.hpp`
- Test: `private/test/unit/external_clock_sync.cpp`

- [ ] **Step 1: Copy the user-provided source and write characterization tests**

Create `private/src/ExternalClockSync.hpp` from `/Users/joyo/theallelectricsmartgrid/private/src/ExternalClockSync.hpp`. Preserve the phase-correction calculation:

```cpp
double currentPhase = input.m_theoryOfTime->GetUnwoundMasterIndependent(SampleTimer::GetUBlockIndex());
float multiplier = input.m_theoryOfTime->GetLoopExternalMultiplier(SampleTimer::GetUBlockIndex(), input.m_loopIndex);
double currentTicksOffset = static_cast<double>(m_ticksOffset) / input.m_ticksMultiplier;
double nextTicksOffset = static_cast<double>(m_ticksOffset + 1) / input.m_ticksMultiplier;
double currentExpectedBranch = std::round(currentPhase * multiplier - currentTicksOffset);
double nextExpectedPhase = (currentExpectedBranch + nextTicksOffset) / multiplier;
int samplesUntilNextClock = m_samplesSinceLastClock + 1;
m_freqOut = (nextExpectedPhase - currentPhase) / samplesUntilNextClock;
```

Create tests for:
- Constructor initializes `m_samplesSinceLastClock`, `m_ticksOffset`, `m_freqOut`, and `m_running`.
- Running start resets tick offset while preserving the stopped-clock elapsed sample count and tempo estimate for the next tick.
- Steady 24 ppqn ticks produce a positive frequency estimate scaled to the selected external clock loop.
- A missing tick increases the samples-per-tick interval and lowers the estimate.
- Stop resets running state and tick offset.

- [ ] **Step 2: Run synchronizer tests and confirm expected failures before implementation fixes**

Run: `cmake --build private/build --target tests && private/build/tests --test-case="*ExternalClockSync*"`

Expected before implementation: missing header or compile failures until the helper exists.

- [ ] **Step 3: Make the helper safe without replacing the math**

Keep the public struct style, add a constructor, guard `m_samplesSinceLastClock <= 0`, and use the current `TheoryOfTime::GetLoopExternalMultiplier(size_t, int)` signature. Keep all synchronization state inside this helper.

- [ ] **Step 4: Run synchronizer tests and confirm they pass**

Run: `cmake --build private/build --target tests && private/build/tests --test-case="*ExternalClockSync*"`

Expected after implementation: all synchronizer tests pass.

## Task 3: SquiggleBoy External Clock Integration

**Files:**
- Modify: `private/src/TheNonagonSquiggleBoy.hpp`
- Modify: `private/src/TheNonagonSquiggleBoyQuadLaunchpadTwister.hpp`
- Modify: `private/src/TheNonagonSquiggleBoyWrldBldr.hpp`
- Test: `private/test/system/midi_clock_sync.cpp`

- [ ] **Step 1: Write system tests for run state, one-sample tick consumption, and external tempo source**

Add tests that construct or use the existing system fixture to verify:
- `MessageIn::Mode::MidiStart` and `MidiContinue` set `m_running = true`.
- `MessageIn::Mode::MidiStop` sets `m_running = false`.
- A visible `MidiClock` event sets the synchronizer input true for exactly one sample and false on the next sample.
- Internal mode writes tempo from `Param::Tempo`.
- External mode writes the synchronizer frequency estimate instead of the tempo parameter.

- [ ] **Step 2: Run the new system tests and confirm failures**

Run: `cmake --build private/build --target tests && private/build/tests --test-case="*MIDI clock sync*"`

Expected before implementation: compile or assertion failures because external clock mode and realtime handlers do not exist.

- [ ] **Step 3: Implement internal ownership and realtime application**

Add a public enum and members to `TheNonagonSquiggleBoyInternal`:

```cpp
enum class ClockMode
{
    Internal,
    External
};

ClockMode m_clockMode;
ExternalClockSync m_clockSynchronizer;
bool m_clockTick;
```

Add setters/getters for clock mode, a helper `ExternalClockLoopIndex()` that maps `TheoryOfTimeBase::x_numLoops - switchVal - 1`, and an `ApplyRealtime(MessageIn const& msg)` path. `MidiStart` and `MidiContinue` set `m_running = true`; `MidiStop` sets `m_running = false`; `MidiClock` sets `m_clockTick = true`. In `SetNonagonInputs`, build `ExternalClockSync::Input`, process it each sample, and clear `m_clockTick` after consumption. In internal mode keep the current tempo parameter; in external mode always use the synchronizer-owned effective frequency.

Configure `MidiToMessageIn::x_realtimeRouteId` as `RouteType::Realtime` in both wrapper message buses and forward any realtime `MessageIn` to `m_internal->Apply(msg)` before route-specific switches.

- [ ] **Step 4: Run the system tests and focused realtime tests**

Run: `cmake --build private/build --target tests && private/build/tests --test-case="*MIDI clock sync*,*Realtime*"`

Expected after implementation: all focused integration and realtime tests pass.

## Task 4: JUCE MIDI Input and Inbound Scheduling

**Files:**
- Modify: `JUCE/SmartGridOne/Source/MidiHandlers.hpp`
- Modify: `private/src/MessageInBus.hpp`
- Test: `private/test/unit/midi_realtime.cpp` or a new JUCE-free helper test if a helper header is introduced.

- [ ] **Step 1: Add tests or a small helper for fixed latency timestamp calculation**

Add a JUCE-free helper constant/function near the MIDI primitives:

```cpp
static constexpr size_t x_realtimeLatencyUs = 20000;

static size_t WithRealtimeLatency(size_t timestamp)
{
    return timestamp + x_realtimeLatencyUs;
}
```

Test that source timestamp `1000000` becomes `1020000`, and timestamp `0` becomes `20000`.

- [ ] **Step 2: Implement JUCE MIDI realtime forwarding**

In `MidiInputHandler::handleIncomingMidiMessage`, preserve the existing 3-byte forwarding. Add a one-byte branch for `BasicMidi::IsSupportedRealtimeStatus(data[0])` and forward `BasicMidi(timestampUs, MidiToMessageIn::x_realtimeRouteId, data[0], 0, 0)`. Drop SysEx and unsupported realtime statuses.

- [ ] **Step 3: Implement inbound scheduling**

In `MessageInBus::Push(MessageIn)`, set the queued `MessageIn::m_timestamp` to `timestamp + 20000` microseconds. Have `MessageInBus::Push(BasicMidi)` convert the MIDI message to `MessageIn` and delegate to the same push path. Leave outgoing `MidiSender::ProcessMessagesOut` behavior unchanged.

- [ ] **Step 4: Run build-focused checks**

Run: `cmake --build private/build --target tests && private/build/tests --test-case="*Realtime*"`

Run: `cmake --build JUCE/SmartGridOne/build`

Expected after implementation: core tests pass and the JUCE app target compiles.

## Task 5: Configuration UI and Persistence

**Files:**
- Modify: `JUCE/SmartGridOne/Source/Configuration.hpp`
- Modify: `JUCE/SmartGridOne/Source/ConfigPage.hpp`
- Modify: `JUCE/SmartGridOne/Source/MainComponent.h`
- Modify: `JUCE/SmartGridOne/Source/MainComponent.cpp`
- Modify: `JUCE/SmartGridOne/Source/NonagonWrapper.hpp`

- [ ] **Step 1: Add clock mode config field**

Add `bool m_externalClock;` to `Configuration`, defaulting false. Add `NonagonWrapper::SetExternalClock(bool)` and `NonagonWrapper::IsExternalClock()` forwarding to `TheNonagonSquiggleBoyInternal`.

- [ ] **Step 2: Persist JSON safely**

In save config JSON, write a stable boolean key such as `"external_clock"`. In load, treat missing key as false and call `m_nonagon.SetExternalClock(m_configuration.m_externalClock)` after reading. Do not disturb audio device and MIDI controller persistence.

- [ ] **Step 3: Add ConfigPage toggle**

Add a `juce::ToggleButton m_externalClockCheckbox` with text `External MIDI Clock`. Initialize it from `m_configuration->m_externalClock`, update `m_nonagon->SetExternalClock(...)` on click, and include it in layout without changing existing audio/MIDI rows.

- [ ] **Step 4: Verify build**

Run: `cmake --build JUCE/SmartGridOne/build`

Expected after implementation: JUCE target compiles.

## Task 6: Theory of Time External Sync Loop Selector

**Files:**
- Modify: `private/src/ForEachSmartGridOneParam.hpp`
- Modify: `private/src/TheNonagonSquiggleBoy.hpp`
- Test: `private/test/unit/midi_realtime.cpp` or a new `private/test/unit/external_clock_loop_selector.cpp`

- [ ] **Step 1: Add parameter metadata test**

Write a test that asserts the new parameter has six switch values and that `TheNonagonSquiggleBoyInternal::ExternalClockLoopIndexFromSwitch(0)` equals `TheoryOfTimeBase::x_masterLoop`, while switch value `3` equals `TheoryOfTimeBase::x_numLoops - 3 - 1`.

- [ ] **Step 2: Add the parameter**

Add a new switch-valued Theory of Time parameter to `ForEachSmartGridOneParam.hpp`, following the `SampleLoopIndex` pattern with six switch positions. Suggested public name: `ExternalClockLoop`, short label `ECLK`, group `TheoryOfTime`, switch values `6`.

- [ ] **Step 3: Wire the parameter into synchronizer input**

In `TheNonagonSquiggleBoyInternal`, read `m_squiggleBoy.m_encoders.GetSwitchVal(Param::ExternalClockLoop)` and map it with `TheoryOfTimeBase::x_numLoops - switchVal - 1` before assigning `ExternalClockSync::Input::m_loopIndex`.

- [ ] **Step 4: Run focused tests**

Run: `cmake --build private/build --target tests && private/build/tests --test-case="*External clock loop*"`

Expected after implementation: parameter and mapping tests pass.

## Task 7: Verification and OpenSpec Sync

**Files:**
- Modify: `openspec/changes/add-midi-clock-sync/tasks.md`

- [ ] **Step 1: Run focused tests**

Run: `cmake --build private/build --target tests && private/build/tests --test-case="*Realtime*,*ExternalClockSync*,*MIDI clock sync*,*External clock loop*"`

Expected: all focused tests pass.

- [ ] **Step 2: Run full private test suite**

Run: `cmake --build private/build --target tests && private/build/tests`

Expected: all doctest tests pass.

- [ ] **Step 3: Run JUCE build**

Run: `cmake --build JUCE/SmartGridOne/build`

Expected: JUCE app target compiles.

- [ ] **Step 4: Check OpenSpec status**

Run: `openspec status --change add-midi-clock-sync`

Expected: planning artifacts remain complete and apply-ready.

- [ ] **Step 5: Mark OpenSpec tasks complete**

Update `openspec/changes/add-midi-clock-sync/tasks.md` by checking off only the tasks corresponding to implemented and verified behavior.

## Self-Review Notes

- Spec coverage: realtime input, output latency, synchronizer ownership, per-sample tick, start/continue/stop, external tempo handoff, config toggle/persistence, and loop selector are all mapped to tasks.
- Placeholder scan: no task contains unresolved implementation placeholders; every task names files and expected behavior.
- Type consistency: planned public names are `BasicMidi::x_statusTransportContinue`, `MessageIn::Mode::MidiContinue`, `MidiToMessageIn::x_realtimeRouteId`, `TheNonagonSquiggleBoyInternal::ClockMode`, and `Param::ExternalClockLoop`.
