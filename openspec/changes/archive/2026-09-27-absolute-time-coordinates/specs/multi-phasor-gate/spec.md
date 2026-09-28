## MODIFIED Requirements

### Requirement: Gate Duration of Half a Voice Period
The system SHALL turn a voice's gate off when absolute distance from the trigger origin reaches 0.5 in the voice's normalized phase, retaining the intentional half-voice-cycle note duration independently of whole-cycle timebase rhythms. On gate start, `PhasorBounds::Set` records the current absolute modulated global phase, the voice's `m_voiceCycleRatio[i]`, and `m_globalPeriodSamples`; on each frame, `PhasorBounds::Process` returns the absolute modulated global phase distance from the captured origin multiplied by the captured voice cycle ratio (`abs(globalPhase - startGlobalPhase) * capturedVoiceRatio`). When that normalized phase reaches or exceeds 0.5, `m_gate[i]` and `m_preGate[i]` are cleared; if additionally `!m_newTrigCanStart[i] || m_mute[i]`, the system sets `m_ahdControl[i].m_release = true` and stops tracking (`m_set[i] = false`).

#### Scenario: Gate goes low at half period
- **WHEN** voice i with `m_voiceCycleRatio[i] = 4` is triggered and the absolute modulated global phase then advances by 0.125 of the global loop
- **THEN** the normalized phase reaches 0.5 (0.125 × 4) and `m_gate[i]` transitions from true to false
- **AND** while the transport is still running and the voice unmuted, `m_ahdControl[i].m_release` remains false so the AHD envelope continues through its hold/decay shape

#### Scenario: Active gate retains its trigger-time ratio
- **WHEN** topology changes the requested voice ratio after gate start
- **THEN** the active gate continues using its captured ratio until a new gate trigger


### Requirement: Envelope Timing Published Through AHDControl
The system SHALL supply trigger/release events and trigger-time envelope-period configuration through AHDControl. The period SHALL equal globalPeriodSamples divided by the voice cycle ratio at trigger. The ratio SHALL be the undoubled LCM of the selected clock and all read lens loop cycle ratios, starting at one when no clock is selected; neither clock nor read contributions SHALL be doubled. The AHD trigger path SHALL also supply the selected source phase-to-global cycle ratio, preserving the existing source selection and distinguishing it from the voice gate ratio. Existing AHD instances SHALL derive progress directly from global phase rather than a duplicated elapsed-sample relay. Any remaining non-AHD consumer of that relay SHALL be migrated before its removal.

#### Scenario: Capture envelope period
- **WHEN** global period is 480000 samples and voice gate ratio is 8 at trigger
- **THEN** the captured envelope period is 60000 samples
- **AND** later topology-only changes do not alter the active envelope's period


### Requirement: Gate Signal Scope Limited to Note-Off and Display
The system SHALL drive DSP envelopes exclusively from `AHDControl` (trigger/release events and trigger-time timing configuration); `m_gate[i]` SHALL be used only for note-off bookkeeping and UI display. When `m_gate[i]` is false and no trigger is emitted, the sequencer clears its output gate and records the note end; the UI gate display is fed via `SetGate(i, m_gate[i])`. The aggregate `m_anyGate` SHALL be true whenever any voice's gate is high, and it holds the phasor-timebase running while a note sounds.

#### Scenario: Note end recorded on gate-off
- **WHEN** `m_gate[i]` transitions to false and `m_ahdControl[i].m_trig` is false while the sequencer output gate for voice i is still true
- **THEN** the sequencer records a note-end event at the current wrapped unmodulated global phase position and clears `m_output.m_gate[i]`
- **AND** the AHD envelope is unaffected by the gate-off itself, continuing under `AHDControl` until release or decay completes

#### Scenario: Any held gate keeps the clock running
- **WHEN** the transport stop is requested while at least one voice still has `m_gate[i]` true
- **THEN** `m_anyGate` is true and the phasor-timebase keeps running until all gates have gone low

### Requirement: Per-Voice Trigger Emission
The system SHALL emit a per-voice trigger by setting `m_ahdControl[i].m_trig = m_trigs[i] && m_newTrigCanStart[i] && !m_mute[i]` each time `Process` runs, and SHALL clear `m_ahdControl[i].m_release` on the frame a trigger is emitted. The emitted trigger is the single "start a note" signal consumed downstream: the sequencer latches pitch and records a note event from it, and the DSP voices start their AHD envelopes from it.

#### Scenario: Trigger accepted for an unmuted voice
- **WHEN** `m_trigs[i]` is true, `m_newTrigCanStart[i]` is true, and `m_mute[i]` is false for voice i
- **THEN** `m_ahdControl[i].m_trig` is true for that frame and `m_ahdControl[i].m_release` is set false
- **AND** `m_gate[i]` becomes true and new bounds capture the current global phase, voice cycle ratio, and global period

#### Scenario: Muted voice still tracks phase but emits no trigger
- **WHEN** `m_trigs[i]` and `m_newTrigCanStart[i]` are true but `m_mute[i]` is true
- **THEN** `m_ahdControl[i].m_trig` is false and `m_gate[i]` stays false
- **AND** `m_preGate[i]` is set true and `m_bounds[i].Set(globalPhase, m_voiceCycleRatio[i], m_globalPeriodSamples)` still records the gate start, so phase tracking continues while muted
