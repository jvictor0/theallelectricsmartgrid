## MODIFIED Requirements

### Requirement: Residual Quad Synthesis
The Partial Machine SHALL contain a `ResidualMachine` that adds residual energy into the same `QuadDFT` frame as tracked partial atoms. For each target quad DFT bucket, the residual machine SHALL read the residual envelope at the same DFT bucket index, compute frequency-dependent reduction and quad pan placement from that bucket frequency, compute per-channel magnitude as `residualEnvelope * reduction * pan[channel]`, choose a random phase for the synthesis frame, create a complex value with that magnitude and phase, and write it with `WriteBinCenteredWindowedPartial`. That write SHALL add the on-bin Hann kernel `0.5` at bin `k` and `-0.25` at `k-1` and `k+1`, omit DC, omit a missing upper neighbor at the last stored bin, and leave true DC writes as a no-op. The residual machine SHALL also apply the reduction-feedback parameter to the residual bucket's stored magnitude, writing the feedback-shaped reduced magnitude back into the residual model. Zero residual envelopes SHALL remain zero. For positive envelopes, exponential feedback interpolation SHALL start at the actual envelope and use a target floor no greater than min(deathMagnitude, envelope), so feedback cannot manufacture residual energy from silence or raise an already quieter envelope to the death magnitude.

#### Scenario: Residual buckets share the partial synthesis frame
- **WHEN** a Partial Machine synthesis frame is built
- **THEN** tracked atoms and residual buckets are written into the same `QuadDFT`
- **AND** the frame is submitted to `QuadOLA` once for continuous quad output

#### Scenario: Residual reduction matches frequency response
- **WHEN** a residual bucket frequency is outside the bell-curve pass band
- **THEN** the residual machine reduces the queried envelope magnitude with the same `SynthesisContext::GetReduction` behavior used for atoms at that frequency

#### Scenario: Residual reduction feedback writes back to model
- **WHEN** residual synthesis processes bucket `k` with nonzero reduction feedback
- **THEN** it writes a feedback-shaped magnitude back into residual model magnitude `k`
- **AND** the feedback target is the reduced residual magnitude with a floor capped by both the death magnitude and the current envelope

#### Scenario: Residual pan follows bucket frequency
- **WHEN** a target quad DFT bucket is synthesized from the residual envelope
- **THEN** its quad distribution is computed from the same radius and azimuth mapping used for atoms at that bucket frequency

#### Scenario: Residual envelope and target bucket have matching indexes
- **WHEN** residual synthesis writes quad DFT component `k`
- **THEN** it uses residual model magnitude `k` as the envelope input for that component

#### Scenario: Residual phase is decorrelated per synthesis frame
- **WHEN** residual energy is added to target quad DFT buckets in a synthesis frame
- **THEN** each target bucket uses a random phase rather than the canceled input atom phase or a fixed zero phase

#### Scenario: Residual synthesis preserves existing DFT contents
- **WHEN** tracked atoms have already written partial energy into a quad DFT component
- **THEN** residual synthesis adds its Hann-windowed residual energy to the existing component value
- **AND** it does not clear or replace the existing partial energy

#### Scenario: Residual synthesis uses a bin-centered Hann kernel
- **WHEN** residual synthesis writes bucket `k` with complex value `v`
- **THEN** it adds `0.5 * v` to component `k` and `-0.25 * v` to the neighboring stored bins
- **AND** it does not write DC
- **AND** it omits a neighbor tap that would fall outside the stored DFT bins

#### Scenario: Silent input does not seed residual noise
- **WHEN** a fresh Partial Machine processes zero input across multiple analysis and synthesis hops
- **THEN** its residual envelopes and quad output remain zero for any reduction-feedback amount

#### Scenario: Quiet residual tails decay below the floor
- **WHEN** a residual envelope is below the death magnitude and analysis slews it toward silence
- **THEN** feedback does not raise it back to the death magnitude
- **AND** the envelope continues following its analysis decay
