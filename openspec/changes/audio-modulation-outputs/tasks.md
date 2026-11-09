## 1. Parameter bounds

- [x] 1.1 Promote final root clamping, remove manager source clamping, and run the boundary tests in normal builds.

## 2. Audio modulation outputs

- [x] 2.1 Add the paired source output interface and populate Dual/Physical outputs with cleared Sample/Thru placeholders.
- [x] 2.2 Add source identities, downsampling declarations, and all voice-group slot bindings; publish source, sub, and voice taps.
- [x] 2.3 Cover tap values, switching, lane compatibility, causality, and full-synth audio modulation with regression tests.

## 3. Validation and documentation

- [x] 3.1 Run relevant core and integration checks with assertions enabled and update affected documentation/source correspondence.
- [x] 3.2 Sync current capability specs and review the worktree diff without changing main.
- [x] 3.3 Remove experiment switches, harnesses, and variant-only test code; retain production regression coverage and verify the clean build.

## 4. Modulator presentation

- [x] 4.1 Restore slot-based random colors and reuse RND1/RND2 labels in all voice and Quad groups.
- [x] 4.2 Record the four audio modulation taps with cycle/envelope markers and expose their histories to time-domain visualizers.
- [x] 4.3 Configure source visualizers by source machine in a separate ForEach declaration; add Sub/Voice time-domain views.
- [x] 4.4 Test colors, names, exact tap histories, machine switching, and rendering; sync specifications and validate.

## 5. Shared scope taps

- [x] 5.1 Reuse VCO/Sub/PostAmp scopes, align Sub/Voice taps, and retain only the physical source envelope history.
- [x] 5.2 Update history lookup, visualizer configuration, and regression coverage for shared bipolar scopes.
- [x] 5.3 Sync documentation and validate the shared-scope implementation.

## 6. Mixer followers

- [x] 6.1 Add an engine-rate squared-amplitude follower and seven independent mixer taps with audio/envelope scope writers.
- [x] 6.2 Declare 14 Quad sources and the effect/final-output group bindings.
- [x] 6.3 Add trailing-window audio scopes and follower-driven VU views with shared history lookup.
- [x] 6.4 Test follower timing, exact taps, group bindings, rendering, and full-synth behavior; sync specs and validate.

## Validation results

- Assertion-enabled RelWithDebInfo core suite: 864 passed, four opt-in performance cases skipped.
- Passthrough-rate suite: 39 passed, including the promoted parameter-clamp contracts.
- JUCE visualizer suite: 78 passed.
- OpenSpec strict validation, encoder source-correspondence audit, and `git diff --check` passed.
- Lean proof build was attempted but the worktree-local toolchain is not installed. Lean changes are correspondence comments only; proof definitions are unchanged. No toolchain installation was undertaken.
- Logs are retained under `build/results/audio-modulation-*`. Implementation and validation stayed in the existing worktree; main remained unchanged.

- Cleanup validation: rebuilt core with experiment options removed, passed 43 focused encoder/modulator/full-synth cases and all 39 passthrough-rate cases, and regenerated the standard performance build configuration. Source searches found no remaining modulation-experiment switches or harness references in the code/spec trees. Historical benchmark report and planning notes are retained under ignored `build/results/modulation-clamps-retired/`.

- Modulator presentation validation: 866 core cases passed (four manual performance cases skipped), 39 passthrough-rate cases passed, and all 79 JUCE visualizer cases passed. Coverage includes group colors/names, exact tap histories, fractional cycle markers, per-trio source-machine switching, and rendered waveform lanes. The four tap histories reuse unused former random scope slots; the existing scope-memory assertion remains unchanged and passes. OpenSpec strict validation, encoder source correspondence, and `git diff --check` passed. Logs are in `build/results/modulator-presentation-*`.

- Shared-scope revision validation: 20 focused tap/history cases, all 866 core cases (four manual performance cases skipped), all 39 passthrough-rate cases, and all 79 JUCE visualizer cases passed. VCO/Sub/PostAmp histories now serve modulation views with normalization on read. Sub capture is pre-gain; Voice modulation is pre-track-gain. Only the physical source envelope retains an additional history, and the reduced scope-storage assertion passes. OpenSpec strict validation, encoder source correspondence, and `git diff --check` passed. Logs are in `build/results/shared-modulator-scopes-*`.

- Mixer follower validation: all 870 core cases passed (four opt-in performance cases skipped), all 39 passthrough-rate cases passed, and all 80 JUCE visualizer cases passed. Coverage includes scalar/trio/quad engine-rate mean-square timing, separate audio/envelope captures, full send buses, pre-gain returns, nonzero recorded-master equivalence, fourteen causal source publications, Quad group bindings/lane compatibility, trailing-window rendering, and accepted-sample follower VUs. OpenSpec strict validation, encoder source correspondence, and `git diff --check` passed. Logs are retained under `build/results/mixer-followers-*`.

## 7. Host-rate RMS and main comparison

- [x] 7.1 Keep engine-rate power smoothing, publish host-rate RMS envelopes without extra downsampling, and update scope/VU contracts and tests.
- [ ] 7.2 Compare the complete SynthRig against current main using isolated Release builds, matching fixtures, serial repeated timings, and preserved results.

- Host-rate RMS validation: all 870 core cases (four manual performance skips), 39 passthrough-rate cases, 80 visualizer cases, and all three benchmark smoke/protocol tests passed. Follower tests verify per-engine-sample power smoothing, square root only on host frames, held RMS scope values, and direct envelope publication. The seven mixer audio sources retain downsampling; their seven RMS counterparts bypass it. Main benchmark smoke/protocol tests also passed. Logs are under `build/results/host-rms-*` and `build/results/modulator-main-perf-*`.
