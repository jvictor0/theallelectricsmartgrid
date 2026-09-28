## MODIFIED Requirements

### Requirement: Attack Stage With Constant-Time Configuration
The system SHALL ramp the raw output from `m_startOutput` toward 1.0 during attack as `attackPos = samples * attackIncrement + m_startOutput`. The attack increment is derived from a user time setting in the range 1 ms to 2.5 s (`x_attackTimeMin`/`x_attackTimeMax`) as `1 / (48000 * attackTime)` via an exponential parameter, using the fixed 48 kHz constant in phase-derived sample units. Wall-clock attack duration follows global phase motion and the captured source ratio and envelope period; those two captured factors need not cancel in the production voice path. On retrigger, `m_startOutput` SHALL be set to the current raw output so the attack continues from the present level instead of snapping to zero.

#### Scenario: Attack reaches full scale at the configured time
- **WHEN** an Idle envelope with a 500 ms attack is triggered and its phase-derived elapsed position reaches 24000 samples
- **THEN** the raw output rises linearly in phase-derived samples and reaches 1.0 at 24000 phase-derived samples

#### Scenario: Retrigger from a non-zero level
- **WHEN** an envelope whose raw output is 0.6 receives a new trigger
- **THEN** `m_startOutput` becomes 0.6 and the attack ramps from 0.6 to 1.0, taking only the remaining 40% of the configured attack time

### Requirement: Phase-Driven Stage Progression
At trigger, the envelope SHALL capture its selected source phase's cycle ratio to global phase, the absolute modulated global phase origin, and its envelope period in samples. The trigger producer SHALL supply the ratio; the active envelope SHALL retain no source loop index, winding tracker, or topology-change history. During Running it SHALL compute elapsed samples as abs(currentModulatedGlobalPhase - capturedGlobalPhase) * capturedRatio * capturedEnvelopePeriodSamples. Only a new trigger SHALL replace those captured values. Global phase modulation and reversal SHALL continue to affect progress.

#### Scenario: Topology edit does not change active timing
- **WHEN** an active envelope captured ratio 4 and the source topology later changes its ratio to 6
- **THEN** the active envelope continues using ratio 4 and its captured envelope period
- **AND** its progress and output do not jump solely because of the edit

#### Scenario: Retrigger captures new timing
- **WHEN** the same voice triggers again after the source ratio becomes 6
- **THEN** it captures ratio 6, the new global origin, and the new period
- **AND** its attack starts from the current output level

#### Scenario: Reversal retraces phase progress
- **WHEN** a running envelope's global phase moves back toward its captured origin
- **THEN** its phase-derived elapsed position decreases without an accumulated absolute-distance counter

#### Scenario: Slower phase stretches the envelope
- **WHEN** global phase modulation halves phase advance with captured timing unchanged
- **THEN** elapsed position advances at half speed


### Requirement: Hold Stage in Loop Divisions
The system SHALL hold the raw output at 1.0 after the attack completes for a duration expressed in envelope-period divisions, not absolute time: the hold parameter maps through a zeroed exponential curve to 0-16 loops (`m_hold.SetMax(16.0)`, centered at 1/32), and `holdSamples = holdLoops * capturedEnvelopePeriodSamples`. The hold therefore scales with the captured voice cycle period from the multi-phasor-gate, whose ratio uses the undoubled LCM of selected clock and read loop ratios. Decay begins at `attackEndSamples + holdSamples`, where `attackEndSamples = (1 - m_startOutput) / attackIncrement`.

#### Scenario: Hold tracks the envelope period
- **WHEN** the hold parameter maps to 0.5 loops and the voice's `capturedEnvelopePeriodSamples` is 60000
- **THEN** the raw output stays at 1.0 for 30000 phase-derived samples after the attack ends
- **AND** a subsequent trigger capturing twice the envelope period doubles hold length at the same hold setting; a topology edit alone does not change the active note's captured period
