# Smart Grid One Visualizers

This document describes the visualizer system for the Smart Grid One display. Visualizers render DSP data (scopes, spectra, meters, etc.) in the 24×18 grid layout, driven by the selected encoder bank.

## Architecture

### Components

| File | Purpose |
|------|---------|
| `SmartGridOneMainVisualizerComponent.hpp` | Base class for all visualizers. Defines `Draw()` and optional `OnClick()`. |
| `SmartGridOneVisualizerMain.hpp` | Main container component. Owns visualizer instances, handles layout, dispatch, and metering. |
| `ForEachSmartGridOneVisualizer.hpp` | X-macro list mapping (name, bank, block, constructor, source-machine flags) for each visualizer. |

### Base Class: SmartGridOneMainVisualizerComponent

Visualizers are **not** JUCE Components. They are lightweight structs that implement:

- **`Draw(juce::Graphics& g, juce::Rectangle<int> bounds)`** — Pure virtual. Receives block-local bounds `(0, 0, width, height)` after the main component has set clip region and origin.
- **`OnClick(const juce::MouseEvent& event)`** — Optional override. Event position is in block-local coordinates.

Each visualizer also carries `SourceMachineFlags`, and `SmartGridOneVisualizerMain` only draws visualizers whose flags match at least one visible voice in the active track.

### Layout

The grid is 24×18 cells. Block positions:

| Block | Grid position | Description |
|-------|---------------|--------------|
| 0 | (0, 0) | Top-left 8×8 |
| 1 | (0, 8) | Left 8×8 |
| 2 | (8, 8) | Center 8×8 |
| 3 | (16, 8) | Right 8×8 |
| -1 | (0, 8), 24×8 | Full-width strip (e.g. MelodyRoll) |

Encoder regions at (8,0), (16,0), (8,3), (16,3) are excluded from paint and click handling.

Metering occupies (8, 5), 16×2 cells.

### Bank-to-Visualizer Mapping

The selected encoder bank (`SmartGridOneEncoders::Bank`) determines which visualizers are drawn. Banks:

- **Source** — VCO scopes (Dual VCO only), physical modeling transfer-function response (Physical Modeling only), post-filter scope, source analyzer
- **FilterAndAmp** — Voice meter, post-amp scope, filter scope, filter analyzer
- **PanningAndSequencing** — Sound stage (quad position), MelodyRoll (full-width)
- **VoiceLFOs** — Control scopes 0–3
- **Delay** — Delay analyzer
- **Reverb** — Reverb analyzers
- **TheoryOfTime** — Quad analyzers, TheoryOfTime scope
- **Mastering** — Source mixer reduction/freq, multiband EQ, multiband gain reduction
- **Inputs** — Same as Mastering (inputs bank)
- **DeepVocoder** — Same as Mastering (deep vocoder bank)

## Data Sources

Visualizers read from:

- **`TheNonagonSquiggleBoyInternal::UIState`** — Central UI state. Access via `NonagonWrapper::GetUIState()`.
- **`m_squiggleBoyUIState`** — Contains:
  - `m_audioScopeWriter`, `m_controlScopeWriter`, `m_quadScopeWriter`, `m_sourceMixerScopeWriter`, `m_monoScopeWriter` — Scope buffers
  - `m_voiceMeterReader`, `m_returnMeterReader`, etc. — Meter readers
  - `m_activeTrack`, `m_xPos`, `m_yPos` — Panning/position state
  - `m_voiceFilterUIState` — Filter response for analyzer overlays
  - `m_delayUIState`, `m_reverbUIState` — Effect UI state for quad analyzers
- **`NonagonWrapper`** — Provides `GetAudioScopeWriter()`, `GetControlScopeWriter()`, `GetNoteWriter()`, etc.

## Existing Visualizers

| Component | Bank(s) | Block | Data source |
|-----------|---------|-------|-------------|
| ScopeComponent | Source, FilterAndAmp, VoiceLFOs | 0–3 | ScopeWriter, voice offset |
| AnalyserComponent | Source, FilterAndAmp | 3 | WindowedFFT, voice filter UI state |
| PhysicalModelingFrequencyResponseComponent | Source | 0 | `PhysicalModelingSource::UIState` transfer function |
| VoiceMeterComponent | FilterAndAmp | 0 | m_voiceMeterReader |
| SoundStageComponent | PanningAndSequencing | 0 | m_xPos, m_yPos, m_voiceMeterReader |
| MelodyRollComponent | PanningAndSequencing | -1 | NonagonNoteWriter |
| QuadAnalyserComponent | Delay, Reverb, TheoryOfTime | 0 | m_quadScopeWriter, delay/reverb UI state |
| TheoryOfTimeScopeComponent | TheoryOfTime | 2 | m_monoScopeWriter |
| SourceMixerReductionComponent | Mastering, Inputs, DeepVocoder | 0–3 | m_sourceMixerScopeWriter |
| SourceMixerFrequencyComponent | Mastering, Inputs, DeepVocoder | 1–3 | m_sourceMixerScopeWriter |
| MultibandEQComponent | Mastering, Inputs, DeepVocoder | 2–3 | m_quadScopeWriter |
| MultibandGainReductionComponent | Mastering, Inputs, DeepVocoder | 3 | m_stereoMasteringChainUIState |

## Related

- [Visualization Pipeline](ui-visualization-pipeline.md) — ScopeWriter capture, FFT analysis
- [UI Components and Layout](ui-components-layout.md) — WrldBuildrComponent, grid layout
- [Encoder System](encoder-system.md) — Bank selection, parameter mapping
