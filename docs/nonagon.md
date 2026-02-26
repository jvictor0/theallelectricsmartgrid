# The Nonagon (Sequencer)

The **Nonagon** (`TheNonagonInternal` in `private/src/TheNonagon.hpp`) is the core sequencer logic class. It wires together the [Theory of Time](theory-of-time.md), [LameJuis](lamejuis.md), index arp, and [Multi-Phasor Gate](multi-phasor-gate.md) into a single unified polyphonic sequence generator.

---

## 1. Overview

The Nonagon is responsible for:
- Aggregating inputs for all subcomponents into `TheNonagonInternal::Input`.
- Running the subcomponents in the correct order during `Process()`.
- Distributing outputs from one subcomponent to the inputs of another.
- Emitting the final `TheNonagonInternal::Output` (which includes `m_gate`, `m_voltPerOct`, and `m_extraTimbre`).
- Recording note-on and note-off events into the `NonagonNoteWriter` for MIDI/UI consumption.

It manages **9 voices** arranged in **3 trios**.

---

## 2. Process Flow

During a control frame, `TheNonagonInternal::Process(Input& input)` executes the following steps:

1. **Theory of Time**:
   - `SetTheoryOfTimeInput(input)` prepares the global clock inputs.
   - `m_theoryOfTime.Process(input.m_theoryOfTimeInput)` runs the clock.
   - If the master loop wraps (`m_topIndependent`), it records a start index in the note writer.

2. **Index Arp and LameJuis (Only on change)**:
   - If the Theory of Time had any integer position change (`m_theoryOfTime.m_anyChange`), the sequencer state must be updated.
   - `SetIndexArpInputs(input)` calculates `m_totalIndex` (monodromy) and clock/read gates.
   - `m_indexArp.Process(input.m_arpInput)` runs the arpeggiators to find the point in the range.
   - `SetLameJuisInput(input)` feeds the Theory of Time gates and the index arp outputs (as percentiles or quantization targets) into LameJuis.
   - `m_lameJuis.Process(input.m_lameJuisInput)` evaluates the logic matrix and sheaf to produce pitches and extra timbres.

3. **Multi-Phasor Gate (Only when running)**:
   - If the transport is running (`m_theoryOfTime.m_running`), `SetMultiPhasorGateInputs(input)` evaluates trigger logic (pitch-changed, sub-trigger, mutes, interrupts).
   - `m_multiPhasorGate.Process(input.m_multiPhasorGateInput)` determines which voices emit a trigger and tracks their gate lengths based on the master phasor.

4. **Outputs and Note Writer**:
   - `SetOutputs(input)` gathers the results.
   - For each voice, if a trigger was emitted (`m_ahdControl[i].m_trig`), it:
     - Sets `m_output.m_gate[i] = true`.
     - Applies the trio octave switch to the LameJuis pitch (`Octavize`).
     - Records a note-on event (`m_noteWriter.RecordNote`).
   - If a voice's gate turns off (`!m_multiPhasorGate.m_gate[i]`), it clears `m_output.m_gate[i]` and records a note-off (`m_noteWriter.RecordNoteEnd`).
   - It slews the 3 extra timbre modulators from LameJuis (`m_extraTimbreSlew`) to prevent clicks.
   - It outputs the Theory of Time phasors (`m_totPhasors`) for downstream LFOs.

---

## 3. Trio Octave Switches

The Nonagon applies an octave shift per trio via `TrioOctaveSwitches`. The raw pitch from LameJuis is passed through `Octavize(preOctave, i)`, which adds or subtracts octaves based on the UI state before being sent to the DSP.

---

## 4. Note Writer

The `NonagonNoteWriter` acts as a bridge between the core sequencer logic and the outside world (like MIDI out or UI piano rolls). It records `EventData` containing the voice index, pitch, start position, and extra timbres.

---

## Related

- [Theory of Time](theory-of-time.md)
- [LameJuis](lamejuis.md)
- [Multi-Phasor Gate](multi-phasor-gate.md)
- [Glossary](glossary.md)
