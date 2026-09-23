# Recording Extraction Specification

## Purpose
Provide bounded-memory inspection and exact PCM24 WAV extraction of SmartGrid recordings, including recorded master outputs, stems, incomplete-session recovery, and iPad sync integration.

## Requirements

### Requirement: Streaming Recording Inspection
Python tools SHALL inspect the recording header and enumerate track IDs, names, types, roles, stream semantics, sample rate, block size, timestamp, build provenance, and tap positions. Reading and extraction SHALL process bounded blocks without loading a complete recording into memory. Unsupported versions or invalid audio metadata or framing SHALL produce actionable errors. Audio extraction SHALL support v1 and v2, parse header JSON without interpreting initial-patch contents, and skip v2 event trailers while checking whole-block CRCs.

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


### Requirement: Patch Reconstruction at a Recording Sample
`Reader.PatchAtSample(sample)` and `extract_recording.py patch INPUT --sample N` SHALL reconstruct a patch by copying the v2 `initial_patch` and applying recognized parameter entries with timestamps less than or equal to the requested sample. Each query SHALL start from the initial snapshot, check complete blocks through the target, and avoid decoding audio or building an index. Replay SHALL update the appropriate scene bytes in `nonagon` or `stateSaver` and keep their existing configGrid source-width/selection copies consistent. Untracked data SHALL retain its initial value; replay SHALL not infer missing edits or demand a whole-patch schema validator.

#### Scenario: Repeated query before and after a delta
- **WHEN** a state changes at sample 100 and queries request samples 99, 100, and then 99 again
- **THEN** the results contain the earlier value, the changed value, and the earlier value respectively
- **AND** the stored initial patch is unchanged

#### Scenario: Multiple changes at one sample
- **WHEN** several StateChange entries for one state and scene share the target timestamp
- **THEN** replay applies them in stored order and retains the final value

#### Scenario: Unsupported event type
- **WHEN** a block needed for patch reconstruction contains an event type the reader does not recognize
- **THEN** the query reports an explicit error
- **AND** audio extraction remains independent of event types and patch schemas

#### Scenario: Query outside the captured timeline
- **WHEN** a query specifies a negative sample, a sample past the last recorded audio frame, or a recording without an initial patch
- **THEN** reconstruction reports an error
- **AND** sample zero of a clean empty v2 session returns its initial patch

#### Scenario: Intact prefix followed by an incomplete tail
- **WHEN** the requested sample lies within a complete CRC-valid block before an incomplete tail
- **THEN** reconstruction returns the patch through that sample without requiring later completion

#### Scenario: Reconstruct source monitoring
- **WHEN** a recording contains `sourceMonitor_i` StateChange entries
- **THEN** patch reconstruction applies them to the global `stateSaver` values at the requested sample
- **AND** loading the reconstructed patch restores those monitor settings

### Requirement: Replay All Five Parameter Assignment Types
The reader SHALL decode StateChange, GestureSet, BlendSet, EncoderSet, and EncoderActivate using their type-specific payloads. GestureSet SHALL replace `faders[index]`; BlendSet SHALL replace top-level `blend`. EncoderSet SHALL replace the addressed node's `values.values[scene][track]` in patch units. EncoderActivate SHALL replace `active[scene * 16 + track]`. Traversal SHALL follow each tagged modulator/gesture hop from `squiggleBoy[root_name]`, preserving existing nodes and unrelated values. Missing nested nodes SHALL initialize neutral zero patch values with the parent's track count and inactive gestures. The root parameter SHALL already exist. Unsupported types, truncated payloads, invalid widths/indices/paths, nonfinite floats, and invalid activation bytes SHALL fail patch replay without affecting audio-only extraction.

#### Scenario: Nested encoder first created after recording starts
- **WHEN** an edit addresses a nested modulator or gesture absent from the initial patch
- **THEN** replay creates the neutral path and applies the value or activation at the correct scene and track
- **AND** a query before the edit retains the original patch

#### Scenario: Activation inherits a value
- **WHEN** gesture activation copies a parent value and records that value separately
- **THEN** replay restores both the copied value and activation independently of ordering between event types

#### Scenario: Exact block boundary and repeated queries
- **WHEN** mixed parameter events occur at the first sample of a new block
- **THEN** a query at the preceding sample excludes them, a query at the boundary includes them, and repeating either query yields the same patch

#### Scenario: Seeded engine edits reconstruct saved patches
- **WHEN** the existing seeded encoder/scene test records edits and saves patches at checkpoints
- **THEN** Python replay through each checkpoint sample equals the complete live saved patch
- **AND** the real engine can load a reconstructed patch containing all five event types
