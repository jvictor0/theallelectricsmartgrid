## ADDED Requirements

### Requirement: Generic morphing wavefolder
The processor SHALL process one engine sample for any supported lane count, with independent positive physical gain and normalized Shape per lane. For `t=tanh(gain*input)` and `s=sin(gain*input)`, Shape SHALL linearly crossfade adjacent, equally spaced landmarks in this order: `t`, `t-t^3`, `t-t^2*s`, `t-t*s^2`, `t-s^3`, `s-s^3`, `s`. The sine and tanh SHALL each be evaluated once for the driven audio vector and reused by the polynomial landmarks. The processor SHALL use the sine table directly without a zero-input override and SHALL allocate no memory during processing.

#### Scenario: Endpoints and intermediate morphs
- **WHEN** Shape selects a landmark or a position between adjacent landmarks
- **THEN** the pre-DC result matches that landmark or their linear interpolation with the configured level compensation
- **AND** crossing each landmark remains continuous

#### Scenario: Independent lanes
- **WHEN** different lanes receive different gains, shapes, and input streams
- **THEN** each lane matches an independent mono processor

### Requirement: Safe level compensation and DC removal
Gain-dependent compensation SHALL use reachable peak bounds rather than only the value of a folding function at full input. The tanh endpoint SHALL retain normalized tanh behavior before DC removal. The pure and mixed cubic-fold bounds SHALL account for their interior extrema. A single 5 Hz one-pole high-pass per lane SHALL follow the morph, at the engine rate, retaining state across gain and shape changes. All outputs SHALL remain finite over the supported full-scale input and Sub drive ranges.

#### Scenario: Low drive retains the fundamental
- **WHEN** a steady sine is processed at any landmark with gain 0.125
- **THEN** the fundamental contains at least 99 percent of the measured odd-harmonic energy
- **AND** the settled output has negligible DC and even-harmonic energy

#### Scenario: Silent modulation and settling
- **WHEN** input is zero and filter state is settled while Gain and Shape change
- **THEN** output remains zero within floating-point precision
- **AND** previous signal history decays according to the 5 Hz pole instead of being reset by controls

#### Scenario: Fold endpoint crosses zero
- **WHEN** a gain makes a folding function zero at full input
- **THEN** compensation and output remain finite and interior signal content is preserved
