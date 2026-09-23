# SGREC FLAC Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Write FLAC-compressed SGREC recordings and extract them losslessly while preserving versions 1–3.

**Architecture:** Keep the one-second outer records and independent scalar streams. Isolate the native encoder behind a JUCE-free header, use existing bundled libFLAC in the app, and a lazy ctypes decoder in Python.

**Tech Stack:** C++17, pinned JUCE/libFLAC, Python 3, ctypes, doctest, unittest.

## Global Constraints

- Version 4 / BLK4 supersedes the unshipped adaptive prototype.
- Descriptor 2/0: u32 little-endian FLAC byte length, then a complete mono PCM24 FLAC stream at session sample rate and outer frame count.
- Level 5, internal block size 1024; source visited once with bounded scratch.
- Constant streams retain 1/0; raw 0/24 allowed when smaller for short buffered streams.
- Sparse tracks, quantization, events, CRC, END1, one-second scheduling and worker ownership are preserved.
- Follow AGENTS.md C++ style. No recording originals are modified. After implementation review, the user authorized landing and the repository’s rebuild/redeploy procedure.

### Task 1: Native FLAC encoder and build integration

**Files:** Create `private/src/RecordingFlacCodec.hpp`, `private/src/RecordingFlacCodec.cpp`, `private/test/unit/recording_flac_codec.cpp`; modify `private/test/CMakeLists.txt`, `JUCE/SmartGridOne/SmartGridOne.jucer`, both Apple `project.pbxproj` files.

**Interfaces:** Produce `RecordingFlacCodec::Encoding { uint8_t m_encoding; uint8_t m_width; }` and `static bool Encode(const int32_t* samples, uint32_t frames, uint32_t sampleRate, bool coordinate, std::vector<uint8_t>& output, Encoding& encoding, bool& audible)`. Successful output appends its complete payload (length included), preserves prefix, reports audible for nonzero audio only, and validates all input even silent coordinates. Failure restores original output length. Maximum total output is 64 MiB. Header exposes no JUCE/FLAC includes.

- [x] Write real round-trip tests before implementation: endpoints, changing waveform longer than 1024, short final block, constant positive/negative and coordinate, invalid/null/zero input, preserved output prefix. Decode with libFLAC, checking samples and metadata, rather than reproducing packing logic.
- [x] Demonstrate failing tests for missing encoder behavior; implement fixed scratch buffering, checked API calls, deterministic cleanup, bounded callback writes and constant/raw selection.
- [x] Standalone target links installed libFLAC via pkg-config; app translation unit uses bundled FLAC headers in `juce::FlacNamespace`, linking existing juce_audio_formats C symbols. Add source to both generated Apple projects and .jucer. The app must not require Homebrew FLAC.
- [x] Run focused native tests and sanitizer variant. Report commands, results, and dependency/build caveats in the task report. Do not commit shared working state.

### Task 2: Container, reader and extraction

**Files:** Modify `private/src/RecordingFormat.hpp`, `private/test/unit/streaming_recording_format.cpp`, `scripts/sgrec.py`, `scripts/tests/test_sgrec.py`; create `scripts/sgrec_flac.py`, `scripts/tests/test_sgrec_flac.py`; remove superseded adaptive production/tests and replace benchmark references.

**Interfaces:** Consume Task 1 Encode interface. Python helper `decode_flac(payload, frame_count, sample_rate, coordinate)` returns signed `array('i')`, raises ValueError for malformed content or unavailable library; reader converts failures to RecordingError. Unselected tracks only validate declared byte boundaries and structural FLAC metadata, without invoking native decode.

- [x] Add independent FLAC fixtures with FLAC CLI and failing Reader tests for descriptor 2/0, audio+coordinates, selection, event replay and legacy descriptor rejection before v4.
- [x] Add malformed tests: size overflow/truncation, metadata shape/rate/depth/channel/count mismatch, corrupt frame, missing/truncated samples, trailing bytes and coordinate-negative values.
- [x] Implement lazy ctypes loader and bounded decoder, metadata/sample validation, exact consumed-byte checks and MD5 verification. Avoid callback exception escape and do not allocate using untrusted dimensions.
- [x] Replace prototype format encoder, retaining descriptor compaction and silence semantics. Adapt the writer golden test to independent raw/constant payload expectations; preserve old reader golden fixture unchanged.
- [x] Run Python suite, native format/worker tests, and cross-language C++ fixture extraction including WAV export and patch replay.

### Task 3: Verification, documentation and review

**Files:** Update `docs/streaming-recording-format.md`, `openspec/specs/streaming-recording/spec.md`, `openspec/specs/recording-extraction/spec.md`, research report, and this checklist.

- [x] Document exact v4 layout, dependencies, backward reading, worker resource bound, and supersession of the adaptive exploration. Remove obsolete adaptive plan/source artifacts from the final implementation diff while retaining research findings.
- [x] Validate standalone full suite where feasible, ASan/UBSan focused suites, both Apple build paths, and representative sampled real data with exact decoded-value equality.
- [x] Obtain independent spec/quality review and resolve concrete findings, then inspect final diff and report results and remaining device-only validation clearly.

## Verification outcome

Completed all three tasks. Production encoder and extractor preserve every integer
in 5795 real-data streams. macOS/iOS builds, 35 native focused cases, 18 sanitizer
cases, and all 53 Python cases pass. Full native suite retains the two documented
pre-existing startup-silence failures (445/447 pass). No unrelated test was changed.
Independent spec/quality review passed; its two minor documentation/test-label
findings were corrected. Detailed results and remaining on-device measurement are
in `docs/research/2026-09-23-sgrec-real-recording-codecs.md`.
