## ADDED Requirements

### Requirement: Streaming Recording Inspection
Python tools SHALL inspect the recording header and enumerate track IDs, names, types, roles, stream semantics, sample rate, block size, timestamp, build provenance, and tap positions. Reading and extraction SHALL process bounded blocks without loading a complete recording into memory. Invalid versions, metadata, or records SHALL produce actionable errors.

#### Scenario: Inspect a long recording
- **WHEN** the user inspects a multi-hour recording
- **THEN** track metadata is available without decoding all sample payloads
- **AND** any requested full integrity scan uses memory bounded by the header and one block

### Requirement: Exact Direct PCM24 WAV Extraction
The tools SHALL export selected mono, stereo, and quad tracks to PCM24 WAV with recorded sample rate, channel order, duration, and exact decoded integer values. Missing tracks SHALL produce the appropriate number of zero samples for each block. The tools SHALL support large outputs with RF64 and 64-bit counters when ordinary RIFF lengths cannot represent the result. Quad exports SHALL retain q0 through q3 and disclose their corner mapping without asserting unverified physical speaker positions. Existing files SHALL require explicit overwrite authorization.

#### Scenario: Silent and partial blocks preserve timing
- **WHEN** a selected track is absent from a full block and present in a final partial block
- **THEN** its WAV contains a full block of silence followed by the exact partial-block samples without extra tail padding

#### Scenario: Large output
- **WHEN** an extracted output exceeds RIFF's representable size
- **THEN** the tool writes RF64 with correct 64-bit byte and frame counts while streaming sample data

### Requirement: Direct Master Stereo and Quad Export
The tools SHALL provide explicit master-stereo and master-quad export selectors resolved by unique metadata roles. They SHALL copy the respective decoded recorded streams into WAV without reapplying mastering, applying listening volume, deriving stereo from quad, or normalizing gain.

#### Scenario: Export both recorded masters
- **WHEN** the user extracts master stereo and master quad from one recording
- **THEN** the results have respectively two and four channels with identical frame counts and the exact recorded master integers
- **AND** both preserve the post-mastering, pre-listening-volume tap

### Requirement: Panned Mono Stem Export
Panned-mono extraction SHALL write its decoded `sample_val` stream directly as a mono PCM24 WAV with the exact recorded audio integers and timeline. Inspection SHALL identify the source as panned mono and describe its x/y streams. Version-one extraction SHALL not add coordinate WAVs, sidecars, or spatial rendering.

#### Scenario: Extract a panned input as a mono stem
- **WHEN** the user selects a panned-mono track for extraction
- **THEN** the tool writes a mono WAV of its `sample_val` stream
- **AND** omitted blocks contribute the correct duration of silence without applying the recorded pan coordinates

### Requirement: Extract Valid Blocks and Report Incomplete Sessions
Extraction SHALL validate each whole block before emitting it. On invalid or truncated data, it SHALL stop at that record, finalize a playable WAV from preceding valid blocks, report the extracted frame count and reason, and exit nonzero. Missing or invalid completion markers SHALL also produce nonzero status. A complete valid recording SHALL return zero. An invalid session header or unknown selected track SHALL produce no WAV. There SHALL be one extraction behavior without separate strict/recovery modes; extraction SHALL not skip corruption or synthesize silence for missing whole blocks.

#### Scenario: Truncated final block
- **WHEN** a recording ends halfway through its final data block
- **THEN** the tool exports only the preceding complete validated blocks and finalizes their WAV header
- **AND** it reports an incomplete recording and exits nonzero

#### Scenario: Capture overrun leaves no completion marker
- **WHEN** a recording has valid blocks but no completion marker because capture stopped with overrun
- **THEN** the tool exports the contiguous recorded prefix and exits nonzero for incomplete recording
- **AND** it does not invent a failure cause that is absent from the file

### Requirement: Legacy-Aware Sync Extraction
The existing iPad recording-sync helper SHALL identify `.sgrec` by its `SMRTGRID` magic and invoke master-stereo extraction, while preserving the existing SoX extraction path for legacy RIFF/RF64 recordings. It SHALL delete a remote recording only after successful download and a zero extraction exit status confirming a complete valid recording. A playable partial WAV SHALL not authorize remote deletion. Format dispatch SHALL not require actual device access in automated tests.

#### Scenario: New recording sync
- **WHEN** the sync helper downloads a valid `.sgrec` recording
- **THEN** it exports the track with role `master_stereo` before remote deletion
- **AND** it does not pass the custom container to SoX as a WAV

#### Scenario: Extraction failure retains the remote original
- **WHEN** a downloaded recording is truncated, corrupt, or lacks a valid completion marker
- **THEN** sync reports extraction failure and does not delete the remote original

#### Scenario: Existing WAV remains supported
- **WHEN** sync downloads a legacy multichannel RIFF/RF64 recording
- **THEN** stereo extraction continues using the last two channels through the existing SoX path
