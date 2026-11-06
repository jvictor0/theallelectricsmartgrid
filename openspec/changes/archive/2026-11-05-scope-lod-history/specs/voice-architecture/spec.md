## MODIFIED Requirements

### Requirement: Per-Voice Scope Observability
The system SHALL write per-voice signals to dedicated scope channels via vector scope holders: audio scopes `VCO1`, `VCO2`, `PostFilter`, `PostAmp`, and `Sub` per voice, and control scopes `LFO1`, `LFO2`, `ModulationEnvelope`, and `AmpEnvelope` per voice. Every scope is written with a whole trio vector once per engine sample. Physical scope index SHALL be logicalScopeIx * 3 + trioIx, with lane equal to voiceIx % 3. The voice bank SHALL have 39 scopes of width three for all audio and control signals. It SHALL store each engine sample without base-rate subsampling, and record-start offsets SHALL retain fractional engine-sample coordinates. Envelope scopes get record-start marks on envelope trigger and record-end marks when the envelope returns to Idle; `PostFilter` cycles start at the source's cycle tops (`SquiggleBoySource::m_top`), with their fractional offsets.

#### Scenario: Amp envelope visible on its scope channel
- **WHEN** voice i plays a note
- **THEN** the `VoiceScopes::AmpEnvelope` channel for voice i shows the amp AHD output starting at the trigger sample and ending when the envelope state returns to Idle
- **AND** `VoiceScopes::PostAmp` for the same voice shows the corresponding audio amplitude contour
