# JUCE Device Configuration Specification

## Purpose
The JUCE device configuration layer covers the Smart Grid One config page's device-selection controls for controller MIDI and application audio devices. It keeps UI dropdown population, current-value selection, change detection, runtime application, and config JSON persistence explicit at the JUCE UI boundary without changing controller protocol, MIDI transport, or DSP behavior.
## Requirements
### Requirement: Typed Config Dropdown Rows
The JUCE config page SHALL present each device or mode selection through a typed row component that owns its label, dropdown items, current selection, and change notification. The row component SHALL use JUCE component overrides for layout/painting behavior and SHALL keep its implementation local to the config-page source surface.

#### Scenario: Row shows selected item
- **WHEN** a row is populated with device names including "None" and "Controller A" and current value "Controller A"
- **THEN** the row selects "Controller A"
- **AND** the row exposes that selection to the config page without requiring the page to scan raw combo-box text itself

#### Scenario: Missing device falls back safely
- **WHEN** a row is populated with device names that do not include the saved current value
- **THEN** the row selects the default "None" or empty/default item
- **AND** the page remains usable

### Requirement: Existing MIDI Configuration Behavior Is Preserved
The JUCE config page SHALL continue to allow the user to configure the existing controller MIDI inputs, MIDI outputs, and controller shapes with the same routing behavior as before the refactor. Launchpad, Encoder, WrldBldr, Generic, and KMix sections SHALL keep their existing supported dropdowns and omissions.

#### Scenario: Launchpad section applies input, output, and shape
- **WHEN** the user selects a MIDI input, MIDI output, and controller shape for a Launchpad section
- **THEN** the corresponding Nonagon launchpad/twister input and launchpad output are opened with the selected device identifiers and shape

#### Scenario: KMix section remains output only
- **WHEN** the config page displays the KMix section
- **THEN** it provides a MIDI output selection
- **AND** it does not provide a MIDI input selection

#### Scenario: Generic section does not offer MIDI output choices
- **WHEN** the config page displays a Generic section
- **THEN** the section does not populate selectable MIDI output devices

### Requirement: Audio Input And Output Selection
The JUCE config page SHALL include rows for selecting the application's audio input device and audio output device from the available JUCE audio devices. These rows SHALL initialize from the current saved configuration or the current JUCE audio device state.

#### Scenario: Audio rows are visible on config page
- **WHEN** the user opens the config page
- **THEN** audio input and audio output rows are displayed alongside the existing stereo and controller configuration controls

#### Scenario: Current audio devices initialize selections
- **WHEN** the current configuration names an available audio input and output device
- **THEN** the audio input row selects that input device
- **AND** the audio output row selects that output device

### Requirement: Audio Device Changes Restart JUCE On Back
The application SHALL restart JUCE audio when leaving the config page if the selected audio input or output device changed while the page was open. It SHALL NOT restart JUCE audio when only MIDI controller selections, controller shapes, or stereo mode changed.

#### Scenario: Changed audio output restarts audio
- **WHEN** the user opens the config page, changes the audio output selection, and presses Back
- **THEN** the selected audio output is written to configuration
- **AND** JUCE audio is restarted using the selected audio output

#### Scenario: Unchanged audio devices do not restart audio
- **WHEN** the user opens the config page, changes only a MIDI controller selection, and presses Back
- **THEN** JUCE audio is not restarted because the audio input and output selections are unchanged

### Requirement: Audio Device Selection Persists In Config JSON
The application SHALL save selected audio input and audio output device names in the existing config JSON save path and restore them from that path on startup. Missing audio device fields in older config JSON SHALL be treated as default/empty selections.

#### Scenario: Audio selections survive config save load
- **WHEN** the user selects audio input "Input A" and audio output "Output B" and the application saves config JSON
- **THEN** the saved config contains the selected audio input and output names
- **AND** a later config load restores those names into application configuration

#### Scenario: Older config remains valid
- **WHEN** config JSON does not contain audio input or audio output fields
- **THEN** loading config succeeds
- **AND** the audio input and output configuration remain default or empty

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
