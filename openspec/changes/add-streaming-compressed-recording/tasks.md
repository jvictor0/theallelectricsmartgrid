## 1. Format, Encoder, and Python Reader

- [x] 1.1 Document the v1 `SMRTGRID` header, type-derived stream schema, legacy PCM conversion, compact BLK1 descriptors, fixed END1 completion marker, CRCs, and size limits in `docs/streaming-recording-format.md`.
- [x] 1.2 Implement C++ PCM conversion and raw/delta encoding helpers; compare output to independent golden bytes for endpoints, rounding ties, signed widths 0 through 25, constants, ramps, noise, raw ties, and padding. Preserve the existing audio quantizer and add no production C++ decoder.
- [x] 1.3 Implement session/block serialization, silent-track omission, contiguous frame positions, alternate declared block sizes, partial final blocks, and the clean-completion marker; verify exact bytes and zero-frame/all-silent sessions.
- [x] 1.4 Add `scripts/sgrec.py` as the shared Python streaming decoder. Verify C++-written files against expected integers for all track types and mixed encodings; reject malformed metadata, widths, derived lengths, ranges, CRCs, padding, and frame discontinuities within bounded memory.

## 2. Single-Ring Recorder and Writer

- [x] 2.1 Implement preallocated frame staging and one SPSC ring using `NextToPush`/`CompletePush` and `PeekPtr`/`Pop`; test that skipped submissions are zeroed and a slot is released only after consumption. Keep stop/error signaling independent of ring capacity.
- [x] 2.2 Add the recorder-owned worker for file/header preparation, transpose, quantization, block assembly/encoding, and direct disk writes. Implement readiness, recording, stop/drain acknowledgement, cancellation while starting, and busy-start rejection using existing atomic state patterns.
- [x] 2.3 Handle overrun, invalid samples, and disk errors without blocking capture; drain accepted frames where possible and omit END1 on capture failure. Implement partial-tail publication and quiesce/drain/join shutdown before buffer destruction.
- [x] 2.4 Use a controllable slow/failing sink to verify queue-full stop, partial pages, open/write/close failure, restart, and shutdown. Check page ownership and that capture/start/stop perform no allocation, locks, sleeps, filesystem work, or joins.

## 3. Mixer Integration

- [x] 3.1 Register stable tracks for every panned input, declared mono lane, three quad returns, and two masters using the engine sample rate. Replace WAV submissions with the existing shared voice/sub reduction and post-saturation return values; verify the current 31-track/78-stream layout.
- [x] 3.2 Submit the actual quad and stereo mastering outputs and commit exactly one frame in `ProcessReturns`, before global listening volume. Cover combined/split processing, source width switches, skipped lanes, and noise-mode transitions without stale samples.
- [x] 3.3 Adapt recording controls/status in `SquiggleBoy.hpp` and `TheNonagonSquiggleBoy.hpp`, plus off-thread preparation and shutdown ownership in `NonagonWrapper.hpp` and standalone tests. Preserve the sampler-looper path.
- [x] 3.4 Add mixer/system tests proving shared voice/sub reduction, monitor-independent stems, return ordering, direct master PCM identity, listening-volume independence, mastering-control effects, and unchanged live DSP when recording is enabled.

## 4. Build SHA

- [x] 4.1 Generate a build-directory header containing the full commit SHA for both JUCE exporters and standalone CMake tests, preserving existing build hooks and direct Xcode support. Avoid unchanged rewrites; add no dirty-state tracking or source-archive override mechanism.
- [x] 4.2 Verify a changed-HEAD rebuild without cleaning updates the SHA recorded by the compiled application/test target, with no runtime Git invocation.

## 5. WAV Extraction and Sync

- [x] 5.1 Add inspection and master/track selectors in `scripts/extract_recording.py`. Export exact PCM24 mono/stereo/quad WAVs, panned-mono audio-only stems, and RF64 for large outputs; test silence insertion, channel order, true final length, overwrite handling, and the RIFF size boundary.
- [x] 5.2 Implement one incomplete-file behavior: emit only validated blocks, finalize the usable prefix, report frames/reason, and return nonzero on corruption or missing completion. Test damaged tails, bad CRCs, invalid headers, and clean empty recordings without adding recovery modes or sidecars.
- [x] 5.3 Update `scripts/sync_ipad.py` to dispatch by magic between the new master-stereo extractor and legacy WAV/RF64 SoX extraction. Mock transfers and verify only a zero extraction exit status permits remote deletion, even when an incomplete export leaves a playable WAV.

## 6. Verification and Documentation

- [x] 6.1 Run the standalone CMake suite, existing sampler-looper round trips, and Python cross-language/extraction tests; use available address/thread sanitizers for the targeted ownership and error cases.
- [ ] 6.2 Build the maintained macOS and iOS targets and smoke-test recording controls, file creation, build SHA, both mastered exports, and shutdown.
- [ ] 6.3 Measure sustained full-layout recording with silence, representative audio/moving pans, and worst-case noise on macOS/iPad. Record compression, worker latency, ring high-water, peak memory, and callback cost; confirm or tune the one-second/32-slot defaults and exercise a stalled sink.
- [x] 6.4 Finish format/CLI documentation, including pre-listening-volume masters and incomplete-export exit status. Record any unavailable hardware checks accurately, validate the OpenSpec change, and attach implementation evidence before checking off tasks.

## Verification notes

See [verification.md](verification.md) for commands, test results, known baseline failures, desktop measurements and platform build evidence. Both Apple builds and the desktop benchmark passed. The 2026-09-15 iPad recording also confirms file creation, the current build SHA, clean completion, sync, and exact stereo/quad extraction. Tasks 6.2 and 6.3 remain unchecked for live playback/shutdown/interruption checks and sustained iPad performance under representative synth load.
