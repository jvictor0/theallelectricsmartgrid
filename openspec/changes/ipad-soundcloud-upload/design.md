## Context

Designed 2026-10-02; approved for implementation 2026-10-11. Approval includes
the recommended Mac API connection, public visibility, recording-start dates,
and duplicate numbering beginning with #2. Account connection itself remains a
one-time setup action using the user's SoundCloud application credentials.

The current flow is already close to the desired interaction:

- `SyncPage::Begin` selects a receiver, loads its remembered Keychain pairing,
  and starts sync.
- `SyncClient.mm` owns Bonjour, pinned HTTPS, and a transfer worker. Its current
  single request slot and cancellation flag serve one transfer at a time.
- `sync_receiver.py` stores originals under `~/Documents/SmartGridOne/recordings`
  and publishes extracted `*_stereo.wav` files. Extraction has its own worker.
- iPad work stops when the page closes or the app becomes inactive. The Mac
  receiver has an independent lifetime. Nothing involves the audio thread.
- Existing files include timestamped stereo WAVs and older numbered recordings.
  `StreamingRecorder::SessionPath` writes a local timestamp into the filename
  and UTC start time into SGREC metadata.

The [SoundCloud profile](https://soundcloud.com/theallelectricsmartgrid) shows
nightly date-based posts, Electronic genre, and occasional event-specific titles.
Some historical titles contain typos; title text cannot reliably identify a
local recording.

The [official API guide](https://developers.soundcloud.com/docs/api/guide)
documents account-authorized uploads and optional artwork. API authorization
requires a registered application and account consent; a website login is not
an API credential. The repository contains no existing SoundCloud integration.

## Goals / Non-Goals

**Goals:** Select a Mac, start sync, browse its stereo masters, and publish one
with sensible defaults. Keep editing optional and give each recording an
accurate, persistent upload status. Reuse the iPad's existing receiver pairing.

**Non-Goals:** Audio changes, mastering, artwork generation, scheduled or bulk
publication, playlists, editing/deleting published tracks, cross-Mac database
replication, browser automation, or background iPad activity. No additional
confirmation dialog in the normal upload flow.

## Decisions

### 1. Extend the receiver; the Mac performs the upload

Recommended approach: use the official SoundCloud API from the existing Python
receiver. The large WAV is already on the Mac. Only commands, status, and an
optional small image cross the iPad connection.

Alternatives considered:

| Approach | Trade-off |
| --- | --- |
| Mac receiver + official API | Fits the current architecture and survives leaving the iPad page; needs Mac API authorization once. |
| Upload from the iPad | Requires iPad SoundCloud authorization and transferring or retaining the stereo file there; conflicts with the desired flow. |
| Automate the logged-in Mac browser | Can reuse that website session, but depends on a live browser, UI changes, and browser-control permissions. Poor fit for a small dependable sync service. |

### 2. One screen, with the recording list beside a small editor

Before selecting a Mac, retain the existing receiver list and pairing controls.
Selecting a remembered Mac starts sync and immediately opens its recording view.
Existing Mac files do not wait for that sync to finish.

```text
Sync with Mac                     Joyo's Mac ▾
Syncing recording…  64%                  [Cancel sync]

Stereo recordings                       SoundCloud
Oct 1 · 9:43 PM · 42:18       Ready       The All Electric Smart Grid
Sep 30 · 9:07 PM · 38:02      Uploaded ↗
Sep 29 · 10:50 PM · 51:20     Uploaded ↗  Title
                                        Smart Grid Nightly 2026/10/01
                                        Electronic · Public
                                        Image: None  [Choose image]
                                        [Upload to SoundCloud]
```

Times and durations above are illustrative. The list shows recording start time,
duration, and upload status; the source filename is available in the selected
recording's details. Newest recording first, stable filename tie-break. Default
selection is the newest available recording without an upload receipt, or the
newest recording if all have receipts. Updating the list never changes a user's
selection or overwrites an edited title or selected artwork.

Selecting a recording fills the inline editor. Title defaults are editable;
`Use default` restores automatic naming. `Choose image` opens the iPad system
picker for Photos or Files. Show a thumbnail and `Remove image` once selected.
The picker reads only the selected image and normalizes it to a supported JPEG
or PNG off the message/audio threads. Do not require library-wide photo access.
Artwork starts empty for each new draft; it is not inherited from the last post.

`Upload to SoundCloud` submits immediately. The row becomes Queued, then
Uploading with progress, then Uploaded with an external link. Uploaded rows show
the actual posted title and link in place of another Upload button. Clear
failures show a concise error and Retry; uncertain outcomes use the exceptional
flow in decision 6. Selecting a Mac or recording never publishes anything.

The sync progress strip and publishing status are independent. Cancel sync
stops only the iPad file transfer. A short note while a publishing job is active
says `Uploading from your Mac; you can leave this screen.` Version one has no
remote upload cancellation control.

### 3. Deterministic nightly defaults

- Title: `Smart Grid Nightly YYYY/MM/DD`.
- Date: local recording start date from the timestamped filename. Keep that
  date across midnight, sync, upload, and timezone changes. For older numbered
  files, use original SGREC start metadata converted to the Mac's local timezone;
  otherwise use original-file modification time, falling back to stereo-file
  modification time. Store the derived date on first cataloging and show it.
- Genre: `Electronic`; public visibility; no description or tags added.
- Artwork: omit the artwork field when no image is selected. SoundCloud may
  display its own fallback artwork; the app supplies no image by default.
- The first known post for a recording date has no suffix. Later default posts
  use ` #2`, ` #3`, etc. Never rename the first post retroactively.

The receiver imports the account's own track metadata once when the recording
view is opened, with pagination, and on explicit Refresh. This refresh runs in
the background; it does not hold up sync or the local list. Reuse this catalog
for naming and historical-link suggestions; do not fetch remote tracks for each
row or each status poll. Retain the last successful snapshot between visits.
Snapshots, receipts, and numbering are scoped to the configured account ID;
switching accounts cannot reuse another account's uploaded flags or counters.

For numbering, parse valid nightly titles case-insensitively, treating the
unsuffixed title as slot 1 and recognizing a trailing `#N`. Also count local
upload receipts with that recording date, including custom titles. Deduplicate
by SoundCloud track identifier. Choose one plus the larger of the distinct post
count and the highest known nightly suffix, or no suffix when both are zero.
Pending local jobs reserve slots under the same database transaction. Clear
failures release reservations; uncertain outcomes retain them until resolved.

For example, known posts with the base title and `#3` produce `#4`. A first local
post with a custom title still makes the next default post for its date `#2`.
Historical arbitrary titles acquire a recording date when explicitly linked;
their SoundCloud publication date is not substituted for the recording date.

The UI presents a suggested title. An automatic-title submission contains
`title: null`; the receiver allocates the actual title when accepting the job and
returns it. An explicit override is used verbatim after trimming surrounding
whitespace and rejecting an empty title; it gets no automatic suffix.

If the initial SoundCloud listing fails and no complete cached snapshot exists,
show that failure and Retry in the publishing panel while sync continues. Do not
guess an unused sequence number. With a cached snapshot, continue using it and
show its refresh failure without an additional confirmation step.

### 4. A small Mac catalog and durable upload receipts

Add `recording_catalog.py` for local listing and persistence, and
`soundcloud_publisher.py` for the remote API and one upload worker. The receiver
remains the HTTPS adapter and extraction coordinator.

Store non-secret state in `<root>/.soundcloud.sqlite3`, using Python's built-in
SQLite support. Catalog entries have an opaque recording ID, relative stereo
path, file version (size and modification time), recording date/start time,
duration, and optional original-source identity. Jobs store request ID,
recording ID/version, account ID, title, optional staged image, state, and the
remote track identifier/permalink on success. Preserve the identifier returned
by the API as an opaque string.

Index existing final `*_stereo.wav` exports on receiver use; do not expose
original multichannel files, temporary files, or actively extracting outputs.
Read header metadata when discovering a file or its version changes. New LAN
extraction inserts the catalog entry as soon as the final output is published.
Explicit Refresh rescans for exports made by the USB sync tool. Listing does
not hash or decode the whole recording archive.

A receipt identifies a particular local file version and SoundCloud account.
Restarting the app or receiver preserves Uploaded. Replacing a file invalidates
that association for the new version while keeping the old receipt for history.
Renaming outside the service is not automatically tracked in version one.

Historical manual uploads have no local receipt. The imported remote catalog
can suggest a match when a parsed nightly date and duration (within one second)
agree uniquely in both directions. Show `Possible existing upload` with a link
and `Link this upload`; do not silently treat a title/date match as proof. The
same small `Link existing upload` action offers the account's tracks for custom
titles or ambiguous matches. Selecting a match records the association and the
row then says Uploaded. No audio fingerprinting or automated archive audit.

### 5. Keep pairing and page lifetime; separate control from bulk transfer

All new receiver endpoints use the existing pinned HTTPS connection and bearer
access code. They operate only on catalog IDs, not arbitrary filesystem paths
or caller-supplied upload destinations. Existing pairing therefore also permits
publishing to the SoundCloud account configured on that receiver.

Extend the client with two independently cancellable request channels: the
existing bulk sync worker and a lightweight publishing/control worker. They
share receiver identity and authentication, but not a single `m_task` pointer,
progress field, or cancellation flag. Keep the transport primitive small and
reusable rather than duplicating certificate validation.

Selecting a receiver establishes the page session, launches file sync, and
requests the publishing catalog plus one asynchronous refresh. Ordinary catalog
GETs do not initiate remote refreshes. Sync completion or Cancel sync leaves catalog
and upload controls connected. Switching Macs cancels and drains the old page
requests, clears its selection/drafts, and starts the new Mac's sync. Accepted
jobs remain owned by the old Mac.

Poll receiver status every two seconds while sync or publishing work is active;
otherwise refresh on selection, foreground return, or explicit Refresh. Polls
read the receiver's local state, not SoundCloud. The existing 200 ms UI timer
can repaint snapshots without issuing network calls. Catalog deltas become
visible as extraction completes.

On page close or app inactivity, cancel and drain both iPad channels and stop
discovery and timers, as today. Returning to the still-open page reconnects to
the selected Mac when found and fetches its catalog/jobs, without automatically
restarting a cancelled file sync or submitting another upload. Presenting and
dismissing a system image picker preserves the draft; a real inactivity event
still stops networking. There are no changes to realtime DSP or recording code.

Suggested additive wire interface:

| Endpoint | Responsibility |
| --- | --- |
| `GET /v1/publishing` | Account connection state, local recordings, suggested titles, job progress, receipts, and cached historical candidates. |
| `POST /v1/publishing/refresh` | Request a local rescan and remote track-list refresh; acknowledge immediately. |
| `POST /v1/publishing/uploads` | Multipart recording ID/version, request UUID, nullable title, optional image; persist accepted job and return job/title/state. |
| `POST /v1/publishing/links` | Associate a recording version with a track from the configured account's cached catalog. |

The upload endpoint returns 202 only once the entire request, including any
image, is stored and the job is committed. A repeated request UUID returns the
same job; changed parameters with that UUID are rejected. A second request for
the same recording version returns its existing active job or successful
receipt. An explicit retry after a definite failure creates a new attempt.
An explicit `Upload again` after an unknown outcome identifies the prior job in
the request, resolves its reservation, and records the deliberate new attempt;
it is distinct from retrying a lost iPad submission with the same UUID.

Use normal field, image, and file-version validation at these boundaries. An
old receiver returns 404 for the new endpoint; show `Update the Mac receiver to
enable SoundCloud uploads` and keep normal sync usable. No version-negotiation
subsystem is needed.

### 6. Submit once and keep the result

The Mac worker streams the existing stereo WAV and optional image in one
multipart upload. It serializes publishing jobs; extraction remains on its
existing worker. File size and duration already in the catalog permit a simple
unsupported-file error. There is no new conversion/mastering pipeline.

Persist Queued before acknowledging submission, and Uploading before sending
the remote request. A successful creation response with a track identifier and
permalink is sufficient to store the receipt and show Uploaded. This means
SoundCloud accepted the track; its processing may continue. Do not download the
track, listen to it, hash it again, or poll SoundCloud until encoding completes.

A definite rejection is Failed and offers Retry. When transmission has started
but the response is lost, retain `Upload result unknown` and an account link;
never automatically POST again. Use the historical-link control to attach the
created track, or an explicit `Upload again` action after the user has checked.
This exceptional action explains that another copy could be created. An iPad
request timeout is resolved by fetching the receiver's job state using the same
request identity, not by inventing a second upload.

When the receiver restarts, persisted Queued jobs can run; jobs left Uploading
become Unknown, and completed receipts remain Uploaded. The local files survive
all publication outcomes. No distributed transaction or automatic retry engine.
Stopping the receiver is distinct from leaving the iPad screen: it can interrupt
its outgoing upload.

### 7. No additional iPad login; Mac authorization is a separate setup fact

The feature adds no iPad account, SoundCloud login screen, pairing code, or
per-upload authorization. The account name is visible in the publishing panel.

The preferred backend needs a SoundCloud application registered for this purpose
and one account authorization on the Mac, unless an existing authorized uploader
already supplies suitable credentials. Use authorization-code flow with PKCE
and state; store its client secret and account tokens in Mac Keychain, outside
the catalog and repository. Serialize refresh and save each rotated token set
before another operation uses it. Only the Mac talks to SoundCloud.

Missing or revoked authorization produces `Connect SoundCloud on your Mac` in
the panel; browsing and normal sync continue. Connection/repair belongs to a
small receiver setup command with the registered browser callback, not to the
iPad's routine upload interaction. It must use the configured target account.

If the user's constraint forbids even this one-time Mac authorization and only
a logged-in website session exists, the recommended backend does not meet that
literal constraint. Do not describe existing LAN pairing as SoundCloud consent
or quietly add browser automation; resolve this choice with the user first.

## Risks / Trade-offs

- Historical titles do not establish file identity → one-time explicit linking
  for old manual posts; new posts always get exact local receipts.
- Manual publishing elsewhere while this view is open can make the cached
  numbering stale → explicit Refresh; serialize this receiver's own submissions.
  No promise of account-wide atomic naming across independent uploaders.
- Remote acceptance followed by connection loss cannot guarantee exactly-once
  creation → show Unknown and avoid automatic re-upload.
- A cached Uploaded receipt does not monitor later deletion on SoundCloud →
  its link remains the historical record; continuous remote auditing is outside
  this feature.
- Mac must be awake and the receiver running → failures appear inline; no new
  launch agent, cloud relay, or wake-on-LAN requirement.
- Older numbered recordings lack a reliable original local date → visible,
  stable fallback date and editable title.

## Migration Plan

1. Resolve the Mac authorization choice. Configure SoundCloud access as a
   separate setup action when implementation is authorized.
2. Deploy the receiver extension and create its small catalog lazily. Existing
   recordings and receiver pairing stay in place.
3. Deploy the iPad UI/client extension. Older receivers still support sync.
4. Link historical manual posts as needed. No bulk renaming or re-uploading.

Rollback uses the prior receiver/app; the publishing database can remain on disk
and existing audio and pairing state are untouched. Published SoundCloud tracks
are external results and are not removed during rollback.

## Focused Verification

Use a fake SoundCloud service for catalog ordering, naming/slot allocation,
optional metadata, one successful upload, definite failure, and lost-response
behavior. Exercise persistence across restart and duplicate submission. Extend
the native client harness to cover controls during sync and page-close/
foreground lifecycle. Build both Apple targets and do one iPad walkthrough for
selection, image picking, and reconnecting to a Mac job. Use a real account
upload only when separately authorized. Run the existing sync/extraction tests
for regressions; no repeated whole-library checks.

## Approval

The user approved implementation on 2026-10-11. Use the recommended official API
backend with a one-time Mac account connection. Creating an actual public test
post is separate from implementing this feature.
