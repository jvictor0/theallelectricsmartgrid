## MODIFIED Requirements

### Requirement: Per-Voice Sub Meter and Spectrum
The metering strip SHALL split each voice's original meter footprint into four equal bars ordered left to right: sub reduction, sub level, main level, main reduction. Both level bars SHALL use the voice color, and both reduction bars SHALL use red. Sub levels and reductions SHALL come from the corresponding mono lane's independent saturation meter before monitoring. The source, return, and master meters SHALL retain their original positions and widths. The Sub page SHALL show the standard audio scope in its first visualizer slot and a spectrum analyzer spanning the other two slots. Both SHALL display the active trio's shaped, enveloped, DC-blocked sub audio before final voice gain, using the shared all-voices or individual-voice selection and voice colors. The analyzer SHALL omit the main voice filter-response overlay.

#### Scenario: Each sub is metered beside its own voice
- **WHEN** a voice's sub saturates while its main signal remains quiet
- **THEN** the leftmost reduction bar and adjacent sub level reflect that mono lane
- **AND** the main level and rightmost reduction bar reflect only the main voice signal

#### Scenario: Sub views follow the selected trio
- **WHEN** the Sub page is selected and the active trio changes
- **THEN** the scope and analyzer read that trio's sub audio channels, including release tails
- **AND** main voice audio and main filter settings do not alter the displayed sub waveform or spectrum

#### Scenario: Time and frequency views share the visualizer row
- **WHEN** the Sub page is selected
- **THEN** the time-domain scope fills slot 0 and the analyzer fills slots 1 and 2
- **AND** selecting a voice in either view updates the shared voice selection for both

#### Scenario: Sub waveform stays synchronized to its oscillator
- **WHEN** a sub oscillator wraps its phase
- **THEN** its scope SHALL record a cycle start with that lane's fractional top offset in engine samples
- **AND** scope decimation preserves the fractional crossing independently of main-source and envelope triggers
