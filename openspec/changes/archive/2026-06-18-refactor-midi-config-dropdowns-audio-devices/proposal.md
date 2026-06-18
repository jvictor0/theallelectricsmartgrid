## Why

The JUCE config page currently wires several MIDI dropdowns directly inside `ConfigPage`, duplicating selection, population, and apply logic in a way that is hard to extend safely. Adding audio input/output selection should not make that page more fragile or leak config-page UI concerns into unrelated files.

## What Changes

- Refactor the config-page dropdowns into a local, typed row/dropdown component in `JUCE/SmartGridOne/Source/ConfigPage.hpp`.
- Preserve the current MIDI input, MIDI output, controller-shape, KMix, WrldBldr, Launchpad, Encoder, Generic, and stereo behavior.
- Add config-page rows for selecting the JUCE audio input device and audio output device.
- Apply audio device selections through the owning component so JUCE audio is reset only when those selections changed before leaving the config page.
- Persist selected audio input and output device names in the existing JSON config save/load path.
- Keep the implementation scoped to the existing config-page/MainComponent configuration surface; no DSP, controller, MIDI transport, or unrelated UI refactors.

## Capabilities

### New Capabilities
- `juce-device-configuration`: Covers the JUCE config page's typed device rows, preserved MIDI/controller selection behavior, audio input/output selection, audio restart behavior, and config JSON persistence.

### Modified Capabilities

## Impact

- Affected code:
  - `JUCE/SmartGridOne/Source/ConfigPage.hpp`
  - `JUCE/SmartGridOne/Source/MainComponent.h`
  - `JUCE/SmartGridOne/Source/MainComponent.cpp`
  - `JUCE/SmartGridOne/Source/Configuration.hpp`
- Existing config JSON gains optional audio input/output device name fields. Missing fields remain backwards compatible.
- Existing MIDI controller JSON and runtime MIDI behavior should be unchanged.
- No new dependencies or project files are expected.
