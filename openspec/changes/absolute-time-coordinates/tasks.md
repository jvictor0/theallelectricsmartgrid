## 1. Signed arithmetic and indices

- [x] 1.1 Add failing Euclidean division/modulo and negative/wide IndexArp tests.
- [x] 1.2 Implement signed helpers and widen gate-step/motive propagation through bounded output mapping.
- [x] 1.3 Verify arithmetic and sequencer behavior.

## 2. Absolute time core and topology

- [x] 2.1 Add deterministic absolute-phase fixtures and regressions for multi-cycle/negative coordinates, reset indices, simultaneous parent boundaries, seeks, and transport.
- [x] 2.2 Extract TheoryOfTimeBase and implement the canonical PhaseDomain/query API without winding state.
- [x] 2.3 Evaluate edit eligibility from a pre-edit snapshot, apply aligned parent/multiplier pairs, and compare indices in consistent lattice units.
- [x] 2.4 Interpolate global phase before applying interval topology, including lookahead slot 8, and migrate all time API signatures.
- [x] 2.5 Verify core, topology, transport, and existing clock integration tests.

## 3. Playback and periodic LFO outputs

- [x] 3.1 Add failing half-speed/reverse/window playback tests and real PolyXFader fractional-shaping/topology interpolation regressions.
- [x] 3.2 Apply playback speed to absolute phase before output wrapping; preserve PolyXFader partial-lobe shaping and reduce phase in double.
- [x] 3.3 Migrate delay coordinate projections and buffer/display boundaries while preserving write-head glue and stopped motion.
- [x] 3.4 Verify playback/LFO/delay integration.

## 4. Captured envelope and gate timing

- [x] 4.1 Add matched-envelope tests that observe accepted multiplier/reparent edits during active attack/hold/decay and test fresh timing on retrigger.
- [x] 4.2 Capture AHD global origin, source cycle ratio, and envelope period at trigger; remove active source-loop identity and winding reconstruction.
- [x] 4.3 Migrate gate bounds and trigger plumbing, preserving the distinction between source cycle ratio and voice gate ratio.
- [x] 4.4 Remove redundant elapsed-sample relay/filter fields after auditing all AHDControl consumers; strengthen old warning-only continuity tests.
- [x] 4.5 Verify AHD/gate/reverse/retrigger/system behavior.

## 5. Integration and naming

- [x] 5.1 Add domain-correct recording/sync/MIDI/scope regressions under nonzero phase modulation.
- [x] 5.2 Migrate remaining timestamps, ticks, scopes, and UI adapters to canonical queries and output-only wrapping.
- [x] 5.3 Remove duplicate/legacy APIs and ToT winding consumers, preserve serialization keys, and verify all affected integration tests.

## 6. Documentation and completion

- [x] 6.1 Update current theory, glossary, LFO, AHD, gate, sample, delay, and mathematical documentation to the agreed terms and behaviors.
- [x] 6.2 Run the full standalone test suite and compile the JUCE app without signing or deployment.
- [x] 6.3 Validate OpenSpec, check the diff, review complete requirement coverage, and report verified results.

Detailed execution steps, interfaces, assertion seeds, and commands are in `docs/superpowers/plans/2026-09-07-absolute-time-coordinates.md`.

## Execution notes

The implementation is consolidated on `codex/absolute-time-coordinates` and rebased onto `origin/main` at `ba0fe08` for a GitHub pull request. Tests are grouped in `theory_of_time_absolute.cpp`, `absolute_time_consumers.cpp`, `absolute_time_envelopes.cpp`, and `absolute_time_wiring.cpp`, with existing integration fixtures migrated in place. Core processing no longer needs a separate `Preprocess` call. The unused circle tracker file and include were removed after auditing all callers.

- Fresh core/consumer review: no actionable findings.
- Fresh AHD/gate review: no actionable findings; numeric same-instance retrigger and live hold checks added from coverage suggestions.
- Focused AddressSanitizer/UndefinedBehaviorSanitizer run: 28 tests, 1,308 assertions passed.
- JUCE Debug application: build succeeded with code signing disabled.
- Mathematical TeX source and tracked PDF updated; `pdflatex` succeeded.
- Full run before the Astra review and rebase: 304/306 tests passed; 1,509,412/1,509,414 assertions passed. Both startup-silence failures reproduce on unchanged HEAD, with identical output peak 0.000655346 and frozen global phase.

## Astra review follow-up

- Fixed the delay diagnostic's integer format mismatch (`%zu` to `%d`).
- Added simultaneous ancestor/descendant topology coverage and exercised Nonagon source/gate ratio plumbing, physical-model slewed hold controls, and QuadDelayInputSetter tape-head coordinates through rollover, tempo changes, and stop.
- Retained the agreed loop-relative physical-model hold behavior; documented its compatibility impact relative to the former temporary input's 48,000-sample period.
- Preserved current main's replacement random-LFO implementation during rebase; its removed clock-boundary wiring no longer needs migration.
- The preexisting MIDI slot-0/block-timing limitation is outside this refactor; unmodulated-domain coverage does not claim exact sample-aligned MIDI tick timing.
- Rebased sanitizer run: 32 tests, 1,780 assertions passed.
- Rebased JUCE Debug application: build succeeded with signing disabled.
- Current-main startup baseline: both silence assertions fail at peak 0.000688948 while the global phase remains frozen at zero.
- The unfiltered rebased suite aborts in `PartialMachine: espace etale patch remains finite after load` at `VectorPhaseShaper.hpp:340` (`phi_vps < 1`). The isolated test reproduces the identical assertion on unchanged current main. The rest of the suite is run with only that case excluded.
- Rebased remainder run: 329/331 tests passed and 1,630,125/1,630,127 assertions passed, with only the aborting patch test excluded. The only failures are the two independently reproduced startup-silence assertions.
