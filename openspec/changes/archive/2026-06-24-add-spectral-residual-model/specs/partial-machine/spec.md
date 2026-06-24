## ADDED Requirements

### Requirement: Residual Spectral Analysis
The system SHALL compute an analysis residual on each Partial Machine analysis hop after extracting input atoms. Each organic `AnalysisAtom` SHALL store its analysis phase from the source DFT, and the analysis residual SHALL be produced by writing extracted organic analysis atoms back into the input DFT at opposite phase so the modeled partials cancel. The remaining DFT magnitudes SHALL be copied into `SpectralModel::ResidualModel::Input::m_analysisResidualMagnitudes` with one residual bucket per DFT component, so residual envelope bucket `k` equals the remaining DFT magnitude at component `k`. Synthetic harmonics SHALL NOT be canceled from the input residual because they are generated model content rather than original input content.

#### Scenario: Pure tracked partial leaves low residual
- **WHEN** the analysis frame contains a single sinusoid that is extracted as an organic atom
- **THEN** residual analysis stores that atom's analysis phase and writes the atom back into the DFT at opposite phase
- **AND** the residual bucket magnitudes near the atom frequency are lower than the original DFT magnitudes at those bucket frequencies

#### Scenario: Broadband energy remains after atom cancellation
- **WHEN** the analysis frame contains a sinusoid plus broadband noise
- **THEN** residual analysis cancels the extracted sinusoid from the DFT
- **AND** residual bucket magnitudes still report the remaining broadband energy component-by-component

#### Scenario: Synthetic harmonics do not erase input residual
- **WHEN** synthetic harmonics are enabled for atom tracking
- **THEN** residual analysis skips synthetic analysis atoms during DFT cancellation
- **AND** only organic input atoms are subtracted before residual magnitudes are sampled

### Requirement: Residual Model Bucket Smoothing
The system SHALL store a `ResidualModel` inside `SpectralModelGeneric`. The residual model SHALL contain a float magnitude array with `DFT::x_maxComponents` entries, fixed DFT bucket frequencies, and a fixed precomputed log-frequency helper array for parameter indexing. Its processing function SHALL take both `SpectralModel::Input` and `ResidualModel::Input`; for each DFT bucket, it SHALL map the precomputed log-frequency value to a frequency-dependent parameter index without recomputing the logarithmic conversion every hop, then fold the analysis residual magnitude into the stored residual magnitude using the existing attack and decay parameters for that index. The residual model SHALL expose a query function that returns the residual envelope at a requested DFT bucket index.

#### Scenario: Residual attack follows frequency-dependent lane
- **WHEN** a residual bucket receives a larger analysis residual magnitude than its stored magnitude
- **THEN** the residual model updates that bucket using `m_slewUpAlpha` evaluated at the parameter index for the bucket's precomputed log-frequency value

#### Scenario: Residual decay follows frequency-dependent lane
- **WHEN** a residual bucket receives a smaller analysis residual magnitude than its stored magnitude
- **THEN** the residual model updates that bucket using `m_slewDownAlpha` evaluated at the parameter index for the bucket's precomputed log-frequency value

#### Scenario: Equal parameter lanes make residual smoothing scalar
- **WHEN** all attack and decay parameter lanes contain equal values
- **THEN** residual buckets use the same smoothing behavior regardless of their bucket frequency

#### Scenario: Residual envelope query matches DFT component index
- **WHEN** Partial Machine synthesis asks the residual model for the envelope at DFT bucket `k`
- **THEN** the residual model returns the smoothed residual magnitude stored at index `k`

### Requirement: Residual Quad Synthesis
The Partial Machine SHALL contain a `ResidualMachine` that adds residual energy into the same `QuadDFT` frame as tracked partial atoms. For each target quad DFT bucket, the residual machine SHALL read the residual envelope at the same DFT bucket index, compute frequency-dependent reduction and quad pan placement from that bucket frequency, compute per-channel magnitude as `residualEnvelope * reduction * pan[channel]`, choose a random phase for the synthesis frame, create a complex value with that magnitude and phase, and add it directly into the target quad DFT component without using a windowed-partial write. The residual machine SHALL also apply the reduction-feedback parameter to the residual bucket's stored magnitude, writing the feedback-shaped reduced magnitude back into the residual model with the same floor policy used by tracked atom reduction feedback.

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
- **AND** the feedback target is the reduced residual magnitude using the same death-magnitude floor policy as tracked atom reduction feedback

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
- **THEN** residual synthesis adds its random-phase complex value to the existing component value
- **AND** it does not clear or replace the existing partial energy
