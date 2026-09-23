# Recording Events Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Record the initial live patch and sorted StateSaver deltas, then reconstruct a patch at a recording sample.

**Architecture:** Extend the existing recorder and codec with a v2 event trailer inside each block CRC. Capture a dedicated patch snapshot directly in the start call, immediately queue audio and events, and sort/group on the writer. Keep audio extraction independent of patch/event semantics; replay deltas only for explicit patch queries.

**Tech Stack:** C++17, arena JSON, existing SPSC queues, Python standard library, doctest/unittest.

## Global Constraints

- Events sort stably by `(type, name, sample)`; each `(type, name)` occurs once per block.
- Replay includes events at the requested sample. Sample timestamps start at the first recorded frame.
- Initial patch includes unsaved edits and every existing patch section.
- StateChange is type 1; no new hooks for encoders, resets, scene copies, or other untracked edits.
- Audio extraction does not interpret or validate the patch or event semantics.
- Existing worktree and user draft code are the starting point. Follow repository C++ style.
- Preserve existing CRC, completion, queue-overrun, and v1 reading behavior.

---

### Task 1: Wire format and value capture

**Files:** `private/src/ParamEvent.hpp`, `private/src/RecordingFormat.hpp`, `private/test/unit/streaming_recording_format.cpp`.

**Interfaces:** `ParamEvent` gains `m_valueLen`; `RecordingFormat::EncodeHeader(session, output, JSON initialPatch = {})`; `EncodeBlock(session, samples, frames, startFrame, output, std::vector<ParamEvent> events = {})`.

- [x] Add a failing test capturing a two-byte state in scene 3, retaining its earlier value after the live value changes. Add independent expected trailer bytes for unsorted names/times and same-sample ties:
  ```cpp
  auto event = ParamEvent::MkStateChange(&state, 5);
  DOCTEST_CHECK(event.m_valueLen == 2);
  DOCTEST_CHECK(static_cast<uint8_t>(event.m_value[0]) == 0x34);
  DOCTEST_CHECK(static_cast<uint8_t>(event.m_value[1]) == 0x12);
  ```
- [x] Run the focused C++ tests and confirm missing width/incorrect copied bytes or missing v2 trailer.
- [x] Copy exactly `state->m_len` bytes from live storage into zero-initialized event storage. Write version 2 header with nested snapshot. Emit `BLK2`; retain all audio encoding. Stable-sort events and append `group_count:u32`, then groups `(type:u8, width:u8, name_len:u16, count:u32, name UTF-8)`, followed by `(sample_offset:u32, scene:u8, value[width])`. Keep CRC last.
- [x] Adapt existing independent audio-codec assertions for the v2 envelope and rerun focused tests.

### Task 2: Recorder lifecycle and engine snapshot

**Files:** `private/src/StreamingRecorder.hpp`, `private/src/ParamEventLogger.hpp`, `private/src/TheNonagonSquiggleBoy.hpp`, host/harness sample entry points as needed; tests `private/test/unit/streaming_recorder.cpp`, `private/test/system/sys_streaming_recording.cpp`.

**Interfaces:** Recorder owns a dedicated snapshot arena and accepts `Start(JSON initialPatch = {})`. The engine start call builds the snapshot directly and passes it through SquiggleBoy and QuadMixer. Start immediately establishes sample zero and enables capture through `BeginFrame`/`CommitFrame`. The worker opens the destination and writes the header before draining queued audio and events.

- [x] Add failing recorder tests with a real memory sink: changing live state after snapshot does not change header; events at samples 0, 7, 8 belong to blocks `[0,8)` and `[8,9)`; same-sample changes preserve order; overflow and restart do not silently drop or leak events.
- [x] Build/run tests to observe absent event queue/snapshot behavior.
- [x] Allocate the queue/arena in `Prepare`; capture once at actual start; origin is `SampleTimer::GetSample()`. The engine start call executes `ToJSON(arena)` directly on audio and passes the snapshot to the recorder. Publish snapshot before worker reads it. The immutable arena survives through worker close.
- [x] Publish events before their audio page; worker drains events before encoding completed blocks and while awaiting pages. Keep future events for subsequent blocks and discard events outside the accepted audio tail. On queue overflow use `Overrun`; reset session-owned data after close.
- [x] Wire the record control to the engine start call; edits after that call, including later in the same sample, become events. Exercise the actual `ParamEventLogger` hook in a real engine test and write a recording fixture for Python.
- [x] Run focused C++ recorder/engine tests and allocation check. Preserve immediate start/stop, empty recordings, and disk error behavior.

