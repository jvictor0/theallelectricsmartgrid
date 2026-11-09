## ADDED Requirements

### Requirement: Consistent modulation group presentation
Every voice and Quad group SHALL label its local random pair RND1 and RND2 and color them with the original slot 2 yellow and slot 3 fuchsia, respectively. The independent random source identity SHALL NOT change its name or color between pages. Source slots 0/1 SHALL retain the original red/mauve colors; other existing populated slots SHALL retain their established colors. Global presentation SHALL remain unchanged.

#### Scenario: Moving between page groups
- **WHEN** the user moves between Source, Shape, PanMelody, LFO, Sub, or Quad page groups
- **THEN** the independent randoms in slots 2/3 still display RND1/RND2 in yellow/fuchsia
- **AND** the zero-based even and odd random generators consistently use those respective names and colors

### Requirement: Audio modulation time histories
Sub and Voice modulation encoders SHALL use time-domain visualizers backed by the existing Sub and PostAmp scopes. Dual waveshaping modulation SHALL reuse VCO1/VCO2 scopes. Sub scope capture SHALL occur after its envelope and shaper but before frequency compensation, sub gain, and track gain. Voice modulation SHALL use the existing PostAmp tap before track gain. These shared scopes SHALL store bipolar audio once and map it as 0.5 + 0.5*sample when read as modulation, without clipping. Source VPS and sub oscillator wraps SHALL retain their lane-specific fractional cycle starts, and PostAmp SHALL retain its AHD boundaries. Only the physical source's internal envelope SHALL add a separate scope history; physical noise SHALL use the existing noise visualization. All histories SHALL use the existing UI publication and LOD pipeline.

#### Scenario: Modulation scope uses the correct gain stage
- **WHEN** a Sub or Voice modulation encoder is displayed
- **THEN** Sub shows the enveloped and shaped sub before every volume gain, while Voice shows the amp output before track gain
- **AND** bipolar samples are mapped as 0.5 + 0.5*sample when reading their shared audio histories, without clipping

### Requirement: Source modulation visualizer configuration
Source modulation visualizers SHALL be assigned through a separate ForEach configuration indexed by source machine and source modulator. Dual waveshaping SHALL use the existing LFO time-domain visualizer for both outputs. Physical modeling SHALL use the noise visualizer for its filtered-noise output and the envelope time-domain visualizer for its internal envelope. Sample and Thru SHALL have no source modulation visualizer. Lookup SHALL follow the source's owning trio and current published machine, independently of the actively navigated trio.

#### Scenario: Source machine changes
- **WHEN** a trio changes from dual waveshaping to physical modeling
- **THEN** every one of its groups switches source slot 0 to noise and slot 1 to the envelope view
- **AND** changing to Sample or Thru removes those source views without affecting Sub or Voice views

#### Scenario: Inactive trio retains its own source views
- **WHEN** an encoder belongs to a different trio from the active navigation selection
- **THEN** its source view and historical samples follow its owning trio's machine and lanes

## MODIFIED Requirements

### Requirement: Per-Voice Sub Meter and Spectrum
The metering strip SHALL split each voice's original meter footprint into four equal bars ordered left to right: sub reduction, sub level, main level, main reduction. Both level bars SHALL use the voice color, and both reduction bars SHALL use red. Sub levels and reductions SHALL come from the corresponding mono lane's independent saturation meter before monitoring. The source, return, and master meters SHALL retain their original positions and widths. The Sub page SHALL show the standard audio scope in its first visualizer slot and a spectrum analyzer spanning the other two slots. Both SHALL display the active trio's shaped, enveloped, DC-blocked sub audio before frequency compensation, sub gain, and track gain, using the shared all-voices or individual-voice selection and voice colors. The analyzer SHALL omit the main voice filter-response overlay.

#### Scenario: Each sub is metered beside its own voice
- **WHEN** a voice's sub saturates while its main signal remains quiet
- **THEN** the leftmost reduction bar and adjacent sub level reflect that mono lane
- **AND** the main level and rightmost reduction bar reflect only the main voice signal

#### Scenario: Sub views follow the selected trio
- **WHEN** the Sub page is selected and the active trio changes
- **THEN** the scope and analyzer read that trio's sub audio channels, including release tails
- **AND** main voice audio and main filter settings do not alter the displayed sub waveform or spectrum

#### Scenario: Time and frequency views share the visualizer row
- **WHEN** the Sub page is selected
- **THEN** the time-domain scope fills slot 0 and the analyzer fills slots 1 and 2
- **AND** selecting a voice in either view updates the shared voice selection for both

#### Scenario: Sub waveform stays synchronized to its oscillator
- **WHEN** a sub oscillator wraps its phase
- **THEN** its scope SHALL record a cycle start with that lane's fractional top offset in engine samples
- **AND** scope decimation preserves the fractional crossing independently of main-source and envelope triggers


## ADDED Requirements

### Requirement: Mixer follower visualizers
Each mixer audio modulator SHALL show a configurable trailing scope window, defaulting to 100 ms, ending at the latest accepted scope sample without requiring cycle or envelope markers. Each mixer envelope modulator SHALL show four lane-colored VU needles using the latest accepted RMS envelope history, converted with 20*log10 for display. These VUs SHALL share drawing math with existing meters but SHALL read only the new follower data, without adding peak hold or deriving envelopes from existing meter values. Both histories SHALL use the existing scope publication and LOD pipeline. The new send/return/final-output histories SHALL coexist with existing audio analyzers and meters.

#### Scenario: Continuous recent audio
- **WHEN** a mixer audio source has no cycle markers
- **THEN** its scope still displays the most recent configured time window and advances with accepted samples

#### Scenario: Follower VU publication
- **WHEN** a new envelope sample has been written but not published and accepted
- **THEN** the VU continues displaying the previously accepted RMS envelope
- **AND** accepting the new sample updates the corresponding lane needle independently of existing meters
