# Effects audit — 15 September 2026

Audited commit: `5ec7071f00b4e5c957de1e0136acb81939ad2cbb`.

**Historical audit:** the findings and measurements below describe that original commit. The subsequent repairs, explanations, and current validation are recorded in [Effects repairs](effects-repairs-2026-09-15.md).

The reverb has a confirmed, substantial reduction in its mixer return gain. Partial Machine has the reported frequency-parameter discontinuity, a separate pitch/phase defect, and an incorrectly scaled residual subtraction. Delay passed the steady-tone and octave-shift probes used here, although its existing tests have a significant audible-output coverage gap.

This is a source audit with executable DSP probes. No production DSP, settings, patches, or existing tests were changed. The subjective listening report was not reproduced on the iPad or with the user's current patch. Measurements below use the repository's 48 kHz sample rate.

## 1. Reverb return is capped at −14.76 dB

**Priority: high. Confirmed gain behavior; original intent is undocumented.**

`private/src/SquiggleBoy.hpp:1676` passes `ReverbReturn / 2` into `ZeroedExpParam::Update`. Delay and Partial Machine pass their entire knob value through the same gain curve (`:1657`, `:1698`). The default base is 20; the curve in `private/src/PhaseUtils.hpp:239` is:

```
gain(k) = (20^k − 1) / 19
reverb return(k) = gain(k / 2)
```

| Return knob | Reverb gain | Reverb dB | Delay / Partial Machine gain | Their dB |
|---|---:|---:|---:|---:|
| 25% | 0.023906 | −32.43 | 0.058671 | −24.63 |
| 50% | 0.058671 | −24.63 | 0.182744 | −14.76 |
| 75% | 0.109226 | −19.23 | 0.445127 | −7.03 |
| 100% | 0.182744 | −14.76 | 1.000000 | 0.00 |

Dividing the knob before this curve attenuates considerably more than halving the audio would. It also compresses the usable return range. This line predates the recent work: current blame attributes it to `e8df549` (20 February 2026).

The internal feedback receives the raw reverb output (`SquiggleBoy.hpp:711–713`), before the return fader. Cross-effect sends also use raw returns (`QuadMixer.hpp:82`). Consequently, turning feedback high enough can build a large internal signal while the listening return stays attenuated. This explains why feedback can seem necessary to obtain an adequate wet level.

### Remaining reverb gain stages

The eight parallel input all-pass filters are **averaged**, not cascaded (`DelayLine.hpp:653–662`). Although each all-pass has unity magnitude response individually, their phase differences cause cancellation when averaged. The measured impulse energy is `0.439999955`, equivalent to **−3.565 dB** total energy. This is a topology/tuning choice rather than an established arithmetic bug; replacing the division by eight with another normalization would also change peaks and frequency response.

The four-channel Hadamard transform is correctly normalized by two and preserves total channel energy. The normalized saturator has a small-signal gain of `1.0733945`, or **+0.615 dB**; its constructor argument `0.5` does not halve quiet signals. The damping filter is another frequency-dependent loss. The former all-pass initialization out-of-bounds bug was already fixed in `c61f68e`.

A deterministic five-second white-noise probe used one input channel, amplitude ±0.01, default reverb time/damping, no modulation, and the previous output as feedback. Parameters were settled first; energy was measured over the final three seconds and summed across the four output channels:

| Feedback knob | Actual feedback multiplier | Raw wet gain | Gain after maximum return |
|---|---:|---:|---:|
| 0% | 0.000000 | −7.19 dB | −21.96 dB |
| 75% (default) | 0.655649 | −5.37 dB | −20.13 dB |
| 95% | 1.103692 | +48.20 dB | +33.44 dB |
| 100% | 1.250000 | +51.27 dB | +36.51 dB |

The last two rows are nonlinear feedback buildup, not useful linear transfer gains or recommended settings. The audible result also depends on source spectrum, spatial distribution, send level, and the master chain, which this probe excludes.

**Recommended correction:** restore a full return-control range. If attenuation is musically necessary, make it a deliberate audio gain stage after mapping the knob. Existing patches will become louder if this cap is removed; audition that change separately from diffuser or feedback retuning.

## 2. Partial Machine skips the fourth-to-first interpolation segment

**Priority: high. Confirmed discontinuity.**

`private/src/FrequencyDependentParameter.hpp:174–179` multiplies the coordinate by four, but wraps the segment index modulo **three** (`x_numParameters - 1`). Both parameter interpolators already support a fourth segment through modulo-four endpoint lookup.

The current sequence is:

