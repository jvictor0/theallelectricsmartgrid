# soundcloud-publishing Specification

## Purpose
TBD - created by archiving change ipad-soundcloud-upload. Update Purpose after archive.
## Requirements
### Requirement: Browse Mac stereo recordings during sync
Selecting a paired receiver on the iPad Sync page SHALL start existing file sync
and independently display that Mac's final stereo exports, newest recording
first. Newly completed receiver extractions SHALL appear without waiting for the
whole sync. Rows SHALL show recording date/time, duration, and publishing state.
Refreshing the list SHALL preserve an existing selection and edited draft.

#### Scenario: Select a Mac with existing recordings
- **WHEN** the user taps a remembered Mac
- **THEN** file sync starts and the stereo list loads using the existing pairing
- **AND** an existing stereo recording can be selected and uploaded while other files sync

#### Scenario: A new stereo extraction finishes
- **WHEN** the receiver publishes a final stereo export during sync
- **THEN** it appears in recording-time order on the active page
- **AND** originals and incomplete exports are not offered for publication

### Requirement: Publish with nightly defaults and optional edits
The selected recording SHALL default to `Smart Grid Nightly YYYY/MM/DD`, genre
`Electronic`, public visibility, no supplied artwork, and no added description
or tags. The date SHALL be its local recording start date from its timestamped
filename, with stored metadata/file-time fallbacks for legacy names as specified
in the design. The date SHALL NOT change merely because upload happens later.

The user SHALL be able to override the title, restore the automatic title, and
choose or remove an image using the iPad's system Photos/Files picker. The Upload
button SHALL submit once without an additional confirmation dialog. Selecting a
Mac or recording SHALL NOT publish anything by itself.

#### Scenario: Upload yesterday's recording with defaults
- **WHEN** the user selects a recording from yesterday and taps Upload
- **THEN** its automatic title uses yesterday's recording date
- **AND** the Mac submits the stereo export with Electronic genre, public visibility, and no artwork field

#### Scenario: Customize a post
- **WHEN** the user enters a custom title and selects an image before uploading
- **THEN** the post uses that title and image
- **AND** the app adds no date or numeric suffix to the custom title
- **AND** the next recording's draft starts without that image

### Requirement: Allocate same-day titles on the Mac
The receiver SHALL derive automatic titles from its cached account track catalog,
local receipts, and pending reservations. It SHALL treat a recognized unsuffixed
nightly title as slot 1 and use ` #2` for the next post for that recording date.
Further titles SHALL advance beyond both the number of distinct known posts for
the date and the highest recognized suffix. Allocation and local reservation
SHALL be atomic; the first post SHALL NOT be renamed. Explicit custom titles
SHALL bypass automatic numbering but their receipts SHALL count toward later
default posts for the same recording date.

#### Scenario: Another nightly already exists
- **WHEN** the account catalog contains `Smart Grid Nightly 2026/10/01`
- **THEN** a new automatic post for that date is named `Smart Grid Nightly 2026/10/01 #2`

#### Scenario: Two default posts are submitted together
- **WHEN** two different recordings for the same date are submitted to one receiver
- **THEN** each accepted job gets a different reserved title
- **AND** each response includes the actual allocated title

### Requirement: Reuse iPad pairing and keep SoundCloud credentials on the Mac
All publishing endpoints SHALL use existing receiver authentication and pinned
HTTPS. The iPad SHALL NOT require a new login, pairing, or per-post authorization.
SoundCloud credentials SHALL remain on the Mac. The publishing panel SHALL name
the configured account. Missing or revoked Mac authorization SHALL produce a
connection message while leaving file sync and local browsing usable.

#### Scenario: Receiver already has SoundCloud authorization
- **WHEN** a previously paired iPad selects that receiver
- **THEN** it can submit a publication without entering additional credentials

#### Scenario: SoundCloud authorization is unavailable
- **WHEN** the receiver lacks usable account authorization
- **THEN** the panel says to connect SoundCloud on the Mac
- **AND** existing sync continues without a SoundCloud login prompt on the iPad

