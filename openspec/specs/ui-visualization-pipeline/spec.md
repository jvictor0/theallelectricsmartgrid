# UI Visualization Pipeline Specification

## Purpose
The visualization pipeline (`private/src/ScopeWriter.hpp`, `TransferFunction.hpp`, and `PopulateUIState` implementations across the DSP modules) carries runtime signal data from the audio thread to the JUCE thread without locks. DSP code writes engine-rate channel vectors and cycle markers into level-zero ScopeWriter rings and publishes completed positions atomically. Before painting, the UI accepts each bank through a captured position and builds its lower levels; waveform and FFT readers use that accepted frame boundary. Derived UIState snapshots retain their atomic publication protocol. A shared sampled response interface supplies percentile-band frequency-response overlays from audio-thread coefficient captures. Only the partial machine retains the legacy TransferFunction interface. The visualizers that render this data are specified in grid-visualizers; filter UIState providers in filter-system; delay and partial-machine snapshot producers in quad-delay and partial-machine.

## Requirements

### Requirement: Interleaved Circular Buffer Indexing
The system SHALL use ScopeWriterGeneric<Size, NumScopes> with six vector rings per scope, each holding 0.25 seconds at the engine sample rate. All production scopes, including control signals, SHALL store every engine sample in banks grouped by vector width (one, three, or four), including a dedicated quad delay-magnitude bank. Higher levels SHALL be decimated by four successively with 17-tap FIR filters on the UI thread during acceptance. The audio thread SHALL write only level zero, using lock-free atomic scalar lanes. Higher levels SHALL remain UI-owned. Storage coordinates SHALL remain level-zero samples, with FIR delay and decimation phase handled internally. Vector storage SHALL interleave all lanes of one scope sample. Writes and AdvanceIndex occur at engine rate, with production base decimation equal to one. All banks SHALL share one absolute engine-sample origin. GetLod(samplesBack) SHALL select the finest retained level covering the requested lookback; history reads SHALL reject interpolation endpoints outside the accepted bounds. Capacity SHALL be independent of other scopes in the bank.

#### Scenario: Audio and control histories share the engine timeline
- **WHEN** a producer writes audio and control vectors on consecutive engine samples
- **THEN** both histories store each sample and advance by one on every engine sample
- **AND** top/start/stop offsets retain their fractional engine-sample coordinates

#### Scenario: Distinct streams never collide
- **WHEN** two writes occur at the same logical index for different (scope, voice) pairs within configured bounds
- **THEN** their physical indices differ, and `ReadSample` for each pair returns the value written for that pair

#### Scenario: Logical index wraps at capacity
- **WHEN** a stream's logical index advances past the selected level's capacity
- **THEN** the physical index wraps modulo the per-level vector capacity and overwrites the oldest samples of that stream

#### Scenario: Decimated writer stores one sample per decimation period
- **WHEN** a non-production generic writer configured with decimation 8 receives a `Write` and an `AdvanceIndex` on each of eight consecutive engine samples, starting with `m_subIndex == 0`
- **THEN** only the first sample's value is stored, at the starting `m_index`
- **AND** `m_index` has advanced by one and `m_subIndex` is zero again

### Requirement: Lock-Free DSP Writes
The system SHALL provide vector-width-specific scope holders whose Write takes a whole MultiChannelNumber vector. DSP producers SHALL write vectors without locks or allocation; start/end markers remain per lane. Holders SHALL retain inert null behavior and IsWriteSample. Scalar UI reads SHALL use a common read interface independently of writer template parameters.

#### Scenario: Quad write fans out to four voice slots
- **WHEN** a quad holder writes a `QuadFloat`
- **THEN** components 0-3 are written to the four lanes of the holder's scope at the current write index

#### Scenario: Null holder is inert
- **WHEN** a default-constructed vector scope holder receives `Write` or `RecordStart`
- **THEN** no buffer access occurs and no fault is raised

### Requirement: Atomic Publish Boundary
The audio thread SHALL publish its completed level-zero position and writer-owned per-stream cycle-marker counters after writing the data. At the beginning of each UI paint frame, before drawing, the UI SHALL capture a published boundary for each scope bank, build its lower levels from the newly accepted samples, and update its accepted position. All waveform, FFT, and secondary-modulator reads SHALL use these accepted positions throughout the frame. Periodic origins/periods, cycle start/stop metadata, and inverse view bounds SHALL be snapshotted against the accepted boundary using atomic payloads and a publication revision. Acceptance SHALL use a bounded retry budget and keep the previous accepted frame when publication overlaps every attempt. Reaccepting the same position SHALL not rerun the cascade. Publication and acceptance do not pin level-zero slots; UI catch-up SHALL index the circular buffer directly even if the producer has overwritten samples during a long UI stall.

