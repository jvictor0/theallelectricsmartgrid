## ADDED Requirements

### Requirement: Final parameter output bounds
Every top-level parameter SHALL clamp its normalized computed output to [0,1] once per host sample before interpolation and bipolar conversion. Child encoder outputs SHALL retain their existing behavior. Source excursions SHALL contribute unchanged whenever the final parameter retains headroom.

#### Scenario: Saturation after evaluation
- **WHEN** a simple, graph, or editing path computes a finite parameter outside [0,1]
- **THEN** its published output and interpolated engine-rate cache remain within the parameter's unipolar or bipolar bounds
- **AND** previous output history contains the preceding clamped host output

#### Scenario: Headroom preserves modulation
- **WHEN** an out-of-range source at a small signed depth produces an in-range parameter
- **THEN** the final output preserves that contribution without source clipping
