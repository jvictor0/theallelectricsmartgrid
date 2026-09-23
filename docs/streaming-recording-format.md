# SmartGrid streaming recordings (v3)

Performance recordings use `.sgrec` files. They contain separate input stems,
mono sub lanes, quad effect returns, and the actual mastered stereo and quad
outputs. Both masters are recorded **before the global listening-volume knob**.
Turning that knob down does not quiet the recording. No mastering or spatial
reconstruction is applied during extraction.

Sampler-looper WAV recording is unchanged.

The recording pad blinks red four times per second when an error is latched.
The async log records that error once, with its state, accepted/written frame
counts, written bytes and queue high-water mark.

## Inspect and extract

Use Python 3.10 or newer; the extractor needs only the standard library.

```sh
python3 scripts/extract_recording.py info session.sgrec --verify
python3 scripts/extract_recording.py extract session.sgrec --master stereo
python3 scripts/extract_recording.py extract session.sgrec --master quad -o quad.wav
python3 scripts/extract_recording.py extract session.sgrec --track 3 -o stem.wav
python3 scripts/extract_recording.py patch session.sgrec --sample 96000 -o patch.json
```

`info` lists IDs, types and recording taps. `--verify` reads every block and the
completion marker. Track selection uses the numeric ID, not its position in the
table. Panned-mono extraction writes just the audio stream as mono; coordinates
remain available through `scripts/sgrec.py`. Existing outputs are refused unless
`--overwrite` is supplied.

Exports preserve exact PCM24 integers, sample rate, channel order and frame count.
Small outputs use RIFF WAV; large outputs become RF64. The writer reserves the
RF64 header space as a JUNK chunk so extraction needs only one pass. Quad WAVs
have an unspecified speaker mask and preserve engine order.

On corruption, truncation or missing completion, extraction retains a playable
WAV containing only whole validated blocks, reports the frame count and reason,
and **exits nonzero**. An invalid header or unknown selected track creates no WAV.
The reader never skips damaged blocks or invents silence for missing blocks.
A clean empty recording produces an empty WAV.

`scripts/sync_ipad.py` detects new files by `SMRTGRID` magic and extracts their
stereo master. Legacy RIFF/RF64 recordings keep the existing SoX extraction path.
Only a successful, complete extraction permits deletion of the remote original;
a playable partial WAV is insufficient.

`patch` reconstructs the initial patch plus recorded parameter edits through
`--sample`, inclusive. Samples are relative to the first recorded audio frame;
96000 is two seconds at 48 kHz. Omitting `-o` prints JSON to stdout. Existing
output files are refused. The same operation is available as
`sgrec.Reader(source).PatchAtSample(sample)` on a seekable recording file.
Repeated queries start from the initial patch each time, without an index.
The query validates blocks through the target and does not require a clean tail
after that point. A target past the last audio frame is rejected; sample zero of
a clean empty session returns its initial patch.

The five assignment event types cover StateSaver bytes, gesture faders, scene
blend, encoder values, and gesture-encoder activation. These are stored control
values before smoothing and modulation, not the resulting DSP output. Replay
updates the appropriate patch fields, including nested modulators and gestures
created during recording. A gesture's inherited value is captured separately
when activation copies its parent value.

V3 adds PatchLoad and PatchSnapshot events. A load records the supplied patch
and its `restoreFaders` option as one ordered operation, including removal of
old encoder children. Reset records the resulting live patch as a snapshot.
These operations suppress their internal assignment bursts. Replay preserves
the order of edits before and after each operation, even at the same sample.

This is patch reconstruction, not complete deterministic performance replay.
Sample-directory changes and sample assets remain untracked. Ordinary scene
copies and value edits use assignment events. A scene switch alone reads stored
state and needs no per-value event.

Audio extraction accepts v1, v2, and v3 and ignores patch/event semantics.
Patch reconstruction requires a v2 or v3 initial patch and recognized event
types. V2 retains its historical assignment semantics and cannot reconstruct
unrecorded load/reset effects or recover cross-type capture order.

## File layout

All binary integers are little-endian. There is no implicit alignment or padding.

| Field | Bytes |
| --- | --- |
| ASCII `SMRTGRID` | 8 |
| UTF-8 JSON length (`u32`) | 4 |
| JSON session header | declared length |
| Independent `BLK3` records | variable |
| `END1` clean-completion marker | 16 |

Required JSON fields:

```json
{
  "format_version": 3,
  "initial_patch": {"nonagon": {}, "squiggleBoy": {}, "stateSaver": {}, "configGrid": {}, "faders": [], "blend": 0.0},
  "recorded_at_utc": "2026-09-14T12:34:56Z",
  "git_commit_sha": "0123456789abcdef0123456789abcdef01234567",
  "sample_rate": 48000,
  "block_frames": 48000,
  "tracks": [
    {"id": 0, "name": "Voice 1", "type": "panned_mono", "role": "input", "tap": "post_fader"}
  ]
}
```

`initial_patch` contains the complete live patch, including unsaved edits, just
before frame zero. The engine calls `ToJSON` directly when handling the recording
start, passes the snapshot to the recorder, and immediately begins queuing audio
and subsequent parameter edits. That sample is recording sample zero. The worker
opens the file and serializes the header while capture continues; file opening
and header I/O introduce no gap between the snapshot and recorded data. The date remains UTC session creation time on the worker.
The SHA is the full commit ID
embedded at build time. Runtime recording never invokes Git. No dirty-state
field is recorded.

Track IDs are unique unsigned 32-bit integers. Names, roles and taps are nonempty
strings. The table remains fixed for the session. Type determines the streams:

| Type | Ordered streams |
| --- | --- |
| `mono` | `sample_val` |
| `panned_mono` | `sample_val`, `x`, `y` |
| `stereo` | `left`, `right` |
| `quad` | `q0`, `q1`, `q2`, `q3` |

Quad corners are `(0,1)`, `(1,1)`, `(1,0)`, `(0,0)`. Master roles are uniquely
`master_stereo` and `master_quad`, with matching types and tap
`post_mastering_pre_master_volume`.

Audio conversion preserves the previous WAV writer:
`lround(clamp(sample, -1, 1) * 8388607)` using a double intermediate. Thus the
writer emits -8388607 through 8388607, while the codec supports the full signed
24-bit range -8388608 through 8388607. There is no dither or normalization.

Coordinates use `lround(clamp(position, 0, 1) * 8388607)`. Divide by 8388607 to
decode a position. Coordinates 0, 0.5 and 1 produce 0, 4194304 and 8388607.
Nonfinite submissions fail capture before accepting that frame.

## Block records

| Field | Representation |
| --- | --- |
| Tag | `BLK3` (4 bytes) |
| Total record bytes, including CRC | `u32` |
| Start frame | `u64` |
| Frame count | `u32` |
| Included track count | `u16` |
| Descriptors, ascending track ID | variable |
| Stream payloads, descriptor order | variable |
| Event group count | `u32` |
| Event groups | variable |
| CRC-32/ISO-HDLC | `u32` |

The fixed header is 22 bytes. Each descriptor contains `track_id:u32`, followed
by `(encoding:u8, bit_width:u8)` for each type-derived stream. No stream count or
payload length is repeated. Stream payloads start at byte boundaries.

Frame counts range from 1 through the header's `block_frames`. Start positions
are contiguous from zero. Only the last data block may be short. Predictors
reset for every stream in every block. Default block size is one second.

A track is omitted when its quantized audio is zero throughout the block.
For panned mono, only `sample_val` determines omission. Omitted tracks decode to
zero streams, including zero coordinates. An included panned track retains all
coordinates, including those on locally silent frames. Entirely silent blocks
still have records with zero descriptors. With no events they occupy 30 bytes
including CRC; otherwise their event groups follow the empty audio payload.

### Event groups

Before grouping, the writer assigns each event a block-local capture order.
It stable-sorts events by `(type, name, sample)` and groups by `(type, name)`.
Each group contains:

| Field | Representation |
| --- | --- |
| Event type | `u8`; see types below |
| Value width | `u8`; fixed for the group |
| UTF-8 name length | `u16` |
| Entry count | `u32` |
| Name | declared UTF-8 bytes, without terminator |
| Entries | type-specific fields below |

Every v3 entry starts with a block-relative sample offset (`u32`) and capture
order (`u32`). Replay sorts all entries in the block by `(sample, order)`, so
grouping does not reorder assignments around loads or snapshots. Only fields
relevant to the type follow; there is no struct padding or placeholder data.

| Type | Name | Width | Fields after sample offset and order |
| --- | --- | --- | --- |
| 1 StateChange | State name | 1, 2, 4, or 8 | scene:`u8`, value bytes |
| 2 GestureSet | Empty | 4 | gesture/fader index:`u8`, value:`float32` |
| 3 BlendSet | Empty | 4 | value:`float32` |
| 4 EncoderSet | Root parameter name | 4 | scene:`u8`, track:`u8`, path length:`u8`, path bytes, value:`float32` |
| 5 EncoderActivate | Root parameter name | 1 | scene:`u8`, track:`u8`, path length:`u8`, path bytes, active:`u8` |
| 6 PatchLoad | Empty | 0 | restoreFaders:`u8`, JSON length:`u32`, UTF-8 patch JSON |
| 7 PatchSnapshot | Empty | 0 | JSON length:`u32`, UTF-8 patch JSON |