#### Scenario: Unpublished samples are invisible
- **WHEN** samples are written and `AdvanceIndex` is called but `Publish` has not run
- **THEN** `m_publishedIndex` still reports the previous boundary and readers sampling up to it see only previously published data

#### Scenario: Publication waits for UI acceptance
- **WHEN** `Publish()` runs after writes and `RecordStart` calls
- **THEN** `m_publishedIndex` and marker counters advance, but visualizer bounds and accepted markers stay unchanged until the next UI acceptance
- **AND** the audio thread does not process a downsampler

#### Scenario: Fractional cycle starts wait for their readable sample
- **WHEN** the UI captures an exclusive sample count of 24 and the audio thread subsequently publishes a cycle start at 23.75
- **THEN** accepting the captured boundary retains the previous readable cycle, while starts exactly at sample 23 remain eligible
- **AND** the fractional start becomes eligible when the UI accepts sample count 25

#### Scenario: Coarse history has its own live edge
- **WHEN** a reader requests an LOD whose FIR output represents an earlier sample than level zero
- **THEN** strict reads respect that level's valid bounds and display reads may hold its latest available value

#### Scenario: Publish exposes samples and markers together
- **WHEN** `Publish()` runs after writes and `RecordStart` calls
- **THEN** `m_publishedIndex` equals the current write index and each stream's published marker count matches the writer-owned count after all starts and rewinds since the previous publish

### Requirement: Cycle Markers
Event streams SHALL record waveform cycle boundaries per (scope, voice) stream in rings of `x_numStartIndices` (256) start/end index pairs: `RecordStart(scope, voice[, sampleOffset])` stores the upcoming cycle's fractional start index `m_index + (m_subIndex + sampleOffset) / m_decimation`, where `sampleOffset` is in engine samples relative to the current sample, and advances the writer-owned count; `RecordEnd(scope, voice)` stores the end index for the most recently started cycle as `m_index`, rounded up to the next stored sample when called between stored samples. Marker counters become published at `Publish()`. The UI snapshots eligible marker pairs during acceptance, excluding starts beyond its last readable sample, and visualizers use those accepted pairs to segment waveforms into complete cycles.

#### Scenario: Start and end bracket a cycle
- **WHEN** a stream records `RecordStart` at index a, writes samples, records `RecordEnd` at index b, publishes, and the UI accepts through b
- **THEN** the stream's latest marker pair reads (a, b) and a reader can sample exactly the cycle [a, b)

#### Scenario: Fractional start on a decimated writer
- **WHEN** a writer with decimation 8 is at `m_index` 100 and `m_subIndex` 2, and a producer calls `RecordStart` with `sampleOffset` -0.5
- **THEN** the stored start index is 100.1875
- **AND** a `RecordEnd` on that same engine sample stores 101

#### Scenario: Marker ring wraps
- **WHEN** more than 256 cycles are recorded on one stream
- **THEN** new pairs overwrite the oldest entries modulo `x_numStartIndices` while the oldest retained marker advances; warped rewind may reduce the active count but SHALL NOT restore overwritten entries

### Requirement: Cycle-Aware Display Sampling
Unbound ScopeReader instances SHALL retain cycle and transfer calculations in original sample coordinates. Bound readers SHALL compute their axes in continuous warped coordinates and compose sample queries through the published inverse view, as specified in warped-scope-history. It SHALL select one LOD covering both the current cycle and any borrowed previous-cycle segment before sampling, and use that LOD for the full trace. ScopeReaderFactory SHALL retain the current voice/scope context. Strict historical reads reject unavailable history; display sampling holds the latest selected-level value at its delayed live edge. Readers SHALL be empty when required history exceeds all levels or required event markers are unavailable; periodic streams SHALL use explicit origin/period metadata and SHALL NOT require cycle markers. FFT readers SHALL explicitly use level zero.

#### Scenario: Mid-cycle trace borrows the previous cycle
- **WHEN** a reader is created while the current cycle has fewer samples than the previous cycle's length
- **THEN** screen positions before the transfer point sample the current cycle and positions after it sample the previous cycle's corresponding region

#### Scenario: Stale stream yields an empty trace
- **WHEN** the distance from cycle start to the accepted index exceeds all retained levels
- **THEN** the reader marks itself empty and every `Get(sample)` returns an unavailable value

