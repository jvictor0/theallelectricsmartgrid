## Why

Clamping modulation sources discards excursions that could remain within a parameter's available headroom. Clamp final parameter outputs instead, and expose existing source, sub, and voice audio as modulation without changing DSP ownership.

## What Changes

- Remove production source clamps and clamp each top-level parameter before host-to-engine interpolation; retain the existing second-order Butterworth filters.
- Add a two-output source interface, publishing the individual VPS outputs or physical-modeling filtered pre-envelope noise and internal envelope. Sample and Thru publish zero placeholders.
- Bind SourceMod1/SourceMod2 to slots 0/1 and Sub/Voice to slots 12/13 of every voice group through existing ForEach metadata.
- Restore the original slot colors and RND1/RND2 names on every page; reuse VCO/Sub/PostAmp histories with aligned modulation taps and source-machine-configured visualizers; add only the physical source envelope history.
- Add seven engine-rate mean-square mixer followers, fourteen Quad modulators, trailing audio scopes, and follower-driven VU views.
- Preserve patch format, group assignments, source processing order, and child encoder behavior.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `modulator-manager`: Unclamped source filtering and new per-voice and mixer audio/envelope source bindings.
- `encoder-parameter-system`: Final top-level parameter bounds before interpolation.
- `source-machine-oscillators`: Paired modulation outputs supplied by source processors.
- `grid-visualizers`: Consistent group presentation and source-machine-aware audio modulation views plus mixer trailing scopes and follower VUs.

## Impact

Source dispatch/process interfaces, modulator metadata/publication, encoder matrix publication, relevant tests and documentation. Work stays in this worktree. FIR selection, deployment, landing, and patch-format changes are out of scope.
