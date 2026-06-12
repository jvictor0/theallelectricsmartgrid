# UI Visualization Pipeline Specification

## Purpose
The visualization pipeline (`private/src/ScopeWriter.hpp`, `TransferFunction.hpp`, and `PopulateUIState` implementations across the DSP modules) carries runtime signal data from the audio and control threads to the JUCE thread without locks. DSP code writes interleaved samples and cycle markers into ScopeWriter ring buffers and publishes a stable read boundary atomically; readers, FFT analyzers, and derived UIState snapshots consume only published data. A shared TransferFunction interface, implemented exclusively by UIState types, supplies frequency-response overlays. The visualizers that render this data are specified in grid-visualizers; filter UIState providers in filter-system; delay and partial-machine snapshot producers in quad-delay and partial-machine.

## Requirements

### Requirement: Interleaved Circular Buffer Indexing
The system SHALL store all scopes and voices of a `ScopeWriter` in one interleaved float ring buffer of `x_bufferSize` (32 * 1024 * 1024) samples, mapping a logical position to physical storage as `(index * voices * scopes + scope * voices + voice) % x_bufferSize`. `MaxIndexes()` reports the per-stream capacity `x_bufferSize / (voices * scopes)`. Writes target the current write index `m_index` (plus an optional intra-block `uBlockIndex` offset); `AdvanceIndex()` advances the write position by one.

#### Scenario: Distinct streams never collide
- **WHEN** two writes occur at the same logical index for different (scope, voice) pairs within configured bounds
- **THEN** their physical indices differ, and `ReadSample` for each pair returns the value written for that pair

#### Scenario: Logical index wraps at capacity
- **WHEN** a stream's logical index advances past `MaxIndexes()`
- **THEN** the physical index wraps modulo `x_bufferSize` and overwrites the oldest samples of that stream

### Requirement: Lock-Free DSP Writes
The system SHALL let DSP modules write without locks or allocation through `ScopeWriterHolder`, which binds a `ScopeWriter*` to a fixed voice index and scope index and forwards `Write(value)`, `Write(uBlockIndex, value)`, quad writes (four consecutive voice slots), and `RecordStart`/`RecordEnd` calls. A holder with a null writer drops all calls.

#### Scenario: Quad write fans out to four voice slots
- **WHEN** a holder bound to voice base v writes a `QuadFloat`
- **THEN** components 0-3 are written to voices v through v+3 of the holder's scope at the current write index

#### Scenario: Null holder is inert
- **WHEN** a default-constructed `ScopeWriterHolder` receives `Write` or `RecordStart`
- **THEN** no buffer access occurs and no fault is raised

### Requirement: Atomic Publish Boundary
The system SHALL expose new samples to readers only through `Publish()`: the writer thread stores the current write index into the atomic `m_publishedIndex`, and pending cycle-marker advances are committed to the atomic per-stream marker counters in the same call. JUCE-thread readers treat `m_publishedIndex.load()` as the exclusive upper bound of readable data, so partially written frames are never observed.

#### Scenario: Unpublished samples are invisible
- **WHEN** samples are written and `AdvanceIndex` is called but `Publish` has not run
- **THEN** `m_publishedIndex` still reports the previous boundary and readers sampling up to it see only previously published data

#### Scenario: Publish exposes samples and markers together
- **WHEN** `Publish()` runs after writes and `RecordStart` calls
- **THEN** `m_publishedIndex` equals the current write index and each stream's `m_startIndexIndex` atomic has advanced by the number of cycle starts recorded since the previous publish

### Requirement: Cycle Markers
The system SHALL record waveform cycle boundaries per (scope, voice) stream in rings of `x_numStartIndices` (256) start/end index pairs: `RecordStart(scope, voice[, uBlockIndex])` stores the upcoming cycle's start index and stages a marker advance; `RecordEnd` stores the end index for the most recently started cycle. Marker advances become visible to readers only at `Publish()`, and visualizers use the pairs to segment waveforms into complete cycles.

#### Scenario: Start and end bracket a cycle
- **WHEN** a stream records `RecordStart` at index a, writes samples, records `RecordEnd` at index b, and publishes
- **THEN** the stream's latest marker pair reads (a, b) and a reader can sample exactly the cycle [a, b)

#### Scenario: Marker ring wraps
- **WHEN** more than 256 cycles are recorded on one stream
- **THEN** new pairs overwrite the oldest entries modulo `x_numStartIndices` while the atomic marker counter keeps increasing monotonically

