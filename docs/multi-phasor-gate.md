# Multi-Phasor Gate and trigger decision

The **Multi-Phasor Gate** (`MultiPhasorGateInternal` in `private/src/MultiPhasorGate.hpp`) decides, per voice, whether to **emit a trigger** — the gate that starts a note and drives the [AHD](glossary.md#ahd) envelope and the rest of the voice chain. The Nonagon feeds it from the [Theory of Time](theory-of-time.md), [LameJuis](lamejuis.md), and index arp, then the Multi-Phasor Gate combines phasor-based gate timing with **trigger logic** so that only one condition (or the right combination) actually fires a note.

---

## Overview

- There are **9 voices** (3 trios of 3). Each voice has a **gate** (`m_gate[i]`) and an **AHD control** (`m_ahdControl[i]`). The downstream synth uses **m_ahdControl[i].m_trig** as “this voice should start a note this frame.”
- The trigger decision is split into:
  1. **Trigger logic** (`NonagonTrigLogic`) — Builds a boolean **m_trigs[i]** and **m_newTrigCanStart[i]** per voice from LameJuis triggers, index-arp “triggered,” mutes, and interrupt rules.
  2. **Multi-Phasor Gate `Process`** — Combines those with phasor-based timing and mute to set **m_ahdControl[i].m_trig** and **m_gate[i]**, and to turn the gate off after half a voice-period.

---

## 1. Inputs to the trigger logic

The Nonagon sets these before calling `SetInput` (in `SetMultiPhasorGateInputs`):

- **m_running** — From `input.m_running` (transport running). Must be true for any new trigger to be allowed.
- **m_pitchChanged[i]** — True when the Theory of Time had a change this frame **and** LameJuis output for that voice raised a “pitch changed” trigger (`m_lameJuis.m_outputs[trio].m_trigger[voiceInTrio]`).
- **m_subTrigger[i]** — True when the index arp for that voice **m_triggered** this frame **and** the Theory of Time had a change.
- **m_earlyMuted[i]** — Copy of LameJuis “this pitch is muted” for that voice (`m_pitch[i].m_isMuted`). Used to block triggers and “new trig can start.”
- **m_mute[i]** — Per-voice mute from the UI (stored in trig logic state).
- **m_trigOnPitchChanged[trioId]**, **m_trigOnSubTrigger[trioId]** — Per-trio: whether to fire on “pitch changed” and/or on “sub-trigger” (index arp triggered). Defaults: pitch-changed on, sub-trigger off.
- **m_interrupt[trioId][jTrioId]** — If true, a trigger on a voice in trio *j* can **interrupt** (cancel) a trigger on a voice in trio *i* when voice *i* has a higher index than voice *j*.
- **m_unisonMaster[trioId]** — For unison: which voice index in that trio is the “master” for trigger/pitch; -1 means no unison (each voice independent).

---

## 2. How m_trigs[i] and m_newTrigCanStart[i] are computed

`NonagonTrigLogic::SetInput` fills the Multi-Phasor Gate input for all 9 voices.

For each voice *i*:

- **Trio and check index** — `trioId = i / 3`. The “logical” voice used for trigger sources is `ixToCheck = (m_unisonMaster[trioId] == -1) ? i : m_unisonMaster[trioId]` (unison master or self).
- **Raw trigger**:
  - `input.m_trigs[i] = (m_trigOnSubTrigger[trioId] && m_subTrigger[ixToCheck]) || (m_trigOnPitchChanged[trioId] && m_pitchChanged[ixToCheck])`
  - So a trigger is requested if the trio is set to trig on sub-trigger and the (master) voice’s index arp triggered, **or** if the trio is set to trig on pitch-changed and the (master) voice’s LameJuis pitch changed.
- **Early-mute** — `input.m_trigs[i] &= !m_earlyMuted[ixToCheck]`. If the chosen note is muted in the sheaf, no trigger.
- **New-trig-can-start** — `input.m_newTrigCanStart[i] = m_running && !m_earlyMuted[ixToCheck]`. A new note is allowed only when the transport is running and the voice is not early-muted.
- **Mute** — `input.m_mute[i] = m_mute[i]` (per-voice mute).
- **Interrupt** — For each *j* &lt; *i*: if `m_interrupt[trioId][jTrioId]` is true and voice *j* has a trigger this frame and is not muted (`input.m_trigs[j] && !input.m_mute[j]`), then set `input.m_trigs[i] = false`. So a lower-index voice with a trigger can cancel the trigger for voice *i* when interrupt is enabled between their trios.

Result: **m_trigs[i]** is true only when the chosen trigger source (pitch-changed or sub-trigger) fired, the note is not early-muted, and no lower-index voice “interrupts” it. **m_newTrigCanStart[i]** is true when running and not early-muted.

---

## 3. How the Multi-Phasor Gate turns that into m_trig and m_gate

`MultiPhasorGateInternal::Process` runs once per frame with that input.

**Phasor inputs** (set by the Nonagon after `SetInput`):

- **m_phasor** — Master loop phasor from the Theory of Time.
- **m_masterLoopSamples** — Master loop length in samples.
- **m_phasorDenominator[i]** — For each voice, an integer such that the voice’s “period” in master-loop units is `1 / m_phasorDenominator[i]` (derived from the voice’s clock and lens so that the gate length matches the voice’s logical step).

**Per voice *i*:**

1. **Emit trigger** —  
   `m_ahdControl[i].m_trig = input.m_trigs[i] && input.m_newTrigCanStart[i] && !input.m_mute[i]`.  
   So the **trigger** (what the rest of the synth sees) is true only when: the trigger logic requested a trig, a new trig is allowed, and the voice is not muted. If that is true, `m_ahdControl[i].m_release = false`.

2. **Start gate and bounds** — If `input.m_trigs[i] && input.m_newTrigCanStart[i]`:
   - If not muted: `m_gate[i] = true` and `m_ahdControl[i].m_samples = 0`.
   - In all cases: `m_preGate[i] = true`, `m_set[i] = true`, and `m_bounds[i].Set(input.m_phasor, input.m_phasorDenominator[i])`. So we record the current phasor and the voice’s denominator for phase tracking.

3. **Envelope timing** — `envelopeTimeSamples = m_masterLoopSamples / input.m_phasorDenominator[i]` (length of one voice “step” in samples). Stored in `m_ahdControl[i].m_envelopeTimeSamples`.

4. **Phase advance and gate off** — If `m_set[i]`:
   - `thisPhase = m_bounds[i].Process(input.m_phasor)`: distance along the circle from the start of this gate to the current phasor, scaled by the voice’s denominator (so 0 → 1 over one voice period).
   - If **thisPhase ≥ 0.5**: gate is turned off (`m_gate[i] = false`, `m_preGate[i] = false`). If in addition `!input.m_newTrigCanStart[i] || input.m_mute[i]`, then `m_ahdControl[i].m_release = true` and `m_set[i] = false`.
   - `m_ahdControl[i].m_samples = thisPhase * envelopeTimeSamples` so the AHD envelope sees the correct elapsed time.

5. **m_anyGate** — If any voice has `m_gate[i]` true, `m_anyGate` is true (used e.g. to keep the Theory of Time “running” while a note is held).

So: **m_ahdControl[i].m_trig** is the single “emit a trigger” flag. The gate **m_gate[i]** is true from the frame the trigger is accepted until the phasor has advanced half a voice period (0.5 in normalized phase), giving a 50% duty cycle per step unless a new trigger restarts the bounds.

**m_gate is not used for envelopes.** Envelope attack/hold/decay are driven by **m_ahdControl** (`m_trig`, `m_samples`, `m_envelopeTimeSamples`, `m_release`); the DSP copies `m_ahdControl` into each voice’s AHD input. **m_gate** is used instead for: (1) **note-off and output** — when `!m_multiPhasorGate.m_gate[i]`, the Nonagon sets `m_output.m_gate[i] = false` and records note end; (2) **UI** — `SetGate(i, m_gate[i])` so the interface can show which voices have gate high. So **m_gate** tracks “note still held” for release timing and display; the envelope sees only **AHDControl**.

---

## 4. Summary

- **Trigger emitted** when: trigger logic says trig (pitch-changed or sub-trigger, per trio) **and** not early-muted **and** not interrupted by a lower-index voice **and** `m_newTrigCanStart` (running, not early-muted) **and** voice not muted.
- **Gate on** when a trigger is accepted; **gate off** when the master phasor has moved 0.5 in the voice’s normalized period (PhasorBounds).
- **AHD** receives `m_trig` (start note), `m_samples`, `m_envelopeTimeSamples`, and `m_release` from the same structure, so envelope and voice are driven by this decision.

---

## Related

- [Theory of Time](theory-of-time.md) — supplies the master phasor and “any change.”
- [LameJuis](lamejuis.md) — supplies pitch, “pitch changed” trigger, and early-mute.
- [Glossary](glossary.md) — **Multi-Phasor Gate**, **NonagonTrigLogic**, **AHD**, **PhasorBounds**.
- [Documentation index](index/README.md#major-components).
