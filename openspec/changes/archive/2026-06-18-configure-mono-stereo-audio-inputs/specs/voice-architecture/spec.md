## ADDED Requirements

### Requirement: Trio Source Channel Assignment
For each trio, the system SHALL derive voice source assignments from selected source channels without infill. A selected Mono source SHALL contribute one Source Left assignment. A selected Stereo source SHALL contribute two assignments, Source Left followed by Source Right. The first assignment SHALL be applied to the first voice in the trio, the second assignment to the second voice, and the third assignment to the third voice. If fewer than three assignments exist, all remaining voices in the trio SHALL receive Silent source assignments.

The selected source-channel count for a trio SHALL NOT exceed three. When selecting a source or changing a selected source from Mono to Stereo would exceed three selected channels, the system SHALL unselect sources or revert the width change deterministically so the selected source-channel count remains at most three, then rerun propagation for affected trios.

#### Scenario: Stereo source consumes two trio voices
- **WHEN** only source 1 is selected for a trio and source 1 is configured as Stereo
- **THEN** the trio's first voice is assigned source 1 Left
- **AND** the trio's second voice is assigned source 1 Right
- **AND** the trio's third voice is assigned Silent

#### Scenario: Mono source leaves the remaining voices silent
- **WHEN** only source 2 is selected for a trio and source 2 is configured as Mono
- **THEN** the trio's first voice is assigned source 2 Left
- **AND** the trio's second and third voices are assigned Silent

#### Scenario: Multiple selections fill voices without repetition
- **WHEN** source 0 is selected as Mono and source 1 is selected as Stereo for a trio
- **THEN** the trio's voices are assigned source 0 Left, source 1 Left, and source 1 Right in source-selection order
- **AND** no selected source is repeated to fill additional voices

#### Scenario: Width change enforces the three-channel limit
- **WHEN** three Mono sources are selected for a trio and one selected source is changed to Stereo
- **THEN** source selection is adjusted or the width change is reverted so no more than three source channels remain selected
- **AND** the trio's voice assignments are propagated from the adjusted selected source channels

## MODIFIED Requirements

### Requirement: Voice Configuration
Each voice SHALL be configured by a `SquiggleBoyVoiceConfig` carrying: the source machine selection (`DualWaveShapingVCO`, `PhysicalModeling`, `Thru`, or `Sample`), the filter machine selection (`Ladder4Pole` or `SVF2Pole`), a source assignment, and an `AudioBufferBank` pointer used by the Sample source machine. The source assignment SHALL be able to represent Silent, Source Left for a source index, and Source Right for a source index. Defaults are `DualWaveShapingVCO`, `Ladder4Pole`, source 0 Left for compatibility, and a null buffer bank. The filter and amp sections read the active configuration through a shared pointer in the voice input, selecting the DSP path per micro-block.

#### Scenario: Filter machine selection switches the DSP path
- **WHEN** a voice's config has `m_filterMachine = Ladder4Pole`
- **THEN** the filter section processes through the 4-pole ladder low-pass followed by the 4-pole SVF high-pass
- **AND** with `m_filterMachine = SVF2Pole` it instead processes through the saturated 2-pole SVF low-pass/high-pass chain

#### Scenario: Thru source pins the filter base frequency
- **WHEN** a voice's config has `m_sourceMachine = Thru`
- **THEN** the filter section overrides the VCO base frequency with 80 Hz normalized to the sample rate so external audio is filtered against a fixed reference

#### Scenario: Silent source assignment is representable
- **WHEN** a trio source selection leaves a voice unassigned
- **THEN** that voice's config can store a Silent source assignment for the Thru source machine to render as silence