### Requirement: Cycle-Aware Display Sampling
The system SHALL build display traces with `ScopeReader`, which snapshots the latest published marker counter, derives the current and previous cycle start indices, and maps each of `numXSamples` screen positions to an interpolated buffer read (`Read` linearly interpolates between adjacent samples). When the current cycle is still shorter than the previous cycle's length, positions past the transfer point are filled from the previous cycle so the trace stays stable mid-cycle; when the apparent cycle span exceeds `MaxIndexes()` the reader reports empty and `Get` returns 0. `ScopeReaderFactory` mints readers with the current voice/scope context.

#### Scenario: Mid-cycle trace borrows the previous cycle
- **WHEN** a reader is created while the current cycle has fewer samples than the previous cycle's length
- **THEN** screen positions before the transfer point sample the current cycle and positions after it sample the previous cycle's corresponding region

#### Scenario: Stale stream yields an empty trace
- **WHEN** the distance from cycle start to the published index exceeds `MaxIndexes()`
- **THEN** the reader marks itself empty and every `Get(sample)` returns 0

### Requirement: Windowed FFT Analysis with Per-Bin Smoothing
The system SHALL compute spectra with `WindowedFFT`: it pulls the window of `BasicWaveTable::x_tableSize` samples ending at the writer's published index, applies a Hann window, runs a DFT, and feeds each bin magnitude through a per-bin `OPLowPassFilter` so the displayed spectrum is temporally smoothed. `GetMagDb(freq)` linearly interpolates between smoothed bins and returns 20*log10 of the magnitude. `QuadWindowedFFT` runs the same computation for four channels of one scope, smoothing per-channel real, imaginary, and magnitude components for quad spectrum display.

#### Scenario: Window ends at the publish boundary
- **WHEN** `Compute(voiceIx)` runs
- **THEN** the DFT input is exactly the published-index-aligned window read from the ScopeWriter, Hann-weighted sample by sample

#### Scenario: Spectrum is smoothed across frames
- **WHEN** two consecutive `Compute` calls process windows with differing bin magnitudes
- **THEN** each bin's reported value moves toward the new magnitude by its low-pass filter response rather than jumping, and `GetMagDb` interpolates between adjacent smoothed bins

### Requirement: TransferFunction Interface Implemented Only by UIState Types
The system SHALL define `TransferFunction` (`private/src/TransferFunction.hpp`) as the shared frequency-domain interface with pure virtual `FrequencyResponse(freq)` and `TransferFunctionValue(freq)` (complex). Only UIState snapshot types implement it — `DampingFilter::UIState`, `CombFilterWithOnePole::UIState`, `FIR::UIState`, `PhysicalModelingSource::UIState`, the voice filter UIState, and the partial machine's per-speaker transfer functions; DSP classes running on the audio thread never inherit from it. Implementations read atomic coefficient snapshots, e.g. `DampingFilter::UIState` returns the product of its one-pole high-pass and low-pass responses from `m_hpAlpha`/`m_lpAlpha`.

#### Scenario: Damping response is the HP and LP product
- **WHEN** a `DampingFilter::UIState` with stored alphas is queried at frequency f
- **THEN** `FrequencyResponse(f)` equals the one-pole low-pass response at `m_lpAlpha` times the one-pole high-pass response at `m_hpAlpha`, and `TransferFunctionValue(f)` is the product of the corresponding complex values

#### Scenario: Audio-thread DSP objects are not providers
- **WHEN** an analyzer needs a response curve for a filter, delay, reverb, or partial-machine overlay
- **THEN** it queries a UIState object's `TransferFunction` methods, and no audio-thread DSP class is reachable through the `TransferFunction` interface

### Requirement: Derived UI Snapshots Populated on the Control Thread
The system SHALL populate aggregated visualization state in `PopulateUIState(...)` methods executed on the control thread, never the audio thread, writing into atomic UIState fields that the JUCE thread reads directly. Current producers include `QuadDelay::PopulateUIState` (per-channel min/max envelope buckets remapped against the master-loop position plus relative read/write head positions), `AudioBufferBank`/`SampleSource` UIState (waveform min/max buckets, read-head position, and active start/length window), and `PartialMachine::PopulateUIState` (the tracked spectral atom list and frequency-dependent transfer functions).

#### Scenario: Delay envelope view reads snapshot slots
- **WHEN** the quad delay envelope visualizer renders a frame
- **THEN** every envelope bucket and head position it draws comes from `m_delayUIState` fields written by the most recent `PopulateUIState` call, not from the delay's audio-thread buffers

#### Scenario: Partial-machine atoms come from the published list
- **WHEN** the partial-machine spectrum or spatial view renders
- **THEN** the atoms drawn are exactly those in `m_partialMachineUIState`'s last published snapshot, positioned via its frequency-dependent transfer functions
