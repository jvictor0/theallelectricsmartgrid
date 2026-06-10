# Known Issues Found During Test Backfill

Issues discovered while writing the test suite that were too big (or too judgment-requiring)
to fix as part of the test project. Small local fixes were made inline and are noted in
commit messages instead.

## Latent hazards

### TheoryOfTime dereferences a null MessageOutBuffer if driven unwired
`TheoryOfTime::Process` dereferences `m_messageOutBuffer` on start/stop transitions and on
every master-loop clock tick. A `TheoryOfTime` constructed standalone (buffer pointer
defaults to null) segfaults as soon as it runs. The real host always wires one, so this is
latent, but any new consumer (or test) must wire a buffer first. Also: `MessageOutBuffer`
has fixed capacity 16 and `Push` silently no-ops when full — if a host ever stops draining
it, clock messages are silently dropped. Found in WP-3 (`private/src/TheoryOfTime.hpp`).

### CombFilterWithOnePole: suspect inverted condition / dead branch
`private/src/CombFilterWithOnePole.hpp:103-106` — in `SetParams(combFreq, alpha)`, the
branch `if (m_feedbackSlew.m_target < 0.0f) { combFreq *= 2; }` looks inverted or dead:
the target initializes to 0 and only goes negative via `SetDampingTime` with negative
feedback. Worth a deliberate look at the intended comb tuning for negative feedback.
Found in WP-4.

### LadderFilterLPZDF: debug meters left enabled
`private/src/LadderFilter.hpp:373` — `static constexpr bool x_debugMetersOn = true` in
`LadderFilterLPZDF` (the non-ZDF `LadderFilterLP` has it `false`). Looks unintentional;
adds per-stage instrumentation overhead in release builds. One-character fix, but left for
a deliberate decision since it changes runtime behavior of the shipping synth. Found in WP-4.

## Quality observations (not bugs)

### Math::Sin2pi THD
The polynomial sine approximation produces measurable harmonic distortion; the VCO THD
test asserts < 0.5 hard and warns at < 0.1. If purer sines matter, the table/polynomial
could be revisited. Found in WP-4 (`private/test/unit/dsp_vco.cpp`).
