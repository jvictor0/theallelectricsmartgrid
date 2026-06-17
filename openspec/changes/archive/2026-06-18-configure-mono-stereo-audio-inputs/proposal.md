## Why

External audio routing currently assumes four mono source mixer inputs and fills trio voices from selected sources. That makes stereo inputs awkward, hides per-input width from the configuration surface, and prevents users from intentionally leaving unused trio voices silent.

## What Changes

- Request up to eight JUCE audio input channels for four configurable source mixer inputs, with each configured source owning left/right lanes for stereo-capable routing.
- Add per-input configuration for mono or stereo width, persisted with patch/config state.
- Include per-source configuration objects when populating the source mixer input state so DSP code owns each input's mono/stereo interpretation and color assignment.
- Populate source input audio from the left channel of each configured pair while the right side is silent when the configured source is mono.
- Add a Squiggle Boy config section with four inputs and a mono/stereo toggle for each input.
- Color source-selection, monitor, deep-vocoder, and stereo-toggle controls with the source color instead of generic white controls.
- Update source mixer metering so mono sources keep the existing meter/reduction offset layout, while stereo sources split the same source footprint into half-width left reduction, left meter, right meter, and right reduction bars.
- Change trio source assignment so selected mono sources consume one voice, selected stereo sources consume two voices as left/right, and any remaining trio voices are silent.
- Remove the current infill behavior that repeats selected sources to fill all three voices.
- Keep enforcing the maximum of three selected channels per trio by unselecting sources when selecting or changing a source to stereo would exceed the limit.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `source-machine-oscillators`: external Thru sources now support four configured mono/stereo inputs backed by up to eight physical channels, source color ownership in DSP/UIState, stereo pair routing, and silent unassigned trio voices.
- `voice-architecture`: voice configuration now needs to represent silent source assignment and stereo pair channel assignment within each trio.
- `ui-shell-layout`: the Squiggle Boy config grid now exposes four input selectors and four width toggles, with source-colored controls.
- `grid-visualizers`: source mixer meters and reduction visualizers now render left/right source lanes without increasing the source meter footprint, while quadraphonic meters keep their current size.

## Impact

- DSP/source routing: `private/src/SourceMixer.hpp`, `private/src/SquiggleBoySource.hpp`, `private/src/SquiggleBoy.hpp`, and `private/src/SquiggleBoyVoiceConfig.hpp`.
- Config grid and persistence: `private/src/SquiggleBoyConfig.hpp` and patch/config JSON handling.
- JUCE audio IO: `JUCE/SmartGridOne/Source/MainComponent.cpp`, `JUCE/SmartGridOne/Source/MainComponent.h`, and `JUCE/SmartGridOne/Source/NonagonWrapper.hpp`.
- Visualizers and meters that iterate source mixer inputs or source colors: `JUCE/SmartGridOne/Source/MeterComponent.hpp`, `JUCE/SmartGridOne/Source/MasteringComponents.hpp`, and related source mixer visualizer code.
- Tests should cover source assignment, mono/stereo width changes, silent voices, input-channel population, persistence, stereo source metering, and the four-source UI state.
