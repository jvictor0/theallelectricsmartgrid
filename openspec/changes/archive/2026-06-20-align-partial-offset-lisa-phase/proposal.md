## Why

The Partial Machine already supports a shared azimuth offset, but that offset is not currently driven by the same phase source that animates the voice Lissajous panning. Aligning them makes the Partial Machine's spectral orbit move with the global pan phase instead of staying static.

## What Changes

- Drive the Partial Machine azimuth offset from the shared pan phase used by the voice Lissajous LFO.
- Keep existing Partial Machine lane controls, panning math, and UI publication behavior unchanged.
- Add focused coverage that the Partial Machine input state receives the shared pan phase.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `partial-machine`: Partial Machine azimuth offset follows the shared Lissajous pan phase.

## Impact

- Affected code: `private/src/SquiggleBoy.hpp`, where voice panning and Partial Machine input state are wired.
- Affected tests: focused DSP or runtime wiring coverage under `private/test/`.
- No API, preset schema, or dependency changes expected.
