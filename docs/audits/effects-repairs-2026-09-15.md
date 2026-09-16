# Effects repairs — 15 September 2026

These repairs follow the [original effects audit](effects-audit-2026-09-15.md) of commit `5ec7071f00b4e5c957de1e0136acb81939ad2cbb`. The original probe results remain historical evidence. This report describes the resulting working-tree changes.

## What changed

- Reverb return now uses the full return-control value. At maximum it reaches unity instead of 0.182744, restoring 14.76 dB. The control's exponential curve is otherwise unchanged.
- FrequencyDependentParameter now interpolates all four segments, including fourth-to-first, for both linear and geometric interpolation and negative coordinates.
- Partial Machine keeps its original unshifted phase accumulator and multiplies emitted phase by both unison detune and pitch-shift ratio. Organic and synthetic output gains now affect emitted partials.
- Analysis refines selected peaks with a complex Hann-kernel fit and stores the resulting magnitude and frame-start phase in each measured atom. Tracking and residual reconstruction use this same estimate.
- Residual extraction subtracts those analysis atoms through the unchanged `WriteWindowedPartial` helper, using twice the stored analysis magnitude. There is no separate residual-only parameter fit.
- Equal-height neighboring peak bins identify one peak, allowing a sinusoid exactly halfway between FFT bins to be tracked.
- VectorPhaseShaper maps a wrapped phase that rounds to exactly 1.0 back to 0.0. This resolves the assertion exposed by the existing saved-patch test.
- Effects tests now check output energy, gain, frequency, emitted phase, and reconstruction, with meaningful lower bounds as well as finiteness and upper bounds.

Reverb feedback mapping, damping, diffuser topology, and Partial Machine's quad-to-mono input topology retain their existing behavior. Existing patches can become substantially louder from the return correction, and frequency-dependent patterns change because the missing segment is restored. Device listening and sustained CPU measurements remain to be done.

## Why high feedback gets so loud

There are two different nonlinearities.

First, the reverb feedback knob `k` maps to a multiplier through an exponential control curve:

```
g(k) = 1.25 * (9^k - 1) / 8
```

| Knob | Feedback multiplier |
|---|---:|
| 50% | 0.312500 |
| 75% | 0.655649 |
| 95% | 1.103692 |
| 100% | 1.250000 |

This is in `QuadReverbInputSetter`, which sets the curve's center to 0.25 and multiplies its result by 1.25. Thus 95% is already above unity feedback. The return fader follows a separate base-20 curve and lies outside this feedback loop.

Second, the audio passes through a normalized saturator on each trip through the loop. Approximately, its operation is `S(x) = tanh(0.5*x) / tanh(0.5)`; the implementation uses a clipped rational approximation to tanh. Its measured slope near zero is 1.0733945 (+0.615 dB), while its slope decreases as the signal becomes large. The constructor argument 0.5 controls drive, rather than multiplying quiet output by 0.5.

The relevant quantity is the gain for an entire trip through the loop: feedback multiplier, saturator slope, filters, delays, and channel mixing. A resonant mode with round-trip gain above one grows until the saturator limits it. The threshold depends on damping and frequency; it is not simply the point where the knob passes a fixed percentage. The very large high-feedback/input ratios in the original audit measured this buildup relative to a tiny input, rather than a linear amplification factor. The corrected return fader makes ordinary wet output audible without relying on that buildup.

The Hadamard transform's division by two preserves four-channel energy. The parallel input all-pass average still has phase-dependent cancellation; the audit measured about -3.56 dB of impulse energy there. Neither normalization was changed.

## The pitch algebra is correct; the old code omitted one multiplier

Let `H` be the hop size, `f` the atom's frequency in cycles per sample, `r` the pitch-shift ratio, and `d` one unison voice's detune ratio. Consistency requires:

```
emitted frequency = f * r * d
phase advance per hop = H * f * r * d
```

The old code instead did:

```
emitted frequency = f * r * d
emitted phase = accumulatedUnshiftedPhase * d
next accumulatedUnshiftedPhase += H * f
```

The ratio multiplying phase was the unison detune `d`. Pitch shift `r` was absent. So `H * unshifted * ratio = H * shifted` is indeed correct, but that was not the calculation being performed.

For a concrete example, a bin-18 tone in a 4096-sample frame advances 4.5 cycles per 1024-sample hop. An octave-up tone must advance 9 cycles. The old code still advanced 4.5 cycles, putting successive frames half a cycle out of alignment. Their overlapping audio nearly cancelled. Certain other frequencies happened to differ by whole cycles and concealed the defect.

The user specified that the original phase accumulator must remain unchanged. The repair therefore computes `emittedPhase = synthesisPhase * unisonDetune * pitchShiftRatio`, and retains `atom.UpdatePhase()`, which advances by `H * unshiftedFrequency`. For a fixed ratio, advancing the source phase by 4.5 cycles and then multiplying by two gives the required 9-cycle advance. The pitch ratio is computed once per atom and used for both frequency and phase. Tests also verify that changing the ratio scales the emitted phase while the stored phase continues to advance at the unshifted frequency.

