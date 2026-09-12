## Why

Theory of Time wraps phase and integer positions early, then reconstructs lost cycles through winding trackers and recursive monodromy. This also restarts fractional-speed sample playback every source loop. Absolute coordinates make playback continuous across loop boundaries and simplify time consumers.

## What Changes

- Store absolute unmodulated and modulated global phases as doubles; derive loop phases and signed 64-bit lattice positions directly from phase and accepted topology.
- **BREAKING**: Replace synonymous direct/indirect, independent/dependent, master/global, and unwound accessors with one consistently named time API; migrate all callers in the same change.
- Replace recursive monodromy with absolute gate-step division and optional ancestor-reset modulo, including negative positions.
- Accept a requested parent and multiplier together only at simultaneous cycle boundaries of the old and requested parents.
- Apply sample-playback speed before wrapping at the playback output, including fractional and negative speeds.
- Preserve PolyXFader's existing periodic waveform shaping, user controls, automatic amplitude weights, and smoothing.
- Capture an AHD's phase-to-global ratio at trigger and track only modulated global phase thereafter; active envelopes do not follow later topology edits.
- Remove timebase-related winding reconstruction from gates, recording, synchronization, playback, and display adapters. Preserve transport, persistence keys, and realtime scheduling.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `phasor-timebase`: Absolute time, consistent API, simultaneous parent boundaries, direct gate-step indices, and interpolation under changing topology.
- `source-machine-oscillators`: Absolute-phase sample playback with fractional and reverse speed.
- `polyxfader-lfos`: Absolute phase inputs with existing per-loop periodic waveform semantics.
- `ahd-envelopes`: Trigger-captured ratio and global phase origin, with topology-stable envelope timing.
- `multi-phasor-gate`: Global absolute phase distance and consistent trigger-captured timing.
- `lamejuis-sequencer`: Signed gate-step indices and Euclidean rhythm/motive decomposition.

## Impact

The headers-mostly C++17 DSP clock and its consumers, standalone doctest coverage, related documentation, and the mathematical time description. No new dependency, patch format change, UI control, or clock mode is required. Independent versus modulated topology-boundary timing remains as it is; this change does not promise continuity across that existing mismatch or user parameter edits.
