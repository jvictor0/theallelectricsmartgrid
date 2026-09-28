# Grid Visualizers Specification

## Purpose
The grid visualizer system (`JUCE/SmartGridOne/Source/SmartGridOneVisualizerMain.hpp`, `SmartGridOneMainVisualizerComponent.hpp`, `ForEachSmartGridOneVisualizer.hpp`, `ForEachModulationVisualizer.hpp`) renders DSP data — scopes, spectra, meters, envelopes, spatial views — into a 24x18 cell display grid. Visualizers are lightweight render closures registered through X-macro lists, selected by the active encoder bank and filtered by each track's active source machines. They read exclusively from published UIState data: ScopeWriter ring buffers, meter readers, and transfer-function snapshots, as specified in ui-visualization-pipeline. Bank selection itself belongs to encoder-parameter-system; voice and source-machine state to source-machine-oscillators and voice-architecture.

## Requirements

### Requirement: Visualizer Render-Closure Abstraction
Visualizers SHALL implement the `SmartGridOneMainVisualizerComponent` interface: a pure virtual `Draw(juce::Graphics&, juce::Rectangle<int> bounds)` plus an optional `OnClick(const juce::MouseEvent&)` override that defaults to a no-op. Visualizers are not JUCE Components; the single hosting component `SmartGridOneVisualizerMain` owns them, sets the clip region and origin before each call, and passes block-local bounds anchored at (0, 0).

#### Scenario: Draw receives block-local bounds
- **WHEN** `SmartGridOneVisualizerMain` dispatches paint to a visualizer assigned to a block
- **THEN** the graphics context is clipped to that block's rectangle, its origin is translated to the block's top-left, and `Draw` receives `(0, 0, blockWidth, blockHeight)`

#### Scenario: Visualizer without OnClick ignores clicks
- **WHEN** a click lands on a visualizer that does not override `OnClick`
- **THEN** the base-class no-op runs and no state changes

### Requirement: X-Macro Registration with Bank Tagging
The system SHALL register every main visualizer in `ForEachSmartGridOneVisualizer.hpp` as an `F(name, bank, block, constructor, sourceMachineFlags)` entry, where bank is a `SmartGridOneEncoders::Bank` value (Source, FilterAndAmp, PanningAndSequencing, VoiceLFOs, Delay, Reverb, PartialMachine, QuadLFOs, TheoryOfTime, Mastering, Inputs, DeepVocoder). `SmartGridOneVisualizerMain` instantiates one member per entry at construction and, on each paint, draws only the entries whose bank tag equals the currently selected encoder bank.

#### Scenario: Bank selection switches the drawn set
- **WHEN** `NonagonWrapper::GetSelectedEncoderBank()` returns `Bank::Delay`
- **THEN** only entries tagged Delay (the delay quad analyzer and quad delay envelope view) are dispatched, and Source-tagged entries such as the VCO scopes are not drawn

#### Scenario: Registration assigns flags at construction
- **WHEN** `SmartGridOneVisualizerMain` is constructed
- **THEN** each X-macro entry's constructor runs and the entry's `m_sourceMachineFlags` member is set from the macro's flags argument

### Requirement: Source-Machine Filtering
The system SHALL draw a visualizer only when its `VoiceMachine::SourceMachineFlags` match a voice in the active track: `ShouldShowVisualizer` loads `m_activeTrack` from the UIState, scans the track's voices' `m_voiceSourceUIState[voiceIx].m_sourceMachine` atomics, and requires `ShouldShowForMachine(machine)` to return true for at least one. Entries tagged `All()` always pass; `DualVCOOnly()`, `SampleOnly()`, and `PhysicalModelingOnly()` restrict views to their source machine.

#### Scenario: Sample-only view hidden without a Sample voice
- **WHEN** the Source bank is selected and no voice in the active track reports `SourceMachine::Sample` in its UIState
- **THEN** the SampleTrioWaveform visualizer is not drawn

#### Scenario: Matching voice enables the view
- **WHEN** at least one active-track voice's `m_sourceMachine` atomic reads `Sample`
- **THEN** `ShouldShowVisualizer` returns true for the SampleTrioWaveform entry and it is drawn into block 1

### Requirement: 24x18 Cell Grid Layout
The system SHALL lay out visualizers on a 24x18 cell grid: four 8x8 blocks numbered 0-3 at cell positions (0,0), (0,8), (8,8), and (16,8); a 24x8 full-width strip at (0,8) for entries registered with block -1 (e.g. MelodyRoll); twelve 2x2 modulation panels (slots 0-11) along the bottom row at y = 16; and a 16x2 metering strip at (8,5). Encoder regions occupy 8x2 areas at (8,0), (16,0), (8,3), and (16,3); the upper-right rectangle from (8,0) spanning 16x5 cells is excluded from the paint clip and from click dispatch.

#### Scenario: Block index maps to fixed cell position
- **WHEN** a visualizer registered with block 2 is drawn with cell size s
- **THEN** its bounds start at grid offset plus (8s, 8s) and span 8s x 8s pixels

#### Scenario: Full-width entry spans the strip
- **WHEN** an entry registered with block -1 is dispatched
- **THEN** it is drawn into the 24-cell-wide, 8-cell-tall rectangle at cell position (0, 8)

#### Scenario: Encoder region is masked
- **WHEN** the main component paints or receives a mouse-down inside the rectangle covering cells x in [8, 24), y in [0, 5)
- **THEN** no visualizer pixels are painted there and no visualizer `OnClick` is dispatched for that position

