# iPad SoundCloud Upload Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish a Mac stereo recording from the paired iPad Sync screen.

**Architecture:** Extend the Python HTTPS receiver with a local catalog and one
SoundCloud worker. Add a separate page-owned control channel alongside existing
bulk sync, then expose recording selection, title, artwork, and upload status in
the JUCE Sync page. SoundCloud authorization stays on the Mac.

**Tech Stack:** Python 3.10+ standard library, SQLite, macOS Keychain, SoundCloud
OAuth 2.1/PKCE and multipart API, C++17/Objective-C++, Foundation/UIKit/JUCE.

## Global Constraints

- No additional iPad login; reuse pinned HTTPS and the receiver access code.
- No audio-engine changes or work on the audio thread.
- All iPad discovery, requests, and polling remain scoped to the foreground Sync page.
- Accepted uploads run on the Mac after the iPad leaves the page.
- Title: `Smart Grid Nightly YYYY/MM/DD`; Electronic genre; public visibility; no supplied artwork by default.
- First post has no suffix; subsequent default titles begin at ` #2`.
- Normal publication uses one streamed creation request and its response, without post-upload audio verification or encoding polling.
- Do not perform real SoundCloud publication as a test. Use temporary files/accounts in local fixtures; no production recordings are modified by tests.
- Follow AGENTS.md C++ naming/braces/comments rules and no default C++ arguments.
- Existing linked managed worktree is the implementation workspace; do not create another.

## Shared wire contract

Existing bearer authentication applies to every endpoint. JSON responses:

```json
{
  "available": true,
  "account": {"id": "urn-or-id", "name": "The All Electric Smart Grid", "connected": true},
  "refreshing": false,
  "error": "",
  "catalog_ready": true,
  "recordings": [{
    "id": "opaque-id", "version": "opaque-file-version", "filename": "recording-2026-10-01T210000_stereo.wav",
    "recorded_at": "2026-10-01T21:00:00", "date": "2026/10/01", "duration_seconds": 2400.0, "size": 1234,
    "suggested_title": "Smart Grid Nightly 2026/10/01", "state": "ready", "title": "",
    "permalink": "", "job_id": "", "error": "", "progress": 0.0, "candidate_id": ""
  }],
  "tracks": [{"id": "urn-or-id", "title": "title", "permalink": "https://soundcloud.com/owner/track", "duration_ms": 2400000}]
}
```

- `GET /v1/publishing`: local snapshot; no remote fetch per polling request.
- `POST /v1/publishing/refresh`: JSON `{}`; accepts background local rescan and paginated remote metadata refresh; returns 202.
- `POST /v1/publishing/uploads`: bounded multipart body with `metadata` JSON and optional `artwork` JPEG/PNG file. Metadata is `{"recording_id":"id","version":"v","request_id":"uuid","title":null,"retry_of":null}`. Null title chooses automatic allocation, nonempty string is custom. Reply 202 with job fields `job_id,state,title,permalink,error` after durable acceptance. A successful prior receipt can return 200. Exact retries return the existing job; conflicting request identity is 409. `retry_of` names a failed/unknown attempt for explicit retry.
- `POST /v1/publishing/links`: JSON `{"recording_id":"id","version":"v","track_id":"id"}`; link a track from the configured account catalog without uploading; reply 200.
- States: `ready`, `queued`, `uploading`, `uploaded`, `failed`, `unknown`.
- An unavailable older receiver responds 404 to publishing; keep sync working.

### Task 1: Mac catalog, official API connection, and durable publishing endpoints

**Files:**
- Create `scripts/recording_catalog.py`, `scripts/soundcloud_api.py`, `scripts/soundcloud_publisher.py`, `scripts/soundcloud_connect.py`.
- Modify `scripts/sync_receiver.py`.
- Create `scripts/tests/test_soundcloud_publishing.py`, `scripts/tests/test_soundcloud_api.py`.
- Modify `scripts/tests/test_sync_receiver.py` only for additive feature integration coverage.

