## ADDED Requirements

### Requirement: Numeric Float Reads Accept Whole-Number Tokens
The arena JSON library SHALL provide a numeric floating-point read path for schema fields that semantically expect real-valued numbers. This read path SHALL return the stored value for both `JsonType::Real` and `JsonType::Integer` nodes, including integer nodes produced by parsing JSON tokens without a decimal point or exponent.

Strict type-specific reads MAY remain available for callers that need to distinguish integer and real nodes, but persisted float fields MUST have an accessor that treats JSON numeric spellings such as `1`, `1.0`, and `1e0` as the same numeric value.

#### Scenario: Parsed whole number reads as a float
- **WHEN** JSON text `{"value":1}` is parsed into an arena-backed tree
- **THEN** reading `value` through the numeric floating-point read path returns `1.0`

#### Scenario: Parsed real token reads as the same float
- **WHEN** JSON text `{"value":1.0}` is parsed into an arena-backed tree
- **THEN** reading `value` through the numeric floating-point read path returns `1.0`

#### Scenario: Strict reads remain type-specific
- **WHEN** a caller uses strict integer or real accessors
- **THEN** those accessors preserve their documented type-specific behavior
