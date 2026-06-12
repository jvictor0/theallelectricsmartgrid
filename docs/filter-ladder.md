# Ladder Filter Machine (4-Pole)

The **4-Pole Ladder Filter Machine** is one of the two filter options available per voice in the DSP engine (`FilterSection` in `private/src/SquiggleBoy.hpp`).

## Structure

This filter machine consists of two 4-pole filters in series:
1. **Low-Pass Filter**: A 4-pole transistor ladder filter emulation (`LadderFilterLP`).
2. **High-Pass Filter**: A 4-pole linear State Variable Filter (SVF) configured as a high-pass (`LinearSVF4PoleHighPass`).

The audio signal from the source machine passes through the low-pass ladder filter first, and the output is then processed by the high-pass SVF.

## Features

- **Cutoff Tracking**: The HP cutoff frequency tracks the base frequency of the voice's oscillator (`m_vcoBaseFreq`) multiplied by `m_hpCutoffFactor`. The LP cutoff is computed relative to the HP cutoff: `vcoBaseFreq * hpCutoff * lpCutoff`, so the LP is anchored relative to the HP. The `lpCutoffFactor` is an `ExpParam(0.25, 1024)`, so the LP can sit well above the HP anchor; it is not bounded below the HP.
- **Resonance**: Both filters feature adjustable resonance (`m_lpResonance` and `m_hpResonance`).
- **Saturation**: The ladder filter includes an internal saturation stage, controlled by `m_saturationGain`, which drives the filter into non-linear distortion.
- **Sample Rate Reduction**: After the filtering stages, the signal passes through a sample rate reducer (`SampleRateReducer`), adding digital aliasing and grit.

## Related
- [DSP Overview](dsp-overview.md)
- [SVF Filter Machine (2-Pole)](filter-svf.md)
