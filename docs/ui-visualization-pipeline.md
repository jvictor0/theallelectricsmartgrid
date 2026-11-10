# Visualization Pipeline

This page documents how runtime DSP/control data is captured and rendered in JUCE visualizer components.

## Capture layer: `ScopeWriter`

Defined in `private/src/ScopeWriter.hpp`.

`ScopeWriterGeneric<Size, NumScopes>` captures channel vectors in six circular levels per scope. Production banks store every engine sample. Level zero has atomic scalar lanes; levels 1–5 belong to the UI thread and decimate successively by four through 17-tap FIR filters. Each level holds the same number of vectors, so coarser levels retain longer history. Logical storage positions remain original sample indices, with FIR delay and decimation phase handled by the reader.

`Write(scope, vector)` stores the current sample; `AdvanceIndex()` commits it and advances once per engine sample. Generic writers can also decimate their input. `Publish()` exposes completed samples, changed event markers, periodic origins/periods, and time-map metadata. Before painting, `AcceptSamples()` captures coherent marker metadata and builds the lower levels through its accepted sample boundary. It retries overlapping publication a bounded number of times and retains the previous frame if all attempts overlap. Atomic level-zero access permits a stalled UI to encounter overwritten values without a data race.

`RecordStart(scope, voice, sampleOffset)` retains a fractional real sample index. `RecordEnd` retains the corresponding end. Bound streams also store their continuous warped coordinates. On descending movement the writer undoes every crossed end and start: undoing an end reopens its start, and undoing a start restores the preceding interval. Backward start/end callbacks are ignored. Starts at an unchanged coordinate remain valid for existing transport behavior. Marker rings retain 256 intervals; expired entries cannot reappear during rewind.

Top events use `SampleTop`: a trigger flag and a double offset in engine samples
from the sample carrying the event. VPS derives the offset from the wrapped phase
and frequency (`SampleTop::FromWrap`); the dual VCO records each oscillator's top
on its own scope and forwards VCO 1's top to the source, whose post-filter scope
uses it. These values travel through their existing top members and buffers.
Theory of Time stores it in each loop's crossing events; PolyXFader combines
those events for the Theory of Time scope. Periodic LFO scopes publish origin
and period instead. Event-scope producers pass the interpolated crossing offset
to `RecordStart`; other start events are recorded at the current sample.

## Write helpers: `ScopeWriterHolder`

`ScopeWriterHolder` carries:

- a vector-width-specific `ScopeWriterInput<Size>*`
- bound `scopeIx`

Modules write with minimal overhead:

- `Write(vector)` captures all lanes together
- `IsWriteSample()`, so a producer can skip computing a value the writer would drop
- `SetPeriodicAtVoice(voiceIx, {origin, period})` for LFO and delay axes
- `RecordStart()` / `RecordStart(sampleOffset)` / `RecordStartAtVoice(voiceIx, sampleOffset)` / `RecordEnd()`

This is how voice VCO/filter/amp/LFO and quad buses push data into visualization buffers.

## Capture layer: derived UI snapshots

Not every visualizer is backed by `ScopeWriter`. Some views publish derived UI data directly into nested UI-state structs during `PopulateUIState(...)`.

Current examples:

- `SampleTrioWaveformVisualizerComponent` reads `m_voiceSourceUIState[i].m_audioBufferBankUIState` and `m_sampleSourceUIState`
- `AudioBufferBank::PopulateUIState(...)` publishes min/max waveform buckets for the selected or blended bank position, while `SampleSource::PopulateUIState(...)` publishes the read-head position and active start/length window
- `PartialMachineInputSpectrumComponent` and `PartialMachineSpatialComponent` read `m_partialMachineUIState`
- `PartialMachine::PopulateUIState(...)` publishes the current tracked spectral atoms and frequency-dependent transfer functions used by the analyzer overlays

This path is useful for views that need aggregated state rather than raw time-series buffers.

## Read layer: `ScopeReader` and factories

`ScopeReader` builds a display-ready sampling view from published data:

- supports transfer-aware sampling between cycle segments,
- interpolates sample values for screen-space X positions,
- uses explicit periodic origin/period metadata for LFOs and delay, and marker history for event streams.

For event streams, the unfinished right side samples the previous cycle through its recorded end,
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

`ScopeReader::m_sampleExtent` measures real engine-sample duration separately
from logical axis width. Bound readers measure the available plotted points
through their inverse; unbound readers retain the segment durations. Cursor
visibility (the existing 10 Hz threshold) and longest-source selection use this
real duration, including for modulation overlays.