### Requirement: Responsive Sizing Preserving Aspect Ratio
The system SHALL compute an integer cell size from the available component bounds so the 24:18 grid aspect ratio is preserved: when the component is wider than 24:18 the cell size derives from height and the grid is centered horizontally; otherwise it derives from width and the grid is centered vertically. All block, strip, meter, and panel rectangles are multiples of this cell size.

#### Scenario: Wide window centers the grid horizontally
- **WHEN** the component aspect ratio exceeds 24/18
- **THEN** `m_cellSize` equals height/18, the grid width is 24 times the cell size, and `m_gridX` offsets the grid to the horizontal center while `m_gridY` is 0

### Requirement: Click Dispatch in Block-Local Coordinates
The system SHALL forward mouse-down events to the visualizers of the selected bank whose block (or full-width strip, or modulation panel) contains the click position, translating the event into block-local coordinates before calling `OnClick`. Clicks inside the excluded encoder region are dropped before any dispatch.

#### Scenario: Click is translated to block-local space
- **WHEN** a mouse-down lands inside block 3 while a bank with a block-3 visualizer is selected
- **THEN** that visualizer's `OnClick` receives the event position minus the block's top-left corner

### Requirement: Modulation Panel Visualizers
The system SHALL register modulation visualizers in `ForEachModulationVisualizer.hpp` as `G(name, slot, constructor, mode, color)` entries and draw an entry only when the mode for the selected encoder bank (`GetModeForEncoderBank`) equals the entry's mode tag. Each drawn panel occupies its 2x2 slot on the bottom row, inset behind a rounded border stroked in the entry's `SmartGrid::Color` (with `Color::Off` substituted by `Color::Grey`), and receives clicks within its slot bounds.

#### Scenario: Mode mismatch hides the panel
- **WHEN** the selected bank's mode differs from a modulation entry's mode tag
- **THEN** that entry is neither painted nor offered click events

#### Scenario: Off-colored border falls back to grey
- **WHEN** a modulation entry registered with `SmartGrid::Color::Off` is drawn
- **THEN** its border is stroked with `SmartGrid::Color::Grey` and `Draw` receives the inset panel bounds

### Requirement: Published UIState Data Sources
Visualizers SHALL read display data only from published UI state, never from live DSP objects: waveform views sample ScopeWriter ring buffers (audio, control, quad, source-mixer, mono, mono-audio) cycle-aware via the writers' published cycle markers; analyzers compute `WindowedFFT`/`QuadWindowedFFT` windows ending at each writer's published index and overlay transfer-function curves from UIState objects (voice filter response, damping filters, partial-machine per-speaker functions); envelope, waveform-bucket, and atom views read derived snapshots (`m_delayUIState`, sample-source and buffer-bank UIState, `m_partialMachineUIState`); meters read meter readers such as `m_voiceMeterReader`. The capture and publish semantics of these sources are specified in ui-visualization-pipeline.

#### Scenario: Scope views segment by cycle markers
- **WHEN** a ScopeComponent renders a voice's trace
- **THEN** the samples it reads lie between cycle start/end indices recorded by the ScopeWriter and at or before the writer's atomically published index

#### Scenario: Analyzer overlay uses transfer-function values
- **WHEN** an analyzer draws a filter response overlay for a voice
- **THEN** each curve point equals the UIState transfer function's `FrequencyResponse(freq)` value for the sampled frequency, not a value queried from the audio-thread DSP object

### Requirement: Metering Strip
The system SHALL draw a metering component into the 16x2 cell rectangle at grid position (8, 5) on every paint, independent of the selected bank, by clipping to the meter bounds and delegating to a `MeteringComponent` constructed against the UIState.

#### Scenario: Meter draws regardless of bank
- **WHEN** the selected encoder bank changes between any two banks
- **THEN** the metering component still paints into the rectangle at cells (8, 5) through (23, 6)

### Requirement: Source Mixer Stereo Meter Rendering
The metering strip and source mixer reduction visualizer SHALL render configured source mixer inputs without increasing the total horizontal footprint allocated to source mixer meters. A Mono source SHALL keep the existing two-slot layout with its meter and reduction horizontally offset. A Stereo source SHALL split the same source group into four half-width horizontal slots ordered left reduction, left meter, right meter, and right reduction.

Quadraphonic return meters SHALL retain their existing channel widths and layout; source mixer stereo metering SHALL NOT shrink or rearrange quad return meters.

#### Scenario: Mono source keeps the existing source meter layout
- **WHEN** source 0 is configured as Mono and receives input audio on its left lane
- **THEN** the source mixer meter for source 0 shows its meter in the original meter slot
- **AND** its reduction in the original horizontally offset reduction slot

#### Scenario: Stereo source meters independent horizontal lanes
- **WHEN** source 1 is configured as Stereo and receives different left and right input levels
- **THEN** the source mixer meter for source 1 shows its left reduction in the first half-width slot
- **AND** left-lane activity in the second half-width slot
- **AND** right-lane activity in the third half-width slot
- **AND** its right reduction in the fourth half-width slot

#### Scenario: Source reductions split left and right
- **WHEN** source 2 has different left and right reduction values
- **THEN** the reduction display for source 2 draws the left reduction in the first half-width slot of source 2's meter group
- **AND** the right reduction in the fourth half-width slot of source 2's meter group

#### Scenario: Quad meters keep their footprint
- **WHEN** source mixer stereo meters are drawn
- **THEN** quadraphonic return meters keep the same slot widths and positions they used before source mixer stereo metering was added

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
