## 1. Processor

- [x] 1.1 Implement generic seven-landmark wavefolder, cached gain compensation, and engine-rate 5 Hz DC blocking.
- [x] 1.2 Verify morph endpoints/continuity, spectra, DC settling, finite output, and independent vector lanes.

## 2. Sub integration

- [x] 2.1 Replace inline sub saturation, reduce Drive range, and append the Shape encoder without changing existing IDs.
- [x] 2.2 Verify Sub controls, envelope/volume behavior, scope tails, and patch persistence; update catalogue expectations.
- [x] 2.3 Update sub documentation with ordering, gain mapping, and DC behavior.

## 3. Validation

- [x] 3.1 Run focused tests, the full core suite, and the app build in canonical build directories; review the final diff.

Validation: 834 core cases and 36 passthrough-rate cases passed via `make core-test`; the focused wavefolder/Sub/catalogue checks passed; `make build` produced a signed universal macOS Release app (arm64 and x86_64). OpenSpec strict validation and `git diff --check` passed.

## 4. Sub waveform view

- [x] 4.1 Add the standard audio scope in the first Sub visualizer slot, span the analyzer across the other two, and document the layout.
- [x] 4.2 Run the existing visualizer checks, build and refresh the iPad app, and review the diff.

Sub view validation: all 71 visualizer cases passed via `make visualizers-test`; `make ios-deploy` built the signed iOS Release app, installed it over Wi-Fi, and launched it successfully on the paired iPad. OpenSpec strict validation and `git diff --check` passed.

## 5. Sub scope synchronization

- [x] 5.1 Expose fractional sine VCO tops and record each sub scope start on its own lane's oscillator wrap.
- [x] 5.2 Verify per-lane starts and fractional timing through scope decimation, run relevant DSP checks, and refresh the iPad build.

Synchronization validation: 29 focused VCO, Sub, ScopeReader, and AHD scope cases passed (595 assertions). The regression exercises independent trio wraps, event clearing, and fractional scope starts at decimation 1 and 4. The signed iOS Release app built, installed over Wi-Fi, and launched successfully. OpenSpec strict validation and `git diff --check` passed.

## 6. Similarity-ordered odd shapes

- [x] 6.1 Replace the seven landmarks with the requested tanh-to-sine cubic sequence, remove the zero-input override, and update peak compensation and documentation.
- [x] 6.2 Verify the new curves, normalization, low-drive fundamental, DC behavior, and lane independence, then refresh the iPad build.

Revised shape validation: `make core-test` passed 835 core cases (4 existing skips) and all 36 passthrough-rate cases. Independent dense sweeps checked the normalized landmarks and morphs; spectral tests verified that the fundamental retains over 99 percent of measured odd-harmonic energy at gain 0.125 for every landmark. The signed iOS Release build installed and launched successfully on the paired iPad. OpenSpec strict validation and `git diff --check` passed.
