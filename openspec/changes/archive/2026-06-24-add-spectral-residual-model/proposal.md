## Why

The Partial Machine currently resynthesizes tracked spectral atoms but drops the broadband energy that remains after those atoms are removed from the analysis frame. Adding a residual path lets the effect preserve noisy, breathy, and transient spectral material while still shaping it with the same frequency-dependent controls as the partials.

## What Changes

- Add a residual model inside `SpectralModelGeneric` with one smoothed residual envelope bucket per DFT component.
- Keep a precomputed log-frequency helper array for those buckets so frequency-dependent parameter indexes can be computed without doing a logarithmic conversion every hop.
- Extend spectral analysis so extracted atoms are synthesized back into the analysis DFT at opposite phase, canceling the modeled partials, then residual magnitudes are measured from the remaining DFT energy.
- Extend spectral model processing so each residual bucket maps its precomputed log frequency to frequency-dependent parameter indexes and slews incoming residual magnitudes with attack and decay parameters.
- Extend Partial Machine synthesis with a residual machine that reads the residual envelope at each target quad DFT bucket, applies reduction and pan, and adds random-phase complex energy directly into the same `QuadDFT` frame as partial synthesis.
- Apply reduction feedback to residual buckets by writing the feedback-shaped reduced magnitude back into the residual model, matching the tracked atom feedback behavior.
- Add focused tests for residual extraction, bucket smoothing, and residual synthesis behavior.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `partial-machine`: Partial Machine analysis and synthesis includes residual spectral energy after tracked atoms are canceled from the input DFT.

## Impact

- Affected code: `private/src/SpectralModel.hpp`, `private/src/PartialMachine.hpp`, and supporting DFT or random-phase helpers if needed.
- Affected tests: `private/test/unit/dsp_spectralmodel.cpp`, `private/test/unit/dsp_partialmachine.cpp`, or adjacent focused unit tests.
- No new third-party dependencies are expected.
- No preset schema or UI parameter additions are required unless implementation finds residual-specific level controls necessary.
