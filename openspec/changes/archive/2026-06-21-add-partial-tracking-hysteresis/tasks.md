## 1. Tracking Preference Tests

- [x] 1.1 Add focused coverage proving an existing atom chooses a nearby candidate over a slightly louder farther candidate when theta is higher.
- [x] 1.2 Add coverage proving a much stronger farther candidate can still win when its theta is higher.
- [x] 1.3 Add coverage for organic-over-synthetic priority remaining ahead of theta comparison.
- [x] 1.4 Add coverage that no candidate outside the density window is merged when `lower_bound` lands after the upper boundary.

## 2. Core Implementation

- [x] 2.1 Introduce a SpectralModel-local theta helper using `analysisMagnitude * max(0, 1 - distance / density)`.
- [x] 2.2 Update preferred-candidate comparison to use organic-over-synthetic priority, theta, raw magnitude tie-breaking, and distance tie-breaking.
- [x] 2.3 Update `SearchAndMerge` to pass target atom frequency and density into preference selection.
- [x] 2.4 Track whether an in-window candidate was found before merging, so out-of-window candidates cannot be selected.
- [x] 2.5 Keep candidate consumption limited to the existing density window after merge.

## 3. Verification

- [x] 3.1 Run the focused partial-machine or SpectralModel unit tests.
- [x] 3.2 Run the broader DSP unit test target used for `dsp_partialmachine.cpp`.
- [x] 3.3 Confirm `openspec status --change "add-partial-tracking-hysteresis"` reports the change apply-ready.