**Interfaces:**
- Consumes existing `extract_stereo_recording`, safe receiver paths, and server root.
- Produces the shared wire contract above, with fields always present even when empty.
- `PublishingService(root, api)` owns catalog/jobs and worker. `api=None` means unconnected, with local browsing available. Expose `Snapshot()`, `Refresh()`, `Submit(metadata, artwork, artwork_type)`, `Link(metadata)`, `Close()`, `RecordingAdded(path)`. Python defaults are allowed; keep the existing `create_server(host, port, root, token)` call valid and add an optional injectable publisher.
- `SoundCloudAPI` exposes `Account()`, `Tracks()`, `Upload(path,title,artwork_path,progress)`. It owns token access/refresh and distinguishes definite rejection from uncertain transmission outcome. Tests substitute an external API boundary, not catalog/job behavior.
- Main receiver loads saved authorization once; unit-created receivers must not access the real Keychain or internet.

- [x] **Step 1: Write and run failing behavior tests.** Use temporary directories, actual stereo WAV fixtures, and a recording fake external SoundCloud API. For example:

```python
def test_next_title_and_receipt_survive_restart(self):
    self.write_stereo('recording-2026-10-01T210000_stereo.wav')
    self.api.tracks = [{'id': '11', 'title': 'Smart Grid Nightly 2026/10/01',
                        'permalink_url': 'https://soundcloud.com/test/one', 'duration': 5000}]
    self.service.Refresh()
    self.wait_for(lambda: self.service.Snapshot()['catalog_ready'])
    row = self.service.Snapshot()['recordings'][0]
    self.assertEqual(row['suggested_title'], 'Smart Grid Nightly 2026/10/01 #2')
    job = self.service.Submit({'recording_id': row['id'], 'version': row['version'],
                              'request_id': 'request-1', 'title': None, 'retry_of': None}, b'', '')
    self.wait_for(lambda: self.service.Snapshot()['recordings'][0]['state'] == 'uploaded')
    self.restart_service()
    self.assertEqual(self.service.Snapshot()['recordings'][0]['state'], 'uploaded')
```

Also cover newest-first ordering, numbered SGREC date fallback, RF64/float WAV
header duration, changed-file identity, concurrent same-date submissions,
request-ID replay/conflict, custom metadata and artwork, missing credentials,
remote list failure, failed/unknown outcomes, restart state, and historical linking.
Run `python3 -m unittest discover -s scripts/tests -p 'test_soundcloud_*.py' -v`.
Record the expected missing-behavior failure before adding implementation.

- [x] **Step 2: Implement catalog and jobs.** Scan only final stereo exports,
  read bounded headers (support RIFF and RF64 produced by existing extractor),
  persist recording IDs and size/mtime versions, date/duration, account-scoped
  remote metadata, jobs, reservations and receipts in `.soundcloud.sqlite3`.
  Reuse SGREC header reader without decoding blocks. Serialize state changes;
  never hold the database lock over SoundCloud networking. Suggestions require
  a unique nightly date/duration match in both directions. Links are explicit.
  Select automatic slots beyond known count/highest suffix, including pending
  jobs. Release definite failures, retain unknown reservations. Reset interrupted
  uploading jobs to unknown on restart. Stream files; no archive hashing.

- [x] **Step 3: Implement official API and connection setup.** Consult
  `https://developers.soundcloud.com/docs/api/guide` and the official OpenAPI
  reference for exact fields. Stream multipart `POST /tracks` with
  `track[asset_data]`, `track[title]`, `track[genre]=Electronic`,
  `track[sharing]=public`, optional `track[artwork_data]`. Save the returned opaque
  ID/URN and permalink. Single-use refresh tokens require serialized refresh and
  saving the full new token set before reuse. Restrict authenticated pagination
  to SoundCloud's API origin. No blind POST retries after transmission.
  Add an interactive Mac setup command that asks for app ID/secret, opens the
  PKCE authorization URL, checks state on the registered loopback callback,
  verifies `/me`, and stores credentials plus account identity in Keychain.
  Keep secrets out of argv, logs, SQLite, and repository. A small standard-library
  macOS Keychain adapter can use Security/CoreFoundation via ctypes. Prompting
  for app credentials is setup only; do not request them in chat or run setup now.
  Test outgoing fields/body, pagination, refresh rotation, error classification,
  state rejection, and stored credentials through injectable transport/storage.

