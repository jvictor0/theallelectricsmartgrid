## ADDED Requirements

### Requirement: External Clock Synchronizer Ownership
The system SHALL own external MIDI clock synchronization inside `TheNonagonSquiggleBoyInternal`, using a dedicated clock synchronizer helper whose synchronization math is based on the existing external-clock sync source from `/Users/joyo/theallelectricsmartgrid/private/src/ExternalClockSync.hpp`. The Nonagon and Theory of Time SHALL NOT expose synchronizer internals or duplicate synchronization logic; they SHALL receive only the resulting effective tempo, running state, loop selection, and existing timebase inputs.

#### Scenario: Synchronizer is the only external clock logic owner
- **WHEN** external clock mode is enabled and a MIDI clock tick is consumed
- **THEN** `TheNonagonSquiggleBoyInternal` forwards the tick state to its clock synchronizer
- **AND** `TheNonagonInternal` and `TheoryOfTime` receive only normal input fields such as running state and effective frequency

#### Scenario: Missing sync math blocks implementation
- **WHEN** implementation cannot locate the user-provided external clock sync source
- **THEN** implementation stops before replacing the synchronization math
- **AND** the change is not completed with newly invented clock-sync math

### Requirement: Clock Tick Sampling
The system SHALL represent each visible MIDI clock message as a per-sample clock tick status. On any audio sample where a realtime clock event becomes visible from the message bus, the external clock synchronizer SHALL receive a true clock-input value for that sample; on samples without a visible clock event it SHALL receive false.

#### Scenario: Visible clock event marks the sample
- **WHEN** a queued MIDI clock event has timestamp T
- **AND** audio processing reaches sample time T
- **THEN** that sample's clock tick status is true
- **AND** the clock synchronizer processes that sample as containing a clock tick

#### Scenario: No clock event clears the status
- **WHEN** audio processing advances to a sample with no visible MIDI clock event
- **THEN** that sample's clock tick status is false
- **AND** the clock synchronizer processes that sample without a tick

### Requirement: External Tempo Correction
When external clock mode is enabled, the system SHALL set `TheoryOfTime::Input::m_freq` from the clock synchronizer's tempo estimate instead of the tempo encoder parameter. The synchronizer SHALL adjust its tempo estimate from MIDI clock tick timing and phase error so Theory of Time returns toward phase with the incoming clock.

#### Scenario: External mode replaces tempo parameter
- **WHEN** external clock mode is enabled
- **AND** the tempo encoder parameter has value A
- **AND** the clock synchronizer reports frequency B
- **THEN** `TheoryOfTime::Input::m_freq` is set to B for processing

#### Scenario: Non-positive external estimate clamps to minimum positive tempo
- **WHEN** external clock mode is enabled
- **AND** the clock synchronizer's tempo estimate is less than or equal to zero
- **THEN** `TheoryOfTime::Input::m_freq` is set to the smallest floating-point value greater than zero
- **AND** it is not replaced by the default internal tempo fallback

#### Scenario: Internal mode keeps tempo parameter
- **WHEN** internal clock mode is enabled
- **THEN** `TheoryOfTime::Input::m_freq` is set from the tempo encoder parameter
- **AND** the clock synchronizer's tempo estimate does not override that value

#### Scenario: Phase error changes tempo estimate
- **WHEN** incoming MIDI clock ticks indicate the internal phase is behind the external clock
- **THEN** the clock synchronizer adjusts its tempo estimate in the direction that brings the internal phase back into phase with incoming ticks

#### Scenario: Stopped clock estimate uses selected loop
- **GIVEN** external clock mode is enabled
- **AND** the external clock loop selector targets a non-master Theory of Time loop
- **WHEN** MIDI clock ticks are received while transport is stopped
- **THEN** the stopped tempo estimate is scaled by the selected loop's external multiplier
- **AND** the estimate represents master-loop phase advance for that selected-loop tick interval

