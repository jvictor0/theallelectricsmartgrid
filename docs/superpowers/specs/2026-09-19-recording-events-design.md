# Recording events and initial patch snapshot

Extend `.sgrec` recordings to contain the full current patch at recording start
and grouped events at the end of every audio block. The user approved embedding
events inside each block's CRC, grouping by `(type, name)`, and using the audio
sample index as the timestamp. The initial patch snapshot is an additional
requested part of the same extension.

## Initial patch

The version 2 writer includes an `initial_patch` JSON object in the session
header. Populate it using `TheNonagonSquiggleBoyInternal::ToJSON`, including all
existing sections: `nonagon`, `squiggleBoy`, `stateSaver`, `configGrid`, `faders`, and `blend`.
This is the live patch, including unsaved changes, rather than the most recently
saved patch file. Store it as a nested object, not an escaped JSON string.
Referenced sample files remain references, as in ordinary patch JSON.

Audio extraction treats this field as opaque metadata. The existing JSON header
parser already makes it available through `Reader.m_header`; add no patch
presence, type, section, or schema checks during audio extraction.

The snapshot describes the state at the recording start call. The engine calls
`ToJSON` directly on the audio thread into a dedicated, preallocated arena and
passes the resulting snapshot to the recorder. Start immediately enables audio
and event capture with that sample as their common origin. Subsequent changes
are events, including changes later in sample zero.

The writer opens the destination asynchronously and serializes the immutable
snapshot before writing any blocks. Audio and events queue while it does so;
there is no file-open wait, snapshot callback, or per-sample startup hook. The
existing bounded queues report overrun if opening or writing takes too long.

Use separate storage from `StateInterchange` so saving or loading a patch cannot
overwrite a pending recording header. Allocate the arena during preparation
outside audio; reuse it only after worker acknowledgement. Use the existing
8 MiB arena capacity initially. Snapshot exhaustion fails the recording visibly;
never substitute a stale, partial, or later snapshot. Immediate stop and shutdown
must drain accepted data even if the worker has not opened the file yet. A clean
empty session still writes the header and zero-frame completion marker.

Measurements on 2026-09-19 found 100 saved patch JSON files under
`~/Documents/SmartGridOne/patches`: 33,380–395,825 bytes on disk. Older files
contain substantial indentation. Python compact reserialization measured
31,725–83,460 bytes; its numeric formatting may differ slightly from the app's
serializer. Recent saved patches are approximately 33–71 KB. The checked-in
patch fixture is 71,223 bytes. These are serialized sizes, not arena footprint.
Retain the existing 1 MiB limit for the complete serialized header, including
track metadata, and report a clear recording error if exceeded.

## Event blocks

New recordings declare `format_version: 2` and use `BLK2` records. Retain the v1
audio descriptors, compression, sparse-track behavior, and frame numbering.
Append an event section after all audio payloads and before the block CRC:

```text
Block header
Audio descriptors and payloads
Event group count: u32
    Type: u8, value width: u8, name bytes: u16, entry count: u32
    UTF-8 name
        Block-relative sample offset: u32, type-specific fields, value bytes
        ...
    ...
Block CRC covering audio and events
```

Each `(type, name)` appears once per block, regardless of the number of changes.
The first event type is `StateChange` (numeric value 1). Its value width is fixed for a group and
is one of the existing state widths: 1, 2, 4, or 8 bytes. Each entry retains its
own scene index and a copy of the value at that instant. Values use the same
representation as the saved state's bytes. Never reread the live value later.
Use the existing `State::m_name` as the name. Matching widths within a group are checked by the writer; no new identity registry or naming scheme is needed.

Preserve every submitted parameter event, including repeated values and multiple
changes at the same sample. Stable-sort by `(type, name, sample)` before
grouping, preserving insertion order for exact ties. Groups are ordered by
`(type, name)` and entries by sample. Independent state assignments do not need
a cross-group tie-breaker.

For a block starting at frame `S` with `N` frames, events belong to the half-open
interval `[S, S + N)`. Store their offsets from `S`; reconstruct a recording
sample index as `S + offset` and seconds as that index divided by sample rate.
An event exactly at the next boundary belongs to the next block. No separate
wall-clock timestamp is stored per event. Preserve the existing UTC session
creation timestamp in the header with its existing meaning.

Silent blocks still carry events. The final partial block carries events within
its accepted frames. Events for an unaccepted frame are excluded, including a
pending sample when recording stops before accepting its audio. A clean empty
session has the patch header and completion marker, with no event-only block.

## Capture and writer responsibilities

Keep the existing audio SPSC queue and add a prepared, bounded event SPSC queue.
The audio thread only captures fixed-size event records and publishes them; it
does not group, allocate strings, encode, or write. Stable name references may
be used while their owners outlive the worker's drain and shutdown.

Publish a frame's events before publishing its audio. The worker may finalize a
block only after it has consumed the audio through the block's final frame and
drained all corresponding events. It also drains events regularly while audio
pages are filling, retaining future-block events for the appropriate block.
Group and serialize on the worker. Bound pending event storage using the
existing maximum block size and enforce that size on the combined encoded audio
and event record. Queue exhaustion or exceeding the size limit uses the existing
recording error path, never a silent event drop.

