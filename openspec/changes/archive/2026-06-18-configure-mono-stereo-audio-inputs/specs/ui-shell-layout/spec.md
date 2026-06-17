## ADDED Requirements

### Requirement: Squiggle Boy Config Audio Input Controls
The Squiggle Boy config grid SHALL expose controls for four external input sources. For each source, the grid SHALL provide a source-selection button and a mono/stereo width toggle. The width toggle SHALL be off for Mono and on for Stereo.

The source-selection controls, source monitor controls, deep-vocoder send controls, and mono/stereo width toggles SHALL each display the configured color for their corresponding source. Pressing a source-selection button SHALL toggle that source for the active trio and rerun source assignment propagation. Pressing a width toggle SHALL change that source between Mono and Stereo, rerun source selection limit enforcement, and rerun source assignment propagation for affected trios.

#### Scenario: Four source selectors are visible in config mode
- **WHEN** the grids mode is Config and the Squiggle Boy config grid is shown
- **THEN** the grid exposes one source-selection control for each source 0 through 3

#### Scenario: Stereo toggle changes source width
- **WHEN** the user presses the width toggle for source 3 while it is configured as Mono
- **THEN** source 3 becomes configured as Stereo
- **AND** trio source assignment propagation is rerun

#### Scenario: Source controls use the source color
- **WHEN** the source color for source 2 is Ocean
- **THEN** the source 2 selector, monitor button, deep-vocoder button, and width toggle are drawn with Ocean or a dimmed Ocean state

#### Scenario: Overflow after stereo toggle is resolved
- **WHEN** the active trio already has three selected mono source channels and the user toggles one selected source to Stereo
- **THEN** the config grid updates selection state so the trio has no more than three selected source channels
- **AND** the visible source-selection buttons reflect any source that was unselected
