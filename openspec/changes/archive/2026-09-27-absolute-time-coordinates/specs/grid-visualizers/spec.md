## ADDED Requirements

### Requirement: Forward Sequencer Melody Roll
The Melody role SHALL render a full-width, three-column view with time on the horizontal axis and pitch on the vertical axis. It SHALL select the active trio and the same voice selector used by basic scopes: all unmuted trio voices, or the explicitly selected voice even if muted. Each frame SHALL call PreProcess once and Process for the displayed voices plus a gate-reference voice if needed. It SHALL display the containing global cycle when the period is below 1024 ticks, otherwise use the bounded cache window. Rendering SHALL check point availability while the cache fills and SHALL perform pitch evaluation outside the audio thread.

#### Scenario: Display follows the active trio
- **WHEN** the active trio changes while all voices are selected
- **THEN** only that trio's unmuted voices are shown in their voice colors
- **AND** the gate ruler uses that trio's read-bit lens

#### Scenario: Large loops remain bounded
- **WHEN** the global period exceeds 1024 ticks
- **THEN** the displayed window contains at most 1024 ticks around the current position
- **AND** unavailable cache points are omitted until computed

### Requirement: Melody Pitch Candidates and Choice Mapping
The pitch axis SHALL span the minimum and maximum arp bounds of the displayed voices, converting Percentile bounds through each displayed time slice's pitch mapping. ClosestModOne candidates SHALL repeat in every visible octave. Percentile candidates SHALL retain their original register and add only nonnegative octave copies through the largest displayed whole-percentile maximum. Percentile choice positions SHALL interpolate between the shared chooser's sorted rank positions i/N, retain duplicate pitches, hold the last rank until the next integer, and add the whole-percentile octave offset. Axis bounds SHALL include extrema at integer-percentile jumps. Muted trio-color horizontal segments SHALL show candidates; bright voice-color segments SHALL show the raw full-chooser pitch. These raw pitches SHALL remain distinct from downstream octave/spread, unison, trigger suppression, and vocoder processing.

#### Scenario: Percentile spans an octave boundary
- **WHEN** the visible arp range crosses an integer percentile
- **THEN** the axis includes extrema on both sides of the octave jump
- **AND** the choice trace uses interpolated pitches while the played-pitch segment uses the discrete chooser result

#### Scenario: ClosestModOne repeats pitch classes
- **WHEN** a candidate belongs to the current time slice in ClosestModOne mode
- **THEN** its pitch class is drawn in every octave intersecting the axis range

### Requirement: Melody Choice Curves and Gate Rulers
The dashed choice trace SHALL hold each global tick's choice for its first half, then cosine-interpolate toward the next tick's choice over its second half. It SHALL connect only within the same forward motive occurrence, retaining held notes over rests and breaking across distinct reset-cycle occurrences. Each motive SHALL be dashed separately. Read-bit gates SHALL appear as merged rounded rectangles, muted gray for high and black for low, with lower-index loops above higher-index loops. Faint vertical separators SHALL mark only changes in visible read-bit gate values and fade when densely packed. Faint horizontal guides SHALL mark whole octaves. The panel SHALL use a black background with margins and SHALL omit text and separate min/max curves.

#### Scenario: Gate stays high across consecutive positions
- **WHEN** a read bit has the same gate value at consecutive cached positions
- **THEN** those positions form one continuous gate rectangle
- **AND** no vertical separator is added merely because the global tick advances

#### Scenario: A new motive starts
- **WHEN** adjacent tick choices belong to different motive occurrences
- **THEN** the dashed trace ends and restarts without an interpolated connection

### Requirement: Continuous Melody Playhead
The playhead SHALL use the published absolute modulated global phase and period to derive an integer tick and within-tick fraction from one phase read. Cache/window selection SHALL use that integer tick and drawing SHALL include its fractional offset. Live phase motion SHALL NOT invalidate copied configuration or clear sequence caches.

#### Scenario: Phase advances within one tick
- **WHEN** the published phase advances without crossing an integer global tick
- **THEN** the playhead moves continuously within the tick
- **AND** the cached sequence remains valid