### Task 3: Python audio compatibility and patch reconstruction

**Files:** `scripts/sgrec.py`, `scripts/extract_recording.py`, `scripts/tests/test_sgrec.py`.

**Interfaces:** `Reader.PatchAtSample(sample: int) -> dict`; CLI `patch INPUT --sample N [-o PATCH.json]`. Existing `Reader.Blocks()` remains audio-only. Use an optional internal event-decoding path for patch queries; no event index/cache.

- [x] Write independent v2 test records and assert replay behavior:
  ```python
  reader = Reader(io.BytesIO(recording))
  self.assertEqual(reader.PatchAtSample(4)['nonagon']['Mute_0'], [0, 0])
  self.assertEqual(reader.PatchAtSample(5)['nonagon']['Mute_0'], [0, 1])
  self.assertEqual(reader.PatchAtSample(4)['nonagon']['Mute_0'], [0, 0])
  ```
  Also cover same-sample ties, bytes above 127, unknown events ignored by audio but rejected by replay, missing snapshot irrelevant to audio, and target beyond EOF.
- [x] Run `python3 -m unittest discover -s scripts/tests -p test_sgrec.py` and verify the new tests fail on unsupported v2 or missing replay.
- [x] Accept v1/BLK1 and v2/BLK2. Derive audio end from descriptors; v2 audio skips trailer through CRC. For replay, decode bounded group records without decoding audio, copy snapshot, locate state in `nonagon` or `stateSaver`, and replace bytes at `scene * width`. Mirror sourceWidth/sourceSelected into their existing configGrid aliases. Start each query from the data offset and reject unsupported types only when replay is requested.
- [x] Add CLI output using `json.dump`; file output refuses overwrite. Run Python suite and replay the real C++ fixture, including loading the reconstructed JSON back into the engine.

### Task 4: Documentation, review, and verification

**Files:** `docs/streaming-recording-format.md`, `openspec/specs/streaming-recording/spec.md`, `openspec/specs/recording-extraction/spec.md`, relevant state documentation.

- [x] Update exact wire fields, timing, replay API/CLI, and explicitly limited State::Set coverage.
- [x] Run focused C++/Python suites, full standalone suite, strict affected OpenSpec validation, `git diff --check`, and `make build`. Existing two startup-silence test failures were reproduced before this change; investigate any different failures.
- [x] Obtain code review focused on capture ordering, queue lifecycle, value representation, reconstruction and audio compatibility; resolve actionable findings.
- [x] Report the working CLI/API and validation results. Leave unrelated user edits intact; do not land or push this branch.


## Verification results — 2026-09-20

- `make build`: succeeded.
- Focused C++ recording, codec, mixer, engine, and arena tests: 34 passed.
- Python recording suite with real C++ state and mixer fixtures: 33 passed.
- Reconstructed sample-3 patch loaded back into the real C++ engine: passed.
- Frame-zero queued UI edit retained while the header kept its earlier value: passed.
- AddressSanitizer: reproduced the direct-owner event-name use-after-free, fixed
  owner shutdown order, and passed all 23 focused sanitizer tests.
- Audio-thread snapshot plus first state/audio capture: zero heap allocations;
  observed 72–88 microseconds on this Mac, with 449,359 arena bytes used.
- Full standalone suite: 423/425 passed. The two startup-silence failures match
  the previously reproduced baseline, including peak 0.000657712.
- Both affected OpenSpec specifications validate strictly; diff whitespace check passes.
- Independent final review found no remaining actionable issues.

Changes remain in the existing worktree; no landing or push was performed.

## Follow-up: immediate recording start — 2026-09-20

- [x] Replace deferred snapshot callback and file-open readiness with a direct engine `ToJSON` call and snapshot passed to `Start`.
- [x] Remove `BeginRecordingSample`, `BeginSample`, ready flags, and the `Starting` state. Start queues audio/events immediately while the worker opens the file.
- [x] Add a regression test for audio and parameter events captured during delayed opening, stopped before opening finishes; adapt frame-zero UI and allocation tests.
- [x] Update timing documentation and lifecycle specs.
- [x] Run focused C++/Python checks, `make build`, strict spec validation, and independent lifecycle review.

