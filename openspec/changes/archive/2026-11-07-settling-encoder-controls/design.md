## Context

The encoder currently distributes edits over their last inter-arrival interval. Recordings contain approximately 30 Hz bursts with millisecond follow-ups, which collapse pending ramps. The matrix already distinguishes editing and settled paths. The user requires Lean verification before C++ implementation.

## Goals / Non-Goals

**Goals:** A fixed 20 ms one-pole, exact settling at normalized error `1e-6`, retained double-precision state during bursts and reversals, and no pole arithmetic for settled scene values. Settled trees return to cached evaluation. Preserve live modulation, output interpolation, target serialization and same-target patch reload behavior.

**Non-Goals:** Restore output low-pass filtering, change modulation bandwidth, redesign physical encoder acceleration, or claim that continuity proves inaudibility.

## Decisions

1. Run the control filter at the 48 kHz host rate. Its fixed coefficient is `1-exp(-1/(0.020*48000))`; retain the exact binary64 constant in the numerical model. A new target changes no current/filter state. Snap after a step when absolute residual is at most `1e-6`.
2. Track moving scenes explicitly, so their double state survives retargeting and idle scenes perform no filtering. Retain the final dirty publication and dependency refresh; release editing state only once the cell and its relevant descendants settle. The existing prepared evaluator and host-to-engine interpolation remain responsible for audio-rate modulation.
3. Keep the old linear policy pinned. Factor common mathematical contracts over the control policy, with separate historical/current configurations. Prove one-pole bounds, contraction, finite settling, target continuity, and settled no-op behavior before changing production sources. Numerical rounding and native optimization correspondence remain explicit C++ test obligations.
4. Use a conservative finite progress budget in the model in addition to the exact exponential settling rule. Early exact settling is observationally equivalent to further processing at the target; it must not require waiting out that budget in C++.
5. Preserve raw recorded controls and compare the new implementation with the existing investigation. Add burst and reversal regressions, audio-rate modulation checks, and idle/moving benchmarks.

## Risks / Trade-offs

- A 20 ms time constant adds intentional lag: about 60 ms to 95% and 92 ms to 99%. Full-scale settling at `1e-6` takes roughly 277 ms.
- Float state can stall before the threshold. Double state and native endpoint/reversal tests guard against this.
- Premature parent settlement can freeze nested edits. Keep ancestors dirty until descendants settle and cover this through the real matrix.
- Filter state can be lost on patch/gesture operations. Test matching reloads, inherited values and nested controls.
- A parameter filter placed after modulation would attenuate audio-rate sources. Only stored scene values receive the new one-pole.

## Migration Plan

First validate Lean models and contracts, then implement C++ and update source correspondence. Run core, formal, audio and focused performance checks before landing. Patches continue to serialize targets; no format migration is required.
