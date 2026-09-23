# SGREC codec measurements on actual recordings — 2026-09-23

## Decision

Adaptive first-difference runs fall below the approximately 1.5x target on native recordings. FLAC level 5 with bounded 1024-sample blocks is worth a separate integration trial: approximately 1.47x on the sampled large native recording, 1.86x on a complete short native capture, and 2.01x–3.13x on two older converted performances. These measurements preceded implementation. The approved FLAC integration now writes v4 streams; see the follow-up below. The adaptive prototype was superseded.

All ratios mean original SGREC bytes divided by candidate bytes. For example, 1.5x means the candidate is two-thirds as large.

## Original engine test

The earlier 1.09x result used stereo masters from `/tmp/smartgrid-random-params.sgrec`, exported by `sys_patch_roundtrip: seeded encoder values survive save/load` in `private/test/system/sys_patch_roundtrip.cpp`. It randomizes up to six connected encoders across three scenes with fixed seeds, saves a patch, overwrites values, reloads the saved patch, and checks restored values while the engine runs. The recording was 171010 frames (3.56 seconds). This is a save/load regression fixture, not a representative performance corpus; it did not justify dismissing compression on actual performances.

## Real recording results

Four locally available files were analyzed in full. Each of the two large local files was sampled at 96 evenly spaced existing outer-block positions, including the first and last blocks. The index covers every outer block; only selected audio payloads were decoded for the large files. This produced 5795 included scalar streams and 575571452 decoded PCM bytes. All included stems, returns, masters, and coordinates count toward size; omitted tracks remain omitted.

The September 18 recording is an iCloud/dataless placeholder and could not be read. It is excluded.

| Recording | Original GB (decimal) | Coverage | Adaptive 64 | FLAC fixed 1024 | FLAC level 5, 1024 | FLAC level 5, 4096 |
|---|---:|---|---:|---:|---:|---:|
| `recording-2026-09-16T014721-949538-0.sgrec` | 3.644169 | 96 / 1523 blocks | 1.151x | 1.376x | 1.470x | 1.483x |
| `recording-2026-09-15T225029-614762-0.sgrec` | 0.007633 | complete | 1.274x | 1.665x | 1.856x | 1.766x |
| `recording-2026-09-16T043656-635302-0.sgrec` | 0.000514 | complete | 1.080x | 1.230x | 1.421x | 1.460x |
| `recording-00013.sgrec` | 4.123707 | 96 / 11268 blocks | 1.378x | 1.812x | 2.009x | 1.937x |
| `recording-2026-02-10T181510.869.sgrec` | 0.016938 | complete | 1.134x | 3.013x | 3.128x | 3.228x |
| `recording-2026-02-01T143437.076.sgrec` | 0.000004 | complete | 1.000x | 1.000x | 1.000x | 1.000x |

The first three September recordings in the table are native SGREC captures. `recording-00013` and the February recordings were converted from legacy multichannel WAVs and have different historical track layouts and 8192-frame outer blocks. The February 1 recording has no included audio payloads: its small size is metadata and silent-block framing. The native captures use 48000-frame outer blocks.

For complete files, candidate totals retain the exact original header, END1, and block/descriptor overhead and replace only stream payload sizes. Large-file ratios are estimates from aggregate selected record bytes, with the same unchanged framing. They are not full re-encodes or statistical guarantees. The original codec reproduced every sampled original payload length exactly.

Across the native files, weighting by original file bytes gives approximately 1.15x for adaptive runs and 1.47x for FLAC level 5 at 1024 samples; the large native recording dominates that weight. Across all six local files, the corresponding estimates are 1.26x and 1.72x.

## Other schemes

| Candidate | Large native recording | Older 00013 performance | Status |
|---|---:|---:|---|
| First deltas, fixed groups of 64, 5-bit width | 1.129x | 1.247x | Exact size model |
| Orders 0–2, fixed groups of 128, signed packing | 1.242x | 1.400x | Exact size model |
| Orders 0–2, fixed groups of 128, Rice codes | 1.311x | 1.542x | Exact size model |
| FLAC fixed predictors, 128-sample blocks | 1.292x | 1.640x | Actual libFLAC output |
| FLAC level 8, 4096-sample blocks | 1.505x | 1.994x | Actual libFLAC output |

The custom models include their proposed per-group headers, predictor warmup, raw fallback, final byte padding, and the existing three-byte whole-constant representation. They do not claim production CPU performance. The Rice model exhaustively searches its parameter for reliable size accounting. FLAC outputs include complete per-stream FLAC metadata/frame overhead; constant streams retain the existing three-byte SGREC representation. No inter-stream/stereo decorrelation was used: every scalar stream was encoded independently.

## CPU and bounded input

Measurements use the installed libFLAC 1.5.0 through its C API, one encoder at a time, 24-bit mono input and the original 48 kHz sample rate. Level 0 uses fixed predictors, block size 128 or 1024/4096, with Rice partition order 0 for 128 and 0–3 for larger blocks. Levels 5 and 8 use their presets with the block size explicitly overridden. Metadata padding and seek tables are absent. Level 5 at 1024 is a useful starting point; 4096 helps the large native file slightly but hurts several other recordings. Level 8 only moves the large native file from about 1.48x to 1.50x while nearly doubling its FLAC encoding time.

