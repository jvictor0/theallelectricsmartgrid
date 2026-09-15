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

## Review and remaining checks

Independent review found no DSP changes, ring ownership defects or unnecessary
abstractions. Fixed the reported master-metadata validation mismatch, exception
during sink close, and sync retry behavior. Worker stderr logging avoids adding
accesses to the shared logger's audio sample counter and SPSC queue.

Tasks **6.2 and 6.3 remain open for physical device checks**: live macOS/iPad
recording controls, interruption/device shutdown, exported playback, and iPad
storage/callback performance under representative synth load. The available
desktop build, fixture, lifecycle and benchmark portions are complete. Hardware
results are not inferred from host tests. No archive or main-branch integration
has been performed.

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
