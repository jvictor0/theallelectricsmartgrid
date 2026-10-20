# Smart Grid One Visualizers

This document describes the visualizer system for the Smart Grid One display. Visualizers render DSP data (scopes, spectra, meters, etc.) in the experimental connected World Builder page, driven by the selected encoder bank. The original `WrldBuildrComponent` always displays its controller widgets.

## Architecture

### Components

| File | Purpose |
|------|---------|
| `SmartGridOneMainVisualizerComponent.hpp` | Base class for all visualizers. Defines `Draw()` and optional `OnClick()`. |
| `WrldBuildrVisualizerPage.hpp` | Connected page. Sole consumer of the main visualizer catalogue; owns the displayed visualizers, meters and voice-config overlay. |
| `ForEachSmartGridOneVisualizer.hpp` | X-macro list mapping (name, bank, slot, slot span, region, constructor, source-machine flags) for each visualizer. |
| `VisualizerRegion.hpp` | Full-slot, top/bottom and quadrant bounds shared by drawing and click routing. |
| `ForEachModulationVisualizer.hpp` | X-macro list with panel entries and encoder-only modulator entries. |
| `ModulatorVisualizers.hpp` | Owns encoder background instances, indexed by mode and modulator. |
| `EncoderModulatorVisualizers.hpp` | Initializes the encoder backgrounds for both pages from the unchanged modulation catalogue. |
| `EncoderModulatorComponents.hpp` | Individual Sheafy dials, Noise traces, and Spread bars for encoder backgrounds. |
| `GangedRandomLFOComponent.hpp` | Predictive wait/move view for one fixed-size correlated random gang. |
| `PartialMachineComponents.hpp` | Partial Machine input-spectrum and spatial atom views. |

### Base Class: SmartGridOneMainVisualizerComponent

Visualizers are **not** JUCE Components. They are lightweight structs that implement:

- **`Draw(juce::Graphics& g, juce::Rectangle<int> bounds)`** — Pure virtual. Receives visualizer-local bounds `(0, 0, width, height)` after the main component has set clip region and origin.
- **`OnClick(const juce::MouseEvent& event)`** — Optional override. Event position is in visualizer-local coordinates.

Each visualizer also carries `SourceMachineFlags`, and `WrldBuildrVisualizerPage` only draws visualizers whose flags match at least one visible voice in the active track.

### Layout

The connected page uses a 64×48 grid with slots 0, 1 and 2. Each catalogue entry declares its starting slot, slot span and `VisualizerRegion`: `Full`, `Top`, `Bottom`, `TopLeft`, `TopRight`, `BottomLeft` or `BottomRight`. Top/bottom stack two views vertically; the corner regions split a slot four ways. `Full` retains the existing drawing behavior. A span includes the gaps between its slots; the melody roll spans slots 1–2. Drawing and click routing use the same clipped, local bounds.

All banks fit in the three-slot row. The catalogue is checked at compile time so entries cannot silently fall outside the visible slots. Mastering uses the final output multiband meters; Inputs and DeepVocoder use the input source meters. Each keeps both frequency plots. No standalone modulation panels are moved into these slots.

| Region | Grid bounds |
|--------|-------------|
| Blank top strip | (0, 0), 64×3 |
| Slot 0 | (0, 3), 21×21 |
| Slot 1 | (21.5, 3), 21×21 |
| Slot 2 | (43, 3), 21×21 |
| File, Config, CPU | (0, 24), 16×4 |
| Meters | (16, 24), 48×4 |
| Encoders | (0, 28), 64×20; two rows of eight 8×8 dials with 8×2 badge strips facing inward |
| Voice config overlay | (16, 13), 48×15; covers the meters and extends upward while hardware config mode is active |

Standalone modulation panels are temporarily absent. The original 24×18 controller page has no visualizer-mode switching and keeps its grids, encoders, faders and joysticks visible.

### Modulator backgrounds in encoders

`EncoderBankUIState` publishes the selected bank mode and each visible cell's encoder type and index. Connected `ModulatorAmount` cells use those fields to look up their source's background, including nested selected depths at (3,3). Base parameters show a background when exactly one affecting modulator has a visualizer; unassociated sources and gesture assignments do not count. Gesture cells do not draw a background. Depth cells always retain their own source, even when another modulator affects the depth, and depth zero still shows its source.