The sampled large native capture contains 95.615 seconds of timeline. Encoding all its included streams took 216.9 ms with the legacy stream codec, 2710.6 ms with the adaptive prototype, and 1409.6 ms with FLAC level 5 / 1024. That FLAC result is 14.74 ms of host encoding per second of recorded multitrack audio, roughly half the adaptive prototype cost. These are host measurements excluding disk I/O and signal decoding; they are not iPad measurements or a complete recorder benchmark.

FLAC consumes input in one forward pass and buffers bounded blocks; it can examine that block multiple times internally for prediction/coding. At 48 kHz, 1024 samples are 21.3 ms and 4096 samples are 85.3 ms. This meets a bounded-buffer interpretation of the requirement, but does not preserve the prototype’s much smaller 64-delta window.

## Integrity and limitations

- Originals were opened only for reading. No recordings were rewritten or deleted.
- Selected block CRCs, audio lengths, signed ranges, and padding were checked through the existing SGREC audio decoder. Entire-file block-index continuity and END1 count/CRC were checked for all six local files.
- Actual FLAC encoding completed for every selected stream. FLAC internal decode-and-compare verification was enabled in additional runs for every 97th stream in each profile; byte sizes matched the corresponding unverified run.
- Alternative cost models passed ASan/UBSan, 12 hand-calculated examples, and 1200 independent reference comparisons.
- The three WAV-converted recordings have `git_commit_sha: null`, which the current production header validator rejects. The research harness kept this provenance unchanged and invoked block/audio validation directly. Fixing that separate reader compatibility issue was outside this experiment.
- At the time of the initial experiment, framing, extraction, and build integration were outstanding. The follow-up below records their completion. iPad worker-load measurements remain outstanding.

## Reproduction artifacts

The research scripts, manifests, decoded test corpus and raw measurements are in `/tmp/sgrec-real-research/`: `extract_corpus.py`, `index.json`, `manifest.tsv`, `selection.json`, `measure.cpp`, `measure4096.cpp`, `results.csv`, `results4096.csv`, `summary-final.json`, and `summarize.py`. The cost-model helper and independently checked reference are `/tmp/sgrec-alternative-costs.hpp` and `/tmp/sgrec-alternative-costs-test.cpp`. No decoded user audio was added to the repository.

FLAC background: [RFC 9639: predictors and residual coding](https://www.rfc-editor.org/rfc/rfc9639.html#section-4.4) and [Xiph format overview](https://www.xiph.org/flac/documentation_format_overview.html).

## Implementation follow-up

The approved v4 implementation retains the one-second outer blocks and uses
FLAC level 5 / 1024 on each stream's actual PCM24 samples. Every FLAC stream
carries a four-byte length prefix. Constants remain three bytes; short buffered
streams may use raw PCM24 when smaller. Versions 1–3 remain readable. The
unshipped adaptive prototype has been removed from the implementation.

The production encoder was linked against JUCE's pinned bundled libFLAC 1.4.3
and run on the same 5795-stream corpus. Every output was then decoded by the
production Python reader's stream decoders and compared with all 575571452
original decoded PCM bytes; every integer matched. Ratios below include the
new length fields and the same original container overhead used above.

| Recording | Existing SGREC / implemented codec |
|---|---:|
| `recording-00013.sgrec` | 2.008x |
| `recording-2026-02-10T181510.869.sgrec` | 3.126x |
| `recording-2026-09-15T225029-614762-0.sgrec` | 1.856x |
| `recording-2026-09-16T014721-949538-0.sgrec` | 1.470x |
| `recording-2026-09-16T043656-635302-0.sgrec` | 1.421x |

This confirms about **1.47x** for the sampled large native capture, with about
1.37 seconds of host encoding for its roughly 95.6 recorded seconds. The
results remain samples of the large recordings, not complete file rewrites.
Original recordings were never changed.

Verification completed:

- macOS universal Release and unsigned iOS Release builds succeeded. The iOS
  build needed ordinary access to Xcode's platform services outside the sandbox.
- Native recording/engine/FLAC tests: 35 cases, 2102 assertions passed.
- Native format and FLAC ASan/UBSan tests: 18 cases, 301 assertions passed.
- Python recording/extraction tests with all C++ fixtures enabled: 53 passed,
  none skipped; exact mastered WAVs, events, patch reloads and PCM endpoints included.
- Full native suite: 445 of 447 cases passed. The two failures are the existing
  startup-silence assertions at `sys_startup_stability.cpp:95` and `:272`,
  matching the previously documented baseline (peak 0.000657712). No recording
  test failed, and these unrelated assertions were not weakened or changed.
- Both updated OpenSpec specifications validate strictly. Independent review
  passed spec compliance and code quality with no correctness findings.
- Five-second real-time host worker runs using the full registered test track
  layout completed without capture errors or capture-thread allocations.
  Maximum block encode/write time was 86.6 ms for sine/moving-coordinate audio
  and 125.6 ms for independent noise; queue high-water marks were 5 and 6 pages
  of 32. These are host results, not iPad performance guarantees.

The iPad app reuses bundled FLAC and requires no external FLAC installation.
Python selected-FLAC extraction requires libFLAC (on macOS, `brew install flac`);
patch queries and legacy extraction do not. Device CPU/thermal/storage behavior
still needs an on-device run. These verification results were collected before
landing or deployment.

Follow-up artifacts: `/tmp/sgrec-flac-real-check.cpp`,
`/tmp/sgrec-flac-real-results.csv`, `/tmp/sgrec-flac-verify-real.py`,
`/tmp/sgrec-flac-real-verified.json`, and the `/tmp/sgrec-flac-*.log` build/test logs.
