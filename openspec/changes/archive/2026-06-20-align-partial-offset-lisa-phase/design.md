## Context

Voice panning is driven by `SquiggleBoy::m_panPhase`, a shared `PhaseUtils::SimpleOsc` advanced once per audio sample before voice processing. Each voice receives that same phase as its Lissajous input, then adds its static and user phase shifts.

The Partial Machine already has `SynthesisContext::Input::m_azimuthOffset`, and `GetAzimuth` adds it to the frequency-derived azimuth before wrapping. Current SquiggleBoy wiring fills the Partial Machine's frequency-dependent lane parameters but leaves the azimuth offset at its default value, so the spectral orbit does not follow the Lissajous pan phase.

## Goals / Non-Goals

**Goals:**

- Use the same shared pan phase that drives Lissajous panning as the Partial Machine azimuth offset.
- Keep the Partial Machine's frequency-to-azimuth, radius, unison, pitch, and UI transfer-function math unchanged.
- Keep the implementation small and realtime-safe.

**Non-Goals:**

- Add a new encoder parameter for Partial Machine offset.
- Change the pan oscillator rate, per-voice static offsets, or Lissajous figure behavior.
- Change preset serialization or patch compatibility.

## Decisions

Set `m_partialMachineState.m_synthesisContextInput.m_azimuthOffset` or the equivalent Partial Machine input value from `m_panPhase.m_phase` during SquiggleBoy state wiring.

Rationale: the pan phase is already advanced once per sample and is the canonical driver for Lissajous panning. Reusing the current value avoids adding another oscillator, parameter, or phase accumulator.

Alternative considered: add an offset field to `PartialMachine::InputSetter::Input`. That would work, but it introduces another pass-through member for a value already available at the call site and makes the change larger than necessary.

## Risks / Trade-offs

- Phase timing could be off by one sample if the offset is assigned before `m_panPhase.Process()`. Mitigation: assign after the pan phase is advanced, matching the value delivered to voice pan inputs for the current sample.
- Tests could become brittle if they depend on private timing details. Mitigation: test observable input/UI-state offset wiring rather than the full spectral output.
