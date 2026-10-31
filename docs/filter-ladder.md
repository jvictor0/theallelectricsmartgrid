# Ladder Filter Machine (4-Pole)

The **4-Pole Ladder Filter Machine** is one of the two filter options available per voice in the DSP engine (`FilterSection` in `private/src/SquiggleBoy.hpp`).

## Structure

This filter machine consists of two 4-pole filters in series:
1. **Low-Pass Filter**: A 4-pole transistor ladder filter emulation (`LadderFilterLP`).
2. **High-Pass Filter**: A 4-pole linear State Variable Filter (SVF) configured as a high-pass (`LinearSVF4PoleHighPass`).

The audio signal from the source machine passes through the low-pass ladder filter first, and the output is then processed by the high-pass SVF.

## Features

- **Cutoff Tracking**: The HP cutoff frequency tracks the base frequency of the voice's oscillator (`m_vcoBaseFreq`) multiplied by `m_hpCutoffFactor`. In Hz, the LP cutoff is `261.6256 * (vcoHz / 261.6256)^0.707 * hpCutoffFactor * lpCutoffFactor`. LP tracks 0.707 octaves per VCO octave around middle C (261.6256 Hz), while the HP cutoff factor multiplies both cutoffs linearly. At a middle-C VCO frequency, the original LP/HP relationship is preserved for every HP cutoff factor. The `lpCutoffFactor` remains an `ExpParam(0.25, 1024)`; both cutoffs are independently clamped at the base-rate Nyquist frequency.
- **Resonance**: Both filters feature adjustable resonance (`m_lpResonance` and `m_hpResonance`).
- **Saturation**: The ladder filter includes an internal saturation stage, controlled by `m_saturationGain`, which drives the filter into non-linear distortion.
- **Sample Rate Reduction**: After the filtering stages, the signal passes through a sample rate reducer (`SampleRateReducer`), adding digital aliasing and grit.

## Related
- [DSP Overview](dsp-overview.md)
- [SVF Filter Machine (2-Pole)](filter-svf.md)
