## ADDED Requirements

### Requirement: Separate Panned and Mono Recording Taps
Performance recording SHALL capture every panned mixer input as a `panned_mono` track and each declared `m_monoIn` lane as a separate `mono` track. For ordinary mixing, recorded panned audio SHALL equal input times fader gain times the existing shared per-input meter reduction, with the current pan coordinates; recorded mono audio SHALL equal the mono lane times that same reduction without the voice fader gain. Recording SHALL remain independent of monitor flags and SHALL NOT change the live mix, sends, meters, or saturation processing.

#### Scenario: Voice and sub share their original reduction
- **WHEN** a voice and its centered mono lane jointly drive the per-voice saturation
- **THEN** both recorded contributions use the reduction already computed for their combined level
- **AND** panning and summing the two unquantized contributions reproduces the former recorded quad stem

#### Scenario: Monitor-disabled input remains recordable
- **WHEN** a source monitor flag is false while the source has audio
- **THEN** its panned-mono recording track still captures its post-gain, post-reduction signal
- **AND** its contribution to live output buses retains the existing monitor behavior

### Requirement: Quad Return Recording Taps
Performance recording SHALL capture the delay, reverb, and Partial Machine returns as three stable quad tracks using each return's post-gain, post-saturation contribution to the quad bus, preserving the engine's q0 through q3 order.

#### Scenario: Return recording matches the quad contribution
- **WHEN** an effect return is scaled and saturated in `ProcessReturns`
- **THEN** its recorded track contains that resulting quad value after the specified PCM24 quantization
- **AND** the return is not processed through saturation a second time for recording

### Requirement: Independently Recorded Mastered Outputs Before Listening Volume
Performance recording SHALL capture both `DualMasteringChain` output formats directly after their respective mastering chains and before `SquiggleBoy` applies global master volume. The tracks SHALL have unique `master_quad` and `master_stereo` roles and `post_mastering_pre_master_volume` tap metadata. The stereo master SHALL come from the parallel stereo mastering chain. Both SHALL be captured regardless of hardware output selection. The global master-volume control SHALL affect listening level without affecting recorded master samples.

#### Scenario: Listening volume changes during recording
- **WHEN** otherwise identical processing runs use different global master-volume values
- **THEN** the recorded quad and stereo PCM24 master samples are identical between runs
- **AND** the live outputs reflect the different listening volumes

#### Scenario: Mastering controls remain audible in the recording
- **WHEN** mastering EQ or mastering-chain gain changes
- **THEN** each master recording contains the corresponding output of its own changed mastering chain
- **AND** stereo extraction does not fold down the recorded quad or recompute mastering

#### Scenario: Both output formats are always available
- **WHEN** the device is configured for stereo or for quad playback
- **THEN** the recording contains both master track definitions and their frame-aligned samples, subject to silent-block omission

### Requirement: One Complete Recording Frame per Mixer Sample
The mixer SHALL preserve its `ProcessInputs`/`ProcessReturns` processing sequence and existing start/stop/toggle control shape while replacing flat WAV-channel writes with typed track submissions. `ProcessReturns` SHALL complete exactly one recording frame after submitting both mastered outputs. Frame storage SHALL start with zero values so skipped submissions cannot retain data from earlier frames. Noise mode SHALL preserve skipped input/return recording behavior while still recording mastered outputs and advancing the timeline.

#### Scenario: Split and combined mixer calls
- **WHEN** a caller processes one sample using `Process` or a paired `ProcessInputs` and `ProcessReturns`
- **THEN** exactly one complete frame is submitted with matching input, return, and master timestamps

#### Scenario: Noise mode does not corrupt frame layout
- **WHEN** a recording enters noise mode or an input becomes unused
- **THEN** skipped input/return samples are zero for the affected frames and both masters remain recorded
- **AND** subsequent frames retain their correct track identities and contiguous positions