The modulation visualizer catalogue associates each supported panel with a `VoiceModulator`, `QuadModulator`, or `GlobalModulator` enum. `G(name, slot, constructor, mode, color, modulator)` defines a panel and its optional encoder association. The panel slot is independent of this association; `std::nullopt` leaves a panel unassociated (currently the combined Sheafy panel). `H(modulator, constructor)` defines an encoder-only view: Sheaf 1–3, Voice/Quad Spread, and Noise in all three modes. These entries only supply encoder backgrounds. Standalone panel drawing and click handling are currently disabled for all entries.

`InitializeEncoderModulatorVisualizers` constructs each associated encoder background once per page, without creating standalone panels. Encoder-only constructors also run once. Background instances occupy a fixed 3×15 owning table (26 populated entries). All three voice bank modes share the Voice row and follow the existing active-trio UI state. Scope buffers and random-LFO snapshots are shared, while the main visualizers and encoder backgrounds keep separate voice selections.

Each encoder draws its background inside the square dial area at 50% opacity using a JUCE transparency layer. Its arcs, indicators, name and badges retain their normal appearance. The badge strip is outside the clip, whether above or below the dial. Depths use direct lookup. Base parameters inspect only their affecting-modulator bits and stop at the second eligible source. Drawing constructs no visualizer objects; background views receive no mouse events. Ganged-random and quad LFO indicator dots use the supplied modulator color, while their path colors stay unchanged.

Individual Sheafy dials fit inside the encoder's voice rings and share its 270-degree sweep. Spokes show the selected axis's attainable values for the active trio's current lens and time slice. Three concentric filled arcs show the actual held values in voice colors, read from `m_nonagonUIState.m_extraTimbre`; these include unison selection and may differ from the next pending pitch choices. The existing combined Sheafy panel is unchanged.

Noise draws a fresh UI-generated random trace each frame, without reading or advancing the DSP random generator. Spread draws three or four narrow bars for its fixed values from zero to one, centered inside the innermost encoder ring. Its bars use active-trio voice colors in Voice mode and fixed voice colors in Quad mode. Both follow the corresponding visual styles in Sheaf. All named modulators now have backgrounds, so combining Noise or Spread with an LFO counts as two eligible sources and hides the base parameter's background.

### Bank-to-Visualizer Mapping

The selected encoder bank (`SmartGridOneEncoders::Bank`) determines which visualizers are drawn. Banks:

- **Source** — Slot 0: VCO 1 on top and VCO 2 below (Dual VCO), sample trio waveform (Sample), or physical modeling transfer-function response (Physical Modeling). Slot 1: post-filter scope. Slot 2: source analyzer.
- **AmpAndRouting** — Post-amp scope in slot 0; VU dials removed.
- **Shape** — Filter scope in slot 0, filter analyzer in slot 1; slot 2 is blank.
- **PanningAndSequencing** — Sound stage in slot 0, melody roll across slots 1–2.
- **VoiceLFOs** — LFO 1 and LFO 2 in slots 0–1. The ganged-random scopes are removed from this page; their encoder backgrounds remain unchanged.
- **Delay** — Delay analyzer, quad delay envelope view
- **Reverb** — Reverb analyzers
- **PartialMachine** — Partial Machine analyzer, mono input spectrum with tracked atoms, spatial atom view
- **QuadLFOs** — Polyphonic PolyXFader LFO 1 and LFO 2 scopes
- **TheoryOfTime** — Quad analyzers, quad delay envelope view, TheoryOfTime scope
- **Mastering** — Final output multiband level/reduction meters in slot 0, source mixer frequency plot in slot 1, multiband EQ in slot 2.
- **Inputs** — Input source level/reduction meters in slot 0, source mixer frequency plot in slot 1, multiband EQ in slot 2.
- **DeepVocoder** — Input source level/reduction meters in slot 0, source mixer frequency/vocoder plot in slot 1, multiband EQ in slot 2.

## Data Sources

Visualizers read from:

