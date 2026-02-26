# Phase-Driven AHD Envelopes

Unlike traditional synthesizers that use time-based ADSR (Attack, Decay, Sustain, Release) envelopes, the Smart Grid One uses **phase-driven AHD (Attack, Hold, Decay)** envelopes (`AHD` in `private/src/AHD.hpp`).

## Phase-Driven Envelopes

The progression of the envelope is not tied to absolute wall-clock time but rather to the phase of the [Theory of Time](theory-of-time.md) clock. This has profound implications for how the envelopes behave:
- **Stretching**: If the global clock slows down, the envelopes stretch proportionally.
- **Reversing**: If the global clock reverses (e.g., through extreme phase modulation), the envelopes will also play backward.
- **Synchronization**: The envelope is always perfectly synchronized to the musical grid, regardless of tempo changes or modulation.

## Constant Time Configuration

While the envelope is driven by phase, the user interface and parameter system configure the Attack and Decay stages in terms of absolute time (e.g., milliseconds or seconds).

To reconcile this, the system calculates the required phase increments for the Attack and Decay stages based on the *current, unmodulated speed* of the clock. This means that if you set an Attack time of 500ms, it will take 500ms at the current tempo. If you then modulate the tempo (e.g., with an LFO), the 500ms Attack will stretch or compress, but the *base* setting remains constant relative to the unmodulated clock.

## The Hold Stage

The **Hold (H)** stage is handled differently. It is not set in absolute time but is instead relative to the specific time loops affecting the voice.

The length of the Hold stage is derived directly from the voice's gate length, which is determined by the [Multi-Phasor Gate](multi-phasor-gate.md) using the voice's clock loop and any read loops from its lens. This ensures that the sustain portion of the note perfectly matches the rhythmic subdivision assigned to that voice, creating tight, interlocking sequences.

## Related
- [Theory of Time](theory-of-time.md)
- [Multi-Phasor Gate](multi-phasor-gate.md)
- [DSP Overview](dsp-overview.md)