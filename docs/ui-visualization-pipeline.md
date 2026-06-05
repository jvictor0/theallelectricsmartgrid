# Visualization Pipeline

This page documents how runtime DSP/control data is captured and rendered in JUCE visualizer components.

## Capture layer: `ScopeWriter`

Defined in `private/src/ScopeWriter.hpp`.

`ScopeWriter` is a shared multi-scope, multi-voice circular buffer:

- `m_buffer`: interleaved sample storage.
- logical indexing via `(scope, voice, index) -> physical index`.
- `AdvanceIndex()` advances write position each sample/frame boundary.
- `Publish()` atomically exposes latest stable index (`m_publishedIndex`) and commits marker updates.

It also stores cycle markers:

- `RecordStart(scope, voice[, uBlockIndex])`
- `RecordEnd(scope, voice[, uBlockIndex])`

These markers are used for cycle-aware waveform views.

## Write helpers: `ScopeWriterHolder`

`ScopeWriterHolder` carries:

- `ScopeWriter*`
- bound `voiceIx`
- bound `scopeIx`

Modules write with minimal overhead:

- `Write(value)` / `Write(uBlockIndex, value)`
- `RecordStart()` / `RecordEnd()`

This is how voice VCO/filter/amp/LFO and quad buses push data into visualization buffers.

## Capture layer: derived UI snapshots

Not every visualizer is backed by `ScopeWriter`. Some views publish derived UI data directly into nested UI-state structs during `PopulateUIState(...)`.

Current examples:

- `QuadDelayEnvelopeVisualizerComponent` reads `m_delayUIState`
- `QuadDelay::PopulateUIState(...)` copies per-channel envelope snapshots and relative read/write head positions into rotating UI slots
- the delay envelope data is derived from `DelayLineMovableWriter` min/max buckets, remapped through `PositionalBufferRecorder` so the UI can sample the delay buffer against the current master-loop position
- `SampleTrioWaveformVisualizerComponent` reads `m_voiceSourceUIState[i].m_audioBufferBankUIState` and `m_sampleSourceUIState`
- `AudioBufferBank::PopulateUIState(...)` publishes min/max waveform buckets for the selected or blended bank position, while `SampleSource::PopulateUIState(...)` publishes the read-head position and active start/length window
- `PartialMachineInputSpectrumComponent` and `PartialMachineSpatialComponent` read `m_partialMachineUIState`
- `PartialMachine::PopulateUIState(...)` publishes the current tracked spectral atoms and frequency-dependent transfer functions used by the analyzer overlays

This path is useful for views that need aggregated state rather than raw time-series buffers.

## Read layer: `ScopeReader` and factories

`ScopeReader` builds a display-ready sampling view from published data:

- supports transfer-aware sampling between cycle segments,
- interpolates sample values for screen-space X positions,
- uses marker history to stabilize wave drawing.

`ScopeReaderFactory` provides lightweight creation with current voice/scope context.

## FFT/analyzer layer

### `WindowedFFT`

- pulls latest published window from `ScopeWriter`,
- applies Hann window,
- computes DFT,
- smooths magnitudes with per-bin low-pass filters.

### `QuadWindowedFFT`

- computes per-channel complex spectra for quad streams,
- supports per-channel magnitude display.

## JUCE visual components

Defined primarily in `JUCE/SmartGridOne/Source/ScopeComponent.hpp`.

Main views:

- `ScopeComponent` (audio/control traces)
- `AnalyserComponent` (per-voice spectrum + filter response overlays)
- `QuadAnalyserComponent` (delay/reverb/partial-machine/master quad spectra)
- `QuadDelayEnvelopeVisualizerComponent`
- `PartialMachineInputSpectrumComponent`
- `PartialMachineSpatialComponent`
- `SampleTrioWaveformVisualizerComponent`
- `TheoryOfTimeScopeComponent`
- `SoundStageComponent` (quad position + meter-weighted bubbles)
- `MelodyRollComponent` (note events via `NonagonNoteWriter`)

Additional mastering/meter views are in `MasteringComponents.hpp` and `MeterComponent.hpp`.

## Wiring into UI state

`SquiggleBoyWithEncoderBank::UIState` owns multiple `ScopeWriter` instances:

- audio scopes
- control scopes
- quad scopes
- source-mixer scopes
- mono scopes
- mono audio scopes

DSP/nonagon modules write into these through `SetupScopeWriters(...)`; UI reads from published state on the JUCE thread.

## Related

- [Smart Grid Visualizers](smart-grid-visualizers.md) — Visualizer architecture, layout, bank mapping, and how to add new visualizers
- [Partial Machine](partial-machine.md)
- [UI Components and Layout](ui-components-layout.md)
- [DSP Overview](dsp-overview.md)
- [The Nonagon (Sequencer)](nonagon.md)
