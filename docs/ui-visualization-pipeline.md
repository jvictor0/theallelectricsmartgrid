# Visualization Pipeline

This page documents how runtime DSP/control data is captured and rendered in JUCE visualizer components.

## Capture layer: `ScopeWriter`

Defined in `private/src/ScopeWriter.hpp`.

`ScopeWriter` is a shared multi-scope, multi-voice circular buffer:

- `m_buffer`: interleaved sample storage.
- logical indexing via `(scope, voice, index) -> physical index`.
- decimation: `ScopeWriter(voices, scopes)` stores every engine sample;
  `ScopeWriter(voices, scopes, decimation)` stores one value per `decimation`
  engine samples.
- `Write(scope, voice, value)` is called every engine sample. Only the sample
  with `m_subIndex == 0` (`IsWriteSample()`) is stored, at `m_index`; the other
  writes are dropped.
- `AdvanceIndex()` runs once per engine sample. It increments `m_subIndex` and,
  when that wraps at the decimation, advances `m_index` by one.
- `Publish()` atomically exposes latest stable index (`m_publishedIndex`) and commits marker updates.

It also stores cycle markers:

- `RecordStart(scope, voice[, sampleOffset])` stores the fractional index
  `m_index + (m_subIndex + sampleOffset) / decimation`. `sampleOffset` is in
  engine samples relative to the current sample.
- `RecordEnd(scope, voice)` stores `m_index`, rounded up to the next stored
  sample when it is called between stored samples.

These markers are used for cycle-aware waveform views.

Top events use `SampleTop`: a trigger flag and a double offset in engine samples
from the sample carrying the event. VPS derives the offset from the wrapped phase
and frequency (`SampleTop::FromWrap`); the dual VCO records each oscillator's top
on its own scope and forwards VCO 1's top to the source, whose post-filter scope
uses it. These values travel through their existing top members and buffers.
Theory of Time stores it in each loop's crossing events, which PolyXFader forwards
to the LFO and Theory of Time scopes. Each producer interpolates the phase
crossing and passes the offset to `RecordStart`; the writer stores the resulting
double position. Other start events are recorded at the current sample.

## Write helpers: `ScopeWriterHolder`

`ScopeWriterHolder` carries:

- `ScopeWriter*`
- bound `voiceIx`
- bound `scopeIx`

Modules write with minimal overhead:

- `Write(value)` / `WriteAtVoice(voiceIx, value)` / `Write(QuadFloat)` (four consecutive voices)
- `IsWriteSample()`, so a producer can skip computing a value the writer would drop
- `RecordStart()` / `RecordStart(sampleOffset)` / `RecordStartAtVoice(voiceIx, sampleOffset)` / `RecordEnd()`

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

The unfinished right side samples the previous cycle through its recorded end,
excluding any gap before the current start. Without an explicit end marker, the
current start remains the previous cycle's endpoint.

Voice audio and AHD envelope scopes end at the first relevant modulated cycle
crossing while their envelope is idle. Relevant cycles are the trio's selected
clock and unmuted time bits. Theory of Time changes update the ratio and crossing
result; AHD carries the crossing as a one-sample event. Each scope must have an
open start before it records an end, so later idle crossings leave that end fixed.

Cycle starts, lengths, transfer positions, and requested X positions use doubles.
The reader linearly interpolates captured values at the fractional positions.
The path and marker drawing retain those fractional positions. Views whose start
is not yet published or has fallen outside the retained history are empty.

`SampleTop` owns phase interpolation, top combination, and conversion to sample
positions (`GetPosition`). Offsets are always in engine samples. Producers pass
them unchanged to `RecordStart`, and the writer converts them to its own index
units by dividing by its decimation. The reader and visualizer have no decimation
information.

Control, quad control, global control, and mono writers use
`ScopeWriter::x_controlDecimation` (`8 * SampleTimer::x_oversample`), one value
per 8 host frames at every ratio, so control histories keep their duration and
write rate. Audio, quad, source-mixer, and mono audio writers use
`ScopeWriter::x_audioDecimation` (`SampleTimer::x_oversample`), so they store one
value per host frame and FFT analyzers see cycles per host sample. Producers
write host-rate signals where they exist (mixer output, Partial Machine input)
and otherwise subsample the engine-rate signal on host frames without an
anti-alias filter. Voice audio scopes and the quad Dry and Sub scopes are
subsampled this way, so their spectra fold engine-rate content above the host
Nyquist into the display; the audio itself is decimated properly. `SquiggleBoyWithEncoderBank::UIState::AdvanceScopeIndices()`
advances every writer once per engine sample, at the end of
`TheNonagonSquiggleBoyInternal::ProcessSample`. It also stores `m_firstAudioSample`
and `m_firstControlSample`, the engine samples of audio index 0 and control index
0; `m_firstAudioSample` is stored on an audio write sample. The audio-scope sample
range multiplies published audio indices by `x_audioDecimation`. The modulator history (`SmartGridOneModulatorHistory.hpp`) maps an engine
sample to a control index as `(sample - m_firstControlSample) / x_controlDecimation`.

Theory of Time keeps one current sample (`m_sample`), which `Process` advances in
place; consumers read that current sample. The Theory of Time scope writes the
wrapped modulated global-loop phase on the mono writer's write samples, so it
stores one phase per `x_controlDecimation` engine samples. The phase-modulation
PolyXFader runs before the base advances, reading the crossings still held in
`m_sample`. When it reports a top, the scope calls `RecordStart(offset)` with the
top's fractional offset. For example, with decimation 8, at writer index 100 and
sub-index 2, an offset of `-0.5` engine samples is stored at
`100 + 1.5 / 8 = 100.1875`.

The producers of fractional tops and their scope capture are:

| Location | Behavior |
| --- | --- |
| VPS | On a wrap, `SampleTop::FromWrap(wrapped, freq)` sets the offset to `-wrapped / freq` engine samples. Phase advancement and wrap detection do not depend on the offset. |
| Dual VCO | Each oscillator's scope calls `RecordStart(top.m_offset)` on its wrap. VCO 1's top becomes the source top. |
| Sample source | `SampleTop::FromPhases` applies the floor-crossing test between the previous and current total phase and adds its fractional timing. |
| Theory of Time base | `SetCrossings` sets each loop's crossing flags, then calls `InterpolatePhases` once per domain; the start sample keeps offset zero. Crossings are set before topology acceptance, so timing travels with the event that made an edit eligible. |
| PolyXFader | Initializes each lane with `AndIdentity`, so the `&&` combination retains the latest fractional crossing when all active loops cross. With zero total weight the lane keeps a true event at offset zero. |
| QuadLFO | Retains the unwrapped phase so `SampleTop::FromPhases` can interpolate the actual step, including phase synchronization, and records it with `RecordStartAtVoice(i, offset)`. |
| Filter and SquiggleLFO scopes | Pass the source or PolyXFader top's offset to `RecordStart`. |
| Theory of Time scope | Writes the modulated phase only when `IsWriteSample()` is true and records the phase-modulation PolyXFader's top with `RecordStart(offset)`. |
| Boolean-only delay trigger | Reads `m_triggered` from the crossing result; timing is irrelevant to sample-and-hold triggering. |
| Scope writer | Stores double start positions and integer end positions. The pending-marker counter starts at zero so the first event has a defined slot. |

Reader interpolation, readable-history checks, and drawing execute on the UI
side. Integer buffer addresses and publication counters remain integers.

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
