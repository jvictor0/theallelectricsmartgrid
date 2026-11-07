## Context

Amp AHD uses inverted amplitude polarity: `output = (1 - depth) + raw * depth`. Its existing release closes raw output but leaves the drone floor when depth is below one. The preceding tempo-relative release change exposes the basic clock increment while stopped.

## Goals / Non-Goals

**Goals:** A simple monotone gate envelope, smooth removal of the drone floor on mute/stop, shared amp timing parameters, and independent voice lanes.

**Non-Goals:** New knobs or persistence, early-mute/unison/trigger logic in the gate, extra release triggers, direct multiplication of the audio output, or modulation after ToT stops.

## Decisions

- Add scalar `ASD` plus `ASDGeneric<Size>` using AHD's control/input/processing pattern. The gate is persistent. A double accumulator starts at zero; gate high adds `abs(dt) * attackIncrement` up to one, gate low subtracts `abs(dt) * decayIncrement` down to zero. No stage enum, amplitude, sustain parameter, or trigger is needed.
- Store signed modulated phase movement once in the ToT sample, with a query for consumers. Normal running samples use current minus previous modulated global phase. Startup uses one basic clock increment; stopped samples report zero modulated movement. ASD explicitly uses the existing basic increment while stopped, preventing phase resets from becoming elapsed travel.
- Convert global phase movement into reference samples with the amp AHD input's ratio and envelope period. Copy those fields, the clock pointer, and the attack/decay increments during amp processing. This avoids duplicated parameter mapping and keeps ASD on the amp's current timing scale.
- Wire the control to `theoryOfTime.m_sample.m_running && !voiceMuted` only. ToT can keep running briefly after the user presses Stop while note gates finish; this follows the explicitly requested ToT condition.
- Process ASD before amp AHD and use the confirmed depth formula `1 - (1 - amp) * muteenv`. Restore the original input depth after processing; the AHD's reported amplitude and scope output reflect the effective depth. Shared event consumption remains unchanged.

## Risks / Trade-offs

- Existing tests that intentionally drive a drone while transport is off no longer represent audible voices → explicitly open their test gate or start transport where audio is the purpose of the test.
- A fully stationary modulated clock supplies zero running dt → ASD holds its value until time moves; reversing time still moves it toward the gate-selected endpoint.
- Full depth alone does not force a still-running raw AHD to zero → existing mute/stop releases retain responsibility for that raw tail, as specified by the requested blend.

## Migration Plan

No patch migration. Validate scalar ASD, amp depth blending, all nine gate lanes, stopped tempo behavior, and the full core suite in the existing build directory.

## Open Questions

None. The user confirmed the corrected depth polarity.
