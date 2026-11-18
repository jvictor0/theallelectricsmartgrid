# SVF Filter Machine (2-Pole)

The **2-Pole SVF Filter Machine** is one of three filter options available per voice in the DSP engine (`FilterSection` in `private/src/SquiggleBoy.hpp`), alongside the Naive and Conservative Reupping Ladders.

## Structure

This filter machine consists of two 2-pole State Variable Filters (SVF) in series, interspersed with non-linear saturation stages:
1. **Low-Pass Filter**: A 2-pole linear SVF configured as a low-pass (`TPTSVFLowPassFilterGeneric`).
2. **High-Pass Filter**: A 2-pole linear SVF configured as a high-pass (`TPTSVFHighPassFilterGeneric`).

The audio signal from the source machine passes through the low-pass SVF first, then the sample rate reducer, and the reduced signal is then processed by the high-pass SVF. The high-pass follows the reducer so it also removes low-frequency content the reducer creates.

## Features

- **Cutoff Tracking**: The HP cutoff frequency tracks the base frequency of the voice's oscillator (`m_vcoBaseFreq`) multiplied by `m_hpCutoffFactor`. In Hz, the LP cutoff is `261.6256 * (vcoHz / 261.6256)^0.707 * hpCutoffFactor * lpCutoffFactor`. LP tracks 0.707 octaves per VCO octave around middle C (261.6256 Hz), while the HP cutoff factor multiplies both cutoffs linearly. At a middle-C VCO frequency, the original LP/HP relationship is preserved for every HP cutoff factor. The `lpCutoffFactor` remains an `ExpParam(0.25, 1024)`; both cutoffs are independently clamped at the base-rate Nyquist frequency.
- **Resonance and cutoff**: The generated SVFs retain the app mapping `Q = 0.5 * 11^resonance`. Cutoff is the prewarped natural frequency (`g = tan(pi * cutoff)`), so each LP/HP section has magnitude Q at cutoff: -6.02 dB at zero resonance, and -3.01 dB only at Q = 1/sqrt(2). The wrappers preserve the engine-rate 0.499 cutoff ceiling. Coefficients use `Math4096` trig and generated audio uses explicit FMA.
- **Saturation**: The SVF machine features a `TanhSaturator` placed before the low-pass filter, between the low-pass filter and the sample rate reducer, and after the high-pass filter. This drives the filters into non-linear distortion, controlled by `m_saturationGain`.
- **Sample Rate Reduction**: Between the low-pass and high-pass stages, a sample rate reducer (`SampleRateReducer`) adds digital aliasing and grit. Aliases folded below the high-pass cutoff are attenuated by the high-pass.

## Related
- [DSP Overview](dsp-overview.md)
- [Naive Reupping Ladder](filter-naive-reupping-ladder.md)
