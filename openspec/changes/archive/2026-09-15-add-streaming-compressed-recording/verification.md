# Implementation evidence

Verified on an arm64 macOS host on 2026-09-14 (local date), against base commit
`1f779626d77bddff4294207ef3039dbb53128e5b`. Initial validation ran before the
implementation commit.

## Implemented

- `SMRTGRID` v1 PCM24 raw/delta encoder, compact descriptors, CRCs and completion.
- One preallocated 32-page SPSC ring and recorder-owned worker; nonblocking
  capture/start/stop, partial-tail drain, latched errors and shutdown acknowledgement.
- All 31 mixer tracks / 78 streams, independent mastered stereo and quad before
  listening volume, stable source lanes, shared voice/sub reduction, zeroed skips.
- Build-directory Git SHA generation for CMake and both JUCE/Xcode exporters.
- Bounded Python decoder, PCM24 WAV/RF64 extraction and safe sync retries.
- Format/CLI documentation in `docs/streaming-recording-format.md`.

## Automated checks

The standalone CMake target configures and builds successfully:

```sh
cmake -S private/test -B /private/tmp/smartgrid-recording-build
cmake --build /private/tmp/smartgrid-recording-build -j6
SMARTGRID_CODEC_FIXTURE=/private/tmp/smartgrid-codec.sgrec \
SMARTGRID_RECORDING_FIXTURE_OUTPUT=/private/tmp/smartgrid-mixer.sgrec \
  /private/tmp/smartgrid-recording-build/smartgrid_tests \
  --test-case='recording format:*,streaming recorder:*,recording mixer:*,recording engine:*'
```

The recording-focused run passed 19 cases / 2,286 assertions, including
restart after each open/write/close/throwing-close error.
Coverage includes stalled-sink overrun at all 32 occupied pages, producer stop
while full, invalid-frame rejection, skipped submissions, partial and empty
sessions, repeated cancellation/restart, master identity, mastering changes,
listening-volume independence, source-width switches and unchanged live DSP.

AddressSanitizer + UndefinedBehaviorSanitizer and ThreadSanitizer builds each
passed all 18 codec/recorder/mixer cases: 1,951 assertions under ASan/UBSan;
1,971 under TSan with the additional error-restart assertions. macOS LeakSanitizer is unavailable; requesting
`detect_leaks=1` is rejected by the runtime. Tests were then run with the supported
default address/undefined sanitizer options. No sanitizer suppression was added.

The C++ codec output matches the independent checked-in golden file byte-for-byte.
The C++ mixer fixture contains 65 frames × 78 expected integer samples; Python
verifies every scalar, SHA, sample rate, completion and both exported master WAVs.

```sh
SMARTGRID_RECORDING_FIXTURE_OUTPUT=/private/tmp/smartgrid-mixer.sgrec \
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s scripts/tests -v
PYTHONDONTWRITEBYTECODE=1 TMPDIR=/private/tmp \
  python3 -m unittest scripts/test_generate_build_info.py -v
```

Python: **24/24 passed**, including sync retries after a partial export and a
failed remote deletion. Build SHA: **3/3 passed**, including a changed-HEAD
incremental CMake rebuild, unchanged-header mtime, and execution after removal
of disposable Git metadata. The Make-based test waits one timestamp tick because
macOS Make 3.81 ignores subsecond prerequisite changes.

## Existing suite failures

The unmodified baseline `ctest` aborts in
`PartialMachine: espace etale patch remains finite after load` at the
`VectorPhaseShaper.hpp` assertion `phi_vps < 1`. The existing fuzz test already
documents this assertion as left unfixed by request. No DSP fix was made here.

Running the implemented suite with only that fatal case excluded completed:
**360 cases, 358 passed, 2 failed, 1 skipped; 1,634,520 assertions.** This includes
all five existing sampler-looper recording/persist/reload/shutdown cases.

The remaining failures are `sys_startup_stability.cpp:95` and `:272`:
pre-start output peak `0.000688948` violates the silence expectation. Both were
reproduced with identical values in a pristine `git archive HEAD` source copy,
compiled separately and run only on `startup:*`. They also fail when run alone
on the modified tree, so new recording tests are not contaminating them.

Logs from this session:

