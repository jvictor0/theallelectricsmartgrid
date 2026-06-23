## MODIFIED Requirements

### Requirement: Internal-Only Clock Source
The system SHALL keep `TheoryOfTime::ClockMode` and `TheoryOfTime::Input::m_clockMode` absent.
The timebase SHALL advance from the frequency value supplied in `TheoryOfTime::Input::m_freq`; it SHALL NOT expose PLL, external phasor input, tick-to-phasor input, or MIDI clock routing as selectable product clock modes inside Theory of Time. External MIDI synchronization, when enabled, SHALL occur outside Theory of Time by changing the supplied effective frequency.

#### Scenario: Clock mode surface is absent
- **WHEN** product code constructs a `TheoryOfTime::Input`
- **THEN** it has no `m_clockMode` member
- **AND** no `ClockMode` enum is available

#### Scenario: Supplied frequency advances the phasor
- **WHEN** the timebase processes a running sample with supplied frequency `m_freq`
- **THEN** the true phasor advances by `m_freq`
- **AND** the phasor wraps into [0, 1)

#### Scenario: External sync remains outside Theory of Time
- **WHEN** external MIDI clock mode is enabled by the surrounding integration
- **THEN** Theory of Time still advances from `Input::m_freq`
- **AND** it does not inspect MIDI clock ticks directly

## ADDED Requirements

### Requirement: Effective Tempo Source
The system SHALL accept `TheoryOfTime::Input::m_freq` as the already-selected effective tempo source. In internal mode this value comes from the tempo parameter; in external mode this value comes from the external clock synchronizer.

#### Scenario: Internal effective tempo
- **WHEN** internal clock mode is selected
- **THEN** the integration writes the tempo-parameter frequency into `TheoryOfTime::Input::m_freq`

#### Scenario: External effective tempo
- **WHEN** external clock mode is selected
- **THEN** the integration writes the clock synchronizer's frequency estimate into `TheoryOfTime::Input::m_freq`
