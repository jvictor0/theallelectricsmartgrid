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
- supports both direct channel display and eigen-style combined views.

## JUCE visual components

Defined primarily in `JUCE/SmartGridOne/Source/ScopeComponent.hpp`.

Main views:

- `ScopeComponent` (audio/control traces)
- `AnalyserComponent` (per-voice spectrum + filter response overlays)
- `QuadAnalyserComponent` (delay/reverb/master quad spectra)
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

DSP/nonagon modules write into these through `SetupScopeWriters(...)`; UI reads from published state on the JUCE thread.

## Related

- [UI Components and Layout](ui-components-layout.md)
- [DSP Overview](dsp-overview.md)
- [The Nonagon (Sequencer)](nonagon.md)
