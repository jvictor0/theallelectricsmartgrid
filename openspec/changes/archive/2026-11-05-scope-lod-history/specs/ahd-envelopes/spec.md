## MODIFIED Requirements

### Requirement: Two Envelope Instances Per Voice With Scope Output
Each voice SHALL host two AHD instances driven by the same `AHDControl` timing — the main amp envelope (`m_ahd`) and the modulation envelope (`m_modulationAHD`) — with independent attack, hold, decay, and amplitude parameters set through separate `InputSetter`s. Their outputs SHALL be written as whole trio vectors on every engine sample to the per-voice scope channels `VoiceScopes::AmpEnvelope` and `VoiceScopes::ModulationEnvelope`, which store every engine sample at level zero and are accepted by the UI before painting, with record-start marked on trigger and record-end marked when the envelope transitions to Idle.

#### Scenario: Independent shapes from shared timing
- **WHEN** a voice is triggered with the amp envelope set to a short decay and the modulation envelope set to a long decay
- **THEN** both envelopes enter attack on the same sample, the `AmpEnvelope` scope trace returns to zero first, and the `ModulationEnvelope` trace continues until its own decay completes

#### Scenario: Scope recording brackets the envelope
- **WHEN** voice i's amp envelope is triggered and later returns to Idle
- **THEN** the `VoiceScopes::AmpEnvelope` channel records a start mark at the trigger sample and an end mark on the sample `m_changed` is set with state Idle in engine-sample coordinates
