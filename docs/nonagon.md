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

For every micro block, the Theory of Time computes all samples in that block **plus** the first sample of the next block. Slot 8 holds the first sample of the next micro block (computed in the current block). At the start of each micro block, `RolloverMicroblockBuffer` copies slot 8 into slot 0—the first sample of this block was computed in the previous block. The Nonagon then runs at that sample (sequencer logic, outputs), and `Process(j)` for j=1..8 computes the rest of this block and the first sample of the next. This ensures that interpolation anywhere inside a micro block always has accurate boundary samples.

During a control frame, `Process` executes the following steps:

1. **Rollover and Theory of Time (sample 0)**:
   - `SetTheoryOfTimeInput(input)` prepares the global clock inputs.
   - `m_theoryOfTime.RolloverMicroblockBuffer()` copies slot 8 into slot 0 (sample 0 of this block was computed in the previous block).
   - If the global loop crosses an unmodulated cycle boundary (`CrossedCycleBoundary`), it records a start index in the note writer.

2. **Index Arp and LameJuis (Only on change)**:
   - If the Theory of Time reported position motion, startup, stop, or an accepted topology edit in the micro block (`m_theoryOfTime.AnyChangeInMicroBlock()`), the sequencer state must be updated.
   - `SetIndexArpInputs(input)` samples signed `m_totalIndex` from `GetGateStepIndex(clockLoop, 0, resetLoop)` when `AnyTick(clockLoop)` is true. Read flags follow ticks of lens dimensions, even when neighboring loop rhythm values are equal. A step spans a complete loop cycle.
   - `m_indexArp.Process(input.m_arpInput)` runs the arpeggiators to find the point in the range.
   - `SetLameJuisInput(input)` feeds the Theory of Time gates and the index arp outputs (as `m_choiceArg` for the chosen strategy) into LameJuis.
   - `m_lameJuis.Process(input.m_lameJuisInput)` evaluates the logic matrix and sheaf to produce pitches and extra timbres.

3. **Multi-Phasor Gate (Only when running)**:
   - If the transport is running (`m_theoryOfTime.m_samples[0].m_running`), `SetMultiPhasorGateInputs(input)` evaluates trigger logic (pitch-changed, sub-trigger, mutes, interrupts).
   - `m_multiPhasorGate.Process(input.m_multiPhasorGateInput)` determines which voices emit a trigger and tracks their gate lengths based on the absolute modulated global phase.

4. **Outputs and Note Writer**:
   - `SetOutputs(input)` gathers the results.
   - For each voice, if a trigger was emitted (`m_ahdControl[i].m_trig`), it:
     - Sets `m_output.m_gate[i] = true`.
     - Applies the trio octave switch to the LameJuis pitch (`Octavize`).
     - Records a note-on event (`m_noteWriter.RecordNote`).
   - For each triggering voice, it latches the extra timbre modulators from the LameJuis section into `m_output.m_extraTimbre[i][j]` (via `result.m_section.Timbre(j)`). These are captured per voice at trigger time and are not slewed.
   - If a voice's gate turns off (`!m_multiPhasorGate.m_gate[i]`), it clears `m_output.m_gate[i]` and records a note-off (`m_noteWriter.RecordNoteEnd`).

5. **Theory of Time (samples 1–8)**:
   - `m_theoryOfTime.Process(j, input.m_theoryOfTimeInput)` is called for `j = 1` through `8` to compute the rest of this micro block (samples 1–7) and the first sample of the next block (slot 8).
   - When the user transport is stopped and no voice gate keeps the timebase alive, those calls take the stopped branch of `TheoryOfTimeBase::Process`, keeping accepted topology and periods current and clearing phases, positions, gates, and crossing flags. Multi-Phasor Gate and LameJuis are reset before outputs are set.

---

## 3. Voice timing and loop rhythms

For each voice, `SetMultiPhasorGateInputs` takes the LCM of its selected clock loop's cycle ratio and every read (non-co-muted) lens loop's ratio. It starts at one when no clock is selected. Neither the clock contribution nor the read contribution is doubled. For clock ratio 3 and read ratio 4, `m_voiceCycleRatio` is 12.

The captured envelope period is `globalPeriodSamples / voiceCycleRatio`. Multi-Phasor Gate still ends a note gate at half of that voice cycle; this note duration is separate from the full-cycle Theory of Time rhythm step. The change does not alter clock frequency or the gate's 0.5 cutoff. AHD's source/global ratio, supplied from loop 0, is also separate from the voice ratio.

Each of the six input bits now has an editable loop rhythm, defaulting to a full cycle on and a full cycle off. Rhythm edits hold until that loop's next modulated tick and do not create synthetic sequencer changes. The left rhythm and right ancestor-reset pages are available in Wrld.Bldr's TheoryOfTimeRhythm mode; see [Controller Integrations](ui-controller-integrations.md).

## 4. Trio Octave Switches

The Nonagon applies an octave shift per trio via `TrioOctaveSwitches`. The raw pitch from LameJuis is passed through `Octavize(preOctave, i)`, which adds or subtracts octaves based on the UI state before being sent to the DSP.

---

## 5. Note Writer

The `NonagonNoteWriter` acts as a bridge between the core sequencer logic and the outside world (like MIDI out or UI piano rolls). It records `EventData` containing the voice index, pitch, wrapped unmodulated global phase position, and extra timbres. Unmodulated global cycle crossings split held notes at the display-loop boundary.

---

## Related

- [Theory of Time](theory-of-time.md)
- [LameJuis](lamejuis.md)
- [Multi-Phasor Gate](multi-phasor-gate.md)
- [Glossary](glossary.md)