```
lane 1 → lane 2 → lane 3 → lane 4 → abrupt jump to lane 1
```

For constant mapping frequency `1` and parameter lanes `[1, 2, 4, 8]`, the actual header produces:

| Log-frequency coordinate | Frequency at 48 kHz | Interpolated value |
|---:|---:|---:|
| 0.74998999 | 78.833519 Hz | 7.99977779 |
| 0.75000000 | 78.834038 Hz | 1.00000000 |
| 0.75001001 | 78.834557 Hz | 1.00002778 |

The negative-coordinate wrap has the same defect. This is unrelated to ordinary logarithmic interpolation or the anchor lookup. It affects differing lanes; identical lanes conceal it. Pitch, bandwidth, envelopes, density, unison, and spatial parameters can therefore jump as the mapped coordinate crosses a wrap. Both spectral atoms and residual buckets use this provider.

**Recommended correction:** wrap over four segments, including lane 4 → lane 1. Add independent continuity expectations at every boundary, negative coordinates, and unequal lanes. Do not test only that two entry points return the same index; both currently share the defect. Correcting the period will change existing frequency patterns.

## 3. Partial Machine pitch shifting uses the wrong inter-frame phase advance

**Priority: high. Confirmed synthesis defect.**

`private/src/PartialMachine.hpp:223–228` writes the shifted frequency into each synthesis frame, but obtains phase from the unshifted atom. `SpectralModel.hpp:188–190` then advances that phase by `hop × unshifted frequency`. Overlapping frames therefore disagree about the phase of the shifted tone.

A probe used the actual `SynthesisContext::ProcessAtom`, `QuadDFT`, and `QuadOLA`, with one stable atom, unity reduction, no unison, and an octave-up shift. A 4096-sample FFT measured the steady output:

| Source tone | Expected octave-up tone | Current result |
|---|---|---|
| Bin 16: 187.5 Hz | Bin 32: 375 Hz | Correct by coincidence: hop phase increments differ by whole cycles |
| Bin 17: 199.21875 Hz | Bin 34: 398.4375 Hz | Peak at bin 33: 386.71875 Hz; intended bin nearly absent |
| Bin 18: 210.9375 Hz | Bin 36: 421.875 Hz | Almost complete cancellation: RMS `0.000001494` instead of `0.432946655` |

Changing only the next-frame phase advance in the diagnostic to use the shifted frequency restored the expected frequency and level. Production code remains untouched. These are isolated atom-synthesis results; the full processor's residual output can mask the disappearance.

**Recommended correction:** maintain synthesis phase consistent with emitted frequency, including pitch and detune. Test non-integer hop phase increments and parameter changes. A robust modulation fix needs persistent phase accumulation; multiplying an accumulated phase by a changing ratio can introduce new discontinuities.

## 4. Residual extraction subtracts only half of a tracked sinusoid

**Priority: medium. Confirmed normalization defect.**

`private/src/SpectralModel.hpp:537` subtracts a windowed partial using the detected Hann-windowed peak magnitude. `AdaptiveWaveTable.hpp:202` multiplies that magnitude by the Hann kernel, whose center is approximately 0.5. The detected peak already includes the analysis window's attenuation, so the subtraction removes only half the peak.

Using the same Hann preprocessing as `PartialMachine::Process`, a clean, bin-centered 0.1-amplitude sine at bin 32 produced:

- Original peak magnitude: `0.025000002`.
- Tracked atom magnitude: `0.025000006`.
- Residual peak magnitude: `0.012500299` — **50.0012% remains**.
- Diagnostic subtraction with twice the magnitude: `0.000000596` remains.

`PartialMachine.hpp:262–266` resynthesizes residual buckets with a new random phase each hop. Thus even a tracked pure tone feeds this random-phase layer. This provides a concrete source of added noise-like energy around tonal input; the perceived character was not listening-tested.

**Recommended correction:** establish a consistent amplitude/phase contract between analysis peak estimation and windowed reconstruction. The doubled subtraction establishes the scale error for a bin-centered sine; off-bin tones also need reconstruction tests before declaring a general fix.

## Additional observations

