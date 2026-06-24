## 1. Spectral Residual Model

- [x] 1.1 Add `ResidualModel` inside `SpectralModelGeneric` with `DFT::x_maxComponents` envelope buckets, stored magnitudes, DFT bucket frequencies, precomputed log-frequency helpers, and an input struct containing analysis residual magnitudes.
- [x] 1.2 Add residual model processing that maps each bucket's precomputed log-frequency helper to a parameter index and slews stored magnitudes with frequency-dependent attack and decay.
- [x] 1.3 Add `ResidualModel::GetEnvelope(bucketIndex)` to return the smoothed residual envelope for the matching DFT component.
- [x] 1.4 Add tests covering residual bucket attack, decay, equal-lane scalar smoothing behavior, and DFT-index envelope queries.

## 2. Residual Analysis Extraction

- [x] 2.1 Add analysis phase storage to `AnalysisAtom` and fill it from the source DFT during peak extraction.
- [x] 2.2 Extend spectral analysis so organic extracted atoms can be canceled from the analysis DFT at opposite phase after peak extraction.
- [x] 2.3 Copy post-cancellation DFT component magnitudes directly into matching residual model input buckets.
- [x] 2.4 Wire Partial Machine hop analysis to process both atom tracking and residual model input while preserving existing non-residual spectral model callers.
- [x] 2.5 Add tests showing a tracked sinusoid leaves reduced residual energy near its frequency and broadband energy remains component-by-component after cancellation.

## 3. Partial Machine Residual Synthesis

- [x] 3.1 Add `ResidualMachine` inside `PartialMachine` to read the residual envelope at each target quad DFT bucket index.
- [x] 3.2 Reuse `SynthesisContext` reduction, radius, azimuth, and pan helpers to compute `residualEnvelope * reduction * pan[channel]`.
- [x] 3.3 Add residual synthesis that creates random-phase complex values and adds them directly into existing `QuadDFT` components without calling `WriteWindowedPartial`.
- [x] 3.4 Apply reduction feedback to each residual bucket by writing the feedback-shaped reduced magnitude back into the residual model with the same floor policy as atom feedback.
- [x] 3.5 Integrate residual synthesis into `ProcessSynthesisFrame` so residuals and tracked atoms share one `QuadDFT` and one `QuadOLA::Write`.
- [x] 3.6 Add tests covering finite/bounded residual output, reduction behavior, reduction-feedback writeback, direct component addition, and same-frame synthesis with tracked atoms.

## 4. Verification

- [x] 4.1 Run focused SpectralModel and Partial Machine unit tests.
- [x] 4.2 Run the broader DSP/unit test target used by this repository.
- [x] 4.3 Run `openspec status --change "add-spectral-residual-model"` and confirm the change is apply-ready.
