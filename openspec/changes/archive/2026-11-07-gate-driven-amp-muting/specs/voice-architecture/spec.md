## ADDED Requirements

### Requirement: Amp Muting Envelope
Each voice's amp section SHALL contain an ASD muting envelope in addition to its amp and modulation AHDs. Its gate SHALL be exactly ToT's current running flag AND NOT that voice's mute flag, with no early-mute, unison, note-gate, or trigger condition. The ASD SHALL copy the amp AHD input's attack and decay increments, clock pointer, phase ratio, and envelope period without duplicating parameter mapping. Before amp AHD processing, effective depth SHALL be `1 - (1 - ampDepth) * mutingEnvelope`. The configured depth SHALL remain available unchanged for subsequent processing. Modulation AHD depth SHALL be unaffected.

#### Scenario: Open muting envelope preserves the chosen depth
- **WHEN** the muting ASD value is one
- **THEN** effective amp depth equals the configured amp depth
- **AND** repeated processing does not compound or overwrite that configured depth

#### Scenario: Closed muting envelope removes the drone floor
- **WHEN** a muted or stopped voice's muting ASD reaches zero
- **THEN** effective amp depth is one even if configured depth is zero
- **AND** once raw amp AHD has finished its existing release the amp envelope output is exactly zero

#### Scenario: Every lane uses only ToT running and its own mute
- **WHEN** ToT runs and one voice is muted
- **THEN** only that voice's ASD gate is low among otherwise unmuted voices
- **AND** changing early-mute, trigger, or unison flags does not alter those ASD gates
- **AND** all gates are low when ToT is stopped, even if the outer transport flag differs
