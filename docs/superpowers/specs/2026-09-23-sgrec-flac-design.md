# SGREC FLAC streams

Approved direction: retain the one-second recording container and replace first-difference packing with FLAC on the actual quantized samples. Real-recording measurements are in `docs/research/2026-09-23-sgrec-real-recording-codecs.md`.

The writer emits format version 4 / BLK4. The earlier adaptive v4 prototype was never shipped and is superseded, including its decoder and tests. Versions 1–3 remain readable. Track descriptors, sparse audio detection, PCM24 quantization, event groups, CRCs, completion markers, and the recorder's worker-thread model stay intact.

Stream descriptor 2/0 means a little-endian u32 byte length followed by one complete native FLAC stream. FLAC contains one channel, 24-bit signed samples, the session sample rate, and exactly the outer block's frame count. The encoder uses level 5 and 1024-sample internal blocks. Every scalar audio or coordinate stream is encoded independently. Constant streams use existing descriptor 1/0 and one PCM24 value. For short streams fitting in the scratch buffer, raw PCM24 (0/24) is used when smaller. Longer incompressible streams rely on FLAC's verbatim subframes.

The source samples are visited once, copied/validated in fixed 1024-sample scratch, and fed to libFLAC. FLAC may examine its bounded internal buffer repeatedly. Encoding and allocation happen on the existing recording worker, never the audio callback. Failure returns false and discards the partial stream; the recorder's existing error handling applies.

The application reuses libFLAC bundled in the pinned JUCE dependency through its C API, isolated in one implementation file; standalone tests link installed libFLAC. No system FLAC library is required on iPad. Python loads libFLAC lazily through ctypes, with an actionable missing-dependency error only for selected FLAC streams. It validates metadata, exact sample count and payload consumption, sample/coordinate range, decoder errors, and MD5 when present. Length-prefixed unselected streams can be skipped without decoding or loading libFLAC.

Verification includes real encoder/decoder round trips, PCM endpoints, constant/silent/coordinate streams, partial and multiple outer blocks, events and seeking, malformed/truncated FLAC, legacy fixtures, app build integration, and representative real-recording size checks. iPad performance measurements require device availability and are distinct from host/build validation.
