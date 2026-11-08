## MODIFIED Requirements

### Requirement: Voice LFO Integration into the Modulation Matrix
The system SHALL run two SquiggleLFO instances per voice and publish their raw (pre-amplitude-centering) outputs and amplitudes to the corresponding globally indexed manager sources; the instrument's default voice groups SHALL gather these sources into slots 6 and 7; encoder cells consume these per-voice values as modulation sources (see encoder-parameter-system).
The LFO also exposes a top flag, true when every weighted contributing loop reports its top (cycle start), used to restart the LFO scope display.

#### Scenario: LFO modulates a routed parameter per voice
- **WHEN** voice 4's LFO 1 outputs 0.8 and a parameter has modulation slot 6 at nonzero depth
- **THEN** the parameter's voice-4 channel output reflects 0.8 through the crossfade modulation math while other voices follow their own LFO values

#### Scenario: Top flag marks cycle start
- **WHEN** all loops with nonzero mix weight report their top on the same control sample
- **THEN** the LFO's top flag is true on that sample and the scope recording restarts

### Requirement: Quad LFO Integration into the Modulation Matrix
The system SHALL run the two quad LFOs as four-lane `SquiggleLFOGeneric<4>` instances, one lane per quad channel, controlled from the `QuadLFOs` encoder bank through interpolated four-lane encoder reads (`GetValue<4>(param, 0)`). Each lane SHALL match an independent mono LFO given that channel's parameters, trigger and phase offset. The system SHALL publish their raw (pre-amplitude-centering) outputs and amplitudes to the corresponding globally indexed manager sources; the instrument's default Quad group SHALL gather these sources into slots 6 and 7.
Phase-shift knobs SHALL offset channel `i` by `knob × i / 4`, so full spread places the four channels a quarter-cycle apart. Each channel's Sample-and-Hold trigger SHALL be the modulated cycle-boundary crossing of that channel's active delay read-head loop (`ReadTapeHead::m_loopSelector`), not the merely requested delay loop index.

#### Scenario: Quarter-cycle channel spread
- **WHEN** the quad LFO 1 phase-shift knob is 1.0
- **THEN** channels 0, 1, 2, and 3 are offset by 0, 0.25, 0.5, and 0.75 cycles

#### Scenario: Delay-loop top captures Sample-and-Hold
- **WHEN** channel 2's active delay loop reports a modulated cycle crossing and `m_shFade` is 1
- **THEN** that channel's pre-slew Sample-and-Hold value updates to the live mixed output on that sample and holds until the next crossing of the same active loop
