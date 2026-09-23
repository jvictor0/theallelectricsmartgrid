# Recorded Patch Loads Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans for the C++ work; the independent Python reader task runs with superpowers:dispatching-parallel-agents. The implementation was kept uncommitted after the checkpoint for review; the user subsequently approved landing to main.

**Goal:** Record patch loads and whole-patch resets without per-field bursts, retaining patch storage until the writer copies it and reconstructing the resulting patch correctly.

**Architecture:** Pin reusable load/save arenas while queued patch events reference them. The writer promptly serializes these references into owned bytes. Version 3 adds capture order to grouped events and bulk load/snapshot payloads; the reader preserves old format support and replays chronologically.

**Tech Stack:** C++17, existing arena JSON and SPSC queue, Python standard library, doctest/unittest.

## Constraints

- Checkpoint `30a7837` contains the preceding work. The user approved committing and landing the follow-up after review.
- No audio-thread allocation, deallocation, waits, or mutex acquisition.
- FromJSON routines set raw values without ParamEventLogger calls.
- Keep the depth-16 limit; gestures remain leaves; sample recording assets/directories remain outside capture coverage.
- Keep grouping by `(type, name, timestamp)`; v3 adds `order:u32` after each timestamp so replay can order same-sample loads and edits.
- Type 6: empty name, width 0, `restoreFaders:u8, jsonBytes:u32, JSON` after timestamp/order. Type 7: empty name, width 0, `jsonBytes:u32, JSON`, replacing the patch after reset.

## Tasks

- [x] Add lifetime tests: a stalled writer prevents arena reuse, multiple queued reloads retain it until all copies finish, and failures/stop release references. Add failing engine regressions for full loads/reset without overrun and silent FromJSON assignments.
- [x] Add arena ownership guards, message-thread load deferral, save retry protection, and writer-owned serialized patch payloads. Keep pin acquisition/release bounded and release every discarded event.
- [x] Encode v3 typed bulk events and capture order. Add independent byte assertions and retain existing codec/error coverage.
- [x] Route loaded/saved patches through one bulk recording boundary; replace FromJSON setters with raw writes. Capture a post-reset snapshot in preallocated storage while suppressing reset deltas.
- [x] Implement Python v3 decoding and chronological replay, testing partial loads, subtree removal, fader/blend policy, same-sample edits, resets, and old recordings.
- [x] Extend the seeded engine recording test through real patch loading; compare reconstructed patches with live snapshots via generated fixtures.
- [x] Update format/docs/specs and remove outdated patch-load exclusions and examples with children beneath gestures.
- [x] Run relevant C++/Python suites, allocation/lifetime checks, strict spec validation, `make build`, and diff checks. Obtain independent review and resolve relevant findings. Report any pre-existing failures separately.

## Verification — 2026-09-22

- Checkpoint `30a7837` was created before implementation; follow-up edits were left uncommitted for review.
- The new full-load/reset regression first failed with recording Overrun in both cases; it now passes.
- 47 focused C++ cases passed, followed by 30 codec/recorder/allocation cases and 23 final engine/save/load cases after the added tests.
- Both full patch loading (including normal modulators and gesture leaves) and reset snapshot capture perform zero heap allocations on the audio thread.
- All 45 Python cases passed with state, mixer, seeded, and bulk-load fixtures. Eight live engine checkpoints match replay, including exact same-sample operation ordering and absence of per-field load events.
- AddressSanitizer passed all 19 recorder/engine cases with leak detection disabled.
- The full suite before the final two test additions passed 437/439 cases. The only failures were the established startup-silence failures at sys_startup_stability.cpp:95 and :272 (peak 0.000657712). The added cases passed in the subsequent focused runs.
- `make build` passed; both affected recording specs validate strictly; `git diff --check` passed.
- An independent subagent reviewed storage lifetime, queue/error/shutdown cleanup, silent loads, reset snapshots, and replay ordering and reported no actionable findings.

## Landing verification — 2026-09-22

The user approved landing after reviewing the implementation and documentation.
The final 441-case C++ run passed 439 cases, with only the same two documented
startup-silence failures. All 45 Python cases passed using freshly regenerated
C++ fixtures. All five affected capability specifications validate strictly and
`git diff --check` passes. The recording OpenSpec change is already archived;
the unrelated active `absolute-time-coordinates` change is outside this landing.