- **`TheNonagonSquiggleBoyInternal::UIState`** — Central UI state. Access via `NonagonWrapper::GetUIState()`.
- **`m_squiggleBoyUIState`** — Contains:
  - `m_audioScopeWriter`, `m_controlScopeWriter`, `m_quadScopeWriter`, `m_quadControlScopeWriter`, `m_globalControlScopeWriter`, `m_sourceMixerScopeWriter`, `m_monoScopeWriter`, `m_monoAudioScopeWriter` — Scope buffers
  - `m_voiceMeterReader`, `m_returnMeterReader`, etc. — Meter readers
  - `m_activeTrack`, `m_xPos`, `m_yPos` — Panning/position state
  - `m_gangedRandomLFOUIState`, `m_quadGangedRandomLFOUIState`, `m_globalGangedRandomLFOUIState` — Coherent predictive random-LFO snapshots
  - `m_voiceFilterUIState` — Filter response for analyzer overlays
  - `m_delayUIState`, `m_reverbUIState`, `m_partialMachineUIState` — Effect UI state for quad analyzers and other effect-specific visualizers
- **`NonagonWrapper`** — Provides `GetAudioScopeWriter()`, `GetControlScopeWriter()`, `GetNoteWriter()`, etc.

## Existing Visualizers

| Component | Bank(s) | Slot | Data source |
|-----------|---------|-------|-------------|
| ScopeComponent | Source, AmpAndRouting, Shape, VoiceLFOs | 0–1 | ScopeWriter, voice offset |
| GangedRandomLFOComponent | Voice, Quad, Global modulation panels | slots 0–3 | Ganged random LFO snapshots |
| AnalyserComponent | Source, Shape | 2, 1 | WindowedFFT, voice filter UI state |
| PhysicalModelingFrequencyResponseComponent | Source | 0 | `PhysicalModelingSource::UIState` transfer function |
| SampleTrioWaveformVisualizerComponent | Source | 0 | `AudioBufferBank::UIState` waveform buckets plus `SampleSource::UIState` read-head/window data |
| SoundStageComponent | PanningAndSequencing | 0 | m_xPos, m_yPos, m_voiceMeterReader |
| SequencerMelodyVisualizerComponent | PanningAndSequencing | 1–2 | Sequencer, sheaf and global-phase UI snapshots |
| QuadAnalyserComponent | Delay, Reverb, PartialMachine, TheoryOfTime | 0 | m_quadScopeWriter, delay/reverb/partial-machine UI state |
| QuadDelayEnvelopeVisualizerComponent | Delay, TheoryOfTime | 1 | `m_delayUIState` envelope snapshots plus relative read/write head positions |
| PartialMachineInputSpectrumComponent | PartialMachine | 1 | `m_monoAudioScopeWriter` plus `m_partialMachineUIState` atom snapshot |
| PartialMachineSpatialComponent | PartialMachine | 2 | `m_partialMachineUIState` atom snapshot and frequency-dependent spatial mapping |
| TheoryOfTimeScopeComponent | TheoryOfTime | 2 | m_monoScopeWriter |
| SourceMixerReductionComponent | Inputs, DeepVocoder | 0 | Source lanes in m_voiceMeterReader |
| SourceMixerFrequencyComponent | Mastering, Inputs, DeepVocoder | 1 | m_sourceMixerScopeWriter |
| MultibandEQComponent | Mastering, Inputs, DeepVocoder | 2 | m_quadScopeWriter |
| MultibandGainReductionComponent | Mastering | 0 | m_stereoMasteringChainUIState |

`QuadDelayEnvelopeVisualizerComponent` is a non-FFT effect view. It renders min/max envelope snapshots derived from the quad delay's movable-writer buffer and overlays the current relative read/write tape-head positions from `m_delayUIState`.

`GangedRandomLFOComponent` reconstructs each voice's complete current round without storing scope history. It draws elapsed wait/move motion as a solid path, the remaining predicted contour as dashed segments, and the present position as a dot on the shared longest-voice time axis.

`SampleTrioWaveformVisualizerComponent` is another non-FFT Source view. It renders one horizontal strip per voice in the active trio using min/max buckets published by each selected `AudioBufferBank`; the active `SampleStart`/`SampleLength` region is colored by voice, and the current sample read head is drawn as a white vertical line.

`PartialMachineInputSpectrumComponent` draws the mono input sent into the Partial Machine, then overlays the tracked atoms after frequency-dependent reduction. `PartialMachineSpatialComponent` draws those same atoms in the quad field using the Partial Machine's frequency-to-radius and frequency-to-azimuth mapping.

## Related

- [Visualization Pipeline](ui-visualization-pipeline.md) — ScopeWriter capture, FFT analysis
- [Partial Machine](partial-machine.md)
- [UI Components and Layout](ui-components-layout.md) — WrldBuildrComponent, grid layout
- [Encoder System](encoder-system.md) — Bank selection, parameter mapping
