## MODIFIED Requirements

### Requirement: Consistent modulation group presentation
Every populated instance of a local modulation slot SHALL use the same RGB color across Voice, Quad, and Global groups. The fixed palette SHALL be 0 `#FF0047`, 1 `#00DBFF`, 2 `#C2257F`, 3 `#76C70E`, 4 `#FFC628`, 5 `#8D5FFF`, 6 `#FF12D2`, 7 `#556A00`, 8 `#19F8BA`, 9 `#383DF9`, 10 `#00FD1C`, 11 `#8F8F8F`, 12 `#B70000`, 13 `#FCC4FF`, and 14 `#FDFDFD`. Every group's independent random pair SHALL retain the RND1/RND2 labels at slots 2/3. Unassigned slots SHALL retain Off/None presentation. Functional color pairs SHALL be 0/1, 2/3, 4/5, 6/7, and 12/13, while Sheafy SHALL remain a trio. The slot palette SHALL remain an authored catalogue convention rather than a runtime color formula.

#### Scenario: Moving between page groups
- **WHEN** the user moves between Source, Shape, PanMelody, LFO, Sub, Quad, or Global groups
- **THEN** the independent randoms in slots 2/3 display RND1/RND2 in Berry/Lime
- **AND** every other populated slot uses its fixed palette color independently of the bound source identity

#### Scenario: Sheafy matches Nonagon
- **WHEN** a Sheafy modulator and its corresponding Nonagon equation, matrix, RHS, or interval cell are displayed
- **THEN** their source colors share the same Ocean, Indigo, or SeaGreen definition
- **AND** inactive/dim variants are derived from those shared colors

#### Scenario: Parameter encoders identify their modulator
- **WHEN** a Voice or Quad LFO control, modulation/amp envelope control, oscillator-specific VCO control, physical excitation control, or Sub control is displayed
- **THEN** its encoder color matches its corresponding published modulation source
- **AND** modulator glyphs and source visualizers use that same catalogue color

#### Scenario: Neutral sources remain neutral
- **WHEN** Spread or Noise is displayed in any group that binds it
- **THEN** Spread is grey `#8F8F8F` and Noise is white `#FDFDFD`
