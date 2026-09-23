# Streaming Recording Specification

## Purpose
Define the SMRTGRID container and realtime capture lifecycle for lossless, sparse, independently compressed multitrack performance recordings.

## Requirements

### Requirement: Versioned Session Metadata
The performance recorder SHALL write a `.sgrec` v4 container with the eight ASCII magic bytes `SMRTGRID`, a little-endian JSON byte length, and a UTF-8 JSON header containing `format_version: 4`, UTC session creation time, full build Git SHA, sample rate, nominal block frame count, an immutable track table, and the complete live patch in `initial_patch`. The table SHALL contain stable numeric IDs, names, types, roles, and taps. Type SHALL determine stream count, order, and semantics: `mono` has `sample_val`; `panned_mono` has `sample_val`, `x`, `y`; `stereo` has left/right; `quad` has q0 through q3. Quad corner order and integer conversions SHALL be fixed by v1, with corners `(0,1)`, `(1,1)`, `(1,0)`, `(0,0)`.

#### Scenario: Header describes an independently readable session
- **WHEN** a new recording is opened by a reader
- **THEN** the reader can identify every track, its ordered streams and scaling, the sample rate, date, and compiled revision by reading only the header
- **AND** panned mono has ordered streams `sample_val`, `x`, `y`

#### Scenario: Provenance follows the compiled checkout
- **WHEN** a maintained macOS, iOS, or standalone test build is rebuilt after a Git revision change
- **THEN** its next recording reports the revision compiled into that build without invoking Git on the recording thread

### Requirement: Deterministic PCM24 and Position Quantization
The writer SHALL preserve the existing audio conversion: clamp to [-1,1], multiply by 8388607, and round to nearest with ties away from zero. Positions SHALL clamp to [0,1], use the same scale and rounding rule, and occupy nonnegative signed PCM24 codes. The codec SHALL support the full signed PCM24 integer range [-8388608,8388607] and preserve encoded integers exactly without dither. Capture SHALL reject a frame containing a non-finite value with an explicit recording error before accepting that frame.

#### Scenario: Audio and coordinate endpoints
- **WHEN** audio values -1, 0, 1 and positions 0, 0.5, 1 are recorded
- **THEN** the audio codes are -8388607, 0, 8388607 and position codes are 0, 4194304, 8388607
- **AND** raw and delta decoding return those exact codes

#### Scenario: Invalid float ends the accepted prefix
- **WHEN** a submitted frame contains NaN or infinity
- **THEN** that frame is not accepted, the recording reports invalid sample, and already accepted frames remain eligible to drain
- **AND** live audio processing does not wait for file finalization

### Requirement: Contiguous Independent Sparse Blocks
The writer SHALL use one-second blocks by default (`block_frames = sample_rate`), permit another declared positive block size within format/memory limits, and write positive frame counts with contiguous start-frame indices beginning at zero. Every included track SHALL supply all its streams for every frame of that block. Mono, stereo, and quad tracks SHALL be omitted exactly when all their quantized audio values are zero; panned mono SHALL be omitted exactly when its audio stream is entirely zero. Missing tracks SHALL decode as silence with canonical zero coordinates; missing whole blocks SHALL be an error. A final partial block SHALL preserve its actual frame count without padding.

#### Scenario: Silence preserves duration
- **WHEN** an entire one-second interval is silent in a 48 kHz session
- **THEN** a block with frame count 48000 and zero included tracks represents that interval
- **AND** the next block starts 48000 frames later

#### Scenario: Silent moving panned track
- **WHEN** a panned-mono track has zero quantized audio throughout a block but moving nonzero coordinates
- **THEN** the track is omitted and extraction emits silence for that interval
- **AND** if a later block contains audio, that block carries all audio and coordinate values independently of the previous block

#### Scenario: Partial final block
- **WHEN** a session accepts 48123 frames with block size 48000 and then stops
- **THEN** it contains contiguous data blocks of 48000 and 123 frames and terminal total 48123

#### Scenario: Source configuration changes without changing track identity
- **WHEN** an external source changes between mono and stereo while recording
- **THEN** its registered panned-mono lane IDs and stream counts remain unchanged while their samples and coordinates reflect live processing