Floats are finite little-endian IEEE-754 binary32. Scenes are 0..7; tracks and
faders are 0..15. Activation is 0 or 1. Encoder paths have 0..16 hops: each byte
is a modulator index 0..14 or `0x80 | gesture_index` (128..143). An empty path
addresses the root encoder; activation requires a final gesture hop. Gestures
are leaves in supported edits; a nested gesture follows zero or more normal
modulator hops. Readers retain support for legacy tagged paths.
The in-memory `ParamEvent` path uses -1 for unused slots; this sentinel and the
redundant `m_gesture`/`m_isGesture` encoder fields are not serialized.

Each name appears once per type per block. GestureSet, BlendSet, PatchLoad, and
PatchSnapshot groups have zero name bytes. Entries retain every submitted value,
including repeated values.
StateChange copies the specified scene's stored bytes at capture. Replay replaces
bytes at `scene * value_width` in the named `nonagon` or `stateSaver` entry;
wire bytes above 127 become negative byte numbers in patch JSON. It also mirrors
the legacy source-width/selection copies in `configGrid`.

GestureSet assigns `faders[index]`; BlendSet assigns top-level `blend`. Encoder
values are captured in patch units (`ToValue`), including bipolar values, and
assign `squiggleBoy[name]...values.values[scene][track]`. Activation assigns the
node's flat `active[scene * 16 + track]`. Missing nested nodes start with neutral
zero patch values and inactive gestures; existing nodes and other scenes/tracks
are preserved. The root must already exist in the initial patch. New patches
save and load `blend`; older patches without it leave the current blend unchanged.

PatchLoad follows the live loader's partial-load rules. Missing state fields and
encoder roots are preserved. Supplied state byte arrays replace each started
scene value, zero-filling an incomplete final value. A supplied encoder root
loads its values and replaces its child trees, even when `modulators` or
`gestures` are omitted. Neutral normal modulators and inactive gestures are
removed as in the engine. `configGrid` source-width and selection aliases, and
legacy `sourceMonitor`, overwrite their StateSaver counterparts. A present
`configGrid` without `sourceSelected` clears source selections; supplied
selections are normalized to the three-channel limit. Sample directories and
assets are not applied by replay.

`restoreFaders` is 0 or 1. When 0, both the existing faders and blend survive the
load. When 1, a supplied blend is loaded, and at least 16 supplied faders replace
the current 16 faders; a shorter fader array is ignored. PatchSnapshot replaces
the whole reconstructed patch with the recorded JSON object. Both bulk types
must contain a JSON object; malformed payloads fail patch queries while audio
extraction still skips event contents.

An entry's timestamp is `block_start + sample_offset`. Offsets are within the
block's frame count; an event at the next block boundary belongs to that next
block. Silent and final partial blocks carry events. Events for an audio frame
that was never accepted are excluded.

V1 uses `BLK1` and ends immediately after audio payloads and CRC, with a minimum
record size of 26 bytes. V2 uses `BLK2`, assignment types 1–5, and entries that
start with sample offset alone; it has no capture-order field or bulk patch
types. Readers preserve the stored order of same-sample v2 assignments.
V2 and v3 audio readers derive the audio payload length as before, then skip
the remaining bytes before CRC. They need no event decoder.
Patch queries decode the trailer and reject an unknown event type in a block needed by the query. They do not decode audio or validate unrelated patch fields.

### Stream encodings

For `N` frames:

- **0: raw**, width 24. Exactly `3*N` bytes of little-endian two's-complement PCM24.
- **1: delta**, width 0 through 25. First value is PCM24. Remaining values are
  adjacent mathematical differences, computed without 24-bit wrapping.
  Length is `3 + ceil((N-1)*width/8)` bytes.

The writer uses the smallest signed two's-complement width that holds every
difference, and uses delta only if it is strictly smaller than raw. Raw wins
ties. Constant streams use width zero and only the first value. Differences -1
and 0 fit one bit; +1 needs two. Endpoint-to-endpoint jumps can need 25 bits and
select raw. Readers accept valid nonminimal delta widths.

Pack each difference's low `width` bits least-significant-bit first into successive
low-to-high bits of bytes. Unused high bits of the final byte must be zero.
For example `[5,4,4,3]` uses delta width 1 and payload `05 00 00 05`.

