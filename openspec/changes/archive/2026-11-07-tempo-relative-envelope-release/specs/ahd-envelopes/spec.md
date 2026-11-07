## MODIFIED Requirements

### Requirement: Release Ramp on Interruption
The system SHALL, while in the Release state, integrate the current unmodulated global clock rate independently of transport phase. Each call SHALL decrement the raw output by `decayIncrement * clockIncrement * capturedRatio * capturedEnvelopePeriodSamples * input.m_releaseFactor`, where `clockIncrement` is global cycles per engine sample from the current global period. This basic clock increment SHALL remain available after transport stops and SHALL follow live tempo changes. Release SHALL ignore phase modulation, phase reversal, and transport phase resets. Trigger-captured ratio and envelope period SHALL remain unchanged until retrigger. The raw accumulator SHALL use double precision so small per-sample decrements preserve long release durations; rendered output SHALL remain float. The release factor SHALL default to 1 engine sample per call; physical modeling SHALL leave it at that default because it processes one engine sample per call. This factor SHALL affect only explicit release, leaving phase-driven attack, hold, and running decay increments unchanged. When raw output reaches or falls below 0 it SHALL clamp to 0 and the state SHALL return to Idle.

#### Scenario: Release ramps at the basic clock decay rate
- **WHEN** an envelope holding at raw output 1.0 with a 100 ms decay enters Release and its captured ratio times captured period equals the current global period
- **THEN** its raw output reaches 0 after `SampleTimer::SecondsToSamples(0.1)` engine samples, allowing rounding at the final sample, and the state becomes Idle
- **AND** clock-phase modulation does not alter release speed

#### Scenario: Release continues after Stop and follows tempo
- **WHEN** transport stops and its phases reset to zero while an envelope is releasing
- **THEN** release continues integrating the basic clock increment with no jump from the phase reset
- **AND** doubling the basic clock frequency doubles subsequent release speed
- **AND** no modulated phase motion is needed for the tail to reach Idle

#### Scenario: Release preserves captured voice scaling
- **WHEN** a release captured ratio 3 and envelope period 64 reference samples, and the current global period is 128 engine samples
- **THEN** each engine sample advances release by 1.5 reference samples
- **AND** subsequent topology or non-trigger control changes do not replace the captured ratio or period

#### Scenario: Long release preserves configured duration
- **WHEN** an envelope at raw output 1.0 uses a 10 s decay, receives a release request on every engine sample, and its basic clock scaling advances one reference sample per engine sample
- **THEN** its output is 0.5 after 5 s within float output precision
- **AND** it reaches Idle within one engine sample of 10 s

#### Scenario: Fractional-sample release retains elapsed time
- **WHEN** the same envelope at the same tempo is processed four times per engine sample with `input.m_releaseFactor` set to 0.25 engine samples per call
- **THEN** its release reaches zero after the same elapsed engine-sample time as one call per engine sample, allowing floating-point rounding at the final sample
- **AND** running attack, hold, and decay continue to use phase-derived fractional engine-sample positions

#### Scenario: Physical-model short timing range is preserved
- **WHEN** the physical-model envelope uses its shortest attack and decay settings
- **THEN** attack is 100 µs and decay is 500 µs in reference sample units, with a maximum setting of 2.5 s for each
- **AND** explicit release uses the same basic clock scaling as amp and modulation envelopes