#### Scenario: MIDI start arms external transport
- **GIVEN** external clock mode is enabled
- **AND** MIDI clock ticks have been received while the transport is stopped
- **WHEN** a MIDI start or continue message is consumed before the next clock tick
- **THEN** the clock synchronizer enters the armed state
- **AND** the global running state remains false
- **AND** the Nonagon receives stopped running state until the next MIDI clock tick

#### Scenario: Armed clock starts external transport without tempo update
- **GIVEN** external clock mode is enabled
- **AND** the clock synchronizer is armed from a MIDI start or continue message
- **WHEN** the next MIDI clock tick is consumed
- **THEN** the clock synchronizer enters the running state
- **AND** the global running state becomes true
- **AND** the Nonagon receives running state for that sample
- **AND** the clock synchronizer records the armed clock as the consumed start tick
- **AND** it preserves the stopped-clock tempo estimate
- **AND** it does not update tempo from the short start-to-clock interval
- **AND** subsequent running clock ticks estimate tempo from normal tick intervals

### Requirement: Realtime Transport Controls Running State
The system SHALL treat MIDI realtime start, continue, and stop messages as equivalent to pressing the global run/play cell into the corresponding state in internal clock mode. In external clock mode, MIDI start and continue SHALL arm the external clock synchronizer only when it is stopped, MIDI stop SHALL set the synchronizer to stopped, and `TheNonagonSquiggleBoyInternal::m_running` SHALL be derived from the synchronizer state: stopped and armed are not running, running is running.

#### Scenario: MIDI start starts internal transport
- **WHEN** a visible MIDI start message is consumed
- **AND** internal clock mode is enabled
- **THEN** the global running state becomes true
- **AND** the green run/play cell reports its on color through the same state

#### Scenario: MIDI continue starts internal transport
- **WHEN** a visible MIDI continue message is consumed
- **AND** internal clock mode is enabled
- **THEN** the global running state becomes true
- **AND** subsequent Nonagon input setup observes the running state

#### Scenario: MIDI stop stops internal transport
- **WHEN** a visible MIDI stop message is consumed
- **AND** internal clock mode is enabled
- **THEN** the global running state becomes false
- **AND** subsequent Nonagon input setup observes the stopped state

#### Scenario: MIDI start arms external transport
- **WHEN** a visible MIDI start or continue message is consumed
- **AND** external clock mode is enabled
- **THEN** the clock synchronizer state becomes armed if it was stopped
- **AND** the global running state remains false until a MIDI clock tick moves the synchronizer to running

#### Scenario: MIDI stop stops external transport
- **WHEN** a visible MIDI stop message is consumed
- **AND** external clock mode is enabled
- **THEN** the clock synchronizer state becomes stopped
- **AND** subsequent Nonagon input setup observes stopped running state

### Requirement: External Sync Loop Selection
The system SHALL select the Theory of Time loop used by external clock synchronization with the same six-position switch pattern as `SampleLoopIndex`: `loopIndex = TheoryOfTimeBase::x_numLoops - switchVal - 1`. Switch value 0 SHALL select the master loop, and the remaining switch values SHALL select the other Theory of Time loops.

#### Scenario: Switch value zero selects master loop
- **WHEN** the external sync loop switch value is 0
- **THEN** the clock synchronizer receives `TheoryOfTimeBase::x_masterLoop` as its loop index

#### Scenario: Highest switch value selects loop zero
- **WHEN** the external sync loop switch value is `TheoryOfTimeBase::x_numLoops - 1`
- **THEN** the clock synchronizer receives loop index 0

### Requirement: External Clock Mode Defaults Safe
The system SHALL default to internal clock mode for new sessions and for older configuration JSON that lacks an external-clock field.

#### Scenario: Older config loads internal clock
- **WHEN** application config JSON has no clock-mode field
- **THEN** loading config leaves the clock mode set to internal
