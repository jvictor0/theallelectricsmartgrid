## MODIFIED Requirements

### Requirement: Quad LFO Integration into the Modulation Matrix
The system SHALL run the two quad LFOs as four-lane `SquiggleLFOGeneric<4>` instances, one lane per quad channel, controlled from the `QuadLFOs` encoder bank through interpolated four-lane encoder reads (`GetValue<4>(param, 0)`). Each lane SHALL match an independent mono LFO given that channel's parameters, trigger and phase offset. The system SHALL publish their raw (pre-amplitude-centering) outputs and amplitudes to the corresponding globally indexed manager sources; the instrument's four Quad groups SHALL gather these sources into slots 6 and 7.
Phase-shift knobs SHALL offset channel `i` by `knob × i / 4`, so full spread places the four channels a quarter-cycle apart. Each channel's Sample-and-Hold trigger SHALL be the modulated cycle-boundary crossing of that channel's active delay read-head loop (`ReadTapeHead::m_loopSelector`), not the merely requested delay loop index.

#### Scenario: Quarter-cycle channel spread
- **WHEN** the quad LFO 1 phase-shift knob is 1.0
- **THEN** channels 0, 1, 2, and 3 are offset by 0, 0.25, 0.5, and 0.75 cycles

#### Scenario: Delay-loop top captures Sample-and-Hold
- **WHEN** channel 2's active delay loop reports a modulated cycle crossing and `m_shFade` is 1
- **THEN** that channel's pre-slew Sample-and-Hold value updates to the live mixed output on that sample and holds until the next crossing of the same active loop
