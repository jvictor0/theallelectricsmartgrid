## MODIFIED Requirements

### Requirement: Interleaved Circular Buffer Indexing
The system SHALL use ScopeWriterGeneric<Size, NumScopes> with six vector rings per scope, each holding 0.25 seconds at the engine sample rate. All production scopes, including control signals, SHALL store every engine sample in one bank per vector width (one, three, or four). Higher levels SHALL be decimated by four successively with 17-tap FIR filters on the UI thread during acceptance. The audio thread SHALL write only level zero. Logical coordinates SHALL remain level-zero samples, with FIR delay and decimation phase handled internally. Vector storage SHALL interleave all lanes of one scope sample. Writes and AdvanceIndex occur at engine rate, with production base decimation equal to one. All banks SHALL share one absolute engine-sample origin. GetLod(samplesBack) SHALL select the finest retained level covering the requested lookback; history reads SHALL reject interpolation endpoints outside the accepted bounds. Capacity SHALL be independent of other scopes in the bank.

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

### Requirement: Lock-Free DSP Writes
The system SHALL provide vector-width-specific scope holders whose Write takes a whole MultiChannelNumber vector. DSP producers SHALL write vectors without locks or allocation; start/end markers remain per lane. Holders SHALL retain inert null behavior and IsWriteSample. Scalar UI reads SHALL use a common read interface independently of writer template parameters.

#### Scenario: Quad write fans out to four voice slots
- **WHEN** a quad holder writes a `QuadFloat`
- **THEN** components 0-3 are written to the four lanes of the holder's scope at the current write index

#### Scenario: Null holder is inert
- **WHEN** a default-constructed vector scope holder receives `Write` or `RecordStart`
- **THEN** no buffer access occurs and no fault is raised

### Requirement: Atomic Publish Boundary
The audio thread SHALL publish its completed level-zero position and pending per-stream cycle-marker counters after writing the data. At the beginning of each UI paint frame, before drawing, the UI SHALL capture a published boundary for each scope bank, build its lower levels from the newly accepted samples, and update its accepted position. All waveform, FFT, and secondary-modulator reads SHALL use these accepted positions throughout the frame. Cycle start/stop metadata SHALL be snapshotted against the accepted boundary. Reaccepting the same position SHALL not rerun the cascade. Publication and acceptance do not pin level-zero slots; UI catch-up SHALL index the circular buffer directly even if the producer has overwritten samples during a long UI stall.

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

### Requirement: Cycle-Aware Display Sampling
The system SHALL preserve ScopeReader's cycle and transfer calculations in original sample coordinates. It SHALL select one LOD covering both the current cycle and any borrowed previous-cycle segment before sampling, and use that LOD for the full trace. ScopeReaderFactory SHALL retain the current voice/scope context. Strict historical reads reject unavailable history; display sampling holds the latest selected-level value at its delayed live edge. Readers SHALL be empty when required history exceeds all levels or required markers are unavailable. FFT readers SHALL explicitly use level zero.

#### Scenario: Mid-cycle trace borrows the previous cycle
- **WHEN** a reader is created while the current cycle has fewer samples than the previous cycle's length
- **THEN** screen positions before the transfer point sample the current cycle and positions after it sample the previous cycle's corresponding region

#### Scenario: Stale stream yields an empty trace
- **WHEN** the distance from cycle start to the accepted index exceeds all retained levels
- **THEN** the reader marks itself empty and every `Get(sample)` returns 0

## ADDED Requirements

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
