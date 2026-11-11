## MODIFIED Requirements

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

### Requirement: Derived UI Snapshots Populated on the Control Thread
The system SHALL populate aggregated visualization state in `PopulateUIState(...)` methods executed on the audio thread once per process frame (`ProcessFrame`, every `x_samplesPerProcessFrame` engine samples), writing into atomic UIState fields that the JUCE thread reads directly. LFO common periods SHALL be computed from current participating loop ratios during this publication path, not per engine sample, and their periodic axes SHALL be updated before their scope banks publish. Current producers include `QuadDelay::PopulateUIState` (damping alphas, continuous master-phase head positions, and publication of the common magnitude scope), `AudioBufferBank`/`SampleSource` UIState (waveform min/max buckets, read-head position, and active start/length window), and `PartialMachine::PopulateUIState` (the tracked spectral atom list and frequency-dependent transfer functions).

#### Scenario: Delay envelope uses the common scope
- **WHEN** the quad delay envelope visualizer renders a frame
- **THEN** it samples the accepted magnitude scope through the bounded inverse view and converts filtered magnitude to a symmetric dB envelope, with head positions from `m_delayUIState`

#### Scenario: Partial-machine atoms come from the published list
- **WHEN** the partial-machine spectrum or spatial view renders
- **THEN** the atoms drawn are exactly those in `m_partialMachineUIState`'s last published snapshot, positioned via its frequency-dependent transfer functions
