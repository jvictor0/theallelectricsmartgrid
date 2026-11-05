# Quadraphonic Reverb

The **Quad Reverb** (`QuadReverb` in `private/src/QuadReverb.hpp`) is the secondary global effect in the Smart Grid One, providing dense, quadraphonic spatialization.

## Structure

Unlike the highly experimental Phase Vocoder Quad Delay, the Quad Reverb follows a more traditional algorithmic reverb topology, scaled up to four channels:

1. **Input Diffusion**: The incoming quadraphonic signal is first passed through a bank of 8 parallel All-Pass Filters (`QuadParallelAllPassFilter<8>`). This rapidly diffuses the initial transients, smearing them into a dense cluster of early reflections. The delay times for these APFs are chosen as prime numbers (e.g., 37, 61, 113...) to minimize metallic ringing.
2. **Pre-Feedback All-Pass**: The feedback return (`input.m_return`) passes through a quadraphonic All-Pass Filter (`m_preFeedbackFilter`); the filtered return is then scaled by the feedback amount and summed with the diffused input before entering the main delay loop.
3. **Main Delay Line**: A large quadraphonic delay line (`QuadDelayLine`) provides the core reverberant tail. Each channel adds a linear delay offset to its widened base delay. The regular modulation system can drive that offset to add chorus and break up resonances.
4. **Post-Feedback Filtering**: Inside the feedback loop, the signal passes through a `PostFeedbackFilter` block consisting of:
   - An All-Pass Filter (`m_apf1`).
   - A Band-Pass Filter (`QuadOPBaseWidthFilter` acting as a damping stage to roll off high and low frequencies).
   - A second All-Pass Filter (`m_apf2`).
5. **Saturation**: A `TanhSaturator` prevents the feedback loop from blowing up and adds a touch of warmth to the reverb tail.

## Rate

The reverb runs entirely at the engine rate. Its delay lengths are written as host-sample constants and multiplied by `SampleTimer::x_oversample`, the main ring holds `65536 * x_oversample` engine samples, and the damping base range is defined in Hz (`DampingFilter::x_minBaseHz` to `x_maxBaseHz`). Reverb time, all-pass delays, delay offsets and damping corners therefore keep their physical values at any oversampling ratio.

The delay-offset knob maps linearly as `(2 * knob - 1) * 1000` host samples: 0 gives -1000, 0.5 gives zero, and 1 gives +1000. The input setter multiplies this by `x_oversample` before adding it to the engine-sample delay. It applies no separate LFO or depth smoothing; regular parameter modulation supplies any motion. The former internal LFO frequency and phase controls are removed.

## Features

- **Quadraphonic Mixing (Hadamard Transform)**: Inside the feedback loop, the reverb applies a Hadamard matrix transform (`output.Hadamard()` in `TransformOutput`). This mixes the four channels together in an orthogonal fashion, diffusing energy evenly and avoiding simple left/right or front/back swaps. The result is a dynamic, spatially immersive reverberant field with enhanced envelopment and movement. The Quad Delay uses a different approach (`RotateLinear` with a continuous `m_rotate` parameter) for its channel mixing.
- **Modulation**: The regular modulation system can drive each channel's centered delay-offset control.
- **Damping**: The internal band-pass filter allows for precise control over the decay characteristics of high and low frequencies.
- **Return Sends**: The reverb output can be routed into the Quad Delay or the Partial Machine.

## Related
- [Quad Delay](quad-delay.md)
- [Partial Machine](partial-machine.md)
- [DSP Overview](dsp-overview.md)
