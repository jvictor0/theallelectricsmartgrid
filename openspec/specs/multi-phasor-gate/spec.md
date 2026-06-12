# Multi-Phasor Gate Specification

## Purpose
The Multi-Phasor Gate (`MultiPhasorGateInternal` in `private/src/MultiPhasorGate.hpp`) converts per-voice trigger requests from the sequencer into per-voice trigger and gate signals by tracking the master phasor from the phasor-timebase. Its trigger logic (`NonagonTrigLogic`) combines LameJuis pitch-changed triggers, index-arp sub-triggers, mutes, trio interrupt rules, and unison mastering into trigger requests; the gate stage then times note starts and gate-offs against each voice's normalized phase and publishes envelope timing to the AHD envelopes via `AHDControl` (see ahd-envelopes). The nonagon-sequencer supplies the inputs each control frame.

## Requirements

### Requirement: Per-Voice Trigger Emission
The system SHALL emit a per-voice trigger by setting `m_ahdControl[i].m_trig = m_trigs[i] && m_newTrigCanStart[i] && !m_mute[i]` each time `Process` runs, and SHALL clear `m_ahdControl[i].m_release` on the frame a trigger is emitted. The emitted trigger is the single "start a note" signal consumed downstream: the sequencer latches pitch and records a note event from it, and the DSP voices start their AHD envelopes from it.

#### Scenario: Trigger accepted for an unmuted voice
- **WHEN** `m_trigs[i]` is true, `m_newTrigCanStart[i]` is true, and `m_mute[i]` is false for voice i
- **THEN** `m_ahdControl[i].m_trig` is true for that frame and `m_ahdControl[i].m_release` is set false
- **AND** `m_gate[i]` becomes true and `m_ahdControl[i].m_samples` is reset to 0

#### Scenario: Muted voice still tracks phase but emits no trigger
- **WHEN** `m_trigs[i]` and `m_newTrigCanStart[i]` are true but `m_mute[i]` is true
- **THEN** `m_ahdControl[i].m_trig` is false and `m_gate[i]` stays false
- **AND** `m_preGate[i]` is set true and `m_bounds[i].Set(phasor, m_phasorDenominator[i])` still records the gate start, so phase tracking continues while muted

### Requirement: Trigger Source Selection Per Trio
The trigger logic SHALL request a trigger for voice i (`m_trigs[i]`) when the voice's trio is configured to fire on pitch-changed and the voice's LameJuis pitch-changed flag is set, or when the trio is configured to fire on sub-trigger and the voice's index-arp sub-trigger flag is set. Defaults are `m_trigOnPitchChanged = true` and `m_trigOnSubTrigger = false` per trio. The pitch-changed flag is set only when the phasor-timebase reports a change in the microblock and the LameJuis lane raised its trigger; the sub-trigger flag requires the index arp's `m_triggered` plus the same change condition (see nonagon-sequencer).

#### Scenario: Default configuration fires on pitch change only
- **WHEN** a trio uses default trigger configuration and voice i's LameJuis pitch-changed flag is true while its sub-trigger flag is also true
- **THEN** `m_trigs[i]` is true because pitch-changed firing is enabled
- **AND** disabling `m_trigOnPitchChanged` for that trio with `m_trigOnSubTrigger` still false yields `m_trigs[i]` false even though the sub-trigger flag is set

#### Scenario: Sub-trigger mode fires from the index arp
- **WHEN** a trio has `m_trigOnSubTrigger = true` and voice i's index arp triggered during a microblock with a Theory of Time change
- **THEN** `m_trigs[i]` is true even if the LameJuis pitch did not change

### Requirement: New-Trigger Permission and Early Mute
The trigger logic SHALL allow a new trigger to start (`m_newTrigCanStart[i]`) only when the transport is running and the checked voice is not early-muted, and SHALL clear `m_trigs[i]` when the checked voice is early-muted. `m_earlyMuted` is reserved for future mute logic and is currently always set false by the sequencer.

#### Scenario: Transport stopped blocks new notes
- **WHEN** `m_running` is false and voice i's trigger source fires
- **THEN** `m_newTrigCanStart[i]` is false and `m_ahdControl[i].m_trig` stays false
- **AND** a voice whose gate is already tracking phase reaches gate-off and receives `m_ahdControl[i].m_release = true` because `m_newTrigCanStart[i]` is false at the half-period point

### Requirement: Trio Interrupt Rules
The trigger logic SHALL cancel voice i's trigger request when, for any lower-index voice j with `m_interrupt[trio(i)][trio(j)]` enabled, voice j has a trigger request this frame and is not muted. Voices within the same trio SHALL be exempt from interrupting each other while that trio has a unison master assigned.

#### Scenario: Lower-index voice cancels higher-index voice across trios
- **WHEN** `m_interrupt[1][0]` is true, voice 1 (trio 0) has `m_trigs[1]` true and `m_mute[1]` false, and voice 4 (trio 1) also has a trigger request this frame
- **THEN** `m_trigs[4]` is forced false and voice 4 emits no trigger
- **AND** voice 1's own trigger request is unaffected

