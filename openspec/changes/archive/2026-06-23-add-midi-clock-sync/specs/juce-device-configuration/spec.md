## ADDED Requirements

### Requirement: Clock Mode Configuration Toggle
The JUCE config page SHALL include a toggle control for clock mode with internal clock and external MIDI clock states. The toggle SHALL initialize from the current configuration, update the runtime clock mode when changed, and preserve the existing MIDI/audio configuration behavior on the page.

#### Scenario: Config page shows current clock mode
- **WHEN** the user opens the config page and the runtime clock mode is external
- **THEN** the clock mode toggle displays external clock mode

#### Scenario: Toggle updates runtime clock mode
- **WHEN** the user changes the clock mode toggle from internal to external
- **THEN** the Nonagon wrapper or internal integration receives external clock mode before audio processing continues

### Requirement: Clock Mode Persists In Config JSON
The application SHALL save the selected clock mode in config JSON and restore it on startup. Missing clock-mode fields in older config JSON SHALL be treated as internal clock mode.

#### Scenario: External clock survives config save load
- **WHEN** the user selects external clock mode and the application saves config JSON
- **THEN** the saved config contains the external clock mode selection
- **AND** a later config load restores external clock mode

#### Scenario: Older config remains internal
- **WHEN** config JSON does not contain a clock-mode field
- **THEN** loading config succeeds
- **AND** clock mode remains internal
