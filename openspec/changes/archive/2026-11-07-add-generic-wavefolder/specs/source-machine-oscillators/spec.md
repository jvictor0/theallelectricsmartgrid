## MODIFIED Requirements

### Requirement: Sub-Oscillator
Each voice SHALL run a sine sub-oscillator (`VCO`) at half the voice base frequency (one octave down) at the engine rate and emit it as a per-voice mono signal. The sub SHALL run every sample regardless of source machine. Its smoothed amp envelope SHALL multiply the sine before the generic wavefolder, whose output SHALL include a 5 Hz one-pole DC blocker before volume and voice gain.

Sub Drive SHALL span 0.125×–8× (`0.125 * 64^x` for normalized knob position `x`) and retain persisted identifier `SubTanhGain`. Sub Shape SHALL use appended identifier `SubShape`, label SHAP, default zero (tanh), and position (1,1) on the Sub page. Both controls SHALL follow the existing per-voice scene, modulation, trio, and patch persistence behavior. Existing normalized Drive values SHALL remain unchanged on load and use the new physical range.

Sub Volume SHALL use `(20^x - 1) / 19`, spanning exact silence to unity after shaping. The sub trigger SHALL latch into `m_subRunning` at each main AHD trigger. The exponential main amp envelope SHALL supply the sub envelope target when running, and zero otherwise. Bidirectional slew SHALL use exponential rise time 1 ms–1 s and fall time 1 ms–5 s. Frequency-dependent voice gain, Sub Volume, and voice gain SHALL scale the wavefolder output; `m_subOut` SHALL feed the center-panned mono mixer lane. The Sub scope SHALL expose shaped, DC-blocked audio before the final voice gain.

#### Scenario: Sub is silent without its trigger
- **WHEN** an AHD trigger arrives with the amp section's sub trigger input false
- **THEN** `m_subRunning` latches false and the smoothed sub envelope decays to zero
- **AND** the wavefolder's DC-blocker history decays to silence

#### Scenario: Sub level follows the main amp envelope
- **WHEN** the sub is running and the main AHD envelope decays
- **THEN** the sine's input amplitude into the wavefolder follows the smoothed exponential envelope, changing the amount of shaping as the note fades
- **AND** Sub Volume scales the result without changing the envelope or shaping

#### Scenario: Shape and drive survive patch round trips
- **WHEN** distinct Sub Drive and Shape values are saved for different trios and reloaded
- **THEN** each voice retains its controls and the new Drive mapping applies
- **AND** existing parameter identifiers remain stable
