## 1. DSP

- [x] 1.1 Implement configurable OLA history, independent retirement, and bounded per-channel cubic reads.
- [x] 1.2 Enable one-hop OLA latency and per-sample PM in Quad Delay's grain manager and input setter.

## 2. Controls

- [x] 2.1 Add the linear bipolar PM parameter, delay page placement, and encoder-to-DSP wiring.
- [x] 2.2 Update catalogue expectations and test control mapping, persistence, and old-patch defaults.

## 3. Validation and Documentation

- [x] 3.1 Test OLA reference reconstruction, interpolation, endpoint motion, wraparound, clearing, and delay integration.
- [x] 3.2 Update delay and phase-vocoder documentation and validate OpenSpec artifacts.
- [x] 3.3 Run focused DSP/control tests and the full core suite with enabled assertions; review the final diff.

Validation: 32 focused tests and 1,406,301 assertions passed. `make core-test`
passed both the complete core suite and the passthrough-rate suite in the
canonical RelWithDebInfo build with assertions enabled. `git diff --check`
and `openspec validate add-delay-phase-modulation --strict` passed.

## 4. Remove Delay Widen

- [x] 4.1 Remove Widen from the delay catalogue, page, input mapping, DSP, and performance harness; update docs and the projection requirement.
- [x] 4.2 Update catalogue coverage, check legacy Widen patch loading, and run relevant core and performance checks.

Widen-removal validation: `make core-test` passed both suites with assertions
enabled, including the legacy DelayWiden load/save check. `make perf-build`,
strict OpenSpec validation, and `git diff --check` passed.

## 5. Channel-Specific Unison Spread

- [x] 5.1 Add vector exponential bases and the generic/quad GetUnisonBase helper; wire the quad delay to 40/45/50/55-cent maxima and update the specification and docs.
- [x] 5.2 Verify equal-knob channel spreads, generic defaults, reciprocal copy pitches, and scalar range updates; run the core suite.

Unison validation: 3 focused cases and 4,188 assertions passed, including the
existing varying-knob mapping test. `make core-test` passed the complete core
and passthrough-rate suites with assertions enabled. Strict OpenSpec validation
and `git diff --check` passed.

## 6. iPad Deployment

- [x] 6.1 Replace the removed Widen visualizer association with PM so the app catalogue stays complete.
- [x] 6.2 Validate the visualizer mapping, build the signed iOS Release app, and install and launch it on the paired iPad.

Deployment validation: `make visualizers-test` passed. `make ios-deploy` built
and signed the Release app, upgraded the paired iPad over Wi-Fi, launched
`com.theallelectricsmartgrid.smartgridone`, and reported `Deploy complete`.
The deployment log is retained at `build/results/ipad-deploy-unison.log`.

## 7. Phase Label and Interpolation Review

- [x] 7.1 Rename the displayed PM label to PHAS and document the existing encoder ramps separately from audio interpolation.
- [x] 7.2 Run the existing catalogue and delay-control checks for the label change.

Label validation: 5 existing catalogue and delay-control cases passed with
27,892 assertions after `make core-build`. Strict OpenSpec validation and
`git diff --check` passed. The PHAS label has not been redeployed to the iPad.

## 8. Unison Fade Range

- [x] 8.1 Extend the existing unison gain fade to the first third of knob travel and update its docs and specification.
- [x] 8.2 Verify the mapping and audible-copy gains at zero, below one third, at one third, and above it. Do not redeploy.

Fade validation: `make core-build` succeeded. The 6 focused mapping and PM
cases passed with 20,736 assertions, including equal-copy gain at one third
and continued detuning above it. Strict OpenSpec validation and
`git diff --check` passed. No deployment was performed.

## 9. Rebase onto Main

- [x] 9.1 Preserve the feature on a branch, rebase onto main, and align the phase-control documentation with main's 20 ms manual encoder smoothing.
- [x] 9.2 Run the core and visualizer suites on the combined changes and leave the feature worktree clean. Do not redeploy.

Rebase validation: `make core-test` passed both core suites with assertions
enabled, and `make visualizers-test` passed. The rebase preserved the feature
patch without conflicts. Main's subsequent documentation-only formatting
commit was also included. Strict OpenSpec validation and `git diff --check`
passed. No deployment was performed.

## 10. One-Hertz Phase Input Experiment

- [x] 10.1 Apply an independent 1 Hz OPLowPassFilter per channel to the combined PHAS value, with double state and engine-rate coefficients; update its response check and documentation.
- [x] 10.2 Validate the affected delay controls, build the signed iOS Release app, and install and launch it on the paired iPad for listening.

One-hertz validation: `make core-test` passed both suites with assertions
enabled, including the PHAS response check. Strict OpenSpec validation and
`git diff --check` passed. `make ios-deploy` built and signed the app, upgraded
the paired iPad over Wi-Fi, launched it, and reported `Deploy complete`.
Logs are retained at `build/results/delay-phase-1hz-core-test.log` and
`build/results/ipad-phase-1hz-deploy.log`.

## 11. Offset Label

- [x] 11.1 Rename the delay's displayed label to OFFS to match Reverb and update the catalogue expectations and documentation.
- [x] 11.2 Run the existing catalogue and delay-control checks for the label change.

Offset-label validation: `make core-build` succeeded and the existing catalogue
and delay PM cases passed. Strict OpenSpec validation and `git diff --check`
passed. The 1 Hz filter remains unchanged and the label has not been redeployed.

## 12. Restore Raw Offset Modulation

- [x] 12.1 Remove the experimental 1 Hz offset filter, retain OFFS and manual encoder smoothing, and restore direct-mapping checks and documentation.
- [x] 12.2 Validate the affected controls and DSP, then build, install, and launch the raw-modulation build on the paired iPad.

Raw-modulation validation: `make core-build` succeeded, and 25 existing catalogue,
delay, OLA, and rate-boundary cases passed with 2,766,440 assertions. Strict
OpenSpec validation and `git diff --check` passed. `make ios-deploy` built and
signed the app, upgraded the paired iPad, launched it, and reported
`Deploy complete`. This deployment includes the OFFS label. Logs are retained
at `build/results/delay-offset-raw-tests.log` and
`build/results/ipad-offset-raw-deploy.log`.
