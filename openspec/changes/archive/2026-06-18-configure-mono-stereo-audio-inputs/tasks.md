## 1. DSP Data Model

- [x] 1.1 Add a source width enum, source configuration object for Mono/Stereo width, and DSP-owned static source colors.
- [x] 1.2 Keep `SourceMixer::x_numSources` at four and add left/right lane state, setup, population, and iteration code.
- [x] 1.3 Add left/right source lanes for each source and update source input population so Mono fills left and silences right while Stereo fills both lanes.
- [x] 1.4 Replace direct voice source-index routing with a source assignment representation that supports Silent, Source Left, and Source Right.
- [x] 1.5 Update `SquiggleBoySource` Thru processing to read the assigned source lane or render silence for Silent assignments.
- [x] 1.6 Publish left/right source meter data so Mono sources mirror the mono signal to both meter lanes and Stereo sources expose independent meter lanes.

## 2. Audio IO and Persistence

- [x] 2.1 Update JUCE channel setup and `NonagonWrapper` input clamping so the app requests and processes up to eight physical input lanes for four stereo-capable configured sources.
- [x] 2.2 Persist source width configuration and expanded source selection state through state saver and patch/config JSON.
- [x] 2.3 Add compatibility loading so old four-source patches default missing widths to Mono and missing new source selections to unselected.
- [x] 2.4 Update mixer input counts, recording/monitor arrays, and any source-count-dependent config state for four configured sources with eight source lanes.

## 3. Trio Assignment Behavior

- [x] 3.1 Implement a helper that counts selected source channels, expanding Mono to one channel and Stereo to two channels.
- [x] 3.2 Implement source-selection enforcement that keeps each trio at three selected channels or fewer when a source is toggled or width changes.
- [x] 3.3 Rewrite propagation so selected source channels assign once to trio voices in order and remaining voices receive Silent assignments.
- [x] 3.4 Remove the current behavior that prevents zero selected sources and repeats selected sources with modulo infill.

## 4. Config Grid and Source Colors

- [x] 4.1 Add four source-selection controls to the Squiggle Boy config grid and place them in the audio-input section.
- [x] 4.2 Add four mono/stereo toggle controls whose off state means Mono and on state means Stereo.
- [x] 4.3 Color source-selection, monitor, deep-vocoder, and stereo-toggle controls from the source mixer color state instead of generic white.
- [x] 4.4 Update source mixer meters and visualizers to obtain source colors from DSP-owned source state/UIState.
- [x] 4.5 Update `MeteringComponent::DrawSourceMixerMeters` so each source group draws existing mixer input meters in left/right half-width subregions without changing quad meter dimensions.
- [x] 4.6 Update `SourceMixerReductionComponent` so source reduction draws existing mixer input reductions on the left and right sides of the source group.
- [x] 4.7 Ensure width toggles rerun limit enforcement and assignment propagation for affected trios.

## 5. Verification

- [x] 5.1 Add or update focused tests for source input population, including mono right-lane silence and stereo left/right lanes.
- [x] 5.2 Add or update focused tests for source metering, including mono mirrored lanes and stereo independent lanes; confirm source meter split leaves quad meter constants untouched.
- [x] 5.3 Add or update focused tests for trio assignment: stereo-only, mono-only, mixed mono/stereo, zero selection, and overflow on mono-to-stereo.
- [x] 5.4 Add or update persistence tests for loading old configs and round-tripping source width/source selection state.
- [x] 5.5 Build the JUCE target or run the available project test command to catch compile regressions.
- [x] 5.6 Manually verify the Config grid shows four source selectors, four width toggles, source-colored right-side clusters, stereo source meters, and correct visible selection changes after overflow.
