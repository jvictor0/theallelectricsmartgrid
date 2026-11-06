## 1. FIR and history storage

- [x] 1.1 Implement and test the reusable vector FIR downsampler.
- [x] 1.2 Implement templated vector rings, cascade timing, metadata publication, and original-coordinate LOD reads.

## 2. Producers and readers

- [x] 2.1 Convert all DSP scope producers to whole-vector writes and apply logicalScopeIx * 3 + trioIx mapping.
- [x] 2.2 Update cycle readers, FFT/UI consumers, and prepared secondary-modulator readers for fixed LOD selection.

## 3. Validation

- [x] 3.1 Update existing scope tests and add coverage for LOD retention, timing, wrapping, vector isolation, and trio addressing.
- [x] 3.2 Run core and visualizer checks, verify memory allocation, and validate the OpenSpec change.

## 4. Engine-rate scopes and spectra

- [x] 4.1 Use 17-tap scope downsamplers and consolidate storage into one engine-rate bank per vector width.
- [x] 4.2 Increase scope FFT length fourfold and align all frequency overlays to an engine-rate axis with the existing low-frequency limit.
- [x] 4.3 Resolve base history capacity, update regression coverage and specifications, and run core and visualizer validation.

## 5. UI acceptance and SIMD spectra

- [x] 5.1 Move the FIR cascade to UI acceptance and make all visualizer reads use the accepted boundary and markers.
- [x] 5.2 Process scope FFTs with SIMD and replace independent lane transforms with grouped transforms.
- [x] 5.3 Validate deferred acceptance, frame consistency, streaming equivalence, and SIMD spectra; update specs and run core, visualizer, and app checks.
- [x] 5.4 Keep fractional cycle starts beyond the captured last sample out of the accepted frame, and add regression coverage.
