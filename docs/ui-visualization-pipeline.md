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

Top events use `SampleTop`: a trigger flag and a double offset in base-rate audio
samples from the sample carrying the event. VPS uses its existing `deltaT` to
normalize the offset immediately; the dual VCO rebases it within the output sample
before passing it to the source and filter. These values travel through their
existing top members and buffers. Theory of Time stores it in each loop's crossing
events, which PolyXFader forwards to the LFO and Theory of Time scopes. Each
producer interpolates the phase crossing; scope start markers store the resulting
double positions. Other start events keep their recorded sample position.

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

Cycle starts, lengths, transfer positions, and requested X positions use doubles.
The reader linearly interpolates captured values at the fractional positions.
The path and marker drawing retain those fractional positions. Views whose start
is not yet published or has fallen outside the retained history are empty.

`SampleTop` owns phase interpolation, oversample accumulation, top combination,
and conversion to scope positions. `SampleTimer::GetUBlockIndex()` is the base-rate
audio sample within an eight-sample microblock (`0..7`). The dual VCO's oversampled
loop index runs from `0..31`; its existing `baseIndex` runs from `0..7`.
Control-rate LFO scopes use `SampleTop::GetControlPosition` to express these
positions in their eight-audio-samples-per-scope-sample units. The reader and
visualizer have no oversampling information.

The Theory of Time scope retains its existing `Write(j)` and `RecordStart(j)`
association, with the fractional top offset added to the latter. Its existing
audio-indexed writes into a control-rate scope and one-sample-delayed top
association are separate issues; this change does not alter that capture cadence.

Beyond replacing boolean top members, buffers, arguments, and return values, the
audio-thread changes for fractional tops are:

| Location | Change and reason |
| --- | --- |
| VPS | Replace the existing `true` assignment on a wrap with `SampleTop::FromWrap`. Forward the existing `deltaT` to `UpdatePhase`; use double precision for that duration. This supplies fractional timing in base-rate units without changing phase advancement or wrap detection. |
| Dual VCO | Supply the real duration through the existing `deltaT` argument instead of its unused zero placeholder. Replace the two boolean OR assignments with `AccumulateOversample` calls, retaining a top until the base-rate sample is emitted. Record `top.GetPosition(baseIndex)` at the existing scope calls. |
| Sample source | Replace the floor-comparison expression with `SampleTop::FromPhases`, which uses the same crossing test and adds its fractional timing. |
| Theory of Time base | After the existing lattice crossing assignments, call `InterpolatePhases` once per domain. Trigger flags, start/stop behavior, topology acceptance, and buffer rollover stay as before. Timing is attached before topology changes, so it travels with the existing event. |
| PolyXFader | Initialize with `AndIdentity` so the existing `&&` expression retains the latest fractional crossing when all active loops cross. Its original early return and zero-weight true event are preserved. The overloaded operator evaluates both operands; the crossing accessor only reads stored state. |
| QuadLFO | Retain the phase before wrapping so `SampleTop::FromPhases` can interpolate the actual step, including existing phase synchronization. The existing scope call receives `GetControlPosition`. |
| Filter and SquiggleLFO scopes | Replace the existing start argument with `GetPosition` or `GetControlPosition`. The latter accounts for the control writer advancing after audio sample zero. |
| Theory of Time scope | Replace `RecordStart(j)` with `RecordStart(top.GetPosition(j))`; preserve the existing write cadence and index association. |
| Boolean-only delay trigger | Read `m_triggered` from the existing crossing result; timing is irrelevant to sample-and-hold triggering. |
| Scope writer | Accept/store double start positions and allow a fractional position in the existing per-voice helper. Initialize the existing pending-marker counter to zero so the first event has a defined slot. Buffer writes and publication cadence are unchanged. |

Reader interpolation, readable-history checks, and drawing changes execute on the
UI side. Integer buffer addresses and publication counters remain integers.

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
