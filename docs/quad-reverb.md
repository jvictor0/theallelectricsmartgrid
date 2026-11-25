# Quadraphonic Reverb

Quad Reverb is the second global quad send effect in SmartGridOne. The audio core and its owning state are generated from [the Lean reverb runtime](../formal/DSP/Filters/SquiggleBoyQuadReverb/Runtime.lean). The [app adapter](../private/src/QuadReverb.hpp) maps knobs to physical settings and publishes the generated damping response for the analyzer.

## Signal path

1. The input passes through eight Schroeder all-passes in series, ordered from shortest to longest tap.
2. At the four-channel delay write, the diffused input joins the previous wet output scaled by the feedback multiplier.
3. Cubic reads from the main delay line feed an orthogonal Givens mixer, then a wandering all-pass, SAT gain and tanh, a high-pass/low-pass damping stage, and a second wandering all-pass. The last stage supplies the wet output and the next feedback return.

All ten APFs use gain 0.6 and a seeded spline to wander by up to 128 samples around their nominal taps. Every 32 samples the phase clock draws an exponential speed with a mean of one knot per 1,920,000 samples. A 48,000-sample one-pole smooths those speeds before the clock advances. The modulator linearly interpolates between frame endpoints. The audio tap reads use cubic interpolation.

## Controls

The engine runs the reverb at 192 kHz. The main ring holds 262,144 samples per channel. The reverb-time knob uses a 1 Hz slew followed by an exponential map from 240 to 245,760 samples and fixed per-channel widening. The delay-offset knob adds −200 to +200 samples, with zero at 0.5. The offset accepts ordinary parameter modulation on each engine sample. The core reads the supplied delay directly and asserts that each channel's delay is positive in assertion-enabled builds.

The feedback knob uses a zeroed exponential map scaled by 1.25. The input setter divides that value by SAT gain, so the coefficient supplied to the core ranges from 0 to 2.5 at minimum SAT. Damping base maps exponentially from 23.4375 Hz to 24 kHz. Damping width is an exponential multiplier from 1 to 2,048. The reverb return and sends to Quad Delay and Partial Machine are handled in the mixer.

SAT at `(0, 2)` maps exponentially from 0.5 to 4 per channel and defaults to 0.5. The output of the saturator is bounded by ±1. The response plot shows the damping stage.

## Small-signal loop

For the real model, SAT applies `tanh(gain * sample)` after the first loop all-pass. The [SAT derivative](../formal/DSP/Filters/SquiggleBoyQuadReverb/SmallSignal.lean) is `gain` around silence. With equal lane settings and ideal frozen all-passes, the input setter's division cancels the SAT gain in the linearized return, leaving the feedback knob's 0–1.25 range before damping. Delay and all-pass phases still determine where the loop reinforces itself. The [scalar mode proofs](../formal/DSP/Analysis/ModulatedMode.lean) bound how far a pair of all-passes can move the loop phase and show that phase motion alone preserves the magnitude of an unforced scalar mode at fixed round-trip gain.

A frozen calculation using the previous normalized saturator and exponential knot clock gave a median 95th-percentile peak contrast of 12.6 dB at 40 ms, 12.6 dB at 1 second, and 11.5 dB at 5 seconds. Averaging the full 40 seconds gave 3.6 dB. This case used a 40 ms nominal main delay, 90 Hz and 9 kHz damping cutoffs, feedback 0.9, and SAT gain 0.5. Contrast was measured against a 30 Hz local median from 200 Hz to 4.5 kHz. The calculation averages frozen power responses and describes one operating point in the stable small-signal regime.

A separate time-domain multisine run used the same settings and the real tanh slope, with fixed intro taps in both cases. Wandering the loop taps reduced median 2-second contrast from 13.2 to 11.9 dB. Over 40 seconds of power averaging, the contrast went from 13.2 to 7.0 dB.

The uniform clock was evaluated by cutting the loop at the damping filter and taking a unity return through the second APF, main delay, mixer, and first APF. The RMS phase chord over approximately one group-delay lag grew from 0.054 to 0.128 at 1 kHz and from 0.146 to 0.359 at 4 kHz. In a matched control-point comparison, redrawing exponential speed on the 32-sample grid, with or without a 250 ms smoothing time, gave nearly the same results as constant speed. These comparisons isolate the phase movement of the linear loop path across eight seed pairs.

The [frozen Quad Reverb analysis](experiments/reverb-frozen/README.md) contains the Lean model links and reproducible wet RMS curves for the per-lane wander implementation.

## Related

- [Quad Delay](quad-delay.md)
- [Partial Machine](partial-machine.md)
- [DSP Overview](dsp-overview.md)
