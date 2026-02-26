# Quadraphonic Reverb

The **Quad Reverb** (`QuadReverb` in `private/src/QuadReverb.hpp`) is the secondary global effect in the Smart Grid One, providing dense, quadraphonic spatialization.

## Structure

Unlike the highly experimental Phase Vocoder Quad Delay, the Quad Reverb follows a more traditional algorithmic reverb topology, scaled up to four channels:

1. **Input Diffusion**: The incoming quadraphonic signal is first passed through a bank of 8 parallel All-Pass Filters (`QuadParallelAllPassFilter<8>`). This rapidly diffuses the initial transients, smearing them into a dense cluster of early reflections. The delay times for these APFs are chosen as prime numbers (e.g., 37, 61, 113...) to minimize metallic ringing.
2. **Pre-Feedback All-Pass**: The diffused signal passes through another quadraphonic All-Pass Filter (`m_preFeedbackFilter`) before entering the main delay loop.
3. **Main Delay Line**: A large quadraphonic delay line (`QuadDelayLine`) provides the core reverberant tail. The delay times are modulated by a dedicated `QuadLFO` to add chorus and prevent resonant build-up.
4. **Post-Feedback Filtering**: Inside the feedback loop, the signal passes through a `PostFeedbackFilter` block consisting of:
   - An All-Pass Filter (`m_apf1`).
   - A Band-Pass Filter (`QuadOPBaseWidthFilter` acting as a damping stage to roll off high and low frequencies).
   - A second All-Pass Filter (`m_apf2`).
5. **Saturation**: A `TanhSaturator` prevents the feedback loop from blowing up and adds a touch of warmth to the reverb tail.

## Features

- **Quadraphonic Mixing (Hadamard Transform)**: Inside the feedback loop, the reverb applies a Hadamard matrix transform (`output.Hadamard()` in `TransformOutput`). This mixes the four channels together in an orthogonal fashion, diffusing energy evenly and avoiding simple left/right or front/back swaps. The result is a dynamic, spatially immersive reverberant field with enhanced envelopment and movement. The Quad Delay uses a different approach (`RotateLinear` with a continuous `m_rotate` parameter) for its channel mixing.
- **Modulation**: The `QuadLFO` modulates the main delay lines, providing lush, chorused tails.
- **Damping**: The internal band-pass filter allows for precise control over the decay characteristics of high and low frequencies.

## Related
- [Quad Delay](quad-delay.md)
- [DSP Overview](dsp-overview.md)