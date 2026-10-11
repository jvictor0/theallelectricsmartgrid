## 1. Confirm setup and establish the catalog

- [x] 1.1 Resolve the Mac SoundCloud authorization choice in the design before implementing that backend; confirm the proposed recording-date and public-post defaults during design review. Approved 2026-10-11.
- [ ] 1.2 Add the Mac recording catalog and SQLite persistence for recording versions, jobs, remote metadata, and receipts; discover existing stereo exports without reading whole audio files.
- [ ] 1.3 Integrate completed receiver extraction with the catalog and implement stable recording-date fallbacks, durations, newest-first listing, and explicit rescan.

## 2. Add SoundCloud publishing on the Mac

- [ ] 2.1 Implement the approved account connection and Keychain token storage, including serialized token refresh; document app registration and the supported callback setup.
- [ ] 2.2 Add cached, paginated account-track listing and deterministic default-title allocation with atomic local reservations.
- [ ] 2.3 Implement durable queued jobs and streamed multipart creation with Electronic genre, public visibility, optional custom title, and optional artwork.
- [ ] 2.4 Persist successful receipts and implement definite-failure retry, uncertain-outcome handling, and restart behavior without automatic duplicate uploads.
- [ ] 2.5 Add historical match suggestions and explicit account-track linking without assuming title/date equality proves recording identity.

## 3. Extend the receiver and iPad client

- [ ] 3.1 Add the authenticated publishing, refresh, upload, and historical-link endpoints, with request identity deduplication and recording-version checks.
- [ ] 3.2 Separate bulk sync and publishing/control request state in the native client while sharing existing receiver identity, pairing, and certificate validation.
- [ ] 3.3 Add publishing snapshots, active-work status polling, Mac switching, and foreground/page lifecycle behavior; keep Cancel sync independent of accepted Mac publication jobs.

## 4. Extend the Sync screen

- [ ] 4.1 Add the selected-Mac recording view with sync progress, newest-first stereo rows, stable selection, and uploaded links.
- [ ] 4.2 Add the inline title editor, Use default action, image picker/preview/removal, and immediate Upload action with queued/uploading/failed/unknown states.
- [ ] 4.3 Add historical-link selection and graceful messages for unavailable SoundCloud authorization or an older receiver.

## 5. Verify and document the feature

- [ ] 5.1 Cover catalog ordering/date fallbacks, naming, metadata/artwork submission, historical linking, persistence, duplicate requests, and lost remote responses with focused receiver tests and a fake SoundCloud service.
- [ ] 5.2 Extend the native harness for publishing during bulk sync, cancel/close/foreground transitions, and reconnecting to accepted jobs; run existing sync/extraction regression tests.
- [ ] 5.3 Build macOS and iOS and perform one iPad walkthrough for list selection, title/image editing, and leaving/reopening the page during a Mac job. Make a real SoundCloud upload only with separate authorization.
- [ ] 5.4 Update `docs/ipad-tools.md` with Mac setup, everyday uploading, historical linking, and the distinction between leaving the page and stopping the receiver.
