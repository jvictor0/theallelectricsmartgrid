## Context

The current system has outgoing MIDI clock support but no reliable incoming MIDI clock path. `TheoryOfTime::Process` advances from `Input::m_freq`, `Phasor2Tick` emits outgoing clock messages into `MessageOutBuffer`, and `MidiSender::ProcessMessagesOut` converts those messages to realtime MIDI bytes. Incoming JUCE MIDI currently forwards only 3-byte messages, so one-byte realtime messages such as MIDI clock, start, and stop are dropped before they can affect the sequencer.

Transport run state lives in `TheNonagonSquiggleBoyInternal::m_running`, the green run/play cell toggles that same bool, and `TheNonagonSquiggleBoyInternal::SetNonagonInputs` currently writes tempo from the encoder parameter into `m_theoryOfTimeInput.m_freq`. That makes the SquiggleBoy integration the right owner for external clock mode and synchronization, because it already owns the global running state, bridges controller input to the Nonagon, and sets all Nonagon inputs each sample.

The user noted an uncommitted external clock sync file with the desired synchronization math. It is present in the main worktree at `/Users/joyo/theallelectricsmartgrid/private/src/ExternalClockSync.hpp` and should be copied or ported from there as the implementation base. If that file is not available during implementation, the correct next step is to ask for it or for the intended worktree, not to replace the math with a fresh approximation.

## Goals / Non-Goals

**Goals:**
- Follow external MIDI clock by adjusting Theory of Time tempo until the internal phase returns to phase with incoming ticks.
- Preserve sample-accurate timing by timestamping incoming realtime `BasicMidi` events about 20 ms into the future when they enter the existing `MessageInBus`.
- Keep all clock synchronization ownership inside `TheNonagonSquiggleBoyInternal` and the clock synchronizer helper.
- Let MIDI start/continue/stop update the same running state as the green run/play cell in internal clock mode, and drive external clock mode through synchronizer stopped/armed/running state.
- Expose internal/external clock mode on the JUCE config page and a six-position loop selector switch parameter on the Theory of Time page.
- Add focused tests before and during implementation.

**Non-Goals:**
- Reintroduce `TheoryOfTime::ClockMode`, old PLL hooks, or generic external phasor input inside Theory of Time.
- Redesign the controller routing architecture, message bus, encoder system, or patch JSON format beyond the new fields/parameter.
- Add DAW host sync, MIDI song position pointer, tempo map support, or multi-source clock arbitration.
- Guarantee correctness without the external clock sync math file the user called out.

## Decisions

1. Own external clock mode and synchronization in `TheNonagonSquiggleBoyInternal`.

   `TheNonagonSquiggleBoyInternal` owns the global run bool, reads encoder tempo, writes Nonagon inputs, and processes each audio sample. Placing `m_clockSynchronizer`, `m_clockMode`, and per-sample clock tick state there keeps synchronization logic out of the Nonagon and Theory of Time. The Nonagon continues to receive an effective `m_freq` and `m_running`.

   Alternative considered: add external clock fields to `TheoryOfTime::Input` or restore `ClockMode`. That would contradict the recent clock cleanup and leak MIDI transport policy into the timebase.

2. Represent MIDI realtime input as explicit `MessageIn` events routed through the existing message bus.

   `MessageInBus` already provides timestamp-gated delivery on the audio thread. Add realtime message modes or an equivalent typed representation for clock/start/continue/stop and convert one-byte MIDI messages into those events with the MIDI input timestamp and a fixed realtime route. Treat continue (`0xFB`) like start. The route should not depend on Launchpad or Wrld.Bldr controller channels.

   Alternative considered: call synchronizer methods directly from `MidiInputHandler`. That would cross from JUCE callback threads into DSP state and bypass the bus ordering rules.

3. Use a fixed future latency at inbound `BasicMidi` to `MessageIn` enqueue time.

   When `MessageInBus::Push(BasicMidi)` converts incoming MIDI into `MessageIn`, calculate `visibleTimestamp = midiTimestamp + x_midiClockLatencyUs` with a default around 20,000 microseconds, then enqueue the `MessageIn`. The audio thread should see clock/start/continue/stop only when processing reaches that adjusted timestamp. Outgoing `MessageOut` to `MidiSender` behavior stays as-is.

   Alternative considered: apply latency in `MidiSender`. That affects outgoing clock and does not solve the incoming clock tick visibility problem the synchronizer depends on.

4. Keep Theory of Time frequency-driven.

   In internal mode, `SetNonagonInputs` continues to write tempo from the encoder parameter. In external mode, it writes the synchronizer's current frequency estimate. `TheoryOfTime::Process` still advances from `input.m_freq`, so loop topology, phase modulation, stopped-state maintenance, and outgoing clock generation remain localized.

   Alternative considered: make Theory of Time consume tick booleans directly. That spreads external MIDI policy into the timebase and makes phase correction harder to test independently.

5. Preserve the existing external clock math as the synchronizer core.

   The synchronizer helper should expose a small API such as reset/start/stop, per-sample tick input, effective frequency, loop selection, and optional debug phase error. Its implementation should be based on `/Users/joyo/theallelectricsmartgrid/private/src/ExternalClockSync.hpp`, with tests wrapped around the public behavior before integration changes proceed.

   Alternative considered: write a new PLL/PI controller from scratch. The user explicitly said the existing C++ math best explains the intended synchronization, so a clean rewrite is the wrong starting point.

## Risks / Trade-offs

- Missing external sync file at `/Users/joyo/theallelectricsmartgrid/private/src/ExternalClockSync.hpp` -> Stop implementation and ask for the file or the correct worktree path before replacing the synchronization math.
- Timestamp origin mismatch between JUCE MIDI timestamps and `SampleTimer::GetAbsTimeUs()` -> Normalize incoming realtime timestamps before enqueueing, and test that a future-stamped realtime message is applied on the expected audio sample.
- Missing or double latency on incoming MIDI -> Apply the fixed latency exactly once in `MessageInBus::Push(BasicMidi)` and test that a converted realtime event is held until the adjusted timestamp.
- Queue head blocking from future-stamped messages -> Preserve ordered delivery by timestamp per route, or ensure realtime injected messages use monotonic timestamps; add tests around future event holdback.
- External start/continue/stop races with UI run cell -> Keep external transport policy in the synchronizer, derive `m_running` from synchronizer state in external mode, and test that start/continue arm until the next clock tick while stop returns to stopped.
- External clock jitter -> Keep smoothing/phase correction inside the synchronizer and test steady tick tempo, recovery after phase error, and reasonable behavior after missing/extra ticks.

## Migration Plan

1. Add tests for the synchronizer helper from the existing sync math.
2. Add JUCE-free tests for realtime MIDI byte conversion, `MessageInBus` delayed delivery, running-state effects, and inbound timestamp latency where possible.
3. Add integration tests around `TheNonagonSquiggleBoyInternal` to verify external mode tempo handoff and per-sample clock tick consumption.
4. Add config UI and JSON persistence for clock mode, then verify older config JSON defaults to internal clock.
5. Add the Theory of Time loop selector parameter and verify encoder serialization/readback.

Rollback is straightforward: keep the config default as internal clock, and if external sync is unstable the external-mode UI can be hidden or ignored without altering internal tempo behavior.

## Resolved Notes

- The Theory of Time loop selector switch should match the sample-source loop switch pattern: six switch values, with switch value 0 mapping to `TheoryOfTimeBase::x_masterLoop` via `TheoryOfTimeBase::x_numLoops - switchVal - 1`, and the remaining values selecting the other Theory of Time loops.
