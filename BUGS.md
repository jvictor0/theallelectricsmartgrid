# Known Issues Found During Test Backfill

Issues discovered while writing the test suite that were too big (or too judgment-requiring)
to fix as part of the test project. Small local fixes were made inline and are noted in
commit messages instead.

## Crashes

### Use-after-free on shutdown with an in-flight persist task (deterministic SIGSEGV)
In `JUCE/SmartGridOne/Source/NonagonWrapper.hpp:509-510` the `IoTaskThread` member is
declared *before* `TheNonagonSquiggleBoyInternal`, so reverse-order destruction tears down
the audio core (voices, RecordingBuffers, bank sinks) first while the IO thread — which
only joins in its own destructor — may still be running a `PersistRecording` task that
dereferences the freed objects. Reproduced 5/5 deterministically in tests: arm → record →
`StopRecording` → destroy without pumping `Acknowledge()`.
Fix options: drain/stop the IO thread before destroying the audio core, or reorder the
members so the thread is destroyed (joined) first. The test suite documents the hazard and
exercises the safe drain-then-destroy path
(`private/test/system/sys_recording_roundtrip.cpp`). Found in WP-9.

## Latent hazards

### RecordingConfig source is never assignable
`RecordingConfig::m_source` defaults to `Water` (trio 0) and no code path ever reassigns
it, so the sampler-looper always records trio 0. The Earth/Fire enum values are dead. The
recording UI (`SquiggleBoyConfigGrid::RecordingToggleCell` / `SourceSelectCell`) is also
not routed through the QuadLaunchpadTwister pad routes, so arming recording appears
unreachable from that control surface (the x=-1,y=7 record pad drives the *mixer's* master
recording instead). Possibly an unfinished feature. Found in WP-9.

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