Complete the draft hook and queue lifecycle: allocate, drain, reset, and check
push failures. Fix `ParamEvent::MkStateChange` to copy the state's actual width;
scene storage is packed at `scene * m_len`, not at an eight-byte stride. Capture
the specified scene buffer with initialized storage. Reset timing and event state on every
new session after the prior worker close is acknowledged.

The implemented `ParamEvent` types are StateChange (1), GestureSet (2), BlendSet
(3), EncoderSet (4), and EncoderActivate (5). The exact compact layouts and tagged
encoder paths are specified in [the format reference](../../streaming-recording-format.md#event-groups).
Unnamed fader/blend groups omit names; only type-relevant fields go on the wire.
Encoder values use patch units, before smoothing/modulation. The shared
`SmartGridOneContext` owns the scene manager, recorder, and `ParamEventLogger`.

State writes, copies, resets, and loads capture the target scene buffer; simple
scene selection only changes which stored value is live. Encoder assignments
use `SetAndRecordValue`, and gesture activation captures inherited parent values
as separate assignments. Default scene-zero bytes are initialized at registration.
Blend is included in ordinary patch JSON so a recording's starting crossfade
can be restored even before any BlendSet event.

The five assignments do not express deleting or replacing encoder subtrees.
Garbage-collecting and recreating a gesture can therefore retain stale descendants
in reconstruction. Patch load still has uncaptured activation/fader/blend paths.
Sample-directory changes and assets remain untracked, by design while that
feature is unfinished. These limitations must not be mistaken for deterministic
performance replay.

## Audio reader and verification

Keep audio extraction working for both v1 and v2. The existing descriptors give
the audio payload's length, and the existing record length locates the final
CRC. For v2, ensure the audio payload fits before the CRC and skip the remaining
event trailer. No additional trailer length field is needed. V1 keeps its exact
audio-payload length check.

Keep the existing record-size limits, CRC, audio validation, frame continuity,
`END1`, and incomplete-recording recovery. The CRC still covers the whole block,
including events. Audio extraction does not inspect event types, names, scenes,
values, or ordering, and cannot fail because it does not understand an event.
It parses the header JSON normally without interpreting `initial_patch`.

## Patch reconstruction

`Reader.PatchAtSample(sample)` starts from a deep copy of `initial_patch` and
applies recognized parameter entries at recording-relative samples **less than or equal
to** the requested sample. Each call starts from the beginning; use the existing
seekable file/BytesIO input without building an index or cache. It reads and
checks complete blocks through the target but does not decode their audio.
Reject negative samples, v1 files without a snapshot, or targets beyond recorded
audio. Sample zero of a clean empty recording returns its initial patch.

Resolve a state's existing name in the patch's `nonagon` or `stateSaver`
dictionary and replace `value_width` bytes starting at `scene * value_width`.
Patch arrays use signed byte numbers; convert wire bytes accordingly. Replay
only needs checks required to locate and apply the delta, not a whole-patch
schema validator. Keep the duplicated `configGrid.sourceStereo` and
`configGrid.sourceSelected` fields consistent with their recorded StateSaver
values so loading the reconstructed patch does not undo those edits.

Unknown event types fail patch reconstruction explicitly; audio extraction
continues to skip their bytes. The replay switch also assigns faders, blend, encoder values, and activation.
It creates neutral missing nested nodes while preserving existing nodes and
unrelated scenes/tracks. Do not add replay indexes, caches, or a plugin framework.
Add `extract_recording.py patch INPUT --sample N [-o PATCH.json]` as the small
end-to-end interface; default output is JSON on stdout. Existing audio commands
remain unchanged.

Focused tests cover:

- All five compact payloads against independent wire bytes, nested mixed-kind
  paths, bipolar patch-unit conversion, invalid values/indices/paths, and initial
  path termination.
- A real-engine recording with all types across block boundaries, Python replay,
  and loading the reconstructed result back into the engine.
- Recording the existing seeded encoder/scene patch round-trip test and comparing
  complete live patch checkpoints against Python reconstruction.

- The header contains the live unsaved patch at frame zero and remains unchanged
  by subsequent edits or an ordinary patch save.
- Sorting and grouping preserve repeated values and same-sample changes; values
  use the correct width and scene stride. Replay checks before/at/after changes,
  multiple scenes, both state dictionaries, config aliases, and repeated queries.
- Events land in the correct block, including frame zero, silent blocks, and the
  final partial block.
- Event queue overflow reports a recording error; immediate stop, shutdown while
  opening the file, and restart drain or reset ownership correctly.
- V1 and v2 audio extraction agree on audio; v2 extraction ignores patch content
  and event semantics while retaining the existing CRC and truncation behavior.
- Capture allocates no heap memory. Measure the one-time snapshot cost using the
  existing engine test harness.

Update the recording-format documentation and the streaming-recording and
recording-extraction OpenSpec specifications with implementation. Run focused
C++ and Python recording tests, the relevant system tests, and `make build`.