CRC covers every preceding byte in the record, using reflected polynomial
`0xEDB88320`, initial value `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`. This matches
Python's `zlib.crc32`.

## Completion and limits

The 16-byte completion marker is `END1`, `total_frames:u64`, then CRC of those
first 12 bytes. The total must equal the contiguous validated frames. No bytes
may follow it. A zero-frame session contains only the header and completion.

Capture errors drain the accepted prefix where possible and omit completion.
Failure reasons remain in runtime status and logs. CRC and completion detect
damage; they do not promise power-loss durability. A failed close attempts to
remove the completion marker if one was written.

The audio reader validates block framing, audio metadata, IDs, widths, derived
audio sizes, ranges, padding, whole-block CRC, frame continuity and completion.
It parses the header JSON but does not validate or interpret `initial_patch` or
the event trailer. Patch queries check event framing and the fields they need to update, without a whole-patch schema validator.
Allocation limits are 1 MiB header JSON, 128 tracks, 64 MiB encoded record and 64 MiB
decoded int32 block. Discard each `Reader.Blocks()` result before requesting the
next one to retain the streaming memory bound.

## Recorder ownership and measurement

`Prepare` allocates storage and starts the persistent worker outside audio.
One SPSC ring holds 32 pages of 1024 frame-major float frames. The worker retains
each slot until consumed, transposes and quantizes into a one-second block, then
compresses and writes directly. Capture only checks finiteness, stages/copies
floats and publishes pages. A second preallocated SPSC queue holds 1024 fixed
parameter events. Events are published before their audio pages; the worker drains
them before finalizing each block, then sorts and groups them. Queue exhaustion
is a recording error, including while the file is opening. Start and stop do no
file work or joining. Even an immediate stop retains the accepted frames; an
empty session writes its header and zero-frame completion marker.

Source-monitor toggles use the ordinary `sourceMonitor_i` StateChange events and
replay into the global `stateSaver` section. New patches keep no separate monitor
array in `configGrid`; legacy patches with that array remain loadable.

A separate 8 MiB JSON arena holds the initial patch snapshot through worker
closure, independent of ordinary patch saving. Snapshot construction and event
capture allocate no heap memory on audio; JSON text serialization runs on the
worker. The engine drains recording before destroying its registered states and root encoder names.

Each queued load retains its load/save `PatchArena`. The owner cannot reset or
grow that arena until every reader releases it. The worker serializes the patch
as it drains events, releases the arena immediately, and retains its own bytes
until block writing. Busy message-thread loads are deferred before parsing and
retried by the timer; a newer pending request replaces an older pending request.
Save requests and saved-pad reloads defer to a later control frame if necessary.
Error, overflow, shutdown, and discarded-tail paths release retained references.

Whole-patch reset uses another preallocated 8 MiB arena for its resulting
snapshot. Reset remains pending while that arena is still referenced. Capture
publishes one load or reset operation instead of one event for every affected
state and encoder value. Worker-owned payload bytes count toward the 64 MiB
pending-event bound; exhaustion fails recording through the explicit error path.

Stop publishes the partial page and a separate stop flag. Ring exhaustion fails
capture without waiting or overwriting. New starts are rejected until worker
closure is acknowledged. Shutdown runs after audio callbacks are quiescent and
drains/joins before storage destruction.

The current mixer has 31 tracks / 78 streams: 17 panned inputs, 9 mono lanes,
3 quad returns and the two masters. Its ring holds about 0.68 seconds; storage
is about 35 MiB for audio, plus two 8 MiB arenas for initial/reset snapshots and event storage,
before allocator/logging overhead. A longer disk stall can
produce an incomplete recording and visible error. Source-width switches keep
track IDs and types. Noise mode zeroes skipped stems and still records masters.

Build and run the standalone recorder measurement after configuring CMake tests:

```sh
c++ -std=c++17 -O2 -Iprivate/src -I/tmp/smartgrid-recording-build/generated \
  private/test/tools/benchmark_recording.cpp -o /tmp/benchmark_recording -pthread
/tmp/benchmark_recording /tmp/recording-bench audio 30
```

Modes are `silence`, `audio` (sine audio with moving pans), and `noise` (independent
random audio and coordinates). It writes at real-time rate and reports size,
maximum block encode/write time, queue high-water, capture cost, capture-thread
allocation count, peak RSS (bytes on macOS), and errors. This isolates recorder
cost; it is not a measurement of the whole synth callback or iPad storage.

See the change's `verification.md` for measured results and remaining platform
checks. Golden records and their independent construction are in
`private/test/fixtures/streaming-recording/`.