- `/private/tmp/smartgrid-recording-baseline.log`
- `/private/tmp/smartgrid-suite.log`
- `/private/tmp/smartgrid-startup-baseline-6hawfc_u/startup.log`
- `/private/tmp/smartgrid-focused.log`
- `/private/tmp/smartgrid-recorder-final.log`
- `/private/tmp/smartgrid-recorder-asan.log`
- `/private/tmp/smartgrid-recorder-tsan.log`
- `/private/tmp/smartgrid-python.log`
- `/private/tmp/smartgrid-build-sha-tests.log`

## Apple builds and lifecycle

Initialized the repository's pinned JUCE submodule at
`91ad83ae34a81e0833b1a2b0866f54846370ae53` without changing its revision.
Both **macOS and iOS Debug builds succeeded**, using their maintained Xcode
projects, scheme `SmartGridOne - App`, and `CODE_SIGNING_ALLOWED=NO`.
Derived data is in `/private/tmp/smartgrid-macos-derived` and
`/private/tmp/smartgrid-ios-derived`. Build logs are
`/private/tmp/smartgrid-macos-build.log` and `/private/tmp/smartgrid-ios-build.log`.
Both generated headers and compiled binaries contain the expected full base SHA.

The real JUCE lifecycle was inspected: `AudioSourcePlayer::setSource(nullptr)`
takes `readLock` to wait out rendering before clearing its source; only then does
it invoke `releaseResources`. That calls recorder shutdown. Preparation occurs
before attaching the source. Repeated wrapper/member shutdown is safe.
The engine test exercises the actual record cell, error color, sample-rate/layout
registration, preparation, capture, stop and restart. No live app/device session
was launched, no signed deployment was performed, and no iPad files were transferred.

## Desktop measurements

Standalone recorder, full 31-track layout, 48 kHz, one-second blocks, real disk
writes paced at real time. Each final run lasted **30 seconds / 1,440,000 frames**.
Ratio is total file bytes divided by the equivalent 78-stream raw PCM24 payload.
RSS includes the standalone process; the host application's existing DSP and
logging storage is additional.

| Input | File bytes | Ratio | Max block encode/write | Ring high-water | Capture ns/frame | Max 1024-frame capture batch | Peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Silence | 3,544 | 0.0000105 | 10.53 ms | 1/32 | 372 | 2.69 ms | 33.7 MiB |
| Sines + moving pans | 166,588,324 | 0.4944 | 53.57 ms | 3/32 | 362 | 2.90 ms | 39.1 MiB |
| Independent random audio + coordinates | 336,971,944 | 1.00004 | 85.39 ms | 5/32 | 340 | 2.08 ms | 44.3 MiB |

All runs wrote every frame, completed without error, and counted **zero
capture-thread allocations** across start, submission/commit and stop. Timing
excludes signal generation and isolates recorder work, not the whole synth
callback. Maximum worker time measures encode/write; transpose/quantization
occurs while consuming pages before that measurement. The high-water mark is
observed after page publication. A deliberately stalled sink separately proved
the full-ring error/drain path. These initial desktop results support keeping
the proposed one-second blocks and 32 slots.

Final logs: `/private/tmp/smartgrid-final-{silence,audio,noise}.log`.
Reproduce using `private/test/tools/benchmark_recording.cpp` and the command in
the format documentation. This is a smoke measurement, not a long-duration soak.

## Review and remaining checks at initial implementation

Independent review found no DSP changes, ring ownership defects or unnecessary
abstractions. Fixed the reported master-metadata validation mismatch, exception
during sink close, and sync retry behavior. Worker stderr logging avoids adding
accesses to the shared logger's audio sample counter and SPSC queue.

At initial implementation, tasks **6.2 and 6.3 remained open for physical device checks**: live macOS/iPad
recording controls, interruption/device shutdown, exported playback, and iPad
storage/callback performance under representative synth load. The available
desktop build, fixture, lifecycle and benchmark portions are complete. Hardware
results are not inferred from host tests. No archive or main-branch integration
had been performed at that point. The later authorized Wi-Fi measurements below complete those checks.

## PR preparation

Rebased the implementation onto `main` commit
`1d3ffb8` (tanh-radius panning). Its new direct `PartialMachine.hpp` test include
exposed the header's missing `NormGen.hpp` dependency (`RGen` was undeclared).
Added that direct include; the standalone CMake target builds successfully.

