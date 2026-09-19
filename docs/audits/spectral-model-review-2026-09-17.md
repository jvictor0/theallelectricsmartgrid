# Spectral model branch review — 17 September 2026

Scope: the eight uncommitted Partial Machine files copied from the main checkout at `63de3f8881f3450fc6d8225660828fc991a87f44` into `codex/spectral-model-review`, plus the documentation, build repairs, regression coverage, and dead-code cleanup made during review. The earlier effects gain and phase repairs are already in the base commit. This review does not change the audio input summation or phase-accumulator policy.

## Finding: inconsistent density creates a suppression discontinuity [P2]

`AtomMatcher` selects a dominating analysis peak using density evaluated at the old atom's stored parameter index. `MergeAtom` then recomputes that peak's suppression score using density evaluated at the **peak's** index. The two cones agree with scalar density but can differ when frequency-dependent lanes differ.

Reproduction through `TrackAnalysisAtoms`, using frequency-derived lane indices:

- An old atom at `log2(f) = -6` has synthesis magnitude `0.25`.
- Another old atom at `log2(f) = -5.75` claims the sole new analysis peak at that frequency, with magnitude `1`.
- The new peak's density is `1` octave; normal decay is held at zero for the probe.
- Change the first old atom's density around its `0.25`-octave distance from the peak.

| Old atom density | Selection theta | Old magnitude after one hop |
| --- | --- | --- |
| 0.249999 | Ineligible | 0.25 |
| 0.25 | 0 | 0.0833333 |
| 0.250001 | Approximately 0.00000405 | 0.0833333 |

At the boundary the selected cone contributes zero, but attenuation uses the peak's wider cone and abruptly removes **9.54 dB**. The reverse mismatch can select a strong dominator that ultimately applies no suppression.

Follow-up: the user confirmed that suppression must use the old atom's index. `MergeAtom` now evaluates density with `atom.m_index`, matching candidate selection. The lane regression covers both sides of the 0.25-octave boundary and the reverse case of a broad old-atom cone with a narrow peak cone.

The `a²/theta` attenuation itself is value-continuous where it activates at `theta = a`. It can still shorten tails sharply over repeated hops; raw-analysis suppression and attack-independent replacement are intentional design choices, rather than additional findings.

## Finding: Deep Vocoder inherits an unintended density change [P2]

`DeepVocoder::Input::MakeSpectralInput` does not set density. Changing the shared `SpectralModel::Input` default from `1/4096` cycles/sample to `1/1200` octave therefore also changes Deep Vocoder tracking from an approximately 11.72 Hz window at 48 kHz to one cent (approximately 0.254 Hz at 440 Hz).

Using the actual Deep Vocoder input defaults, a tracked 440 Hz partial with magnitude `0.25` followed by a 441 Hz peak gives:

| Revision | Retained partials after update |
| --- | --- |
| Base commit | One continuing atom at 441 Hz, magnitude 0.25 |
| Review branch | Old atom at 440 Hz, magnitude 0.239558; new atom at 441 Hz, magnitude 0.0480283 |

This also reproduces through Hann-windowed extraction: the base advances the existing atom from the fitted 439.9196 Hz to 440.8341 Hz, while the new code leaves the stronger old atom at its stale frequency and attacks a second atom. Small pitch movements can therefore accumulate stale targets in the vocoder's ranking set.

Follow-up: the user chose to retain the new model and give Deep Vocoder an explicit one-semitone (`1/12` octave) matching radius. `MakeSpectralInput` now sets that value; regression tests cover continuation at ±90 cents and separate births at ±110 cents. Both Partial Machine and Deep Vocoder own model instances, so both also carry the new matcher storage.

## Matching correctness and coverage

An independent exhaustive enumeration agreed with the reconstructed maximum-total-theta, noncrossing assignment in **10,000 deterministic randomized cases** with one to six atoms on each side. The 13,323 selected matches had no shared peaks or crossings. The same probe completed under AddressSanitizer and UndefinedBehaviorSanitizer without diagnostics. This checks the assignment algorithm separately from the density application issue above.

Six permanent regression cases cover stationary identities despite reversed synthesis-frequency order, unchanged state during matching, total score before match count, zero-score boundary tie-breaking, the magnitude-ratio gate, domination by an unclaimed peak before its attack, and clearing results when either input side is empty. Three existing single-atom tests now exercise `TrackAnalysisAtoms` instead of the removed `SearchAndMerge` API.

At the time of the original review, the production cap was safe within the existing allocator: at most 1,024 retained atoms plus 1,023 extracted local maxima required 2,047 slots before pruning, below the 8,192-slot capacity. The 18 September follow-up lowers the Partial Machine budget to 256 for both retained atoms and incoming analysis peaks, requiring at most 512 slots before pruning. No new synthetic-harmonic runtime dependencies remain.

## Performance measurement

Optimized local measurements of `Match` alone, with equal old and analysis counts:

| Atoms on each side | One-cent density | One-octave density |
| --- | --- | --- |
| 64 | 25.5 µs | 30.7 µs |
| 256 | 359 µs | 422 µs |
| 558 | 1.76 ms | 2.01 ms |
| 1,023 | 6.00 ms | 6.80 ms |