### Requirement: Bounded FLAC Stream Encoding
Each stream descriptor SHALL contain encoding and width, one byte each. Raw (0/24) SHALL use three little-endian two's-complement bytes per value. Legacy delta (1/0..25) SHALL remain readable with one PCM24 first value and adjacent mathematical differences, packed LSB first with zero high padding, with length `3 + ceil((N-1)*width/8)`. The v4 writer SHALL use 1/0 for whole-constant streams. V4 SHALL support FLAC (2/0): a little-endian u32 byte length excluding itself followed by a complete native FLAC stream. STREAMINFO SHALL declare mono PCM24 at the session sample rate and exactly the outer block frame count. Internal FLAC blocks SHALL be bounded to 1024 samples; the writer SHALL use compression level 5.

The writer SHALL traverse each source integer once, combining validation and activity detection while feeding fixed-size scratch to FLAC. FLAC MAY revisit its bounded internal blocks. A nonconstant stream fitting within scratch MAY use raw when strictly smaller than its length-prefixed FLAC payload, without rereading the source. Longer streams MAY use FLAC verbatim subframes. Encoding and allocation SHALL remain on the recording worker. Predictors SHALL reset in every scalar stream and outer block. Unshipped adaptive v4 experimentation is superseded by this layout.

#### Scenario: Audio and constant coordinates
- **WHEN** a block contains changing audio and constant x/y coordinates
- **THEN** the audio uses FLAC, or raw for a short stream when smaller, while each coordinate stores one PCM24 value

#### Scenario: Exact PCM24 and partial final blocks
- **WHEN** a stream contains -8388608 and 8388607 or ends partway through an internal FLAC block
- **THEN** decoding reconstructs every original integer and exactly the outer frame count

#### Scenario: Bounded worker processing
- **WHEN** the encoder processes a one-second multitrack recording block
- **THEN** each scalar source is consumed once through 1024-sample scratch
- **AND** the real-time audio callback performs no FLAC calls or encoder allocation

### Requirement: Validated Framing and Termination
The writer SHALL use `BLK4` with the existing 22-byte fixed header, track IDs, two-byte stream descriptors and audio payloads, followed by grouped events and CRC-32/ISO-HDLC covering the whole record. Stream counts SHALL derive from track type and payload lengths from legacy formulas or explicit FLAC byte lengths; the wire format SHALL contain no repeated stream counts or reserved padding. Clean completion SHALL write the fixed 16-byte `END1` marker containing total frames and CRC. The total SHALL match the contiguous written frames. Capture errors SHALL omit the completion marker and report their cause through runtime status/logging. Python audio decoding SHALL validate audio metadata/record bounds, IDs, encodings, reconstructed ranges, audio lengths, legacy padding bits, FLAC stream/frame dimensions and integrity, CRCs, continuity, and completion. It SHALL skip event trailers and leave initial-patch contents uninterpreted. Patch queries SHALL decode only the metadata and event data needed for reconstruction.

#### Scenario: Corrupt or hostile record
- **WHEN** a block has a bad CRC, duplicate track ID, invalid width, impossible payload length, or exceeds a declared format bound
- **THEN** the reader rejects it without unchecked allocation, overread, integer overflow, or treating it as silence

#### Scenario: Recording stops before any frame
- **WHEN** an activated session with an initial snapshot stops with no accepted frames
- **THEN** its file has a valid header and clean terminal record with zero frames and no zero-length data block

#### Scenario: Interrupted recording
- **WHEN** a file ends partway through a block or lacks its terminal record
- **THEN** extraction keeps only the preceding fully validated contiguous blocks and reports an incomplete session with nonzero exit status

### Requirement: Bounded Realtime Capture and Worker Ownership
The recorder SHALL accept one complete frame at a time through one preallocated SPSC ring of frame-major pages, using the existing queue's in-place producer and consumer operations. The worker SHALL release a slot only after consuming its page; there SHALL be no separate free-page queue. The dedicated writer SHALL own transposition to per-stream blocks, quantization, activity detection, compression, checksums, file operations, and finalization. The audio path SHALL perform no allocation/deallocation, locking, sleeps, thread startup/join, or disk operations. Stop/error signaling SHALL remain available when the data ring is full.

#### Scenario: Normal capture transfers page ownership
- **WHEN** the producer fills and publishes a capture page
- **THEN** the worker reads through `PeekPtr` and calls `Pop` only after consuming the page, before the producer can reuse that slot
- **AND** publication does not copy a full page through the queue

