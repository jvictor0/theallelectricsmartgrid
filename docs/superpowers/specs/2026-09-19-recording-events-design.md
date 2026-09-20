# Recording events and initial patch snapshot

Extend `.sgrec` recordings to contain the full current patch at recording start
and grouped events at the end of every audio block. The user approved embedding
events inside each block's CRC, grouping by `(type, name)`, and using the audio
sample index as the timestamp. The initial patch snapshot is an additional
requested part of the same extension.

## Initial patch

Version 2 adds a required `initial_patch` JSON object to the session header.
Populate it using `TheNonagonSquiggleBoyInternal::ToJSON`, including all existing
sections: `nonagon`, `squiggleBoy`, `stateSaver`, `configGrid`, and `faders`.
This is the live patch, including unsaved changes, rather than the most recently
saved patch file. Store it as a nested object, not an escaped JSON string.
Referenced sample files remain references, as in ordinary patch JSON.

The snapshot describes the state immediately before recording frame zero.
Starting is asynchronous: pressing Record requests startup; capture begins at
an audio sample boundary after the worker has opened the destination. Changes
while waiting for that boundary are included in the snapshot. Once the snapshot
is taken, subsequent changes are events, including changes at frame zero.

At that boundary the audio thread builds the snapshot into a dedicated,
preallocated JSON arena. It publishes the immutable snapshot to the worker and
begins queuing audio and events with that sample as their common origin. The
worker serializes and writes the header before writing any blocks. Header I/O
must not create an unrecorded interval between the snapshot and frame zero.
Choose the boundary before processing controls for the first recorded sample.

Use separate storage from `StateInterchange` so saving or loading a patch cannot
overwrite a pending recording header. Allocate the arena during preparation
outside audio; reuse it only after worker acknowledgement. Use the existing
8 MiB arena capacity initially. Snapshot exhaustion fails the recording visibly;
never substitute a stale, partial, or later snapshot. Stop during startup and
shutdown must resolve ownership even when no audio frames have been accepted.

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
Event group count
    Event type, UTF-8 name, value width, event count
        Block-relative sample offset, scene, value bytes
        ...
    ...
Block CRC covering audio and events
```

Each `(type, name)` appears once per block, regardless of the number of changes.
The first event type is `StateChange`. Its value width is fixed for a group and
is one of the existing state widths: 1, 2, 4, or 8 bytes. Each entry retains its
own scene index and a copy of the value at that instant. Values use the same
representation as the saved state's bytes. Never reread the live value later.
Names identify the same state consistently for the session; detect conflicting
identities or widths rather than silently combining unrelated states.

Preserve every submitted state event, including repeated values and multiple
changes at the same sample, in insertion order within its group. There is no
additional cross-group ordering guarantee for events at the same sample.
Group order is deterministic by type and name.

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
Group and serialize on the worker with explicit memory bounds. Queue or block
event-capacity exhaustion is a recording error, never a silent event drop.

Complete the draft hook and queue lifecycle: allocate, drain, reset, and check
push failures. Fix `StateEvent::MkStateChange` to copy the state's actual width;
scene storage is packed at `scene * m_len`, not at an eight-byte stride. Capture
the live value with initialized storage. Reset timing and event state on every
new session after the prior worker close is acknowledged.

This extension records events emitted through the `State::Set` hook. It does
not claim full performance replay: low-level scene loads and patch restoration,
encoder changes, faders, and other paths without that hook require their own
future event coverage. The initial patch includes their existing saved state.

## Readers, failure handling, and verification

The Python reader accepts both v1 and v2. V1 blocks expose no events and have no
initial patch. V2 validates the patch object and the entire event section before
exposing a block, even when extracting only one audio track. Retain `END1`, its
total-frame checks, and the existing incomplete-recording recovery rules.
Audio and events share one CRC and one validated-block recovery boundary.

Validate lengths, counts, supported types, state widths, scene range, duplicate
group keys, sample offsets and ordering, exact section consumption, and the
existing allocation limits. Bound total event storage as well as encoded bytes.
Unsupported event types must produce an explicit error rather than guessed
interpretation. Expose decoded groups and the patch through the reader API;
dedicated replay and event-export commands are outside this change.

Tests cover v1 compatibility; v2 independent encoding/decoding; repeated names,
values, and timestamps; every state width and scene stride; block boundaries;
silent and partial blocks; malformed sections and CRC failures; overflow;
restart; and stop or shutdown during startup. Verify that unsaved state appears
in the header, edits after capture do not mutate it, frame-zero events are
retained, concurrent ordinary patch saving uses independent storage, and capture
performs no heap allocations. Measure snapshot capture cost on the real engine.

Update the recording-format documentation and the streaming-recording and
recording-extraction OpenSpec specifications with implementation. Run focused
C++ and Python recording tests, the relevant system tests, and `make build`.