A separate identical stationary, exact-match tracking workload increased from 47.3 µs at the base commit to 5.99 ms with the new matcher at 1,023 atoms. The earlier greedy tracker was cheaper but did not provide the new global assignment guarantee. Narrow windows do not avoid the current quadratic row traversal.

Matcher workspace and results add **753,712 bytes** per model instance; total model size is 1,212,488 bytes in the measured build. A 512-sample callback at 48 kHz has a 10.67 ms budget for the whole audio graph. These development-machine measurements warrant iPad profiling at dense input before calling the new tracking path real-time safe; they do not establish an actual device deadline miss.

Follow-up: the [matcher-only profile and optimization experiments](spectral-matcher-profile-2026-09-17.md) isolate the assignment solver as the bottleneck and measure exact candidate-aware prototypes. Those optimizations have not been applied to the production matcher.

## Changes completed during review

- Fixed missing C++17 `typename` qualifiers on nested result types and migrated stale tests to the new tracking interface.
- Updated the performer documentation and Partial Machine spec for ordered assignment, octave density, births/decay/domination, synthetic removal, fitted Hann analysis, and emitted phase scaling. Corrected stale radius, panning, and unison descriptions to match the existing implementation.
- Updated the ganged-random-LFO spec after synthetic modulation removal and supplied the scenarios required by strict validation.
- Removed unused atom magnitude-sort wrappers, `AtomArray`, both unused `GetPitchShiftedOmega` overloads, and the unused input-setter volume knob/mapper plus its no-op test assignment. Mixer return gain and the synthesis reduction gain remain in use.

## Validation

The final C++17 test target builds after dead-code cleanup. All 35 targeted Partial Machine, spectral-model, and frequency-dependent-parameter tests passed, with 86,867 assertions, before that cleanup; the final full run also includes all of those tests. The full suite passed 391 of 393 tests and 2,510,825 of 2,510,827 assertions both before and after cleanup. The only failures were the two previously documented startup-silence checks, with the same output peak of `0.000657712` recorded in [the effects-repair validation](effects-repairs-2026-09-15.md).

After the user-directed Deep Vocoder density and `MergeAtom` index corrections, the rebuilt target passes **38 targeted tests and 86,889 assertions**, including the three new regression cases. The full suite was not rerun for this narrow follow-up.

### 18 September: rebase and 256-partial budget

Rebased `codex/spectral-model-review` onto local main at `c79a7358c2a39dd49cc5641b959ab9f9dab80214` without conflicts and restored the existing spectral changes. The only new production change is `PartialMachine::InputSetter::x_numAtoms` from 1024 to 256. The matcher optimization prototypes remain unintegrated.

A new two-frame regression supplies 300 separated peaks per frame through the production input setter. It checks that the strongest 256 peaks enter tracking, that the next frame can contain 256 retained tails plus unclaimed new peaks, and that final pruning retains only 256 atoms. Before the limit change it failed with 300 incoming peaks; after the change it passes.

Fresh validation after the rebase and limit change:

- C++17 target builds; **39 targeted tests / 87,411 assertions pass**.
- Full suite: **413 of 415 tests and 2,581,085 of 2,581,087 assertions pass**. The only failures remain the startup-silence checks at `sys_startup_stability.cpp:95` and `:272`, with the unchanged peak `0.000657712`.
- Both modified specs pass strict validation; `git diff --check` passes.
- An independent subagent reviewed the full diff against main, including untracked files, and reported no actionable findings or review blockers. Device build, iPad timing, and listening validation remain outside this review.

Local evidence: `/tmp/spectral-256-red.log`, `/tmp/spectral-256-green-build.log`, `/tmp/spectral-256-green.log`, and `/tmp/spectral-256-full.log`.

Both modified specs pass `openspec validate --type spec --strict --no-interactive`; `git diff --check` passes. Device build, deployment, and iPad timing have not been performed for this review.

Local evidence: `/tmp/spectral_review_probe.cpp`, `/tmp/spectral_review_probe.log`, `/tmp/spectral_review_probe_sanitized.log`, `/tmp/spectral_tracking_bench_base.log`, `/tmp/spectral_tracking_bench_current.log`, `/tmp/deepvocoder_density_probe.cpp`, `/tmp/deepvocoder_density_probe_{base,current}.log`, and `/tmp/spectral-branch-review-{build,targeted,full,final-full}.log`.

### 18 September: iPad deployment

At the user's request, `make ios-deploy` built and signed this worktree's Release app, upgraded SmartGridOne on the paired iPad Air 13-inch (M3), and launched it successfully. The build includes the uncommitted spectral changes and 256-partial limit on base `c79a735`; matcher optimization prototypes are not included.

A fresh app snapshot at **18:51:30 PDT (UTC−07:00)** confirms 48 kHz audio, 512-frame buffers, and four input/four output channels through MAYA44 USB+. The initial seven-callback snapshot records one startup overrun and zero xruns; it establishes audio startup, not sustained timing or listening validation.

Build/deploy log: `/tmp/spectral-256-ipad-deploy.log`. Startup snapshot: `/tmp/spectral-256-ipad-startup-20260918/2026-09-18T18-51-27-067.log`.
