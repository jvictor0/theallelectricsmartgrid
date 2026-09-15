## ADDED Requirements

### Requirement: Versioned Session Metadata
The performance recorder SHALL write a `.sgrec` v1 container with the eight ASCII magic bytes `SMRTGRID`, a little-endian JSON byte length, and a UTF-8 JSON header containing `format_version: 1`, UTC session creation time, full build Git SHA, sample rate, nominal block frame count, and an immutable track table. The table SHALL contain stable numeric IDs, names, types, roles, and taps. Type SHALL determine stream count, order, and semantics: `mono` has `sample_val`; `panned_mono` has `sample_val`, `x`, `y`; `stereo` has left/right; `quad` has q0 through q3. Quad corner order and integer conversions SHALL be fixed by v1, with corners `(0,1)`, `(1,1)`, `(1,0)`, `(0,0)`.

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

### Requirement: Per-Stream Raw or Minimally Packed Delta Encoding
Each stream descriptor SHALL contain only encoding and bit width, one byte each. Raw SHALL use width 24 and three little-endian two's-complement bytes per sample. Delta SHALL store one PCM24 first value followed by mathematical adjacent differences at the minimum signed two's-complement width needed for that stream in the block, using width zero for constant streams and supporting widths through 25. Packed deltas SHALL be least-significant-bit first, byte-aligned per stream, with zero padding bits. Payload lengths SHALL be derived as `3*N` for raw and `3 + ceil((N-1)*width/8)` for delta. The writer SHALL select delta only when strictly smaller, and raw on ties. Predictors SHALL reset in every block.

#### Scenario: Different encodings within one panned track
- **WHEN** a block contains high-entropy audio and constant x/y coordinates
- **THEN** the audio can use raw while each coordinate uses delta width zero with only its first value stored

#### Scenario: Signed minimum widths
- **WHEN** a stream's differences are exclusively -1 and 0
- **THEN** its delta candidate uses one bit per difference
- **AND** differences including +1 require at least two signed bits

#### Scenario: Full-range differences do not wrap
- **WHEN** adjacent PCM values are -8388608 and 8388607
- **THEN** the delta is 16777215 computed without PCM24 wrapping
- **AND** the encoder uses raw when the corresponding 25-bit delta candidate is not smaller

#### Scenario: Small and non-byte-aligned streams
- **WHEN** a stream contains one value or its packed deltas end partway through a byte
- **THEN** a single-value tie selects raw and unused final high bits of packed streams are zero
- **AND** a decoder reconstructs exactly the declared number of values

### Requirement: Validated Framing and Termination
The writer SHALL use the v1 `BLK1` layout with its 22-byte fixed header, track IDs, two-byte stream descriptors, payloads, and CRC-32/ISO-HDLC. Stream counts SHALL derive from track type and payload lengths from the encoding formulas; the wire format SHALL contain no repeated stream counts, per-stream lengths, or reserved padding. Clean completion SHALL write the fixed 16-byte `END1` marker containing total frames and CRC. The total SHALL match the contiguous written frames. Capture errors SHALL omit the completion marker and report their cause through runtime status/logging. Python SHALL own decoding and validate metadata/record bounds, IDs, encodings, reconstructed ranges, derived lengths, padding bits, CRCs, continuity, and completion before accepting data.

#### Scenario: Corrupt or hostile record
- **WHEN** a block has a bad CRC, duplicate track ID, invalid width, impossible payload length, or exceeds a declared format bound
- **THEN** the reader rejects it without unchecked allocation, overread, integer overflow, or treating it as silence

#### Scenario: Recording stops before any frame
- **WHEN** a prepared session stops with no accepted frames
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
The recorder SHALL expose idle, starting, recording, stopping, and error status through the project's atomic state patterns. Start SHALL prepare the session off-thread and activate capture only on a whole-frame boundary after readiness; stop SHALL publish the final partial page and return without draining on the audio thread. Repeated stop SHALL be harmless, stop during starting SHALL cancel, and starts while busy SHALL not create overlapping sessions. The worker SHALL acknowledge drained closure before resources are reused. Shutdown SHALL quiesce capture, drain and join the writer off-thread, and then release recorder storage.

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
