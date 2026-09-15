## Context

The current performance recorder runs through `QuadMixerInternal` → `MultichannelWavWriter` → `FileWriter` → `CircularByteQueue`. It quantizes and packs samples on the caller, can sleep when the byte queue fills, and joins the worker during close.

The current engine supplies 17 panned inputs (nine voices and eight external source lanes), nine mono sub lanes, and three quad returns. Each panned input and its mono lane currently share a recorded quad stem. Both mastered outputs are recorded inside `ProcessReturns`, before `SquiggleBoy` applies the global listening-volume control.

Sampler-looper recording through `RecordingManager`, `RecordingBuffer`, and `IoTaskThread` is separate. Its WAV utilities stay in place. The existing iPad sync helper extracts stereo from downloaded recordings before deleting the remote original.

## Goals / Non-Goals

**Goals:**

- Record typed tracks in independent compressed blocks with exact PCM24 decoding.
- Keep capture bounded and move transpose, quantization, compression, and disk work to one writer.
- Preserve both mastered outputs before listening volume.
- Provide streaming Python inspection and straightforward WAV extraction.

**Non-Goals:**

- Changing live DSP, sampler-looper recording, or the existing audio quantizer.
- A production C++ decoder, coordinate WAVs, metadata sidecars, spatial rendering, or remastering from stems.
- Dirty-state provenance, source-archive build support, additional codecs, or an index.

## Decisions

### 1. Session header and tracks

Files use extension `.sgrec`. All binary integers are little-endian and serialized explicitly without struct padding.

| Field | Representation |
| --- | --- |
| Magic | Eight ASCII bytes `SMRTGRID` |
| JSON byte length | `u32` |
| Session header | Exactly that many UTF-8 bytes |

Required JSON fields are `format_version: 1`, `recorded_at_utc`, `git_commit_sha`, `sample_rate`, `block_frames`, and `tracks`. The timestamp is ISO 8601 UTC session creation time on the worker. Frame zero is the first accepted audio frame after preparation succeeds. Use the engine capture rate, currently `SampleTimer::x_sampleRate = 48000`.

Each track declares `id` (unique `u32`), `name`, `type`, `role`, and `tap`; optional `source` metadata can identify a voice/source/lane/effect. Type determines stream count, names, and semantics; do not repeat that schema in each track.

| Type | Ordered streams |
| --- | --- |
| `mono` | `sample_val` |
| `panned_mono` | `sample_val`, `x`, `y` |
| `stereo` | `left`, `right` |
| `quad` | `q0`, `q1`, `q2`, `q3` |

Version 1 fixes quad corner order to `(0,1)`, `(1,1)`, `(1,0)`, `(0,0)`. Master tracks have unique roles `master_quad` and `master_stereo` with tap `post_mastering_pre_master_volume`.

Register all mixer lanes before capture. The table remains fixed for the session; external source width changes affect samples and pan positions, not identity or type. A structural layout or capture-rate change ends the session with a runtime error.

Embed the full Git commit SHA at build time through a small generated header used by both JUCE exporters and the standalone CMake target. Preserve existing prebuild hooks and refresh the value on a changed-HEAD rebuild without cleaning. Supported builds run from the repository checkout; there is no new source-archive override mechanism or dirty-state field. Git never runs at recording time.

### 2. Preserve PCM conversion

For finite audio, retain the existing `WavWriter` conversion:

`round_away_from_zero(clamp(sample, -1, 1) * 8388607)`

This produces codes from -8388607 through 8388607. The codec still accepts the full signed PCM24 range [-8388608,8388607]. Compression and direct WAV extraction preserve integers exactly; no dither or normalization is added.

Positions use `round_away_from_zero(clamp(position, 0, 1) * 8388607)`. Decode coordinates by dividing by 8388607. Codes for 0, 0.5, and 1 are 0, 4194304, and 8388607. These conversion rules are fixed by format version rather than repeated as configurable header fields.

A non-finite submission causes a recording error before its frame is accepted. Finiteness checks are bounded capture work; quantization remains worker-side.

### 3. Independent sparse blocks

Start with one-second blocks: `block_frames = sample_rate`. Declare the size in the header so it can be tuned later; readers use that value. Full blocks contain that many frames, and the final block can be shorter without padding.

| Block field | Representation |
| --- | --- |
| Tag | Four ASCII bytes `BLK1` |
| Total record bytes | `u32`, including header, descriptors, payloads, and CRC |
| Start frame | `u64`, relative to session start |
| Frame count | Positive `u32`, at most `block_frames` |
| Included track count | `u16` |
| Track descriptors | Ascending track-ID order |
| Stream payloads | Descriptor order, independently byte-aligned |
| Checksum | `u32` CRC-32/ISO-HDLC |

