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
Theory of Time loop crossings need only boolean events. Periodic LFO scopes,
including the Theory of Time scope, publish origin and period instead.
Event-scope producers pass the interpolated crossing offset
to `RecordStart`; other start events are recorded at the current sample.

## Write helpers: `ScopeWriterHolder`

`ScopeWriterHolder` carries:

- a vector-width-specific `ScopeWriterInput<Size>*`
- bound `scopeIx`

Modules write with minimal overhead:

- `Write(vector)` captures all lanes together
- `IsWriteSample()`, so a producer can skip computing a value the writer would drop
- `SetPeriodicAtVoice(voiceIx, {origin, period})` for LFO and delay axes
- `RecordStartAtVoice(voiceIx, sampleOffset)` / `RecordEndAtVoice(voiceIx)`
- `IsStartedAtVoice(voiceIx)` derives whether the most recent retained start lacks an end

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
through their inverse; unbound readers retain the segment durations. LOD and
longest-source selection use this real duration, including for modulation overlays.
Periodic LFO indicators instead use nominal sweep duration: the published axis
period divided by the existing `m_tempoHz` value. They appear above 0.1 seconds
when the cursor sample is available. Warp/history changes do not affect that
threshold decision. Event-based scopes retain the historical-duration rule.

The Theory of Time scope uses an explicit period on **unwarped** continuous phase
and a separate inverse of that phase. It records the bounded displacement
`modulatedPhase - unmodulatedPhase`. Drawing adds the musical unwarped coordinate
and wraps by the published period. It starts a new subpath at each vertical wrap,
at the current/previous-cycle join, and after unavailable history. The curve can
therefore bend backwards without changing the horizontal coordinate or drawing
a line across a wrap. Its cursor uses the same nominal-sweep rule as periodic LFOs.

`SampleTop` owns phase-crossing interpolation. Offsets are always in engine samples. Producers pass
them unchanged to `RecordStart`, and the writer converts them to its own index
units by dividing by its decimation. The reader converts inverse timestamps through the writer’s origin and decimation.

All production scope banks advance at engine rate and share `m_firstScopeSample` as their absolute sample origin. `AdvanceScopeIndices()` runs at the end of each engine sample. Audio and FFT views remain in real time, with engine-rate level-zero spectra.

Voice LFOs, voice envelopes, quad LFOs, and the delay magnitude scope bind to the shared `TimeWarpInverter` owned by Theory of Time. A captured `TimeWarpView` supplies `G`, the chosen forward-history inverse of the continuous write-head coordinate `F`. `ScopeReader` uses explicit origin/period metadata for LFOs and delay, or reversible starts/ends for envelopes, then reads `S(G(w))` from its selected existing LOD. Periodic queries ahead of the cursor move back one period; this preserves full axis width during startup. Absolute inverse timestamps are converted through the writer's origin and decimation. Modulation overlays map each axis point through the same view before reading their real-time histories. Unbound scopes retain real-time axes.

The inverse has a fixed lifetime and atomic payloads. Each cell stores both fallback timestamps and one replacement with an exact double phase cut and relative float timestamps. A separate visit reassigns older islands below its cut to the fallback; turns within a closed-cell visit retain the minimum cut and its earlier timestamp. Main G occupies 32 MiB; unmodulated H occupies 16 MiB. The forward frontier closes a partial cell and freezes during reverse travel.

Readers busy-wait until they observe an unchanged even revision. Writer sections cover bounded cell/frontier transitions, with the scatter loop outside; the audio writer never waits for readers. Captured views reject coordinates outside retention, ahead of the cursor, or whose interpolated timestamp is newer than the real-time cutoff. A newer right endpoint can locate an older accepted timestamp.

Contention does not produce missing points; genuinely absent history breaks the path, including weighted curves with an unavailable contributor. The warp renderer reads the inverse only once per point. Scheduling progress, not a fixed waiting duration, is assumed; reversal count no longer exhausts cell storage. Marker metadata remains coherent, and neither publication nor painting locks the audio thread. Transport rebasing in phase units prevents musical resets from undoing history.

The delay scope stores the absolute value of each completed saturated, decimated delay-line write, held on the engine grid between host writes. Its explicit axis has origin zero and period one master cycle, without start markers. Existing FIR LODs form its envelope; the UI converts magnitude to dB and draws it symmetrically about each lane center, with read/write markers in the same phase axis. The old delay min/max buckets and positional recorder are removed. Signed delay-return audio remains available for spectra.

LFO common periods are computed by `PolyXFader::ComputeLogicalPeriods` during
`PopulateUIState`, using the most recently processed weights and current loop
ratios. Voice and quad LFOs populate their axes before their banks publish;
Theory of Time does the same for its unmodulated axis. No GCD or top-event
aggregation runs in the per-sample LFO path.

Writer-owned marker state uses one ordinary count. Publication copies it into
the separate atomic snapshot; accepted UI state remains unchanged until
acceptance. Open-start status is derived from that count and the latest end flag,
including after rewind. There is no separate pending count or cached started flag.

Cycle crossings and scope capture work as follows:

| Location | Behavior |
| --- | --- |
| VPS | On a wrap, `SampleTop::FromWrap(wrapped, freq)` sets the offset to `-wrapped / freq` engine samples. Phase advancement and wrap detection do not depend on the offset. |
| Dual VCO | Each oscillator's scope calls `RecordStartAtVoice(voice, top.m_offset)` on its wrap. VCO 1's top becomes the source top. |
| Sub VCO | The sine VCO exposes each lane's fractional wrap top; the track passes its offset to the shaped sub scope's `RecordStartAtVoice`. |
| Sample source | `SampleTop::FromPhases` applies the floor-crossing test between the previous and current total phase and adds its fractional timing. |
| Theory of Time base | Stores boolean crossings in both phase domains, computed before topology acceptance. No remaining consumer needs a fractional offset. |
| Filter scopes | Pass the source top's offset to `RecordStart`. |
| SquiggleLFO scopes | Publish the clock offset and common participating loop period; no start events. |
| Theory of Time scope | Writes warp displacement when `IsWriteSample()` is true and publishes an unmodulated periodic axis; no start events. |
| Boolean-only delay trigger | Uses the loop-crossing flag for sample-and-hold triggering. |
| Scope writer | Stores double start positions and integer end positions. Its writer-owned marker count starts at zero and is copied into the atomic publication. |

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
