## Context

Eight ScopeWriter banks embed 128 MiB of samples each. Voice banks currently interleave nine scalar voices per logical scope. DSP already produces trio and quad vectors. The UI reads scalar channels, with cycle transfer logic and secondary modulation samples expressed in original sample coordinates.

## Goals / Non-Goals

**Goals:** Whole-vector DSP writes; reusable vector FIR decimation; a 0.25-second engine-rate base per ring; six levels with rate ratios 1, 4, 16, 64, 256, 1024; fixed LOD for each display reader; unchanged per-voice cycle semantics.

**Non-Goals:** Matching event retention to waveform retention, changing host/engine rates, peak-envelope representations, a new reader ownership protocol, or changing audio output processing.

## Decisions

- Consolidate into three engine-rate banks: 39 physical trio scopes (13 logical types across three tracks), 13 quad scopes, and four mono scopes. Address logical scope s and global voice v as scope `s * 3 + v / 3`, lane `v % 3`. SIMD padding is retained.
- Separate the scalar read/metadata interface from vector-width-specific write interfaces and fully templated storage. No DSP path decomposes a vector into scalar scope writes. Mono producers use width-one vectors. Source input scope values are collected into a four-source vector.
- Each physical scope owns six equal-capacity vector rings and five streaming FIR decimators. Allocate before processing. The audio thread writes only level zero; AdvanceIndex fills unwritten scopes with zero without repeating written vectors. The UI captures all three published boundaries at the beginning of MainComponent::paint and accepts each bank through its captured boundary, advancing its FIR cascade over exactly the unaccepted input interval. Level-zero writes remain immediately available to direct diagnostic reads.
- FIR decimation uses 17 taps with normalized symmetric windowed-sinc coefficients, computes only every Ratio samples, exposes its group delay and supports reset. Scope reads account for cumulative decimation phase and FIR delay while retaining original coordinates. Low-pass history intentionally loses fast content.
- The six equal-capacity rings retain spans of 0.25, 1, 4, 16, 64, and 256 seconds. At 192 kHz this reserves 244,224,000 sample bytes (232.9102 MiB), including trio padding.
- Scope FFTs transform entire trio and quad vectors once per displayed scope. Magnitude computation and smoothing also use channel vectors; scalar lane access is only for drawing. Mono FFTs pack four interleaved sample subsequences into four lanes, run quarter-length SIMD transforms, then combine the subsequences with full-length twiddles. All variants retain the original normalization and smoothing rates, reuse preallocated workspaces, and cache Hann coefficients.
- Scope FFT length is 1,024 times the engine oversample factor (4,096 at 192 kHz), keeping 46.875 Hz bin spacing. The log axis runs from the existing 23.4375 Hz low edge to engine Nyquist. Spectrum points retain the largest covered bin so narrow high-frequency peaks remain visible. Voice, damping, mastering, and source responses already use engine units; partial-machine and vocoder overlays convert from host units. Master scopes capture the post-mastering signal before output decimation.
- Publish the audio boundary after writes and marker counters. UI-owned accepted positions and per-level bounds advance only after the cascade is complete. Snapshot start/stop metadata at acceptance, omitting starts beyond the last readable sample (`acceptedIndex - 1`), including fractional starts, and bounding ends to the accepted boundary. History reads reject samples outside accepted bounds; display reads can hold the last valid coarse value at the delayed live edge. The current and previous cycle share one LOD selected from their oldest required sample.
- Secondary modulation prepares a reader from the complete time interval and a fixed accepted position, then reuses its LOD for all sample-backed sources. Random/note histories retain their existing event limits and prediction semantics.
- Preserve 256 start/stop markers per scope lane, dimensioned to the actual bank. Publication remains atomic metadata over ordinary vector data, explicitly as requested. This does not make concurrent overwrite of a stalled reader data-race-free; no stronger ownership guarantee is claimed.

## Risks / Trade-offs

- FIR latency at coarse levels → keep original-coordinate timing and explicit strict versus display-tail semantics.
- Retained sample span exceeds marker/event span → retain the existing independent event limits; queries report unavailable history normally.
- A UI stalled beyond the level-zero retention window may read overwritten slots during catch-up and propagate those values down the tree. Keep blind ring indexing, without pinning, producer waits, resetting filters, or skipping elapsed samples, as requested. Accepted positions are stable frame coordinates, not ownership of level-zero storage.
- Engine-rate writes and larger FFTs increase CPU work; FIR processing now runs only on the UI thread. The 17-tap filters reduce scope FIR multiplies from 33 to nine per output and deliberately accept weaker alias rejection for UI histories. Compute only on decimated output steps and use vector arithmetic. Target-device performance is not yet measured.

## Validation

- `make core-test`: all 787 core cases and 26 ratio-one cases passed after rebasing the completed change, including the fractional-start fix, onto main's DSP precision and sub-envelope changes.
- The fractional-start regression failed before the fix. After the fix, all 37 focused scope, reader, producer, and modulator-history cases pass, including retaining the previous cycle, accepting a start exactly at the last readable sample, and admitting the fractional start on the next frame. The 63 visualizer cases also pass after the fix.
- After the rebase incorporating the concurrently landed master-tuning visualizer, all 80 focused scope, modulator, trio, tuning, MIDI, recording, and encoder catalogue cases and all 26 ratio-one cases passed.
- `make visualizers-test`: all 68 cases pass with the master-tuning integration, including grouped SIMD spectra and explicit UI acceptance.
- A subsequent clean rebase preserved the independently landed VPS continuity fix without changing the scope patch or resolving any conflicts; the validation above predates that final rebase.
- `make build XCODE_FLAGS=CODE_SIGNING_ALLOWED=NO`: the macOS Release app built successfully. Existing unused-variable warnings in PatchArena remain.
- Additional coverage verifies that audio writes/publication do not run FIRs, accepted bounds and markers stay fixed during a frame, irregular acceptance batches match incremental filtering across every level, and mono/trio/quad spectra agree with independent scalar FFTs.
- Coverage includes FIR lane isolation, DC/ramp timing, 65-tap rejection and the lighter 17-tap response, reset and passthrough; all six LODs and wrapping; publication bounds and display-tail holding; cycle transfer, startup FFT and ultrasonic bins; trio addressing, engine-sample modulation history, and long prepared-reader intervals.
- UI coverage checks the fixed low edge, engine Nyquist, host-rate overlay conversions, narrow spectrum peaks, and agreement between FFT and transfer-function curves on both log and linear axes.
- Production allocation is asserted from 212 physical float lanes, six levels, and 48,000 vector samples per level. Marker and FIR state are additional.
- `openspec validate scope-lod-history --strict` and `git diff --check` (preserving the existing CRLF files) passed.