Post-rebase validation passed all **24 selected C++ cases / 2,305 assertions**
(19 recording cases plus the 5 updated panning cases), and **24 Python tests**
against the newly generated C++ fixture. The build-SHA tests passed 3/3 during
PR preparation. Logs are `/private/tmp/smartgrid-pr-rebase-build.log`,
`/private/tmp/smartgrid-pr-rebase-focused.log`,
`/private/tmp/smartgrid-pr-rebase-python.log`, and
`/private/tmp/smartgrid-pr-sha-tests.log`.

## Requested review follow-ups

Implemented the two owner comments on PR 4: error indication blinks red/off at
4 Hz using `SampleTimer`, and each latched recording error goes through the
async logger once with state, accepted/written frames, written bytes and queue
high-water. Reporting occurs on the capture owner or after shutdown has quiesced
audio; the worker does not read the sample clock or enter the sampler writer's
SPSC logging queue.

The new checks failed before implementation (static error color and no async
error messages), then passed with the changes. The standalone CMake build and
28 selected recording/logger cases passed (2,346 assertions); ThreadSanitizer
passed 20 codec/recorder/mixer cases (1,989 assertions). Logs are
`/private/tmp/smartgrid-review-tests.log` and
`/private/tmp/smartgrid-review-tsan.log`. A subsequent full-PR subagent review is
read-only; its findings are reported to the owner without automatic fixes.

## Rebase and physical iPad recording evidence (2026-09-15)

Rebased onto local `main` at `272a662`, preserving its async USB/Wi-Fi sync,
atomic downloads, and stable remote-size check. Adapted the recording sync tests
to the async API and added a check that a growing remote recording is retained
even when the downloaded snapshot extracts successfully. All **34 Python
recording/iPad-tool tests** and **3 build-SHA tests** passed. OpenSpec strict
validation passed. The C++ sources and tests are unchanged by this rebase.
The preceding full-PR subagent review reported no actionable findings.

The owner captured and synced
`recording-2026-09-15T225029-614762-0.sgrec` from the iPad Air 13-inch (M3).
The supplied sync transcript shows successful download, stereo extraction, and
remote deletion after extraction. The local original remains in
`~/Documents/SmartGridOne/recordings/` alongside the stereo WAV.

The actual file validates with clean completion, all block CRCs, **193,953
frames at 48 kHz (4.0406875 seconds)**, timestamp `2026-09-15T22:50:29Z`, and
compiled SHA `d0cc77025b44570fcc672f6e60a4f50f660cbadf`. Every PCM24 integer in
the synced stereo WAV matches the recorded stereo master. A separate quad
extraction to a temporary WAV likewise matches every recorded quad integer.
The temporary quad export was removed after verification.

The 31-track/78-stream PCM24 equivalent is **45,385,002 bytes**, compared with
**7,632,713 bytes** on disk: **5.946:1**, or **83.18% smaller**. Track payloads,
descriptors, and shared framing account for every byte. Headers and descriptors
total 3,796 bytes. Twenty-two tracks are omitted throughout; `mono_2` is also
omitted in the final 1,953-frame block. This is a short live capture, not a
sustained iPad performance measurement.

Remaining physical checks are live playback, shutdown/interruption behavior,
and sustained iPad recording under representative synth load, including callback
cost, worker latency, queue high-water, and memory. No such results were inferred
from this short recording. Those checks were subsequently completed in the
authorized Wi-Fi measurement run below.

## Authorized Wi-Fi iPad measurements (2026-09-15)

The owner authorized deployment and physical-device measurement rather than
deferring tasks 6.2/6.3. The measured device was the iPad Air 13-inch (M3),
with MAYA44 USB+ input/output at 48 kHz, four hardware channels, and 512-frame
callbacks (10.667 ms deadline). The final run spans approximately
16:34:11–16:37:48 America/Los_Angeles.

A temporary Release measurement build based on `6e01f02` exercised the existing
record-cell start path and recorder stop API inside the real app audio callback.
The loaded patch and live DSP continued running. Synthetic phases replaced only
the samples submitted to the test recording: all 78 streams were exercised with
silence, sines/moving coordinates, and independent random audio/coordinates.
The synthetic recording masters are test data, not exports of the audible mix.
All temporary hooks were removed from the source after building. The unchanged
normal Release app was rebuilt, reinstalled, and launched successfully afterward.