- [x] **Step 4: Integrate receiver.** Add handlers with a bounded image/metadata
  request size and multipart parser, catalog ID/version boundary checks, and
  existing bearer authorization. Bind operations to the server catalog only.
  Start background refresh on demand, update catalog when extraction publishes
  its final output, and close publishing resources on receiver shutdown. Failure
  of publishing must not undo successful stereo extraction/sync deletion receipts.
  Ensure tests instantiate an unconnected or fake-API service without touching
  the user's account. Repeated control requests must not retrigger remote scans.

- [x] **Step 5: Verify and commit.** Run the new tests plus
  `python3 -m unittest discover -s scripts/tests -p 'test_sync_receiver.py' -v`.
  Localhost tests require the supported sandbox escalation. Existing baseline:
  15 tests pass. Update relevant OpenSpec tasks only once tested. Commit task files
  with `feat: publish synced stereo recordings from the Mac receiver`.

### Task 2: Foreground native publishing client and Sync page editor

**Files:**
- Modify `JUCE/SmartGridOne/Source/SyncPage.hpp`, `SyncClient.hpp`, `SyncClient.mm`.
- Create focused `PublishingClient.hpp/.mm` and `PublishingPanel.hpp` if useful;
  add any new compilation units to `SmartGridOne.jucer` and both Apple projects.
- Extend `scripts/tests/sync_client_harness.mm` and `scripts/tests/test_sync_client.py`, or add a separate native publishing harness/test file.

**Interfaces:**
- Consumes the shared wire contract (copied above into the task brief by the controller) and existing `SyncClient::Receiver`, `LoadToken`, `SaveToken`.
- Produces an independent page-owned control channel with `Open`, `Close`,
  `SetForeground`, `Select(receiver,token)`, `Refresh`, `Submit`, `Link`, and
  thread-safe `Snapshot` operations. Define explicit C++ request/row/status
  structs matching the wire fields; no JUCE/audio dependency in network code.
- Bulk sync and publishing share certificate-checking code and receiver identity,
  but have separate request/session, cancellation, progress, and worker state.

- [x] **Step 1: Add a failing native integration scenario.** Use a real loopback
  TLS receiver with fake SoundCloud backend. Harness selects a receiver while a
  delayed bulk sync runs, reads newest rows, submits a custom title/artwork, and
  observes the uploaded link. A second scenario closes/backgrounds the native
  client after 202, lets the fake remote upload finish, then reconnects and sees
  the receipt without another submission. Concrete checks include:

```cpp
if (snapshot.m_recordings.front().m_state != "uploaded"
    || snapshot.m_recordings.front().m_title != "Custom title"
    || snapshot.m_recordings.front().m_permalink.empty())
{
    return 21;
}
```

Compile native tests with clang++/Foundation/Security and run their Python driver.
Missing types/behavior establishes the initial failure. Also cover wrong TLS
fingerprint, no work after Close, cancel sync independent of publishing, and 404.

- [x] **Step 2: Implement isolated control networking.** Use ephemeral sessions
  and pinned HTTPS, drain on Close/inactivity without main-thread waits on
  main-thread callbacks, parse metadata into snapshots on a worker. Serialize
  short controls within that worker; do not block it on bulk transfer. Refresh
  once at selection and fetch local state every two seconds while active sync or
  publishing work exists. On foreground return restore selected receiver only
  after discovery; no auto-restart of bulk sync or publication. Retain submitted
  request identity across a lost response; catalog/job state recovers it.