## Where the first window is applied, and what to subtract

`PartialMachine::Process` copies the last 4096 samples from its circular buffer every 1024 samples. During that copy, it explicitly multiplies each sample by `Math4096::Hann(i)` before calling `ExtractAtomsAndResidual` (`private/src/PartialMachine.hpp:432–438`). The extraction function therefore receives already-windowed audio.

For a bin-centered cosine with peak amplitude `A`, under this code's normalized positive-frequency FFT convention:

| Stage | Center-bin magnitude |
|---|---:|
| Unwindowed cosine | A/2 |
| After the analysis Hann window | A/4 |
| Old subtraction: pass detected A/4 through another Hann kernel | A/8 removed |
| Correct modeled Hann-windowed cosine | A/4 removed |

The analysis window has already attenuated the detected peak. Passing that peak directly to `WriteWindowedPartial` applies the Hann center factor again, leaving half the original peak in the residual. The helper expects the pre-window complex coefficient, A/2 for this example. The Hann form and its 0.5 center coefficient follow the [standard Hann-window definition](https://dsprelated.com/freebooks/sasp/Generalized_Hamming_Window_Family.html).

There was also a phase error in analysis: `ExtractAnalysisAtoms` stored the nearest FFT bin's phase directly, including the Hann kernel's phase offset. The initial repair removed that offset but calculated a separate amplitude/phase fit only for residual subtraction. PR review identified the inconsistent estimates: tracking retained the preliminary parameters while the residual used fitted ones.

The 16 September follow-up moves the coefficient fit into `ExtractAnalysisAtoms`. The existing peak detector and log-parabolic frequency estimate select candidates; the selected measured atoms are then refined in frequency order. If `R[k]` is the remaining complex spectrum and `K[k]` is the shifted Hann kernel for the estimated frequency, analysis computes:

```
c = sum(conjugate(K[k]) * R[k]) / sum(abs(K[k])^2)
analysisMagnitude = abs(c) / 2
analysisPhase = arg(c) / (2*pi)
R.WriteWindowedPartial(analysisPhase + 0.5, 2 * analysisMagnitude, analysisOmega)
```

The sums use the same finite kernel support as synthesis, up to 17 bins. The fitted coefficient supplies amplitude and frame-start phase, and its magnitude is converted back to the existing A/4 analysis convention. Each fit uses a scratch spectrum with earlier selected atoms removed. This preserves the sequential projection behavior, subject to floating-point and trigonometric lookup approximation. Candidate selection happens before fitting, so a reduced atom budget does not discard earlier contributions on which later fits depended. Generated harmonics are not fitted to the input.

`ExtractAtomsAndResidual` subsequently subtracts the returned measured atoms from its original DFT using the same `WriteWindowedPartial` call. It does not estimate their parameters again or make an extra copy for subtraction. The one scratch copy belongs to analysis, which leaves its input spectrum unchanged. `WriteWindowedPartial` itself is unchanged.

Before this follow-up, directly subtracting the corrected-phase but preliminary-magnitude estimates worked for isolated tones, leaving at most 0.145% energy in the tested cases. It failed badly for some overlapping tones: near-cancellation in a neighboring bin made log-parabolic peak magnitudes too large, and a two-tone case left 49.35 times the original energy. Publishing the fitted coefficient in analysis addresses that inconsistency; it does not make the estimated frequencies exact or resolve arbitrarily close sinusoids.

Tests check known frame-start phases, amplitude A/4 within 0.5% for isolated on/off-bin tones, and agreement between the residual reconstructed from returned atoms and the production residual. The overlap checks span four spacings, 32 relative phases, and one/eight-atom budgets, and retain the non-increasing-energy regression. Existing bin-centered and off-bin reconstruction limits are unchanged.

## A better Partial Machine input design

**User decision:** keep the existing input design. The alternative below was discussed and declined; it is not planned work.

The current samplewise sum throws away spatial information before analysis. Equal opposite-polarity channels cancel. Dividing the sum by four would change gain but would not solve that cancellation.

The considered alternative was four-channel analysis feeding one shared partial tracker:

1. Window and FFT each channel independently.
2. Form pooled per-bin magnitude `sqrt(sum_c(abs(X_c[k])^2))` for peak detection and tracking.
3. For each detected frequency, fit and subtract the sinusoid separately in each channel, then pool residual powers.
4. Feed the shared tracked model into the existing quad repanner.

This removes cancellation caused solely by opposite phases in different channels and makes the analysis follow total channel energy. It preserves an intentionally shared model rather than creating four unrelated sets of partials. It needs an explicit convention for each tracked atom's analysis phase and calibrated pooled gain; one option is a phase reference from the strongest channel with continuity handling when that reference changes. Existing synthesis starts its own accumulated phase, but stored analysis phase should still have a clear contract.

The cost is three extra 4096-point FFTs per hop, more input storage, and channelwise residual fitting. Benchmark on the iPad before adoption. Taking `sqrt(sum_c(x_c^2))` directly in the time domain is unsuitable: it discards waveform sign and changes the signal's frequencies. A cheaper pre-pan mono send from source voices would improve those sources' spatial consistency, but would not resolve arbitrary quad input or cross-effect returns.

## Test changes and validation

- Mixer wiring checks unity for all three fully open returns and reverb's midpoint and zero endpoint.
- Frequency mapping checks unequal lanes, the fourth-to-first segment, negative positions, and both interpolation modes across every boundary.
- Dedicated reverb tests exercise audible impulse energy without feedback, a late feedback tail, and silence at ordinary feedback.
- Delay's previously silent passing impulse fixture now starts after warm-up and requires nonzero output. Tone tests check gain and the requested octave. Stress fixtures retain their modulation settings between updates and feed actual output back into the processor.
- Partial Machine tests exercise octave-up, octave-down, a 1.5 ratio, nonzero static unison, emitted phase scaling across pitch changes with an unchanged accumulator, and source-type mute gains.
- Analysis tests check frame-start sinusoid phase, Hann peak normalization, and direct reconstruction through `WriteWindowedPartial` using the returned analysis atoms.
- Residual tests use the production analysis window and check cancellation and non-increasing energy for nearby tones.
- A narrow floating-point phase-wrap regression and the unchanged `espace etale` saved-patch integration test both pass.

Before the user's phase-accumulator correction, the targeted review run passed 8 tests and 8,363 assertions. Independent review reported no remaining actionable findings and independently passed 13 tests and 24,766 assertions. Its local 558-atom extraction benchmark was approximately 177 microseconds versus 140 microseconds at HEAD; this is development-machine evidence, not device performance.

The full suite before that correction ran **380 cases: 378 passed and 2 failed**, with **2,526,255 of 2,526,257 assertions passing**. All effects and added regression cases passed. The only failures were the existing startup silence checks at `private/test/system/sys_startup_stability.cpp:95` and `:272`. Both failures were independently reproduced against archived original source at `5ec7071`, with the identical peak of 0.000657712 and a frozen master clock. Their thresholds were retained. The full suite is not claimed green.

[Validation excerpts](effects-repairs-validation.txt) preserve the final test summary and the two baseline reproductions. `git diff --check` also passed.

After restoring the original accumulator and adding the emitted-phase pitch multiplier, the revised phase-contract test first failed against the prior implementation (8 of 16 assertions failed). The corrected implementation passed **45 targeted tests and 102,403 assertions**, covering Partial Machine, spectral reconstruction, DFT/OLA, wiring, and saved-patch playback. The regression checks both the complex phase of the emitted spectrum and the unchanged unshifted accumulator across ratio changes. The build and `git diff --check` passed. The full suite was not rerun for this narrow correction.

After correcting analysis phase at its source, the new direct-analysis regression first failed on 56 of 96 assertions. The correction then passed **26 targeted tests and 102,348 assertions**, including direct phase/reconstruction checks, spectral tracking, Partial Machine, DFT/residual synthesis, and saved-patch playback. Build and `git diff --check` passed; the full suite was not rerun for this isolated analysis-phase change.

Before opening the PR, the final C++ source was rebuilt and the complete suite rerun: **381 tests, 379 passed, 2 failed; 2,526,355 of 2,526,357 assertions passed**. The only failures remain the two baseline startup-silence checks, with the same 0.000657712 peak. All added regressions and effects tests passed. Final independent read-only review of the requested phase-accumulator behavior and analysis-phase correction found no actionable issues.

After the 16 September shared-analysis-estimate follow-up, both new regressions first failed against the prior implementation: **274 of 784 assertions failed**. The corrected code passed **29 targeted tests and 103,123 assertions**. The fresh full suite passed **381 of 383 tests and 2,527,139 of 2,527,141 assertions**. Only the same two baseline startup-silence failures remain, with the unchanged 0.000657712 peak. Independent read-only review found no actionable issues. This follow-up has not been deployed to the iPad.

Reproduction commands:

```sh
cmake -S private/test -B /tmp/smartgrid-effects-audit-build
cmake --build /tmp/smartgrid-effects-audit-build -j 6
/tmp/smartgrid-effects-audit-build/smartgrid_tests --no-colors
```

The working tree was built as a signed Release app and deployed with `make ios-deploy` on 15 September 2026. Installation and launch succeeded on the paired iPad Air 13-inch (M3). A fresh app-log snapshot at 21:15:40 local time confirmed 48 kHz audio, 512-frame buffers, and four input/four output channels through MAYA44 USB+. This verifies deployment and audio startup; listening with existing patches and sustained device performance were not evaluated. Build/deploy output is in `/tmp/effects-ipad-deploy.log`, and the startup snapshot is in `/tmp/effects-ipad-deploy-startup/2026-09-15T21-15-36-182.log`.