| Input | Accepted/written frames | File bytes | Raw PCM24 / file | Max block encode/write | Ring high-water | Mean callback | Max callback |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Loaded patch, 60-second phase | 2,879,488 | 43,290,272 | 15.56470:1 | 9.41 ms | 1/32 | 4.529 ms | 5.045 ms |
| Silence, 30-second phase | 1,439,744 | 4,052 | 83,144.15:1 | 4.70 ms | 1/32 | 4.529 ms | 5.019 ms |
| Sines + moving pans, 30-second phase | 1,439,761 | 166,561,230 | 2.02270:1 | 30.68 ms | 2/32 | 4.532 ms | 5.007 ms |
| Independent random audio + coordinates, 30-second phase | 1,439,744 | 336,912,548 | 0.99996:1 | 46.28 ms | 3/32 | 4.524 ms | 5.053 ms |

Phase duration includes asynchronous startup; the accepted frame count is the
exact recording length. Compression includes complete container overhead and
silent-track omission. The loaded patch is sparse, so its ratio is not a
prediction for a fully active patch. All 31 tracks were present in every signal
and random-data block, and every silence block omitted all tracks.

The 20-second recording-off baselines measured 4.531 ms mean / 5.416 ms maximum
before recording and 4.525 ms mean / 4.996 ms maximum afterward. All post-warmup
phases had zero audio deadline overruns, long callback gaps, format mutes, and
USB audio xruns. Thermal state remained nominal. Warmup itself recorded one
22.01 ms callback, one long gap, and one format mute before any recording began;
those startup observations are retained rather than counted as steady-state
recording performance.

Recorder calls sampled in 1/128 callbacks took approximately 555 ns per frame
in the loaded-patch phase (429–446 ns in the short lifecycle/recovery phases).
These figures include probe clock overhead and are an instrumented upper
estimate. The worker maximum covers encoding/writing, not earlier page
transposition/quantization. The queue high-water includes all worker stages.
Whole-app physical footprint peaked at 1.813 GiB; this includes the existing
synth/sample/delay state and is not a recorder-only allocation measurement.
The callback means do not resolve a meaningful recording-on versus off
regression in this run. The existing one-second blocks and 32-page ring require
no tuning based on these results.

Lifecycle and failure checks:

- Closing the actual JUCE audio device while recording drained all 240,128
  accepted frames into a complete file. Device close took 1.036 seconds on the
  message thread, including OS audio-device teardown. Reopening the device and
  recording again produced a complete 239,616-frame file without errors.
- A deliberately injected 1.5-second worker stall filled all 32 ring slots.
  Capture stopped with `Overrun` after 79,872 accepted frames; every accepted
  frame drained, and the file omitted `END1`. Audio callback timing and xruns
  remained clean. The async log recorded the failure once.
- Starting again after that error produced a complete 239,681-frame recording.
- All eight final-run recordings passed full bounded-reader block, CRC,
  sample-range, timeline, frame-count, and file-size verification. The seven
  normal files had valid completion markers. The stalled file produced exactly
  the expected missing-completion error after its two valid blocks.
- Both mastered WAVs from the one-minute actual recording matched every one of
  its 2,879,488 recorded frames. Both stalled-file exports matched all 79,872
  valid-prefix frames and reported incomplete status.

The first temporary probe had a phase-boundary timing artifact: an unmeasured
callback could enter the next phase's end-timing hook. That probe was corrected
by pairing each callback's start/end measurement explicitly, and all final
measurements above come from the repeated run. No production fix was needed.
The normal restored app's log confirms 48 kHz / 512 frames / four-channel MAYA
routing, nominal thermal state, and approximately 42.3% reported audio CPU.
This validates the internal digital path; no external analog speaker capture
was performed, and this several-minute exercise is not a long-duration soak.

Local evidence is retained under `/private/tmp/smartgrid-ipad-measurement/`:
`recording-probe.jsonl`, `validation.json`, `validation.log`, `exports.log`,
`build-v2.log`, `install-v2.log`, `launch-v2.log`, `restore.log`, the final app
log, and `restored-app-log/`. `instrumentation-v2.patch` and `probe-v2/` preserve
the temporary measurement changes; `normal-SmartGridOne.app` and
`measurement-v2-SmartGridOne.app` distinguish the two built executables.

The preliminary app upgrade also interrupted a live random-data recording.
Its preserved file contains 17 fully validated blocks / 816,000 frames and
reports exactly `Missing END1 completion marker after 816000 frames`, confirming
the real process-interruption prefix behavior. The 12 measurement-only device
recordings were identified by the captured initial inventory, test build SHA,
and test time window for cleanup; the final-run files, exports, and interrupted
recording are preserved on the Mac. No original user recording was removed.