#### Scenario: Slow writer exhausts available pages
- **WHEN** no free page is available for the next recording frame
- **THEN** capture stops at that frame boundary, exposes overrun in runtime status, and drains the accepted prefix without a completion marker where the sink permits
- **AND** the audio callback does not block, overwrite unread data, or continue with unmarked missing frames

### Requirement: Asynchronous Recording Lifecycle and Observable Errors
The recorder SHALL expose idle, recording, stopping, and error status through the project's atomic state patterns. Preparation SHALL allocate resources off-thread. Start SHALL immediately activate audio and event capture on the audio thread while the writer opens the file asynchronously; stop SHALL publish the final partial page and return without draining on the audio thread. Repeated stop SHALL be harmless, immediate stop SHALL preserve accepted data even before the file opens, and starts while busy SHALL not create overlapping sessions. The worker SHALL acknowledge drained closure before resources are reused. Shutdown SHALL quiesce capture, drain and join the writer off-thread, and then release recorder storage.

#### Scenario: Open or write failure
- **WHEN** opening a recording fails or a later disk write fails
- **THEN** the recorder exposes a persistent error reason instead of reporting successful recording or completion
- **AND** live audio remains independent of file finalization

#### Scenario: Stop and restart
- **WHEN** stop is requested during a partial page and a new start arrives before draining completes
- **THEN** stop returns promptly, all accepted frames are finalized once, and the new start is rejected as busy until closure is acknowledged

#### Scenario: Destruction while work is queued
- **WHEN** application shutdown begins with accepted frames still queued
- **THEN** audio capture is quiesced, its tail is published, and the worker is drained and joined before any referenced buffers or session metadata are destroyed


### Requirement: Initial Patch and Sample-Aligned Parameter Events
The recorder SHALL snapshot the complete live patch, including unsaved edits, on the audio thread directly in the recording start call and pass that snapshot to the recorder. Start SHALL immediately establish sample zero and accept audio and subsequent parameter edits without waiting for file opening. Snapshot storage SHALL be preallocated and independent of ordinary patch saving. The worker SHALL serialize the immutable snapshot to `initial_patch` before writing blocks while audio capture continues into its queues. Every captured parameter edit SHALL copy its relevant fields and recording-relative sample index without allocating on audio. StateChange SHALL capture the specified scene buffer; EncoderSet SHALL capture the stored value converted to patch JSON units, before smoothing or modulation.

#### Scenario: Edits while the file opens
- **WHEN** the patch changes after the start call while the worker opens the file
- **THEN** the initial snapshot retains the patch at the start call and subsequent edits are queued as events alongside the audio
- **AND** event sample indices use the start sample as their origin, including edits later in sample zero

#### Scenario: Immediate stop before the file opens
- **WHEN** recording stops before the worker opens the file
- **THEN** the worker writes the initial snapshot and drains every accepted frame and its events before completion
- **AND** a session with no accepted frames writes a valid zero-frame completion marker

#### Scenario: Source-monitor edits
- **WHEN** a source-monitor control is toggled during recording
- **THEN** it emits an ordinary StateChange for the registered `sourceMonitor_i` state
- **AND** the initial patch stores monitor values in the global `stateSaver` section

#### Scenario: Repeated edits within one block
- **WHEN** one named state changes repeatedly, including several changes at one sample
- **THEN** the writer stable-sorts by type, name, and sample and writes one group per type/name with all entries
- **AND** exact ties preserve capture order, with no value deduplication

#### Scenario: Silent or partial block with events
- **WHEN** edits occur during silent audio or a final partial block
- **THEN** the block retains every event within its accepted audio interval and protects events with its existing CRC
- **AND** events at the next boundary belong to the next block

#### Scenario: State event queue overflow
- **WHEN** the prepared event queue cannot accept an edit
- **THEN** recording reports overrun and omits clean completion rather than silently losing a delta

