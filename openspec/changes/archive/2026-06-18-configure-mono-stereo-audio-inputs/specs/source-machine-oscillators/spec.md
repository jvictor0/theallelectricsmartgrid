## ADDED Requirements

### Requirement: Source Mixer Input Configuration
The SourceMixer SHALL expose four configured external input sources. Each source SHALL have a DSP-owned configuration object that includes whether the source is Mono or Stereo. The SourceMixer SHALL expose the source color used by UIState consumers.

When source inputs are populated from the JUCE audio input buffer, a Mono source SHALL populate its left source lane from the configured input and SHALL populate its right source lane with silence for voice routing. A Stereo source SHALL populate both its left and right source lanes from the configured stereo input pair. The configured stereo input pair SHALL map source `i` to left physical input `2 * i` and right physical input `2 * i + 1`. Source colors SHALL be read from SourceMixer DSP code or SourceMixer UIState by config controls, meters, and source mixer visualizers.

When source lanes are sent to the quad mixer, a Mono source SHALL preserve the existing centered X/Y panning for both lanes. A Stereo source SHALL set each lane's X coordinate from bit 0 of `source * 2 + lane + 1` and SHALL set each lane's Y coordinate from bit 1 of `source * 2 + lane + 1`, so each stereo pair is assigned to opposite quad corners.

When source trim is sent to K-Mix, the system SHALL map each source index to two physical trim channels. Source `i` SHALL set trim for physical even channel `2 * i` from the source's current trim value. If source `i` is Mono, physical odd channel `2 * i + 1` SHALL be trimmed to zero. If source `i` is Stereo, physical odd channel `2 * i + 1` SHALL receive the same current trim value.

#### Scenario: Four external source slots are available
- **WHEN** the source mixer input state is initialized
- **THEN** it exposes four source configuration objects and four source signal slots

#### Scenario: Mono source has a silent right lane
- **WHEN** source 2 is configured as Mono and its left input receives audio
- **THEN** source 2 left lane contains the input audio
- **AND** source 2 right lane contains silence

#### Scenario: Stereo source exposes both lanes
- **WHEN** source 3 is configured as Stereo and its configured stereo input pair receives different left and right audio
- **THEN** source 3 left lane contains the left input audio
- **AND** source 3 right lane contains the right input audio

#### Scenario: Source color comes from DSP-owned state
- **WHEN** a source mixer meter or config-grid source button requests the color for source 2
- **THEN** the color is read from SourceMixer DSP code or SourceMixer UIState for source 2

#### Scenario: Stereo source lanes feed independent mixer input meters
- **WHEN** source 3 is configured as Stereo and its left and right lanes have different levels
- **THEN** source 3's left and right lanes are sent to separate mixer input meter slots

#### Scenario: Mono source lanes stay centered in the quad mixer
- **WHEN** source 1 is configured as Mono and its lanes are sent to the quad mixer
- **THEN** both source lanes keep the existing centered X/Y panning

#### Scenario: Stereo source lanes use incremented source input index bits for quad panning
- **WHEN** source 2 is configured as Stereo and its lanes are sent to the quad mixer
- **THEN** each source lane sets X from bit 0 of `source * 2 + lane + 1`
- **AND** each source lane sets Y from bit 1 of `source * 2 + lane + 1`
- **AND** the source's left and right lanes occupy opposite quad corners

#### Scenario: Mono source trim mutes the unused physical input
- **WHEN** source 1 is configured as Mono and its current trim value is sent to K-Mix
- **THEN** physical input channel 2 receives the current trim value
- **AND** physical input channel 3 receives zero trim

#### Scenario: Stereo source trim applies to both physical inputs
- **WHEN** source 2 is configured as Stereo and its current trim value is sent to K-Mix
- **THEN** physical input channel 4 receives the current trim value
- **AND** physical input channel 5 receives the current trim value

## MODIFIED Requirements

### Requirement: Thru Source Machine
The Thru source machine SHALL pass the external `SourceMixer` source channel assigned by the voice's source assignment through the voice, upsampling the source's base-rate u-block by 4 into the oversampled domain so the voice's filter and amp sections shape outside audio.

A source assignment SHALL identify one of three states: Silent, Source Left, or Source Right. A Silent assignment SHALL produce zero source output. A Source Left assignment SHALL read the selected source's left lane. A Source Right assignment SHALL read the selected source's right lane, which is silence when the selected source is configured as Mono.

#### Scenario: External mono audio is forwarded into the voice chain
- **WHEN** a voice in Thru mode processes a u-block and its assignment is Source Left for a Mono source carrying external input audio
- **THEN** the voice's source output equals the 4x-upsampled left source block, which then feeds the voice's filter machine whose cutoff anchor in Thru mode is specified in filter-system

#### Scenario: External stereo right audio is forwarded into the voice chain
- **WHEN** a voice in Thru mode processes a u-block and its assignment is Source Right for a Stereo source carrying right-channel external input audio
- **THEN** the voice's source output equals the 4x-upsampled right source block, which then feeds the voice's filter machine

#### Scenario: Silent assignment produces silence
- **WHEN** a voice in Thru mode processes a u-block and its source assignment is Silent
- **THEN** the voice's source output is silence before the filter machine
