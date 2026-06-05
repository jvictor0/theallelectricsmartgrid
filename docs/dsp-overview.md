# DSP Overview

The DSP engine (implemented in `private/src/SquiggleBoy.hpp` and related files) is built around a polyphonic voice architecture feeding into a quadraphonic effects and mixdown section.

## Voice Architecture

The synthesizer consists of 9 voices. Each voice (`SquiggleBoyVoice`) is a complete synthesizer chain containing:

1. **Source Machine**: The sound generator. Currently, this can be **Dual Wave Shaping VCO** (dual wavetable oscillator), **Physical Modeling** (noise-excited SVF + one-pole damping comb), **Thru** (external audio input), or **Sample** (per-voice sample-directory source using grain resynthesis).
2. **Filter Machine**: Shapes the frequency content. Currently supports two modes: a 2-pole State Variable Filter (SVF) or a 4-pole Ladder Low-Pass Filter paired with a 4-pole SVF High-Pass Filter.
3. **Amp Section**: Controls the final volume of the voice using a phase-driven AHD envelope (`m_ahd`). It also houses a second phase-driven envelope (`m_modulationAHD`), exposed as a modulation source in the encoder system.
4. **Sub Oscillator**: A simple sub-oscillator running one octave below the main pitch, mixed in parallel with the main signal. See [Sub Oscillator](sub-oscillator.md) for mono routing, saturation, and unison behavior.

## Quadraphonic Routing and Effects

The outputs of the 9 voices are panned quadraphonically using a Lissajous LFO. The mixed quadraphonic signal is then sent to the send effect buses and final output, where the send effects consist of:
- **Quad Delay**: A sophisticated, time-warped phase vocoder delay.
- **Quad Reverb**: A quadraphonic reverberation unit.
- **Partial Machine**: A spectral send effect that converts its quad input to mono, tracks partials, and resynthesizes them into a frequency-dependent quad field.

Finally, the system generates parallel quadraphonic and stereo mixdowns, along with a dedicated sub channel, all glued together by a final multiband saturator.

## Source Machines

Source machine details now live in a dedicated page:

- [Source Machine](source-machine.md) — Dual Wave Shaping VCO, Physical Modeling, Thru, and Sample source modes.
- [Filter Architecture](filter-architecture.md) — Explanation of the filter layout, DSP classes, transfer functions, and UI state synchronization.
- [Partial Machine](partial-machine.md) — Spectral mono-to-quad send effect with frequency-dependent per-partial parameters.