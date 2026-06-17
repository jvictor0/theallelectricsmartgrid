## ADDED Requirements

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
