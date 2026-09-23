# Page-scoped iPad LAN sync

Approved in conversation on 2026-09-23. Implements the full sync_ipad.py behavior
through a Mac receiver and a dedicated Sync page.

## Hard constraints

- NO new code on the audio thread. No changes to render callbacks, recording
  producers, DSP, audio queues, or audio-thread polling.
- The iPad does no sync discovery, resolving, networking, file scanning, hashing,
  or transfer work unless the Sync page is open and the app is foreground active.
- Closing the page cancels and drains its work before navigation completes.
  Backgrounding stops it too. No background URL session or startup singleton.
- The separately launched Mac receiver advertises only while its command runs.

## User flow

File > Sync opens discovery. Bonjour lists SmartGrid receivers on the LAN. On
first use the user compares the receiver certificate fingerprint with the Mac
terminal and enters its access code. The certificate and code identify that
receiver on subsequent visits. A selected receiver starts sync. The page shows
file progress, MiB/s, extraction progress and errors, plus Cancel. Leaving the
page cancels everything locally; completed Mac files remain available.

## Components and transport

An engine-independent C++ interface and Objective-C++ implementation own Bonjour
and an ephemeral NSURLSession. A page-owned worker enumerates files, hashes,
uploads and downloads. Native discovery callbacks run on the message thread;
HTTP delegates use their own operation queue. Cancellation never waits for work
that requires the message thread. App inactivity cancels the page's session.

A Python standard-library HTTPS receiver streams into temporary files, checks
SHA-256, atomically publishes files, and runs the existing stereo extractor in
a separate worker. Bonjour advertising uses macOS dns-sd. Its self-signed
certificate is pinned by the client; a persistent random access code authenticates
clients. Credentials and keys stay outside the repository.

## Sync semantics

Missing patch files copy both ways, never overwrite. Logs copy to the Mac when
larger and stay on iPad. Completed recordings copy to Mac, retain their originals
on Mac, and extract the exact stereo master. Extraction overlaps later uploads.
Only a successful extraction receipt for the uploaded SHA-256 permits iPad
deletion, after rechecking local identity/content. Skip incomplete/growing files.
Current SGREC completion is recognized by its END1 footer; legacy WAV/RF64
requires a finalized header. No audio-engine state is read or modified.

Incomplete downloads are never published. A failed upload/extraction keeps the
iPad original. The first version restarts partial files, as sync_ipad.py does.

## Wire interface

Bonjour type `_sgsync._tcp.`; TXT `version=1`, `id=<persistent UUID>`,
`fingerprint=<lowercase SHA-256 of DER leaf certificate>`.
All HTTPS endpoints require `Authorization: Bearer <access code>`.

- GET `/v1/manifest`: JSON `{version:1, patches:[{path,size,sha256}]}`.
- GET `/v1/patch?path=<encoded relative path>`: raw file bytes.
- PUT `/v1/patch?path=...`, `/v1/log?path=...`, `/v1/recording?path=...`:
  raw bytes, Content-Length and X-SHA256 headers. Return JSON.
- Recording PUT returns `{state:"extracting",sha256:<digest>}` (or complete).
- GET `/v1/recording?path=...&sha256=...`: JSON state `extracting`, `complete`,
  or `failed`, with sha256 and optional error. Complete means extraction finished
  and both original and stereo files were flushed to storage.
- Reject unsafe paths, symlinks, malformed sizes/checksums, unauthorized requests,
  and conflicting recording names. Do not silently overwrite patch conflicts.

## Verification

Loopback tests cover real HTTP/file transfer, hash failures, interruption,
patch conflicts, extraction failure and successful receipts. A standalone native
client harness tests the real client against the receiver, including closing
mid-transfer and reopening. Build macOS and iOS, inspect the audio diff, and run
existing extraction/sync tests. Physical iPad discovery, permissions, throughput
and background lifecycle checks remain explicitly pending while it is offline.
