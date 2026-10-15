## Why

The Mac already receives iPad recordings and extracts their stereo masters, but
publishing a nightly recording still requires a separate SoundCloud workflow.
The existing Sync page should let the user choose a recording and publish it
without leaving the iPad or pairing with another service there.

## What Changes

- Selecting a paired Mac starts the existing sync and opens its stereo recording
  list, newest recording first. The list remains usable during sync.
- Each available recording offers Upload to SoundCloud, with an optional title
  override and image selected on the iPad.
- Defaults are `Smart Grid Nightly YYYY/MM/DD`, genre `Electronic`, public
  visibility, and no supplied artwork. The date is the recording's local start
  date. Further posts for that date use ` #2`, ` #3`, and so on.
- The Mac owns SoundCloud authorization, upload jobs, and persistent upload
  receipts. The existing iPad pairing authorizes these additional operations.
- Uploaded recordings show their status and SoundCloud link. Historical manual
  uploads can be linked once; title similarity alone is not proof of identity.
- Accepted uploads run on the Mac after the iPad leaves the page. All iPad
  discovery, requests, and polling remain scoped to the foreground Sync page.

## Capabilities

### New Capabilities

- `soundcloud-publishing`: Browse Mac stereo recordings and publish them from
  the iPad with nightly defaults, optional artwork, and persistent upload status.

### Modified Capabilities

None. Existing patch, log, recording transfer, extraction, and deletion
requirements remain in force. The new capability extends the LAN Sync page.

## Impact

- JUCE `SyncPage.hpp`, `SyncClient.hpp`, and `SyncClient.mm`.
- Python `scripts/sync_receiver.py`, with small separate catalog and SoundCloud
  publisher modules, plus receiver and native-client tests.
- Additive authenticated receiver endpoints and a Mac-local publishing database.
- SoundCloud's official upload API and Mac-held OAuth credentials. No SoundCloud
  credentials on the iPad and no audio-engine changes.
- Setup dependency: the repository currently has no SoundCloud authorization.
  Official API access needs a registered application and one-time Mac account
  authorization unless an existing integration can supply them. A browser login
  does not itself provide the receiver with API authorization. This distinction
  must be resolved before implementation; it does not change the iPad flow.

## Non-goals

Audio editing or mastering, bulk automatic publication, scheduling, playlist
management, deletion or editing of published tracks, background iPad activity,
browser automation, a generic media library, and repeated upload preflight or
post-upload verification passes.
