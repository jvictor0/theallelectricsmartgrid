## MODIFIED Requirements

### Requirement: Global source snapshots and fixed groups
The instrument SHALL define globally unique modulator indices, fixed enum-addressed groups with 15 slots, and parameter-to-group assignments through ForEach metadata. Each track SHALL have Source, Shape, PanMelody, LFO, and Sub groups with three active lanes, for 15 track groups total. Quad SHALL have Delay, Reverb, PartialMachine, and LFO groups with four active lanes. The Global group SHALL retain its existing bindings. Parameters SHALL follow their pages; voice mixer gain and sends SHALL use Shape. Voice groups SHALL bind SourceMod1/SourceMod2 in slots 0/1 and Sub/Voice in slots 12/13. Voice and Quad groups SHALL retain independent ganged random pairs in slots 2/3. Quad effect groups SHALL bind send envelope/audio to slots 0/1 and pre-return-gain envelope/audio to slots 4/5. All four Quad groups SHALL bind final recorded-output envelope/audio to slots 12/13. Quad LFO slots 0/1/4/5 SHALL remain empty; voice and Global bindings SHALL remain unchanged by the mixer additions. All source values and amplitudes SHALL use QuadFloat. Groups and assignments SHALL be instrument metadata, constant across patches. Child encoders SHALL inherit their parent's group, independently of bank mode.

#### Scenario: Reusing a source across groups
- **WHEN** two group slots bind the same global source
- **THEN** both receive the same host-frame source value and amplitude
- **AND** their parameters read their own group's local slot without a per-route global lookup

#### Scenario: Compatible source lane counts
- **WHEN** a parameter is assigned a modulation group
- **THEN** every assigned source in that group SHALL have the same active lane count as the parameter
- **AND** a configuration test SHALL reject mismatches, while source storage remains QuadFloat

#### Scenario: Existing patch loading
- **WHEN** an existing patch with nested depth slot 8 is loaded
- **THEN** slot 8 refers to slot 8 of the parameter's statically assigned group
- **AND** patch loading and serialization remain unchanged

#### Scenario: Unassigned legacy slots
- **WHEN** an existing patch uses a depth in an unassigned group slot
- **THEN** loading preserves that depth and its existing connection behavior
- **AND** the instrument supplies the legacy zero source value and unit amplitude for that slot

### Requirement: Source filtering and group publication
Each global source SHALL declare its downsampling requirement in ForEach metadata. The manager SHALL filter sources requiring downsampling on every engine sample using the existing second-order Butterworth decimator without clamping their filtered values. Voice envelopes, waveform LFOs, SourceMod1/SourceMod2, Sub, Voice, and the seven mixer audio sources SHALL require downsampling. The seven mixer RMS envelope sources SHALL bypass additional downsampling and publish their held host-rate values directly. Ganged random, sheaf, spread, and noise SHALL bypass downsampling and publish their current raw value on each host frame. Modulation noise SHALL be generated only on host samples. Amplitudes SHALL remain unfiltered. On each matrix sample the manager SHALL gather raw values, filtered values, and amplitudes into group snapshots before change detection. Published modulation sources SHALL retain their DSP owners and processing order; the manager SHALL own their downsampling and group publication.

#### Scenario: Shared filtered source
- **WHEN** a source requiring downsampling changes between host samples and is assigned to several groups
- **THEN** its filter advances once per engine sample regardless of group count
- **AND** each group receives its current snapshot at the next matrix sample

#### Scenario: Gesture and blend propagation
- **WHEN** gesture inputs, selection, or the scene blend change
- **THEN** every group receives the change and affected parameters retain existing evaluation behavior

#### Scenario: Direct host-frame publication
- **WHEN** a source is declared not to require downsampling
- **THEN** its group values equal the source value sampled on the host frame with no decimator smoothing
- **AND** modulation noise is held unchanged between host samples

#### Scenario: Voice audio taps
- **WHEN** a voice publishes modulation for the next matrix sample
- **THEN** every voice group on its track receives the source pair in slots 0/1, the sub after its envelope and shaper but before any volume gains in slot 12, and the amp-stage output before track gain in slot 13
- **AND** bipolar stage audio is mapped as 0.5 + 0.5*sample without clipping
- **AND** publication retains the existing previous-engine-sample ordering

## ADDED Requirements

### Requirement: Engine-rate mixer envelope followers
The mixer SHALL own seven independent quad envelope followers: one at each effect send after return-to-send feedback, one at each effect return before return gain and saturation, and one at the mastered output before monitor gain. The final tap SHALL include the recording headroom gain and precede output decimation, so its downsampled audio matches the recorded quad master. Each follower SHALL slew squared input using a one-pole engine-rate time constant, defaulting to 10 ms, and retain independent per-lane audio and mean-square state without clipping. Once per host frame, each follower SHALL take the square root of that state as its RMS envelope and hold it between host frames. Each SHALL own an audio scope writer and an envelope scope writer. These followers SHALL be independent of the existing level/peak/saturation meters. Fourteen distinct globally indexed sources SHALL expose their RMS envelopes directly and audio as 0.5 + 0.5*sample, using the existing causal previous-engine-sample publication order.

#### Scenario: Muted return remains observable
- **WHEN** an effect produces audio while its return gain is zero
- **THEN** its wet audio and envelope modulators still follow that pre-gain output
- **AND** the existing post-return meter and audible return retain their gain-dependent behavior

#### Scenario: Send includes feedback
- **WHEN** another effect return is routed into an effect send
- **THEN** the send follower measures the complete input delivered to that effect

#### Scenario: Squared-amplitude response
- **WHEN** a silent follower receives constant amplitude A for one default time constant
- **THEN** its power state approaches A squared times (1 - exp(-1)) independently in each lane, and its host-rate RMS envelope is the square root of that state
- **AND** changing the sign of A does not change the envelope

#### Scenario: Recorded-output modulation
- **WHEN** monitor volume changes
- **THEN** the final-output modulation tap remains the mastered recording signal including recording headroom gain
- **AND** every Quad group shares the same final-output envelope and audio sources