- **Latent gain bug:** `PartialMachine.hpp:205–211` applies organic/synthetic gain to `reduction` after `reducedMagnitude` and the feedback magnitude have been computed. The updated value is never used. Setting organic gain to zero and one produced the identical emitted bin magnitude, `0.019133721`. Current UI wiring disables synthetic harmonics and forces organic gain to one, so this is dormant in the normal exposed path.
- **Residual control scope:** residual synthesis uses reduction, radius, azimuth, and reduction feedback, but does not apply pitch shifting or unison. Those controls currently affect tracked atoms only. This becomes especially noticeable when tracked tones leak into the residual layer. Whether residuals should follow pitch/unison is a product decision.
- **Partial Machine input:** all four channels are summed to mono before analysis. Opposite-polarity channels can cancel and correlated channels add. This is documented behavior, not a newly established defect.
- **Documentation drift:** `docs/partial-machine.md` says full spatial radius is reached at twice the bass cutoff; current code uses eight times the cutoff. It describes seven unison copies, while the implementation uses five total voices. `docs/quad-delay.md` describes loop selection as belonging to `WriteTapeHead`; current selection is in `ReadTapeHead`.

## Delay status and test gaps

An actual `QuadDelay::Process` probe used a steady 210.9375 Hz tone, fixed 8192-sample read lag, forward unit-speed heads, nearly open damping, zero feedback, and no modulation. With no pitch shift, output remained at the correct frequency and measured **+0.615 dB**. At a full octave-up shift, it moved to 421.875 Hz and measured **+0.614 dB**. That matches the normalized saturator gain and provides no evidence of a reverb-like return cap or the Partial Machine phase defect in this configuration.

The grain output's `/ 1.5` is consistent with Hann analysis plus Hann synthesis at quarter-window hops. It should not be removed as an apparent arbitrary gain cut. Extreme time warping, reverse playback, transients, long sessions, and device CPU/memory performance were not comprehensively measured here.

Coverage issues found:

- No dedicated quad-reverb gain/impulse regression test was found.
- The delay test named “impulse in → energy arrives” actually returned **exactly zero** in this run and passed: it asserts only finiteness and an upper bound (`private/test/unit/dsp_quaddelay.cpp:178–197`). This does not prove the whole delay is silent; the sustained-tone probe demonstrates output. A delayed pulse away from the startup boundary and an explicit lower energy bound are needed.
- Existing Partial Machine tests largely check finite/bounded output. They do not establish shifted pitch accuracy or frequency-wrap continuity with unequal lanes.
- The residual subtraction test only requires “less than the original magnitude,” and its input omits the production Hann window. It cannot establish correct tonal cancellation.

## Verification and reproduction

The standalone CMake test target configured and built successfully with assertions enabled. Selected results:

- **40/40 tests, 95,945 assertions passed:** delay line, quad delay, Partial Machine, spectral model, and TimeRig unit files.
- **2/2 tests, 455 assertions passed:** delay setter absolute-time wiring and Partial Machine azimuth wiring.
- **Saved-patch integration test failed:** `PartialMachine: espace etale patch remains finite after load` aborted with `assert(phi_vps < 1)` in `private/src/VectorPhaseShaper.hpp:340`. The selected run stopped there. This is a production-source assertion during patch playback, not a build/harness failure. Its root cause and relation to the effects have not been established.

The full suite is therefore **not claimed green**. No tests or assertions were relaxed.

The self-contained [diagnostic source](effects-audit-probe.cpp) and [measured output](effects-audit-results.txt) accompany this report. The diagnostic includes explicitly marked reference experiments; it is not a production patch or a regression test with pass/fail expectations.

Run from the repository root:

```sh
clang++ -std=c++17 -O2 -Iprivate/src -Iprivate/test docs/audits/effects-audit-probe.cpp -o /tmp/smartgrid-effects-audit
/tmp/smartgrid-effects-audit
cmake -S private/test -B /tmp/smartgrid-effects-audit-build
cmake --build /tmp/smartgrid-effects-audit-build -j 6
/tmp/smartgrid-effects-audit-build/smartgrid_tests --source-file='*/unit/dsp_quaddelay.cpp,*/unit/dsp_partialmachine.cpp,*/unit/dsp_delayline.cpp,*/unit/dsp_spectralmodel.cpp,*/unit/time_rig.cpp' --no-colors
/tmp/smartgrid-effects-audit-build/smartgrid_tests --test-case='AbsoluteTime: delay setter*,SquiggleBoy partial machine*' --no-colors
/tmp/smartgrid-effects-audit-build/smartgrid_tests --test-case='PartialMachine: espace etale patch remains finite after load' --no-colors
```

The diagnostic allocates the real delay buffers, approximately 768 MiB of audio and inverse-time storage. Run it on the development machine.

Suggested repair order: restore the reverb return range; repair the four-segment wrap; correct Partial Machine phase accumulation; correct residual reconstruction; strengthen audible-output tests and resolve the saved-patch assertion. Audition each behavioral change independently with existing patches.