Follow-up verification: 26 focused C++ tests and 33 Python tests with regenerated
C++ fixtures passed. The reconstructed sample-3 patch loaded back into the engine.
The direct start/snapshot path counted zero heap allocations (78 microseconds in
the focused run). `make build` and both strict OpenSpec validations passed. The
full suite passed 424/426, with the same two pre-existing startup-silence failures.
Independent review found no immediate-start lifecycle or ownership issues.

## Follow-up: source monitoring through StateSaver — 2026-09-20

- [x] Register source-monitor booleans, route controls/reset/legacy loading through their cached State handles, and remove manual monitor serialization.
- [x] Verify patch round-trip, legacy patch loading/default restore, recorded monitor toggles across block boundaries, and reconstructed engine loading.
- [x] Update documentation, run affected checks and `make build`, and review the change.

Source-monitor verification: 40 focused C++ tests and 33 Python tests with newly
generated recordings passed; the sample-3 reconstructed patch loaded back into
the engine with the expected monitor settings. `make build`, strict validation
of both recording specs, and `git diff --check` passed. Independent review found
no source-monitor issues.

## Follow-up: all ParamEvent types — 2026-09-22

Keep v2 grouping by (type, name) with stable timestamp order and the existing
8-byte group header. Unnamed GestureSet/BlendSet groups have zero name bytes.
StateChange entries remain sample:u32, scene:u8, value[width]. GestureSet uses
sample:u32, gesture:u8, float32; BlendSet uses sample:u32, float32. EncoderSet and
EncoderActivate use sample:u32, scene:u8, track:u8, path_length:u8, path bytes,
then float32 or bool:u8. Path bytes carry a modulator index (0..14) or gesture
index (128..143). No unused scene/track/name/path fields are emitted.

Use -1 to terminate the in-memory encoder path, and capture every hop's kind.
Capture encoder values in patch JSON units and StateChange bytes from the stated
scene. Blend uses a top-level blend field in patch JSON. Replay creates neutral
nested encoder nodes when first edited, preserving existing nodes and scene/track
values. Audio extraction continues to ignore event contents.

- [x] Add independent wire-byte and replay tests for all five types, nested paths, initialization, malformed records, and sample boundaries; observe failures.
- [x] Implement compact writer and minimal capture fixes; retain StateChange wire compatibility.
- [x] Implement typed reader and patch replay, plus initial blend persistence.
- [x] Exercise real engine capture -> Python reconstruction -> engine load, update specs/docs, run affected tests and make build, and review.

The requested subsystem review found a regression in initial State scene storage:
scene zero was uninitialized after removal of outgoing-scene saves. Added a
failing regression and initialized it from the registered live default. The
review also identified deferred subtree deletion/recreation and partial patch-load
capture; these are documented in the format reference and design. Sample-directory
changes/assets remain intentionally untracked.

The existing seeded encoder/scene save-load test now records both seeded edit
sequences and exports complete saved patch checkpoints for independent Python
comparison. A separate real-engine fixture covers all five types, inherited
gesture values, both nested path kinds, and the next-block boundary. All-type
capture plus the initial snapshot performs zero heap allocations in the audio
allocation test (105 microseconds observed for this fixture).

Verification: 65 focused C++ tests and 31 additional format, config,
scene-default, and allocation tests passed; all 36 Python tests passed with
regenerated engine/mixer/seeded fixtures. Loading sample-3 reconstruction into
the real engine passed 21 assertions. `make build`, all four affected strict
OpenSpec validations, and `git diff --check` passed. The full C++ suite passed
432/434, with only the two previously established startup-silence failures at
peak 0.000657712. The subsequently added blend/legacy-patch regression passed
separately. Final independent review found no remaining actionable code findings
within the assignment-event scope; its final documentation clarification about
uncaptured patch-load fader/blend/activation assignments is included.

Changes remain uncommitted in the existing worktree. Generated fixtures use
local temporary paths; sample-recording directories remain untracked.
