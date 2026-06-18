## ADDED Requirements

### Requirement: Encoder JSON Float Load Accepts All Numeric Spellings
When loading named encoder state from JSON, the system SHALL restore normalized per-track per-scene base values from JSON numeric values regardless of whether the token is represented internally as an integer node or a real node. A saved encoder base value of `1.0` MUST NOT become `0.0` solely because the JSON text spells the value as `1`.

This applies to base parameter cells, modulation depth cells, and gesture target cells because all are serialized through the same `StateEncoderCell` value arrays.

#### Scenario: Whole-number base value restores as one
- **WHEN** a named encoder cell is loaded from JSON whose `values` array contains the token `1`
- **THEN** the corresponding stored base value is restored as `1.0`
- **AND** the cell's computed unslewed output is `1.0` after load processing

#### Scenario: Fractional base value still restores
- **WHEN** a named encoder cell is loaded from JSON whose `values` array contains the token `0.5`
- **THEN** the corresponding stored base value is restored as `0.5`

#### Scenario: Nested modulation depth whole-number restores
- **WHEN** a saved modulation depth cell contains a whole-number normalized value in its `values` array
- **THEN** loading the patch restores that depth value numerically instead of converting it to zero
