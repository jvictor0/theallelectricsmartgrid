## Why

Increasing the sub's tanh drive to 32× mostly flattens its waveform. A continuously morphing wavefolder should provide varied spectra while retaining the fundamental at low drive, grouping similar curves, and encapsulating DC removal for reuse.

## What Changes

- Add a lane-generic wavefolder with seven ordered sine/tanh landmarks, adjacent linear crossfades, gain-dependent level compensation, and one 5 Hz DC blocker per lane.
- Replace the sub's inline saturation with the processor after its rise/fall envelope.
- Reduce Sub Drive to 0.125×–8× and add a per-voice Sub Shape encoder, defaulting to tanh.
- Preserve existing parameter identifiers and normalized saved Drive positions; their physical drive intentionally changes with the reduced range.
- Show the Sub audio scope in the first visualizer slot and span the remaining two slots with the spectrum analyzer.

## Capabilities

### New Capabilities

- `wavefolder`: Reusable multichannel shaping, morphing, level compensation, and DC removal.

### Modified Capabilities

- `source-machine-oscillators`: Route the sub through the wavefolder and expose its shape and reduced drive range.
- `grid-visualizers`: Show the Sub waveform beside its spectrum.

## Impact

Core DSP headers, Sub encoder catalogue/page and visualizer registration, unit and synth integration tests, and sub oscillator documentation. No new dependencies or main-voice/master DC blockers.
