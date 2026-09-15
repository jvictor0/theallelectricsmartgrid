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
