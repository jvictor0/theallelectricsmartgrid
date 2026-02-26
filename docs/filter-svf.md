# SVF Filter Machine (2-Pole)

The **2-Pole SVF Filter Machine** is one of the two filter options available per voice in the DSP engine (`FilterSection` in `private/src/SquiggleBoy.hpp`).

## Structure

This filter machine consists of two 2-pole State Variable Filters (SVF) in series, interspersed with non-linear saturation stages:
1. **Low-Pass Filter**: A 2-pole linear SVF configured as a low-pass (`LinearStateVariableFilter`).
2. **High-Pass Filter**: A 2-pole linear SVF configured as a high-pass (`LinearStateVariableFilter`).

The audio signal from the source machine passes through the low-pass SVF first, and the output is then processed by the high-pass SVF.

## Features

- **Cutoff Tracking**: The HP cutoff frequency tracks the base frequency of the voice's oscillator (`m_vcoBaseFreq`) multiplied by `m_hpCutoffFactor`. The LP cutoff is computed relative to the HP cutoff: `vcoBaseFreq * hpCutoff * lpCutoff`, so the LP is always at or below the HP.
- **Resonance**: Both filters feature adjustable resonance (`m_lpResonance` and `m_hpResonance`).
- **Saturation**: The SVF machine features a `TanhSaturator` placed before the low-pass filter, between the low-pass and high-pass filters, and after the high-pass filter. This drives the filters into non-linear distortion, controlled by `m_saturationGain`.
- **Envelope**: The filter section has its own phase-driven AHD envelope (`m_ahd`). Its output is exposed as a modulation source (modulator index 4) in the encoder system, so it can be routed to any parameter including cutoff.
- **Sample Rate Reduction**: After the filtering stages, the signal passes through a sample rate reducer (`SampleRateReducer`), adding digital aliasing and grit.

## Related
- [DSP Overview](dsp-overview.md)
- [Ladder Filter Machine (4-Pole)](filter-ladder.md)
