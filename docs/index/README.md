# Smart Grid One — Documentation Index

This is an experimental synthesizer.

It runs on both **macOS** and **iPad** using [JUCE](https://juce.com/).

It interfaces with a custom MIDI controller.

## Major components

- **[Theory of Time](../theory-of-time.md)** — Global clock: phase on S¹, six time loops, gates, monodromy. Everything else follows this clock; sequencer and LFO state are a *pure function* of it (statelessness).
- **[LameJuis](../lamejuis.md)** — Esoteric sequencer: lens U, sheaf F^M_x(U), index arp, logic operations → M(x) in just intonation; one output per trio; stateless in time.
- **[Multi-Phasor Gate](../multi-phasor-gate.md)** — When the Nonagon emits a trigger per voice: trigger logic (pitch-changed / sub-trigger, mutes, interrupt) and phasor-based gate timing (50% duty per step).
- **[Nonagon (sequencer)](../nonagon.md)** — Theory of Time, voice/arp logic ([LameJuis](../lamejuis.md)), [trigger decision](../multi-phasor-gate.md) (Multi-Phasor Gate), note writer, and related UI.
- **[DSP Overview](../dsp-overview.md)** — Voice architecture (Dual VCO, Physical Modeling, Thru, Filters, Amp, Sub) and quadraphonic effects routing.
  - **[Source Machine](../source-machine.md)** — Dual Wave Shaping VCO (dual VPS wavetable oscillators), Physical Modeling, and Thru source modes.
  - **[Filter Architecture](../filter-architecture.md)** — Explanation of the filter layout, DSP classes, transfer functions, and UI state synchronization.
  - **[Ladder Filter Machine](../filter-ladder.md)** — 4-pole Ladder LP and 4-pole SVF HP.
  - **[SVF Filter Machine](../filter-svf.md)** — 2-pole SVF LP/HP with saturation.
  - **[Sub Oscillator](../sub-oscillator.md)** — Per-voice mono sub signal, saturation, and unison behavior.
  - **[Quad Panning](../quad-panning.md)** — Global phase, voice offsets, and Lissajous LFO.
  - **[AHD Envelopes](../ahd-envelopes.md)** — Phase-driven envelopes synchronized to the clock.
  - **[PolyXFader LFOs](../polyxfader-lfos.md)** — Phase-synced LFOs mixing time loops.
  - **[Quad Delay](../quad-delay.md)** — Time-warped phase vocoder delay with inverse buffer and pitch shifting.
  - **[Phase Vocoder](../phase-vocoder.md)** — Grain-based pitch-preserving resynthesis used by the warped delay.
  - **[Quad Reverb](../quad-reverb.md)** — Quadraphonic algorithmic reverb with APF diffusion and LFO modulation.
  - **[Mixdown & Mastering](../mixdown-mastering.md)** — Quad/Stereo/Sub routing and Multiband Saturator.
  - **[Deep Vocoder](../deep-vocoder.md)** — Spectral analysis quantizer forcing sequenced pitches to match incoming audio partials.
- **[Encoder System](../encoder-system.md)** — Software-defined parameter system with deep polyphonic modulation, macro gestures, scene morphing, and parameter slewing.
- **[Smart Grid Integration](../smart-grid.md)** — Hardware-agnostic controller mapping across pad/encoder controllers.
  - **[Scene Manager](../scene-manager.md)** — Scene selection, crossfading, and scene-change signaling.
  - **[State Saver](../state-saver.md)** — Scene-aware JSON persistence and restore.
- **LED subsystem** — Grids of LEDs used as interface (to be documented).
- **UI + MIDI Integrations**
  - **[UI Components and Layout](../ui-components-layout.md)** — JUCE pad/encoder/fader/joystick components and `WrldBuildrComponent` composition.
  - **[Message Routing and Smart Buses](../ui-routing-buses.md)** — `PadUI`, `MessageInBus`, route typing, and `SmartBusColor` transport.
  - **[Controller Integrations](../ui-controller-integrations.md)** — Wrld.Bldr and QuadLaunchpadTwister integration architecture.
  - **[MIDI I/O Scheduling](../ui-midi-scheduling.md)** — `MidiInputHandler`, `MidiOutputHandler`, and `MidiSender` realtime dispatch.
  - **[Visualization Pipeline](../ui-visualization-pipeline.md)** — `ScopeWriter` capture, FFT analysis, and JUCE visual components.
  - **[Smart Grid Visualizers](../smart-grid-visualizers.md)** — Visualizer architecture, bank mapping, and adding new visualizers.
- **Configuration** — (to be documented).

## Building

The project can be built using the `make` file located in the `JUCE/SmartGridOne` directory.

```bash
cd JUCE/SmartGridOne
make
```

---

See also: [Glossary](../glossary.md)
