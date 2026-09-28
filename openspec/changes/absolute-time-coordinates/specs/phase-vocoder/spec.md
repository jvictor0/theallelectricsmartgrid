## ADDED Requirements

### Requirement: Current Inputs at Deep Vocoder Note Triggers
TransformNote SHALL receive the current Deep Vocoder input and refresh the triggering voice's pitch center, threshold, slopes, and pre/post pitch ratios before selecting a pitch. It SHALL use the current enable flag immediately. FFT hops SHALL update the analyzed atom set and continuing voice tracking, but SHALL NOT delay new-note input changes. Bypass SHALL preserve the trigger and return the current pitch center times the current post ratio; enabled operation SHALL select from the latest available atoms and cancel a trigger when none qualifies.

#### Scenario: Pitch changes between FFT hops while bypassed
- **WHEN** a new note triggers with a changed pitch before another FFT hop and the vocoder is disabled
- **THEN** the output uses that note's current pitch and post ratio
- **AND** the preceding note's cached pitch is not reused

#### Scenario: Enabled selection uses current note parameters
- **WHEN** a note changes pitch, threshold, slopes, or pitch ratios between FFT hops
- **THEN** selection uses those current inputs against the existing atom set
- **AND** enable changes take effect on that trigger without waiting for analysis