### Requirement: Windowed FFT Analysis with Per-Bin Smoothing
The system SHALL compute spectra with `WindowedFFTGeneric<Size>`: it pulls a level-zero window of `1024 * SampleTimer::x_oversample` samples ending at the writer's accepted index, zero-fills unavailable startup samples, applies a Hann window, runs a SIMD FFT, and feeds each bin's vector magnitude through a per-bin `OPLowPassFilterGeneric` so the displayed spectrum is temporally smoothed. `GetMagDb` linearly interpolates between smoothed bins for the requested lane and returns 20*log10 of the magnitude. `QuadWindowedFFT` computes four channels together, preserving its existing gain and magnitude-smoothing rate.

#### Scenario: Window ends at the accepted boundary
- **WHEN** `Compute()` or `Compute(scopeIx)` runs
- **THEN** the FFT input is the accepted-index-aligned level-zero window read from the ScopeWriter, with unavailable samples zero-filled and every sample Hann-weighted

#### Scenario: Spectrum is smoothed across frames
- **WHEN** two consecutive `Compute` calls process windows with differing bin magnitudes
- **THEN** each bin's reported value moves toward the new magnitude by its low-pass filter response rather than jumping, and `GetMagDb` interpolates between adjacent smoothed bins

### Requirement: Sampled Filter Response Interface
`SampledTransferFunction` SHALL expose response queries in Hz and volts/octave relative to middle C, returning named lower, median, and upper magnitude order statistics. For N captured magnitudes sorted ascending, percentile p SHALL select index `floor(p * (N - 1))`; the median SHALL use p = 0.5. Default configuration SHALL retain 4096 snapshots, evaluate the latest 32, select the 10th/50th/90th percentiles, and sample approximately one in 25 host frames using an audio-owned xorshift generator independent of sound generation. At 48 kHz this gives the latest 32 captures an expected window of 1/60 second, matching one nominal UI repaint; the actual window varies with the random draws.

`Sample()` SHALL return IDs into a coherent UI-owned batch, valid until the next `Sample()` call. Every frequency column and lane of a plot SHALL reuse that batch. Frequency conversions and trigonometric terms SHALL be prepared once per column and shared across response evaluations. UI response trigonometry SHALL use the `Math::` lookup functions, including the comb delay phase; lookup approximation error is acceptable for these display curves. DC and Nyquist frequency terms SHALL remain exact. The producer SHALL never wait for a reader or allocate when capturing. Ring overwrite SHALL not mutate a batch already accepted by the UI. Empty histories SHALL draw no curve; stationary histories SHALL have equal bounds.

Coefficient snapshots SHALL retain whole DSP channel vectors. Trio filter/source plots and quad damping plots SHALL evaluate all lanes together, using vector magnitude/complex arithmetic and vector compare/exchange percentile selection before drawing any lane. A lane's statistics SHALL be independent of the ordering of other lanes. Partial cohorts and non-power-of-two configured cohort sizes SHALL retain the same percentile definition. Four independent input-filter responses SHALL share one four-lane snapshot and evaluation batch. Multiband crossover evaluations and FIR tap groups SHALL use vector arithmetic for their independent work. Single-curve providers SHALL vectorize percentile sorting across captured magnitudes.

Response overlays SHALL use a width-independent 1024-column frequency grid. Resizing SHALL only change the screen mapping of those columns. Prepared frequency terms SHALL be reused across paints while the axis settings remain unchanged; logarithmic spectrum buckets SHALL be shared instead of rebuilt for every path drawer. This preserves the previous frequency sampling density without claiming to resolve arbitrary sub-column resonances.

#### Scenario: Independent vector response lanes
- **WHEN** a trio or quad has different coefficients and different magnitude orderings in its lanes
- **THEN** each lane's returned percentiles match independent scalar sorting
- **AND** the analyzer computes every lane before issuing the first response path

#### Scenario: Resize a narrow stationary resonance
- **WHEN** a stationary response plot changes width
- **THEN** its evaluated frequencies and order statistics remain unchanged

#### Scenario: Producer wraps during painting
- **WHEN** the audio producer wraps the retained history after the UI selects samples
- **THEN** every frequency column still evaluates the selected coefficient copies
- **AND** publication skips an occupied slot without waiting

#### Scenario: Analyzer draws percentile motion
- **WHEN** captured response magnitudes vary at a frequency
- **THEN** the analyzer draws a feathered band between the lower and upper statistics with a median trace
- **AND** each statistic is separately converted to display dB

