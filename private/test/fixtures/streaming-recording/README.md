# Streaming recording golden fixture

`golden.sgrec` is an independently assembled v1 recording. Its stream payloads
were hand-derived from signed PCM24 values and LSB-first delta bits; neither
production encoder nor decoder generated those payloads. Header framing and
CRCs were assembled with Python's `struct`, `json`, and `zlib` standard libraries.

`golden.json` records the expected header, each block's exact `record_hex`, its
`start_frame` and `frame_count`, and the decoded integer arrays under
`tracks["<id>"]`. Arrays follow the type-derived stream order. Omitted tracks are
represented by zero arrays in the expectations. `completion_hex` gives the
complete END1 record; `total_frames` is 9.

| Track | Type | First block payloads (hex) |
| --- | --- | --- |
| 1 | mono | delta width 1: `05000005` → `[5,4,4,3]` |
| 3 | panned_mono | raw: `000080ffff7f000000ffffff` → `[-8388608,8388607,0,-1]`; delta width 0: `000040` → x=4194304; delta width 0: `ffff7f` → y=8388607 |
| 7 | stereo master | delta width 2: `00000015` → `[0,1,2,3]`; delta width 1: `00000007` → `[0,-1,-2,-3]` |
| 9 | quad master | four delta-width-zero payloads: `010000`, `020000`, `030000`, `040000` |

The first two blocks have four frames each. Block two omits every track and
therefore represents four frames of silence. The final block has one frame and
includes only stereo master raw samples `feffff` (-2) and `020000` (+2); raw wins
the single-value encoding tie. END1 declares nine frames.

C++ tests can compare independently encoded records with `record_hex`. Python
tests compare each decoded sample with the explicit arrays and can apply those
same expectations to a C++-written recording.

## Parameter-event regression fixtures

V2 tests keep independent expected bytes in `streaming_recording_format.cpp` and
`test_sgrec.py`. They cover all five event types, irrelevant-field omission,
nested gesture/modulator paths, malformed payloads, repeated values, and sample
boundaries. Audio extraction tests deliberately ignore malformed event semantics.

The real-engine test records all five types across a four-frame block boundary.
The existing seeded `sys_patch_roundtrip` test now runs with recording enabled
and can export its two complete live-patch checkpoints for Python comparison.
Generated recordings stay in temporary directories; no sample assets or
sample-recording directories are added to version control.

Run the cross-language checks with an already configured CMake test build:

```sh
cmake --build /tmp/smartgrid-recording-v2-build -j 4
SMARTGRID_STATE_RECORDING_FIXTURE=/tmp/parameter-events.sgrec \
SMARTGRID_RANDOM_PARAM_FIXTURE=/tmp/random-parameters.sgrec \
SMARTGRID_RECORDING_FIXTURE_OUTPUT=/tmp/mixer-recording.sgrec \
  /tmp/smartgrid-recording-v2-build/smartgrid_tests \
  --test-case='recording engine:*,sys_patch_roundtrip: seeded*,recording mixer:*'
SMARTGRID_STATE_RECORDING_FIXTURE=/tmp/parameter-events.sgrec \
SMARTGRID_RANDOM_PARAM_FIXTURE=/tmp/random-parameters.sgrec \
SMARTGRID_RECORDING_FIXTURE_OUTPUT=/tmp/mixer-recording.sgrec \
  python3 -m unittest discover -s scripts/tests -p test_sgrec.py
python3 scripts/extract_recording.py patch /tmp/parameter-events.sgrec \
  --sample 3 -o /tmp/reconstructed-parameters.json
SMARTGRID_RECONSTRUCTED_PATCH=/tmp/reconstructed-parameters.json \
  /tmp/smartgrid-recording-v2-build/smartgrid_tests \
  --test-case='recording engine: reconstructed*'
```

Choose a fresh JSON output path when repeating the command: patch extraction
refuses to overwrite an existing file. The Python suite skips the C++ fixture
checks when their environment variables are absent. The C++ seeded test always
records and checks successful completion even without exporting a fixture.
