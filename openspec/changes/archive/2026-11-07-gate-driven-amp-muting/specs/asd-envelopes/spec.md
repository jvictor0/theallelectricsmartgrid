## ADDED Requirements

### Requirement: Gate-Driven ASD Integration
The system SHALL provide an ASD envelope whose control contains a persistent gate instead of a trigger. Its value SHALL start at zero. Each call with gate high SHALL set value to `min(1, value + abs(dt) * attackIncrement)`; each call with gate low SHALL set value to `max(0, value - abs(dt) * decayIncrement)`. It SHALL retain double-precision accumulation and render a float output. It SHALL have no amplitude or sustain-level parameter. Changing the gate or rates SHALL preserve the current value before applying that call's step. A generic wrapper SHALL process independent voice lanes.

#### Scenario: Gate changes during a ramp
- **WHEN** attack has reached 0.25 and the gate becomes low
- **THEN** decay starts from 0.25 using the decay increment without resetting value
- **AND** a later high gate attacks from the then-current value and remains high without event resubmission

#### Scenario: Reverse motion cannot reverse the ramp
- **WHEN** the ToT delta changes sign with unchanged magnitude
- **THEN** a high gate continues raising value and a low gate continues lowering it at the same rate
- **AND** zero delta leaves value unchanged and a large delta clamps exactly to the selected endpoint

### Requirement: ASD Theory of Time Units
ASD SHALL use signed modulated global phase increments while ToT is running and the basic unmodulated global increment while ToT is stopped. It SHALL convert to reference samples using its input ratio and envelope period, taking the absolute value for integration. It SHALL NOT count transport phase resets as elapsed travel or continue phase modulation while stopped.

#### Scenario: Stopped decay follows tempo
- **WHEN** ToT stops while ASD value is nonzero and the gate is low
- **THEN** ASD continues toward zero using the basic clock and retains its value across the phase reset apart from the current sample's decay step
- **AND** doubling tempo doubles subsequent decay speed