#### Scenario: Partial machine retains its specialized interface
- **WHEN** a partial-machine overlay is drawn
- **THEN** it continues to use the legacy `TransferFunction` interface and host-rate frequency conversion
- **AND** other filter response overlays use the sampled interface

### Requirement: Derived UI Snapshots Populated on the Control Thread
The system SHALL populate aggregated visualization state in `PopulateUIState(...)` methods executed on the audio thread once per process frame (`ProcessFrame`, every `x_samplesPerProcessFrame` engine samples), writing into atomic UIState fields that the JUCE thread reads directly. LFO common periods SHALL be computed from current participating loop ratios during this publication path, not per engine sample, and their periodic axes SHALL be updated before their scope banks publish. Filter coefficient histories are sampled separately at the host-sample cadence described above. Current producers include `QuadDelay::PopulateUIState` (continuous master-phase head positions and publication of the common magnitude scope), `AudioBufferBank`/`SampleSource` UIState (waveform min/max buckets, read-head position, and active start/length window), and `PartialMachine::PopulateUIState` (the tracked spectral atom list and frequency-dependent transfer functions).

#### Scenario: Delay envelope uses the common scope
- **WHEN** the quad delay envelope visualizer renders a frame
- **THEN** it samples the accepted magnitude scope through the bounded inverse view and converts filtered magnitude to a symmetric dB envelope, with head positions from `m_delayUIState`

#### Scenario: Partial-machine atoms come from the published list
- **WHEN** the partial-machine spectrum or spatial view renders
- **THEN** the atoms drawn are exactly those in `m_partialMachineUIState`'s last published snapshot, positioned via its frequency-dependent transfer functions

### Requirement: Prepared Secondary Modulator Sampling
A secondary-modulator display reader SHALL receive its full required history interval and a fixed present position before point sampling, choose a fixed LOD for each sample-backed source, and preserve the original absolute sample timeline. Random and note event histories SHALL retain their existing independent retention and prediction behavior.

#### Scenario: Long primary cycle with secondary envelope
- **WHEN** a display requests an envelope over a cycle older than level zero retains
- **THEN** the envelope is sampled consistently from the selected coarser level over the entire trace

### Requirement: Engine-Rate Spectrum Axis
Scope FFTs SHALL use level zero with 1,024 times the engine oversample factor in samples, preserving the previous host-rate bin spacing and observation duration. The frequency axis SHALL retain its physical low edge and extend to engine Nyquist. FFT rendering SHALL preserve narrow peaks that fall between plotted frequencies. Transfer functions and spectral markers SHALL convert their native frequency units to the same axis. Master scopes SHALL observe engine-rate signals before output decimation.

#### Scenario: Engine-rate FFT retains low resolution and exposes ultrasonic content
- **WHEN** the engine runs at 192 kHz with 4,096 FFT samples
- **THEN** adjacent bins remain 46.875 Hz apart and the axis extends from 23.4375 Hz to 96 kHz
- **AND** an ultrasonic tone is visible at the same physical frequency as a matching transfer-function peak

#### Scenario: Host-rate spectral overlay stays aligned
- **WHEN** partial-machine or vocoder state supplies a frequency in cycles per host sample
- **THEN** its marker divides that frequency by the engine oversample factor to use the engine axis
- **AND** evaluation of its host-unit response multiplies axis frequencies by that factor

### Requirement: SIMD Scope Spectra
Scope FFT components SHALL compute channel vectors together, including vector windowing, complex butterflies, magnitudes, and smoothing. Each displayed trio or quad scope SHALL require one grouped transform rather than separate per-lane transforms. Mono spectra SHALL use SIMD across four interleaved sample subsequences. FFT workspaces SHALL be allocated before computation, and all variants SHALL preserve the existing bin spacing and normalization.

#### Scenario: Independent channel spectra
- **WHEN** each channel of a trio or quad contains a different signal
- **THEN** each lane of the SIMD spectrum matches the corresponding independent scalar FFT without cross-channel leakage

#### Scenario: Mono SIMD spectrum
- **WHEN** a mono scope is transformed using four interleaved SIMD lanes
- **THEN** recombination produces the same spectrum as a full-length scalar transform

#### Scenario: Irregular UI frame intervals
- **WHEN** the UI accepts samples in different batch lengths without falling behind the level-zero ring
- **THEN** every lower-level value and FIR phase equals incremental filtering of the same samples
