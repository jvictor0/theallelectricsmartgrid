## MODIFIED Requirements

### Requirement: Gate Duration of Half a Voice Period
The system SHALL turn a voice's gate off when the master phasor has advanced 0.5 in the voice's normalized phase, yielding a 50% duty cycle per voice step. On gate start, `PhasorBounds::Set` records the current master phasor and the voice's `m_phasorDenominator[i]`; on each frame, `PhasorBounds::Process` returns the absolute modulated global phase distance from the captured origin multiplied by the denominator (`abs(globalPhase - startGlobalPhase) * capturedVoiceRatio`). When that normalized phase reaches or exceeds 0.5, `m_gate[i]` and `m_preGate[i]` are cleared; if additionally `!m_newTrigCanStart[i] || m_mute[i]`, the system sets `m_ahdControl[i].m_release = true` and stops tracking (`m_set[i] = false`).

#### Scenario: Gate goes low at half period
- **WHEN** voice i with `m_phasorDenominator[i] = 4` is triggered and the master phasor then advances by 0.125 of the master loop
- **THEN** the normalized phase reaches 0.5 (0.125 × 4) and `m_gate[i]` transitions from true to false
- **AND** while the transport is still running and the voice unmuted, `m_ahdControl[i].m_release` remains false so the AHD envelope continues through its hold/decay shape

#### Scenario: Active gate retains its trigger-time ratio
- **WHEN** topology changes the requested voice ratio after gate start
- **THEN** the active gate continues using its captured ratio until a new gate trigger


### Requirement: Envelope Timing Published Through AHDControl
The system SHALL supply trigger/release events and trigger-time envelope-period configuration through AHDControl. The period SHALL equal globalPeriodSamples divided by the voice gate ratio at trigger. The AHD trigger path SHALL also supply the selected source phase-to-global cycle ratio, preserving the existing source selection and distinguishing it from the voice gate ratio. Existing AHD instances SHALL derive progress directly from global phase rather than a duplicated elapsed-sample relay. Any remaining non-AHD consumer of that relay SHALL be migrated before its removal.

#### Scenario: Capture envelope period
- **WHEN** global period is 480000 samples and voice gate ratio is 8 at trigger
- **THEN** the captured envelope period is 60000 samples
- **AND** later topology-only changes do not alter the active envelope's period


### Requirement: Gate Signal Scope Limited to Note-Off and Display
The system SHALL drive DSP envelopes exclusively from `AHDControl` (trigger/release events and trigger-time timing configuration); `m_gate[i]` SHALL be used only for note-off bookkeeping and UI display. When `m_gate[i]` is false and no trigger is emitted, the sequencer clears its output gate and records the note end; the UI gate display is fed via `SetGate(i, m_gate[i])`. The aggregate `m_anyGate` SHALL be true whenever any voice's gate is high, and it holds the phasor-timebase running while a note sounds.

#### Scenario: Note end recorded on gate-off
- **WHEN** `m_gate[i]` transitions to false and `m_ahdControl[i].m_trig` is false while the sequencer output gate for voice i is still true
- **THEN** the sequencer records a note-end event at the current independent phasor position and clears `m_output.m_gate[i]`
- **AND** the AHD envelope is unaffected by the gate-off itself, continuing under `AHDControl` until release or decay completes

#### Scenario: Any held gate keeps the clock running
- **WHEN** the transport stop is requested while at least one voice still has `m_gate[i]` true
- **THEN** `m_anyGate` is true and the phasor-timebase keeps running until all gates have gone low
