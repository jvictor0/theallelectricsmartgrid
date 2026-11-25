# Filter architecture and UI snapshots

Generated TPT filters provide audio processing, coefficient preparation and
magnitude-squared response kernels from Lean definitions. The proofs describe
real/complex mathematics; the C++ backend and floating-point approximations are
validated by tests, not a verified compiler.

The app adapters in `private/src/TPTSVFFilter.hpp` and `TPTBaseWidthFilter.hpp`
translate existing controls into generated `LogicalSpec` records. An SVF uses
natural cutoff in cycles per engine sample and Q; its app resonance mapping is
`Q = 0.5 * 11^resonance`. Coefficient updates preserve integrator memories and
cache the matching response snapshot. `Math4096` supplies the generated trig.

A selected generated SVF view (LP, BP or HP) has a composable `Snapshot` and a
`UIState = TransferFunctionGeneric<Snapshot>`. The generated morphing filter
captures its own LP/BP/HP weights. Four-pole HP snapshots contain both sections.
App machines own these child snapshots alongside handwritten child snapshots.
Cascades multiply power
responses; the morph kernel includes phase-dependent cross terms before its
magnitude is squared. Linear-filter plots exclude the surrounding saturation,
envelopes and sample-rate reduction.

The [Naive Reupping Ladder](filter-naive-reupping-ladder.md) uses the same snapshot
path with a measured white-noise power response. The UI decodes the bundled
curve for each sampled configuration before it evaluates the plot.

`SampledSnapshotWriter` decides whether to capture at host-sample boundaries,
after the audio thread has updated coefficients. A capture publishes the whole
composite snapshot so correlated modulation stays together. The UI reads a
sampled history rather than live filter state, evaluates power across prepared
frequency vectors, and plots percentile curves using `10 * log10(power)`.

The source mixer packs one coefficient set per source into four response lanes;
each source's two audio lanes share those coefficients. Voice snapshots retain
independent per-voice coefficients. Standalone envelope followers allocate no UI
history.

Handwritten response providers that need phase can still expose complex
responses. Generated filters and their app composites need only power responses.
See [the codegen guide](../formal/DSP/Codegen/README.md) for the proof and codegen
boundaries, and [the SVF machine](filter-svf.md) for its saturation/reducer order.
