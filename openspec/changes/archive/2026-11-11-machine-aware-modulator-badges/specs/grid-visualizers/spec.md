## ADDED Requirements

### Requirement: Role-specific modulation badges
The system SHALL provide rounded monochrome badges for saturated Sub sine, additive harmonic waveform, audio output, audio send, wet return, and envelope follower VU. Sub SHALL use the saturated sine; Voice and final output audio SHALL share the outward-arrow symbol; dry/send audio SHALL use an inward-arrow symbol; wet/return audio SHALL use three horizontal waves; all mixer envelope followers SHALL use VU. Existing envelope generators SHALL retain the envelope badge. Badge backgrounds SHALL retain the catalogue colors.

#### Scenario: Mixer source roles
- **WHEN** send, return, or final-output modulation sources affect an encoder
- **THEN** each follower shows VU and each audio source shows Send, Wet, or Output respectively
- **AND** Voice uses the same output symbol as final-output audio

#### Scenario: Compact badge rendering
- **WHEN** badges are rendered at small encoder-strip sizes
- **THEN** new glyphs retain rounded strokes, transparent backgrounds, and the existing foreground contrast behavior
- **AND** the additive waveform uses a small fixed harmonic mixture with the generator's cosine phase convention

### Requirement: Source-machine-aware modulation badges
Source-output glyph publication SHALL resolve the machine belonging to the bound source's trio. Dual waveshaping SHALL use the additive waveform for both source outputs; physical modeling SHALL use Noise for its filtered-noise output and the existing envelope glyph for its internal envelope. Sample and Thru SHALL publish no source-pair glyph because their source modulation outputs are zero. Machine changes SHALL update glyphs on the next UI publication without changing the source colors, identities, or saved modulation depths.

#### Scenario: Machine changes in place
- **WHEN** the owning trio switches between dual waveshaping and physical modeling
- **THEN** its source-pair badges switch between harmonic waveforms and Noise/Envelope on UI publication
- **AND** their source colors remain unchanged

#### Scenario: Another trio is active
- **WHEN** a visible encoder belongs to a different trio than the active navigation
- **THEN** its badges follow its own bound source's machine

#### Scenario: Source outputs unavailable
- **WHEN** a source pair belongs to a Sample or Thru machine
- **THEN** its source-pair glyphs are None while stored depths and source colors remain intact
