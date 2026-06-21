## Why

The Partial Machine currently picks the preferred replacement analysis atom inside an existing atom's density window by magnitude only. That lets a slightly louder but farther peak steal a tracked atom from a nearby continuation, which can make partial tracking less stable than the density window implies.

## What Changes

- Replace magnitude-only preferred-analysis selection with a weighted score function theta(magnitude, distance).
- Use distance from the tracked synthesis atom's current analysis frequency to each candidate analysis atom, measured in the same normalized frequency units as `m_omegaDensity`.
- Make theta favor nearby candidates within the density window while preserving magnitude as the primary signal-strength input.
- Keep organic atoms preferred over synthetic atoms when synthetic harmonics are enabled.
- Add focused SpectralModel/Partial Machine coverage for near-versus-far candidate selection within the density window.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `partial-machine`: Spectral atom tracking chooses replacement analysis atoms using both magnitude and distance inside the density window.

## Impact

- Affected code: `private/src/SpectralModel.hpp`, specifically `AnalysisAtom::IsPreferred` and `SearchAndMerge`.
- Affected tests: focused unit coverage under `private/test/`, likely alongside `dsp_partialmachine.cpp` or a new SpectralModel-focused test.
- No UI, preset schema, parameter, or dependency changes expected.
