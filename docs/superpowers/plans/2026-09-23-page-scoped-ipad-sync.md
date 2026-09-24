# Page-scoped iPad Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Full iPad-initiated LAN sync with stereo extraction and no sync activity outside the foreground Sync page.

**Architecture:** Page-owned native client, streamed HTTPS uploads to an explicitly launched Python receiver, Bonjour discovery. The audio engine has no dependency on sync.

**Tech Stack:** JUCE UI, Foundation NSURLSession/NSNetServiceBrowser, C++17, Python standard library, macOS dns-sd and openssl.

## Global Constraints

- NO new code on the audio thread.
- No iPad sync work or networking when the Sync page is closed or app inactive.
- Cancel and drain before page exit; no background sessions or startup discovery.
- Follow docs/superpowers/specs/2026-09-23-page-scoped-ipad-sync-design.md wire contract.
- New C++ uses structs, public members, HammerCase methods, m_ members, x_ constants, matched braces on separate lines.

### Task 1: Streaming Mac receiver

**Files:** scripts/sync_receiver.py, scripts/recording_export.py, scripts/sync_ipad.py, scripts/tests/test_sync_receiver.py.

**Interfaces:** HTTPS endpoints and Bonjour TXT exactly as in the design. Expose `create_server(host, port, root, token)` for loopback tests, returning a server with `.serve_forever()`, `.shutdown()`, `.server_close()`. Plain HTTP allowed only by the test factory; CLI always TLS. Extract stereo helper becomes dependency-free of pymobiledevice3 and is imported by both tools.

- [x] Write and run receiver tests before implementation. Exercise actual server: authenticated manifest, missing-patch copies, interrupted body, wrong hash, path escape/symlink rejection, recording extraction receipts using golden.sgrec, incomplete extraction retained, conflicting filenames.
- [x] Implement bounded streaming and atomic publish, one extraction worker overlapping uploads, hashed receipts, shutdown cleanup. Use same-directory temporary files and fsync before complete acknowledgements.
- [x] Implement CLI certificate/access-code setup and dns-sd child lifecycle, clean Ctrl-C shutdown. Never print credentials in request logs.
- [x] Run `python3 -m unittest discover -s scripts/tests -p test_sync_receiver.py -v` and extraction tests; inspect diff.

### Task 2: Native client and lifecycle

**Files:** JUCE/SmartGridOne/Source/SyncClient.hpp, SyncClient.mm, scripts/tests/sync_client_harness.mm, scripts/tests/test_sync_client.py.

**Interfaces:** C++ `SyncClient` owns native implementation. `Open`, `Close`, `Start`, `Cancel`, `Snapshot` are UI-facing; Snapshot returns value data (receivers/status/progress). Constructor performs no networking. Close cancels discovery, resolving, session and worker, then returns with no live work. Worker never references the audio engine or JUCE main component.

- [x] Write integration harness/tests driving the real client against loopback receiver with golden recordings, patches and logs. Include cancel during stalled response, close/reopen, hash mismatch/source retained.
- [x] Implement Bonjour, certificate pinning, ephemeral session, streamed uploads/downloads, sequential sync with overlapping server extraction. Worker cancellation gates every request and mutation.
- [x] Poll extraction receipts only while page session lives; recheck source before deleting. Skip unfinished SGREC and WAV/RF64.
- [x] Compile harness with Foundation/Security; run integration tests.

### Task 3: Page, app lifecycle, build and documentation

**Files:** Source/SyncPage.hpp, FilePage.hpp, MainComponent.h/.cpp, Main.cpp, SmartGridOne.jucer, both Xcode projects/Info plists, docs/ipad-tools.md.

- [x] Add File > Sync navigation and receiver list, pairing dialog, progress, Cancel. Instantiate page only on navigation. Close destroys client before restoring other page.
- [x] App suspension/inactivity cancels sync immediately. Resume discovery only when still on the page. Page timers exist only during its lifetime.
- [x] Add native source build entries and local-network/Bonjour plist declarations to generated projects and .jucer source.
- [x] Build macOS and iOS without deployment, run sync/extraction/native tests, verify no audio callback or engine changes.
- [x] Document receiver invocation, pairing, lifecycle, retry behavior and physical-device checks still pending.
- [x] Independent review of cancellation, deletion safety and full behavior before reporting completion.

## Execution evidence

Implemented in the managed detached worktree. Full Python/native regression run:
81 tests, 5 existing optional C++ fixture tests skipped, no failures. Native tests
include actual Bonjour discovery and TLS transfer on the Mac. Release builds for
macOS and iOS use CODE_SIGNING_ALLOWED=NO; no device deployment. Independent
review confirmed unchanged audio callbacks and reviewed cancellation/deletion
safety. Physical iPad checks listed in docs/ipad-tools.md remain pending.

Subsequent physical-device verification on 2026-09-23 completed a 4.12 GB sync:
both recordings arrived, stereo extraction completed, logs synced, and verified
iPad originals were removed. The normal signed Release app was restored after
temporary startup diagnostics; a 20-second network observation saw no sync
connections on normal launch. No diagnostic startup code remains. Regression
coverage was extended for uploads paused longer than five seconds and existing
files that must be checked before upload. The receiver now allows 60 seconds of
upload inactivity and consumes any started successful PUT before responding.
Physical screen-lock/background transitions remain unverified; native harness
tests cover foreground loss and page closure.