The fixed block header is 22 bytes. Each track descriptor is `track_id:u32` followed by two bytes per stream: `encoding:u8`, `bit_width:u8`. Stream count comes from track type; payload lengths come from the encoding formulas below. There are no reserved bytes, repeated stream counts, or per-stream length fields. The derived descriptors and payloads plus the fixed header and four-byte CRC must exactly match total record bytes.

CRC covers all record bytes preceding the checksum. Use reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`.

Omit mono/stereo/quad tracks when all their quantized audio is zero for the whole block. Omit panned mono when its audio stream is entirely zero, regardless of coordinates. Omitted tracks decode to zero streams, including canonical zero coordinates. Included panned tracks retain every coordinate, even at locally silent frames. An entirely silent block still has a record with zero included tracks.

Start frames are contiguous from zero; a missing whole block is an error, never implicit silence. A short block must be the final data block. Every block resets all predictors.

Clean completion writes a fixed 16-byte marker: `END1`, `total_frames:u64`, `crc32:u32`. CRC covers its first 12 bytes. The total must match the contiguous data frames. A clean zero-frame session has a header and this marker only. Capture errors drain the accepted prefix where possible and omit the completion marker; error reasons remain in runtime status/logging.

The Python reader validates version, types, unique IDs, encoding widths, derived lengths, PCM/position ranges, padding bits, CRCs, frame continuity, and completion. Apply simple limits before allocation: 1 MiB JSON, 128 tracks, 64 MiB per encoded record, and 64 MiB per decoded block. Reject configurations exceeding those limits using checked size arithmetic. Do not scan past corruption or accept data after completion.

This keeps enough framing to detect damage and extract intact audio without introducing a separate recovery protocol.

### 4. Raw and minimally packed deltas

For a stream with `N >= 1` values:

- Encoding 0 (`raw`): width 24, length `3*N`, little-endian two's-complement PCM24.
- Encoding 1 (`delta`): first value as PCM24, followed by mathematical adjacent differences. Compute in widened integers without wrapping; differences can need 25 signed bits.
- Choose the smallest signed two's-complement width representing every difference. Width zero represents an all-zero difference sequence, including the empty sequence for `N = 1`; otherwise use widths 1 through 25.
- Pack each difference's low `width` bits least-significant-bit first, into consecutive low-to-high bits of bytes. Unused high bits in the final byte are zero.
- Delta length is `3 + ceil((N-1)*width/8)`. Choose delta only when smaller than `3*N`; raw wins ties. Both descriptors are two bytes.

Constant pan streams therefore need only their first value. Differences -1 and 0 fit one signed bit; +1 needs two. A transition from -8388608 to 8388607 needs 25 bits and selects raw when delta would cost more. Python decoding checks reconstructed values; a valid nonminimal width can be decoded even though the writer always chooses the minimum.

C++ implements encoding only. Python owns decoding, including validation. Independent golden bytes and C++-written/Python-read fixtures provide cross-language verification without a second production reader.

### 5. Mixer integration

Keep the start/stop/toggle and typed sample-submission shape, using pre-resolved track IDs. A panned submission supplies `(sample_val,x,y)`; an explicit frame commit prevents skipped submissions from shifting the file.

For ordinary input `i`, reuse its existing meter reduction:

- Panned audio: `m_input[i] * m_gain[i].m_expParam * reduction`, with current x/y.
- Mono audio, where `i < m_numMonoInputs`: `m_monoIn[i] * reduction`.

This preserves the shared voice/sub reduction and existing fader behavior. Recording remains independent of monitor flags. Returns use the existing post-gain, post-saturation quad values. Do not run saturation again for recording.

The current layout is 17 panned tracks, nine mono tracks, three quad returns, and two masters: 31 tracks / 78 scalar streams. External source lanes remain panned mono even when centered, allowing width switches without changing metadata.

Inside `ProcessReturns`, submit the quad and stereo results immediately after `m_masterChain.Process`, then commit once. Global master volume is subsequently applied only to listening outputs. Both masters are captured regardless of hardware output mode; stereo is never reconstructed from quad.

Zero the bounded frame staging area each frame. In noise mode, preserve skipped input/return capture as zeros while still recording masters and committing the frame. Existing combined and split mixer processing calls retain their shape.

### 6. One ring and one writer

Use a single preallocated SPSC ring of frame-major float pages. Start with 32 slots of 1024 frames, sized for the session layout, and one worker-side stream-major block assembler.

Reuse the existing `CircularQueue` in-place operations:

1. Producer fills a slot from `NextToPush`, retains it across frames, and publishes with `CompletePush` when full or stopped.
2. Worker obtains `PeekPtr`, copies/quantizes frames into its block assembler, and calls `Pop` only after consuming that slot.
3. Slot reuse follows FIFO ownership. Do not use `PopPtr`, which releases the slot before processing finishes.

There is no separate free queue or page allocator. Keep the terminal accepted-frame count and stop/error signal outside the data ring so a full ring cannot lose stop notification. Reset session storage only after worker closure is acknowledged.

The recorder owns capture state, the ring, and its worker. Keep format/encoding helpers independent for testing; split source files as needed without adding a separate worker lifecycle abstraction. The worker transposes, quantizes, omits silence, encodes, and writes directly to its file, using `ThreadId::FileWriter`. It does not feed `FileWriter` or share the sample-loading `IoTaskThread`.

Prepare storage and start the worker off the audio thread. Lifecycle remains `Idle → Starting → Recording → Stopping → Idle`, with latched error status. Start activates at a whole-frame boundary after file/header preparation. Stop publishes a partial page and boundary then returns. Repeated stop is harmless; stop during starting cancels; starts while busy are rejected until closure is acknowledged.

If the next frame cannot obtain a ring slot, stop capture and report overrun without waiting, overwriting data, or silently dropping frames. Drain accepted frames where the sink permits, omitting the clean-completion marker on error. Disk failure and invalid samples likewise expose runtime errors. After audio callbacks are quiescent, shutdown publishes any tail, drains, joins, and only then destroys storage.

Capture/start/stop perform no allocation, deallocation, locks, sleeps, filesystem work, compression, or thread joins. Existing atomic UI-state patterns expose recording and error status.

### 7. Small Python extractor

Use `scripts/sgrec.py` for the shared streaming reader and `scripts/extract_recording.py` for the CLI. Support `info`, `extract --master stereo`, `extract --master quad`, and `extract --track <id>`.

Mono/stereo/quad extraction writes decoded PCM24 bytes directly, preserving rate, channel order, and exact frame count. Panned-mono extraction writes just `sample_val` as a mono stem. Inspection explains that the source includes coordinates; v1 extraction adds no coordinate files, sidecars, or spatial renders. Master selectors resolve the unique roles and perform no gain change or DSP.

Use ordinary WAV when sizes fit and RF64 for larger output, with 64-bit counters. A preliminary streaming scan can determine output length; memory stays bounded by one block. Preserve engine quad order and use an unspecified speaker mask rather than inventing physical speaker labels. Refuse existing output paths unless overwrite is explicitly requested.

There is one incomplete-file behavior: validate a whole block before emitting it, stop at the first invalid/truncated record, finalize the WAV from preceding valid blocks, report the extracted frame count and reason, and exit nonzero. Missing/invalid completion markers also return nonzero. An invalid header or unknown selected track produces no WAV. A clean zero-frame session can produce an empty WAV. Never skip a bad block or fill a missing block with silence. There is no separate strict/recovery mode or temporary-output promotion workflow.

Sync detects `SMRTGRID` and calls the shared master-stereo extractor; legacy RIFF/RF64 files retain their SoX path. Only a zero exit status indicating a complete valid recording permits remote deletion. A usable partial WAV does not authorize deletion of the original. Exercise this with mocked transfers.

## Risks / Trade-offs

- **Bounded buffering:** At 48 kHz / 78 streams, raw PCM is 11,232,000 bytes/second. The proposed ring, one int32 block, and worst-case encoded block use about 35 MiB plus scratch. The ring holds roughly 0.68 seconds of audio; a longer stall reports overrun. Measure representative and worst-case signals on macOS/iPad before settling the defaults.
- **PCM and pan precision:** Preserve the existing clipping/rounding behavior. Pan coordinates are quantized and discarded during entirely silent blocks; recorded masters retain the audible result at PCM24 precision.
- **Incomplete files:** Block CRCs and the completion marker detect incomplete sessions, but cannot guarantee power-loss durability. Runtime logs retain failure causes; the container only distinguishes complete from incomplete.
- **Thread ownership:** Test queue-full stop, partial tails, disk failure, restart, and shutdown. Those cases protect the audio callback and recorded frames.

## Migration Plan

1. Implement the C++ encoder and Python reader against shared byte fixtures.
2. Add the single-ring writer lifecycle, then mixer taps and off-thread preparation/shutdown.
3. Embed the build SHA and add WAV extraction plus legacy-aware sync dispatch.
4. Run codec, lifecycle, mixer, cross-language, and existing sampler-looper tests; verify both app builds and sustained recording performance.
5. Switch new performance recordings to `.sgrec` and document the format and CLI. Existing recordings and sampler-looper WAV behavior remain supported.

## Open Questions

The format and tap decisions are settled, including `SMRTGRID` and exclusion of global listening volume. One-second blocks and 32 ring slots are initial performance choices to confirm with measurements.
