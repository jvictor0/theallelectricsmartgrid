## ADDED Requirements

### Requirement: Theory Of Time Loop Selector Parameter
The encoder parameter system SHALL expose a new six-position switch-valued parameter on the Theory of Time global bank for selecting the external sync loop. The parameter SHALL use the same switch-valued encoder pattern as `SampleLoopIndex`, SHALL serialize through the named encoder JSON path, and SHALL publish switch metadata through `EncoderBankUIState`. The fully counterclockwise switch position SHALL select the master loop, with switch values mapping to loop indexes as `TheoryOfTimeBase::x_numLoops - switchVal - 1`.

#### Scenario: Fully counterclockwise selects master loop
- **WHEN** the Theory of Time loop selector parameter is at switch value 0
- **THEN** the effective loop selection is `TheoryOfTimeBase::x_masterLoop`

#### Scenario: Other switch values select other loops
- **WHEN** the Theory of Time loop selector parameter is at switch value 3
- **THEN** the effective loop selection is `TheoryOfTimeBase::x_numLoops - 3 - 1`

#### Scenario: Default selects upper-middle switch value
- **WHEN** the encoder parameter system initializes the Theory of Time loop selector parameter from defaults
- **THEN** the selector publishes switch value 3
- **AND** the effective loop selection is `TheoryOfTimeBase::x_numLoops - 3 - 1`

#### Scenario: Switch metadata is published
- **WHEN** the Theory of Time bank is selected
- **THEN** the loop selector encoder cell is connected
- **AND** `EncoderBankUIState` reports switch values for that cell

#### Scenario: Loop selection survives patch round trip
- **WHEN** the loop selector parameter is set away from its default and the patch is saved then loaded
- **THEN** the named encoder JSON path restores that loop selector parameter value
