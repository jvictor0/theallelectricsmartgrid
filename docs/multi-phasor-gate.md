# Multi-Phasor Gate and trigger decision

The **Multi-Phasor Gate** (`MultiPhasorGateInternal` in `private/src/MultiPhasorGate.hpp`) decides, per voice, whether to **emit a trigger** -- the gate that starts a note and drives the [AHD](glossary.md#ahd) envelope and the rest of the voice chain. The Nonagon feeds it from the [Theory of Time](theory-of-time.md), [LameJuis](lamejuis.md), and index arp, then the Multi-Phasor Gate combines phasor-based gate timing with **trigger logic** so that only one condition (or the right combination) actually fires a note.

---

## Overview

- There are **9 voices** (3 trios of 3). Each voice has a **gate** (`m_gate[i]`) and an **AHD control** (`m_ahdControl[i]`). The downstream synth uses **m_ahdControl[i].m_trig** as "this voice should start a note this frame."
- The trigger decision is split into:
  1. **Trigger logic** (`NonagonTrigLogic`) -- Builds a boolean **m_trigs[i]** and **m_newTrigCanStart[i]** per voice from LameJuis triggers, index-arp "triggered," mutes, and interrupt rules.
  2. **Multi-Phasor Gate `Process`** -- Combines those with phasor-based timing and mute to set **m_ahdControl[i].m_trig** and **m_gate[i]**, and to turn the gate off after half a voice-period.

---

## 1. Inputs to the trigger logic

The Nonagon sets these before calling `SetInput` (in `SetMultiPhasorGateInputs`):

- **m_running** -- From `input.m_running` (transport running). Must be true for any new trigger to be allowed.
- **m_pitchChanged[i]** -- True when the Theory of Time had a change this frame **and** LameJuis output for that voice raised a "pitch changed" trigger (`m_lameJuis.m_lanes[trio].m_trigger[voiceInTrio]`).
- **m_subTrigger[i]** -- True when the index arp for that voice **m_triggered** this frame **and** the Theory of Time had a change.
- **m_earlyMuted[i]** -- Reserved for future mute logic; currently always false. Used to block triggers and "new trig can start" when true.
- **m_mute[i]** -- Per-voice mute from the UI (stored in trig logic state).
- **m_trigOnPitchChanged[trioId]**, **m_trigOnSubTrigger[trioId]** -- Per-trio: whether to fire on "pitch changed" and/or on "sub-trigger" (index arp triggered). Defaults: pitch-changed on, sub-trigger off.
- **m_interrupt[trioId][jTrioId]** -- If true, a trigger on a voice in trio *j* can **interrupt** (cancel) a trigger on a voice in trio *i* when voice *i* has a higher index than voice *j*.
- **m_unisonMaster[trioId]** -- For unison: which voice index in that trio is the "master" for trigger/pitch; -1 means no unison (each voice independent).

---

## 2. How m_trigs[i] and m_newTrigCanStart[i] are computed

`NonagonTrigLogic::SetInput` fills the Multi-Phasor Gate input for all 9 voices.

For each voice *i*:

- **Trio and check index** -- `trioId = i / 3`. The "logical" voice used for trigger sources is `ixToCheck = (m_unisonMaster[trioId] == -1) ? i : m_unisonMaster[trioId]` (unison master or self).
- **Raw trigger**:
  - `input.m_trigs[i] = (m_trigOnSubTrigger[trioId] && m_subTrigger[ixToCheck]) || (m_trigOnPitchChanged[trioId] && m_pitchChanged[ixToCheck])`
  - So a trigger is requested if the trio is set to trig on sub-trigger and the (master) voice's index arp triggered, **or** if the trio is set to trig on pitch-changed and the (master) voice's LameJuis pitch changed.
- **Early-mute** -- `input.m_trigs[i] &= !m_earlyMuted[ixToCheck]`. Reserved for future mute logic; currently always false. When true, blocks triggers.
- **New-trig-can-start** -- `input.m_newTrigCanStart[i] = m_running && !m_earlyMuted[ixToCheck]`. A new note is allowed only when the transport is running and the voice is not early-muted.
- **Mute** -- `input.m_mute[i] = m_mute[i]` (per-voice mute).
- **Interrupt** -- For each *j* < *i*: if `m_interrupt[trioId][jTrioId]` is true and voice *j* has a trigger this frame and is not muted (`input.m_trigs[j] && !input.m_mute[j]`), then set `input.m_trigs[i] = false`. So a lower-index voice with a trigger can cancel the trigger for voice *i* when interrupt is enabled between their trios.

Result: **m_trigs[i]** is true only when the chosen trigger source (pitch-changed or sub-trigger) fired, the note is not early-muted, and no lower-index voice "interrupts" it. **m_newTrigCanStart[i]** is true when running and not early-muted.

---

## 3. How the Multi-Phasor Gate turns that into m_trig and m_gate

`MultiPhasorGateInternal::Process` runs once per frame with that input.

**Timing inputs** (set by the Nonagon after `SetInput`):

- `m_theoryOfTime` supplies absolute modulated global phase at slot 0.
- `m_globalPeriodSamples` is the current global period in samples.
- `m_voiceCycleRatio[i]` is the positive signed 64-bit LCM of the selected clock loop ratio and all read lens loop ratios, starting at one when no clock is selected. Neither contribution is doubled. It determines the voice gate period.
- `m_phaseRatio` is the envelope source/global ratio, currently taken from loop 0. It is distinct from the voice gate ratio.

A trigger is emitted when requested, allowed, and unmuted. Accepted trigger bounds capture global phase, voice cycle ratio, and global period. The gate closes when `abs(globalPhase - startGlobalPhase) * capturedVoiceCycleRatio >= 0.5`. Existing bounds do not adopt later topology changes. A retrigger captures fresh bounds. This 0.5 note-duration cutoff remains intentional; Theory of Time rhythm steps span complete loop cycles and do not change that cutoff or the clock frequency.

The envelope period sent in `AHDControl` is the captured global period divided by the captured voice cycle ratio. AHD captures that period and the source phase ratio when it receives the trigger and evaluates its own elapsed position from global phase. There is no elapsed-sample relay or circle-distance tracker.

Mute and interrupt decisions retain their existing behavior. Muted accepted requests still establish internal bounds; they do not emit an envelope trigger. Once the half-period ends, a muted voice or one that cannot start a new trigger receives release. `m_anyGate` reports whether any voice gate remains high.

`m_gate` controls note-off and the UI. Envelopes receive `AHDControl` (`m_trig`, `m_release`, `m_phaseRatio`, `m_envelopePeriodSamples`) rather than following the boolean gate directly.

---

## Related

- [Theory of Time](theory-of-time.md) -- supplies the global phase and "any change."
- [LameJuis](lamejuis.md) -- supplies pitch and "pitch changed" trigger.
- [Glossary](glossary.md) -- **Multi-Phasor Gate**, **NonagonTrigLogic**, **AHD**, **PhasorBounds**.
- [Documentation index](index/README.md#major-components).
