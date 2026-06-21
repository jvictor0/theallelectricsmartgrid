## MODIFIED Requirements

### Requirement: Spectral Atom Tracking Dynamics
The system SHALL track atoms across hops: each existing atom searches the new analysis peaks within its frequency-dependent density window (`m_omegaDensity`) and merges with the preferred match, slewing its synthesis magnitude toward the analysis magnitude with separate attack (`slewUpAlpha`, 0.01-2 s) and decay (`slewDownAlpha`, 0.01-10 s) rates and gliding its synthesis frequency toward the analysis frequency at the portamento rate (0.01-2 s). Within the density window, preferred-match selection SHALL preserve organic-over-synthetic priority and otherwise score candidates with theta = analysisMagnitude * max(0, 1 - distance / density), where distance is the absolute linear frequency difference from the tracked atom and density is the per-side density-window radius in the same linear frequency units. Distance SHALL be measured in Hz-equivalent cycles per sample, not volt-per-octave or `FrequencyToLinear` space. Matched peaks within the density window are consumed so they cannot spawn duplicates; remaining peaks above the death magnitude (`x_deathMag` = 1e-5) are born as new atoms whose magnitude slews up from zero. Unmatched atoms decay toward zero at the decay rate. Atoms whose synthesis magnitude falls below 1e-5 die and are removed.

#### Scenario: Atom survives a one-hop dropout
- **WHEN** a tracked atom's partial disappears from the analysis for one hop and the decay time is long
- **THEN** the atom is not deleted; its synthesis magnitude slews toward zero at the decay rate and recovers when the partial reappears within the density window

#### Scenario: Density window prunes near-duplicates
- **WHEN** two analysis peaks fall within one atom's `m_omegaDensity` window
- **THEN** the atom merges with the peak that has the strongest preferred-match theta after applying organic-over-synthetic priority, and the other peak is consumed, so no second atom is created at nearly the same frequency

#### Scenario: Nearby continuation can beat a farther louder peak
- **WHEN** an existing organic atom searches two organic analysis peaks inside its density window
- **AND** one peak is closer to the atom's current frequency while the other peak has slightly higher raw magnitude
- **THEN** the atom merges with the closer peak if the closer peak's theta is greater than the farther peak's theta

#### Scenario: Boundary candidates have no distance preference score
- **WHEN** an analysis peak lies at the edge of the atom's density window, where distance equals density
- **THEN** its preferred-match theta is zero, while a same-magnitude peak at the atom's current frequency keeps its full magnitude as theta