The Theory of Time scope uses an explicit period on **unwarped** continuous phase
and a separate inverse of that phase. It records the bounded displacement
`modulatedPhase - unmodulatedPhase`. Drawing adds the musical unwarped coordinate
and wraps by the published period. It starts a new subpath at each vertical wrap,
at the current/previous-cycle join, and after unavailable history. The curve can
therefore bend backwards without changing the horizontal coordinate or drawing
a line across a wrap. Its cursor uses the same real-duration rule as other scopes.

`SampleTop` owns phase interpolation, top combination, and conversion to sample
positions (`GetPosition`). Offsets are always in engine samples. Producers pass
them unchanged to `RecordStart`, and the writer converts them to its own index
units by dividing by its decimation. The reader converts inverse timestamps through the writer’s origin and decimation.

All production scope banks advance at engine rate and share `m_firstScopeSample` as their absolute sample origin. `AdvanceScopeIndices()` runs at the end of each engine sample. Audio and FFT views remain in real time, with engine-rate level-zero spectra.

Voice LFOs, voice envelopes, quad LFOs, and the delay magnitude scope bind to the shared `TimeWarpInverter` owned by Theory of Time. A captured `TimeWarpView` supplies `G`, the chosen forward-history inverse of the continuous write-head coordinate `F`. `ScopeReader` uses explicit origin/period metadata for LFOs and delay, or reversible starts/ends for envelopes, then reads `S(G(w))` from its selected existing LOD. Periodic queries ahead of the cursor move back one period; this preserves full axis width during startup. Absolute inverse timestamps are converted through the writer's origin and decimation. Modulation overlays map each axis point through the same view before reading their real-time histories. Unbound scopes retain real-time axes.

The inverse has a fixed lifetime and atomic payloads. Each cell stores both fallback timestamps and one replacement with an exact double phase cut and relative float timestamps. A separate visit reassigns older islands below its cut to the fallback; turns within a closed-cell visit retain the minimum cut and its earlier timestamp. Main G occupies 32 MiB; unmodulated H occupies 16 MiB. The forward frontier closes a partial cell and freezes during reverse travel. Readers validate an unchanged even revision with bounded retries. Captured views reject coordinates outside retention, ahead of the cursor, or with interpolation endpoints newer than the real-time cutoff. Live writes may cause gaps; reversal count no longer exhausts cell storage. Marker metadata remains coherent, and neither publication nor painting locks the audio thread. Transport rebasing in phase units prevents musical resets from undoing history.

The delay scope stores the absolute value of each completed saturated, decimated delay-line write, held on the engine grid between host writes. Its explicit axis has origin zero and period one master cycle, without start markers. Existing FIR LODs form its envelope; the UI converts magnitude to dB and draws it symmetrically about each lane center, with read/write markers in the same phase axis. The old delay min/max buckets and positional recorder are removed. Signed delay-return audio remains available for spectra.

Theory of Time's own mono phase scope remains unbound. Fractional `SampleTop` offsets continue to travel unchanged from producers to the writer, which converts them to stored-sample units and, for bound streams, interpolates the observed warped step.

The producers of fractional tops and their scope capture are:

| Location | Behavior |
| --- | --- |
| VPS | On a wrap, `SampleTop::FromWrap(wrapped, freq)` sets the offset to `-wrapped / freq` engine samples. Phase advancement and wrap detection do not depend on the offset. |
| Dual VCO | Each oscillator's scope calls `RecordStart(top.m_offset)` on its wrap. VCO 1's top becomes the source top. |
| Sub VCO | The sine VCO exposes each lane's fractional wrap top; the track passes its offset to the shaped sub scope's `RecordStartAtVoice`. |
| Sample source | `SampleTop::FromPhases` applies the floor-crossing test between the previous and current total phase and adds its fractional timing. |
| Theory of Time base | `SetCrossings` sets each loop's crossing flags, then calls `InterpolatePhases` once per domain; the start sample keeps offset zero. Crossings are set before topology acceptance, so timing travels with the event that made an edit eligible. |
| PolyXFader | Initializes each lane with `AndIdentity`, so the `&&` combination retains the latest fractional crossing when all active loops cross. With zero total weight the lane keeps a true event at offset zero. |
| QuadLFO | Retains the unwrapped phase so `SampleTop::FromPhases` can interpolate the actual step, including phase synchronization, and records it with `RecordStartAtVoice(i, offset)`. |
| Filter scopes | Pass the source top's offset to `RecordStart`. |
| SquiggleLFO scopes | Publish the clock offset and common participating loop period; no start events. |
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
