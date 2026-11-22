# Quadraphonic Reverb

Quad Reverb is the second global quad send effect in SmartGridOne. The audio core is generated from [the Lean reverb](../formal/DSP/Filters/SquiggleBoyQuadReverb/Loop.lean). The [app adapter](../private/src/QuadReverb.hpp) maps knobs to physical settings and publishes the generated damping response for the analyzer.

## Signal path

1. The input passes through two parallel banks of four cascaded Schroeder all-passes. The two bank outputs are averaged.
2. The diffused signal is summed with the previous wet output scaled by feedback. A normalized saturator processes the sum before it enters the four-channel delay line.
3. Cubic reads from the main delay line feed an orthogonal Givens mixer, then a wandering all-pass, a high-pass/low-pass damping stage, and a second wandering all-pass. The last stage supplies the wet output and the next feedback return.

All ten APFs use gain 0.6 and a seeded spline to wander by up to 128 samples around their nominal taps. The spline rate parameter is 1,920,000 samples and the modulator is evaluated every 32 samples with linear interpolation between those evaluations. The audio tap reads use cubic interpolation.

## Controls

The engine runs the reverb at 192 kHz. The main ring holds 262,144 samples per channel. The reverb-time knob uses a 1 Hz slew followed by an exponential map from 240 to 245,760 samples and fixed per-channel widening. The delay-offset knob adds −4,000 to +4,000 samples, with zero at 0.5. The offset accepts ordinary parameter modulation on each engine sample.

The feedback knob uses a zeroed exponential map scaled by 1.25. Damping base maps exponentially from 23.4375 Hz to 24 kHz. Damping width is an exponential multiplier from 1 to 2,048. The reverb return and sends to Quad Delay and Partial Machine are handled in the mixer.

## Related

- [Quad Delay](quad-delay.md)
- [Partial Machine](partial-machine.md)
- [DSP Overview](dsp-overview.md)
