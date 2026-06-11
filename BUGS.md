# Known Issues Found During Test Backfill

Issues discovered while writing the test suite that were too big (or too judgment-requiring)
to fix as part of the test project. Small local fixes were made inline and are noted in
commit messages instead.

## Crashes

### LoadPatch can leave an interior grid's active sub-grid pointer NULL → SIGSEGV
Fuzzing found that a patch load interleaved with grid-swap / top-grid-mode / save state
reliably crashes in `TheNonagonSquiggleBoyQuadLaunchpadTwister::Apply`: a queued pad
message routes to an interior grid whose active sub-grid pointer
(`InteriorGridBase::m_grid`, dereferenced as `(*m_grid)->OnPress`) is NULL after
`FromJSON`. Removing LoadPatch from the fuzz action set eliminates essentially all
crashes, so this is the dominant fuzz crash. Deterministic repro: the seeded fuzz
stream in `private/test/system/sys_fuzz.cpp` with load enabled crashes within ~70
actions on base seed `0xA11CE000 + 0x9E3779B1`. Suggested direction: have `FromJSON`
re-establish interior-grid pointers before message processing resumes (and/or
null-check the active sub-grid in `Apply`). This needs a real design decision, so it
was recorded rather than patched. Found in WP-10.

### VectorPhaseShaper: `assert(phi_vps < 1)` trips (NaN/Inf phase) under fuzzed params
`private/src/VectorPhaseShaper.hpp:340` — some fuzzed Source-machine parameter/pitch
combinations drive `phi_vps` to NaN/Inf, tripping the assert in Debug; in Release this
would emit NaN audio instead. Rare (a few percent of long fuzz runs without patch
loads). Needs investigation of the upstream parameter math rather than a clamp at the
assert site. Found in WP-10.

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

## Fixed during the test project (for reference)

- **Out-of-range encoder/pad coordinates crashed the system** — bounds checks added in
  `EncoderGrid::GetVisible` (`private/src/Encoder.hpp`) and `SmartGrid::Grid::Get`
  (`private/src/SmartGrid.hpp`); out-of-range coordinates are now ignored.

## Quality observations (not bugs)

### Startup "instability" is actually pre-start quiescence
The observed odd behavior before the sequencer is first started is not a numeric
instability: before `m_running` is set, the TheoryOfTime master phasor is frozen at 0
and the audio path is silent even with gain faders raised
(`TheoryOfTimeInput.m_running = m_running || m_multiPhasorGate.m_anyGate` — neither is
true pre-start), so the instrument is inert rather than unstable. The first start has a
slightly larger transient than later ones (peak ~0.76 vs ~0.63) but is bounded and
NaN-clean; repeated start/stop cycles are stable. Characterized in
`private/test/system/sys_startup_stability.cpp` (WP-10).

### Math::Sin2pi THD
The polynomial sine approximation produces measurable harmonic distortion; the VCO THD
test asserts < 0.5 hard and warns at < 0.1. If purer sines matter, the table/polynomial
could be revisited. Found in WP-4 (`private/test/unit/dsp_vco.cpp`).