- [x] **Step 3: Build the screen.** Selecting the Mac starts sync and selects its
  publishing control session. Keep receiver switching and sync strip visible.
  Show newest-first rows, stable selection, actual posted title/link, job progress
  and concise errors. Add title editor, Use default, optional image thumbnail,
  Choose/Remove image, Upload, and historical track-link selector. Missing Mac
  authorization and older receiver show inline messages without disabling sync.
  Uploaded rows replace upload with Open on SoundCloud. Failed allows Retry;
  Unknown offers account link, Link existing, and explicit Upload again with
  duplicate-risk text only in that state. Normal Upload has no confirmation.
  Only enable automatic-title submission with a valid account catalog.

- [x] **Step 4: Add the system image picker.** Photos/Files must use native picker
  grants only, preserve drafts on cancellation/inactivity, and pass normalized
  JPEG/PNG bytes to control networking. Decode/resize/encode off the audio and
  message threads. On iPad use PHPicker/UIKit document picker (or proven JUCE
  equivalent) without whole-library authorization. On Mac use a native file
  chooser. Lifetime callbacks use safe ownership and temporary images are cleaned
  up after upload/draft disposal. Keep image transfer bounded to the receiver limit.

- [x] **Step 5: Verify and commit.** Run native tests and existing sync client
  regression tests. Build macOS and iOS using their checked-in Xcode projects,
  with derived data/temp outputs in supported writable locations. Simulator or
  unsigned builds can validate compilation before any device deployment. Fix
  actual compiler errors rather than relaxing warnings/tests. Commit with
  `feat: upload SoundCloud recordings from the iPad Sync page`.

### Task 3: Integration walkthrough and operator documentation

**Files:**
- Modify `docs/ipad-tools.md`, OpenSpec change tasks/design as implementation details settle.
- Fix only integration defects found by the focused final validation.

**Interfaces:**
- Consumes the completed Mac endpoints, setup command, native client and UI.
- Produces tested operator instructions and an honest task completion record.

- [x] **Step 1: Verify supported builds and workflow.** Reuse build results for
  unchanged code. Exercise a real local receiver with fake remote service and
  the native client if not already covered by Task 2. Run existing sync/extraction
  regression tests once. Use a reachable iPad for the single UI walkthrough if
  deployment/access is available; record a concrete limitation otherwise. Do not
  connect the real SoundCloud account or publish a real track as a test.

- [x] **Step 2: Document exact setup and usage.** Show app registration callback,
  command for initial Mac connection, receiver start, choosing the Mac and a
  recording, default metadata, image selection, historical linking, and recovery
  after an unknown result. Explain leaving the page versus stopping the Mac
  receiver. State any remaining user-run account setup or physical-device check.

- [x] **Step 3: Validate and commit.** Run
  `openspec validate ipad-soundcloud-upload --strict`, inspect `git diff --check`,
  mark only verified tasks complete, and commit documentation with
  `docs: describe iPad SoundCloud publishing setup and verification`.

### Verification ledger (2026-10-11)

- Tasks 1 and 2 are complete through `98ed28f`. Task reports record backend/API
  RED/GREEN tests, 19 receiver tests, 53 extraction tests (six fixture-dependent
  skips), native publishing/sync regressions, and final macOS/iPad simulator
  builds. These unchanged checks are reused for Task 3.
- Task 3 records exact operator setup/use/recovery and the attempted walkthrough.
  Strict OpenSpec validation and whitespace checks passed; interactive limits
  remain explicitly unchecked below.
  The focused topology UI-construction fix `33aa724` removed an existing bounds
  crash; both Apple targets rebuilt successfully afterward.
- [ ] Simulator UI walkthrough: launch subsequently aborts in
  `AURemoteIO::Start` / `_ReportRPCTimeout` before Sync UI, including after one
  restart. No list/title/image/picker or leaving/reopening check was completed.
- [ ] Physical-iPad walkthrough: the paired iPad Air 13-inch (M3) remains
  unavailable in `xcrun devicectl list devices`. Compilation and simulator
  checks do not establish physical-device behavior.
- [ ] User-run SoundCloud application registration/consent and live Mac Keychain
  setup. No real account connection or publication was performed as a test.