### Requirement: Compact Typed Parameter Events
Event groups SHALL use type:u8, value_width:u8, name_length:u16, entry_count:u32, then UTF-8 name bytes. All v3/v4 entries SHALL begin with a block-relative sample:u32 and capture-order:u32 assigned from original event order within the block before grouping. StateChange (1) SHALL append scene:u8 and 1/2/4/8 value bytes. GestureSet (2) SHALL append fader_index:u8 and float32. BlendSet (3) SHALL append float32. Both unnamed types SHALL have zero name length. EncoderSet (4) and EncoderActivate (5) SHALL use the root parameter name and append scene:u8, track:u8, path_length:u8, path bytes, then float32 or active:u8 respectively. Floats SHALL be finite little-endian IEEE-754 binary32; activation SHALL be 0 or 1. Irrelevant ParamEvent fields SHALL NOT be serialized.

Encoder paths SHALL contain at most 16 hops, with modulator indices 0..14 or gesture indices encoded as 128..143. The root SHALL have an empty path; activation SHALL end at a gesture. Scenes SHALL be 0..7, tracks and faders 0..15. In-memory unused path slots SHALL be initialized to -1 and SHALL NOT appear on the wire.

#### Scenario: Mixed parameter types in one block
- **WHEN** states, faders, blend, encoder values, and gesture activation change within a block
- **THEN** the writer groups all five types by type/name, preserves stable sample order, and emits only each type's specified fields
- **AND** unnamed events do not serialize a name, scene, or encoder path

#### Scenario: Nested encoder edit and activation
- **WHEN** a nested normal modulator or its gesture leaf changes
- **THEN** the event path identifies the kind and index of every hop
- **AND** activating a gesture that copies a parent value also emits the resulting EncoderSet value

#### Scenario: State scene copy or reset
- **WHEN** an individual StateSaver copy or reset writes a scene buffer during recording
- **THEN** StateChange copies that scene's bytes even when the live pointer represents a different scene

### Requirement: Bulk Patch Loads and Reset Snapshots
Patch loading SHALL assign raw state, encoder values, activation, and configuration without per-field parameter events. The engine SHALL record one PatchLoad (6) containing the input JSON and restoreFaders policy at the sample where the load takes effect. A saved-pad reload SHALL use the same mechanism with restoreFaders false. Saving JSON SHALL NOT itself emit parameter assignments. Whole-patch reset SHALL suppress individual reset deltas and emit one PatchSnapshot (7) containing the complete resulting patch. Snapshot storage SHALL be preallocated; a busy reset arena SHALL defer the reset without blocking audio.

Both new event types SHALL have an empty name and value_width 0. PatchLoad SHALL append restoreFaders:u8, json_length:u32, and UTF-8 JSON bytes after sample/order. PatchSnapshot SHALL append json_length:u32 and UTF-8 JSON bytes. Replay SHALL use chronological sample/order across types and names, applying partial-load semantics for PatchLoad and whole-patch replacement for PatchSnapshot.

#### Scenario: Full patch load during recording
- **WHEN** a patch load replaces thousands of fields and removes old encoder children
- **THEN** one bulk event records the operation without overflowing the parameter queue
- **AND** replay removes the replaced children and honors restored or preserved faders and blend

#### Scenario: Edits around a same-sample load
- **WHEN** parameter edits occur before and after a patch load at the same sample
- **THEN** reconstruction applies them in their capture order despite type/name grouping

#### Scenario: Whole-patch reset
- **WHEN** a new-patch request resets the engine during recording
- **THEN** replay uses the resulting full snapshot and subsequent edits at that sample remain effective

### Requirement: Retained Patch Storage
Each queued patch reference SHALL retain its backing arena until the worker has copied the JSON into worker-owned bytes. Arena reset/growth SHALL require exclusive ownership with no readers. Busy message-thread loads SHALL be deferred and retried before parsing; saves and saved-pad reloads SHALL defer on audio without waiting. The worker SHALL release every retained reference on success, failure, overflow, shutdown, and discarded-tail paths. Pending serialized payloads SHALL count toward the existing bounded pending-event storage.

#### Scenario: Repeated reloads while the writer is delayed
- **WHEN** multiple queued reloads reference one arena
- **THEN** that arena remains immutable until all references have been copied or discarded
- **AND** a subsequent parse or save cannot overwrite those queued patches

#### Scenario: Prompt release before block completion
- **WHEN** the worker drains a bulk event before its one-second audio block is ready
- **THEN** it serializes the patch and releases the arena immediately, retaining owned bytes until block writing

Sample-directory edits and sample assets remain intentionally outside capture coverage while the sample-recording feature is unfinished. The encoder path limit remains 16 hops.
