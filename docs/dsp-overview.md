# DSP Overview

The DSP engine (implemented in `private/src/SquiggleBoy.hpp` and related files) is built around a polyphonic voice architecture feeding into a quadraphonic effects and mixdown section.

## Voice Architecture

The synthesizer consists of 9 voices. Each voice (`SquiggleBoyVoice`) is a complete synthesizer chain containing:

1. **Source Machine**: The sound generator. Currently, this can be either a **Dual Wave Shaping VCO** (dual wavetable oscillator) or **Thru** (external audio input).
2. **Filter Machine**: Shapes the frequency content. Currently supports two modes: a 2-pole State Variable Filter (SVF) or a 4-pole Ladder Low-Pass Filter paired with a 4-pole SVF High-Pass Filter.
3. **Amp Section**: Controls the final volume of the voice using a phase-driven AHD envelope.
4. **Sub Oscillator**: A simple sub-oscillator running one octave below the main pitch, mixed in parallel with the main signal.

## Quadraphonic Routing and Effects

The outputs of the 9 voices are panned quadraphonically using a Lissajous LFO. The mixed quadraphonic signal is then sent to the send effect buses and final output,  where the send effects consist of:
- **Quad Delay**: A sophisticated, time-warped phase vocoder delay.
- **Quad Reverb**: A quadraphonic reverberation unit.

Finally, the system generates parallel quadraphonic and stereo mixdowns, along with a dedicated sub channel, all glued together by a final multiband saturator.

## Source Machines

Source machine details now live in a dedicated page:

- [Source Machine](source-machine.md) — Dual Wave Shaping VCO (dual wavetable VPS source) and Thru input mode.
- [Filter Architecture](filter-architecture.md) — Explanation of the filter layout, DSP classes, transfer functions, and UI state synchronization.