### Requirement: Keep accepted publication jobs on the Mac
The receiver SHALL persist an accepted publication job and any supplied artwork
before acknowledging it. It SHALL stream the Mac's stereo file to SoundCloud on
a publishing worker independent of extraction. Queued and Uploading status SHALL
be observable from the iPad. Leaving the page, switching Macs, losing the iPad
connection, or cancelling file sync SHALL NOT cancel an accepted Mac job.

#### Scenario: Leave the page during upload
- **WHEN** the Mac has accepted a job and the user returns to the instrument
- **THEN** the Mac continues the job
- **AND** reopening the receiver's recording view shows its saved state

### Requirement: Preserve the foreground-only iPad lifecycle
Discovery, catalog requests, upload submissions, and status polling SHALL exist
only while the Sync page is open and the app is active. Closing the page or
becoming inactive SHALL cancel and drain both bulk and control requests. A
foreground return SHALL reconnect to the selected available receiver and fetch
state without resubmitting a publication or restarting a cancelled file sync.
No publishing code SHALL execute on or add work to the audio thread.

#### Scenario: Background the app while viewing progress
- **WHEN** the iPad app becomes inactive
- **THEN** its publishing and sync requests and polling stop
- **AND** accepted Mac jobs continue independently

### Requirement: Persist uploaded status and support historical linking
A successful SoundCloud track-creation response SHALL produce a durable receipt
binding the account, local recording version, remote identifier, posted title,
and permalink. The row SHALL show Uploaded and a link across restarts. Its
ordinary Upload action SHALL be replaced by that receipt. A replacement local
file SHALL NOT inherit a prior version's Uploaded state.

Historical manual posts SHALL support explicit association with an account track.
Date/duration matches SHALL be presented as suggestions, not authoritative
receipts. Confirming a link SHALL persist the association without re-uploading.
Receipts SHALL describe accepted past uploads without continuously checking
whether someone later deleted the remote track.

#### Scenario: Reopen after successful publication
- **WHEN** the app and receiver restart after an accepted SoundCloud upload
- **THEN** the same local recording version still shows Uploaded and its link

#### Scenario: Link an older manual post
- **WHEN** the user selects an existing account track for a local recording
- **THEN** the receiver saves a receipt and shows Uploaded without sending audio

### Requirement: Avoid duplicate submissions and blind remote retries
Repeating a request UUID with the same parameters SHALL return its existing job.
Conflicting reuse SHALL be rejected. Submitting the same recording version while
it has an active job or successful receipt SHALL return that job or receipt.
Definite failures SHALL show an inline error and allow an explicit retry.
An uncertain remote outcome SHALL NOT be retried automatically or shown as
Uploaded. Persisted Uploading jobs found after receiver restart SHALL become
Unknown; persisted Queued jobs SHALL remain eligible to run.

#### Scenario: Submission response is lost on the iPad
- **WHEN** the receiver accepted the job but the iPad did not receive its response
- **THEN** querying or repeating the same request identity recovers the existing job
- **AND** it does not create a second SoundCloud upload

#### Scenario: SoundCloud response is lost after sending audio
- **WHEN** the receiver cannot determine whether SoundCloud created the track
- **THEN** it shows Upload result unknown and preserves the recording
- **AND** the user can link the created track or explicitly choose Upload again after checking

### Requirement: Keep checks proportional to the operation
The feature SHALL reuse a cached remote track listing and local catalog instead
of querying SoundCloud for every row or status poll. Local cataloging SHALL NOT
hash or decode entire audio files. Normal publication SHALL use one streamed
creation request and its result; success SHALL NOT require downloading the track
or waiting for remote encoding. Normal file sync SHALL continue when publishing
fails or an old receiver lacks the additive endpoints.

#### Scenario: SoundCloud accepts a track for processing
- **WHEN** the receiver receives a successful creation response with the remote identifier and link
- **THEN** it records Uploaded immediately
- **AND** it does not run post-upload audio verification or encoding polling