#### Scenario: Unison trio members do not interrupt each other
- **WHEN** trio 0 has `m_unisonMaster[0] = 0`, self-interrupt `m_interrupt[0][0]` is enabled, and voices 0 and 1 both carry the master's trigger request
- **THEN** voice 1's trigger is not cancelled by voice 0, because same-trio interrupts are skipped while a unison master is set

### Requirement: Unison Master Per Trio
The trigger logic SHALL, when `m_unisonMaster[trioId]` is not -1, evaluate every voice in that trio against the master voice's trigger sources (pitch-changed, sub-trigger, early-mute) instead of its own, while each voice keeps its own independent `m_mute[i]`. When `m_unisonMaster[trioId]` is -1 each voice uses its own sources. The sequencer also reads pitch from the master voice for all trio members when emitting notes.

#### Scenario: All trio voices trigger from the master
- **WHEN** `m_unisonMaster[0] = 2` and only voice 2's pitch-changed flag is set
- **THEN** `m_trigs[0]`, `m_trigs[1]`, and `m_trigs[2]` are all true
- **AND** if `m_mute[1]` is true, voice 1 emits no trigger while voices 0 and 2 do

### Requirement: Gate Duration of Half a Voice Period
The system SHALL turn a voice's gate off when the master phasor has advanced 0.5 in the voice's normalized phase, yielding a 50% duty cycle per voice step. On gate start, `PhasorBounds::Set` records the current master phasor and the voice's `m_phasorDenominator[i]`; on each frame, `PhasorBounds::Process` returns the circle distance travelled since gate start multiplied by the denominator (`CircleDistanceTracker::Distance() * m_phasorDenom`). When that normalized phase reaches or exceeds 0.5, `m_gate[i]` and `m_preGate[i]` are cleared; if additionally `!m_newTrigCanStart[i] || m_mute[i]`, the system sets `m_ahdControl[i].m_release = true` and stops tracking (`m_set[i] = false`).

#### Scenario: Gate goes low at half period
- **WHEN** voice i with `m_phasorDenominator[i] = 4` is triggered and the master phasor then advances by 0.125 of the master loop
- **THEN** the normalized phase reaches 0.5 (0.125 × 4) and `m_gate[i]` transitions from true to false
- **AND** while the transport is still running and the voice unmuted, `m_ahdControl[i].m_release` remains false so the AHD envelope continues through its hold/decay shape

### Requirement: Envelope Timing Published Through AHDControl
The system SHALL compute each voice's envelope period as `envelopeTimeSamples = m_masterLoopSamples / m_phasorDenominator[i]` and store it in `m_ahdControl[i].m_envelopeTimeSamples` every frame, and SHALL set `m_ahdControl[i].m_samples = thisPhase * envelopeTimeSamples` while phase tracking is active, so the AHD envelope sees phase-derived elapsed time rather than wall-clock time. The denominator is supplied per voice by the sequencer as the least common multiple of the voice's clock-loop multiplier and the loop multipliers of all read (non-co-muted) lens bits.

#### Scenario: Elapsed samples follow the phasor
- **WHEN** the master loop is 480000 samples, voice i has `m_phasorDenominator[i] = 8`, and the normalized phase since gate start is 0.25
- **THEN** `m_ahdControl[i].m_envelopeTimeSamples` is 60000 and `m_ahdControl[i].m_samples` is 15000
- **AND** if the clock is phase-modulated so the phasor moves faster, `m_samples` advances proportionally faster

### Requirement: Gate Signal Scope Limited to Note-Off and Display
The system SHALL drive DSP envelopes exclusively from `AHDControl` (`m_trig`, `m_samples`, `m_envelopeTimeSamples`, `m_release`); `m_gate[i]` SHALL be used only for note-off bookkeeping and UI display. When `m_gate[i]` is false and no trigger is emitted, the sequencer clears its output gate and records the note end; the UI gate display is fed via `SetGate(i, m_gate[i])`. The aggregate `m_anyGate` SHALL be true whenever any voice's gate is high, and it holds the phasor-timebase running while a note sounds.

#### Scenario: Note end recorded on gate-off
- **WHEN** `m_gate[i]` transitions to false and `m_ahdControl[i].m_trig` is false while the sequencer output gate for voice i is still true
- **THEN** the sequencer records a note-end event at the current independent phasor position and clears `m_output.m_gate[i]`
- **AND** the AHD envelope is unaffected by the gate-off itself, continuing under `AHDControl` until release or decay completes

#### Scenario: Any held gate keeps the clock running
- **WHEN** the transport stop is requested while at least one voice still has `m_gate[i]` true
- **THEN** `m_anyGate` is true and the phasor-timebase keeps running until all gates have gone low
