# Effects repairs — 15 September 2026

These repairs follow the [original effects audit](effects-audit-2026-09-15.md) of commit `5ec7071f00b4e5c957de1e0136acb81939ad2cbb`. The original probe results remain historical evidence. This report describes the resulting working-tree changes.

## What changed

- Reverb return now uses the full return-control value. At maximum it reaches unity instead of 0.182744, restoring 14.76 dB. The control's exponential curve is otherwise unchanged.
- FrequencyDependentParameter now interpolates all four segments, including fourth-to-first, for both linear and geometric interpolation and negative coordinates.
- Partial Machine keeps its original unshifted phase accumulator and multiplies emitted phase by both unison detune and pitch-shift ratio. Organic and synthetic output gains now affect emitted partials.
- Analysis phase now removes the nearest bin's Hann-kernel phase offset before storing an atom. The stored phase describes the estimated sinusoid at the start of the analysis frame.
- Residual extraction fits and subtracts a complex Hann-windowed sinusoid with consistent amplitude and phase. It fits across the complete reconstruction support, using the remaining residual, so nearby partials cannot cause subtraction to add spectral energy.
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

There was also a phase error in analysis: `ExtractAnalysisAtoms` stored the nearest FFT bin's phase directly. For an isolated off-bin sinusoid, that includes the Hann kernel's phase offset. Analysis now removes that offset before constructing the atom: `analysisPhase = arg(X[phaseBin] * conjugate(HannKernel(exactBin - phaseBin))) / (2*pi)`. This references phase to the start of the frame at the estimated frequency. Frequency-estimation error still limits phase accuracy; the current parabolic estimator is approximate.

The new analysis regression checks known sinusoid phases at eight bin positions and four starting phases, including both sides of the nearest-bin boundary. It also directly subtracts the analyzed atom through the existing `WriteWindowedPartial` with twice its detected magnitude, without residual fitting, and requires less than 0.2% remaining energy for the tested off-bin tones. The existing residual projection below remains unchanged by this analysis-phase correction.

The general operation is to subtract a matching **windowed model** from the windowed input spectrum. If `R[k]` is the remaining complex spectrum and `K[k]` is the shifted Hann kernel for the estimated frequency, the repair computes:

```
c = sum(conjugate(K[k]) * R[k]) / sum(abs(K[k])^2)
R[k] -= c * K[k]
```

The sums use the same finite kernel support as synthesis, up to 17 bins. The complex coefficient `c` supplies both amplitude and phase. Doubling the detected magnitude illustrates the bin-centered normalization error, but does not reliably reconstruct a tone between bins or overlapping tones. The multi-bin fit also handles the kernel's phase away from its center. Direct complex subtraction supplies the negative sign; the old polar helper expressed subtraction by adding half a cycle to phase.

Each fit uses the remaining residual, so overlapping atoms cannot subtract the same original contribution repeatedly. This is a least-squares projection: its squared residual norm cannot increase, apart from floating-point roundoff. Review caught an adjacent-tone example where a one-bin fit increased residual energy to 1.450917 times the input; the final projection leaves 0.954495 times the input. The tone pair is unresolved at this window size, so a substantial residual is expected.

Tests cover four bin offsets, three phases, and adjacent-tone spacings at 32 relative phases. Bin-centered residual energy is below 1e-7 of the original; tested off-bin tones leave below 0.002. These are algorithmic regression bounds, not claims of exact reconstruction for arbitrary polyphonic input.

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
- Analysis tests check frame-start sinusoid phase and direct reconstruction through `WriteWindowedPartial`, independently of the residual fitter.
- Residual tests use the production analysis window and check cancellation and non-increasing energy for nearby tones.
- A narrow floating-point phase-wrap regression and the unchanged `espace etale` saved-patch integration test both pass.

Before the user's phase-accumulator correction, the targeted review run passed 8 tests and 8,363 assertions. Independent review reported no remaining actionable findings and independently passed 13 tests and 24,766 assertions. Its local 558-atom extraction benchmark was approximately 177 microseconds versus 140 microseconds at HEAD; this is development-machine evidence, not device performance.

The full suite before that correction ran **380 cases: 378 passed and 2 failed**, with **2,526,255 of 2,526,257 assertions passing**. All effects and added regression cases passed. The only failures were the existing startup silence checks at `private/test/system/sys_startup_stability.cpp:95` and `:272`. Both failures were independently reproduced against archived original source at `5ec7071`, with the identical peak of 0.000657712 and a frozen master clock. Their thresholds were retained. The full suite is not claimed green.

[Validation excerpts](effects-repairs-validation.txt) preserve the final test summary and the two baseline reproductions. `git diff --check` also passed.

After restoring the original accumulator and adding the emitted-phase pitch multiplier, the revised phase-contract test first failed against the prior implementation (8 of 16 assertions failed). The corrected implementation passed **45 targeted tests and 102,403 assertions**, covering Partial Machine, spectral reconstruction, DFT/OLA, wiring, and saved-patch playback. The regression checks both the complex phase of the emitted spectrum and the unchanged unshifted accumulator across ratio changes. The build and `git diff --check` passed. The full suite was not rerun for this narrow correction.

After correcting analysis phase at its source, the new direct-analysis regression first failed on 56 of 96 assertions. The correction then passed **26 targeted tests and 102,348 assertions**, including direct phase/reconstruction checks, spectral tracking, Partial Machine, DFT/residual synthesis, and saved-patch playback. Build and `git diff --check` passed; the full suite was not rerun for this isolated analysis-phase change.

Before opening the PR, the final C++ source was rebuilt and the complete suite rerun: **381 tests, 379 passed, 2 failed; 2,526,355 of 2,526,357 assertions passed**. The only failures remain the two baseline startup-silence checks, with the same 0.000657712 peak. All added regressions and effects tests passed. Final independent read-only review of the requested phase-accumulator behavior and analysis-phase correction found no actionable issues.

Reproduction commands:

```sh
cmake -S private/test -B /tmp/smartgrid-effects-audit-build
cmake --build /tmp/smartgrid-effects-audit-build -j 6
/tmp/smartgrid-effects-audit-build/smartgrid_tests --no-colors
```

The working tree was built as a signed Release app and deployed with `make ios-deploy` on 15 September 2026. Installation and launch succeeded on the paired iPad Air 13-inch (M3). A fresh app-log snapshot at 21:15:40 local time confirmed 48 kHz audio, 512-frame buffers, and four input/four output channels through MAYA44 USB+. This verifies deployment and audio startup; listening with existing patches and sustained device performance were not evaluated. Build/deploy output is in `/tmp/effects-ipad-deploy.log`, and the startup snapshot is in `/tmp/effects-ipad-deploy-startup/2026-09-15T21-15-36-182.log`.
