## Why

An amp envelope with less than full depth leaves a drone floor even after its raw AHD output closes. Muted voices and stopped transport need a smooth transition to full envelope depth so that floor closes too.

## What Changes

- Add a simple ASD envelope with a persistent gate, attack and decay increments, and a clamped 0–1 output; no amplitude or sustain-level parameter.
- Integrate absolute modulated ToT increments while running, using the basic clock while stopped.
- Add an ASD to each amp lane, copying the amp AHD's increments and timing scale.
- Drive its gate solely from ToT running and the voice being unmuted.
- Process amp depth as `1 - (1 - amp) * muteenv`, preserving the original setting for subsequent calls.

## Capabilities

### New Capabilities

- `asd-envelopes`: Gate-driven monotone attack/decay from absolute ToT increments.

### Modified Capabilities

- `voice-architecture`: Per-voice muting ASD controls effective amp envelope depth.
- `phasor-timebase`: Expose signed per-sample modulated global phase increments with explicit transport-boundary behavior.

## Impact

`ASD.hpp`, `TheoryOfTimeBase.hpp`, amp processing, Nonagon-to-voice control wiring, core tests, and envelope documentation. Builds on the existing `tempo-relative-envelope-release` change. No new controls, patch fields, early-mute rules, or trigger rules.
