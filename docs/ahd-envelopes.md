# Phase-Driven AHD Envelopes

`AHD` in `private/src/AHD.hpp` integrates attack, hold, and decay using changes in phase-derived elapsed time. At a trigger, its input captures:

- the current sample's modulated global phase;
- the relevant source loop's cycle ratio to the global loop;
- the envelope period in samples.

The running envelope computes:

```
samples = abs(globalPhase - startGlobalPhase) * phaseRatio * envelopePeriodSamples
deltaSamples = samples - previousSamples
```

It retains an attack, hold, or decay stage and double-precision stage progress. Attack adds `deltaSamples * attackIncrement` to the level; decay subtracts `deltaSamples * decayIncrement`. Hold accumulates fractional progress using `deltaSamples / (holdLoops * envelopePeriodSamples)` while its output stays at 1. Any time left after a stage boundary carries into the next stage, in either direction, so fractional samples and large phase steps preserve timing. Zero-length hold is skipped on the next nonzero time step.

Changing attack, hold, or decay affects subsequent progress without recalculating the envelope's past. With phase stationary, changing these controls leaves the raw output unchanged. Changing hold during hold preserves the fraction already completed; changing it during decay cannot move the envelope back into hold. Retriggering resets the elapsed-time baseline and starts attack from the current raw output.

It does not retain a source loop index, track topology edits, or reconstruct winding. A multiplier or parent edit during attack, hold, or decay cannot change the captured timing. Retriggering captures the new timing. In the production voice path the source ratio comes from loop 0; the voice cycle ratio used to calculate the envelope period is a separate quantity. That voice ratio is the undoubled LCM of the selected clock and read loop ratios, and `envelopePeriodSamples = globalPeriodSamples / voiceCycleRatio`. The gate still closes after half a voice cycle; the Theory of Time rhythm advances once per complete loop cycle.

The absolute distance preserves existing reverse behavior: moving back toward the trigger reverses progression through decay, hold, and attack while the envelope is running; moving past the trigger increases distance again. Reversal uses the current control values, without replaying historical knob changes. Reverse attack is bounded by the level captured at the trigger. Once decay reaches idle, moving backward does not restart the envelope. Explicit release uses sample-driven decay, independent of phase motion. `AHD::Process(input)` decrements a double-precision raw output by `decayIncrement * input.m_releaseFactor`, preserving long release durations at the engine rate; rendered output remains float. The factor defaults to 1 engine sample per call and affects only explicit release, leaving phase-driven attack, hold, and running decay unchanged.

Each AHD instance consumes its own input events: `Process` clears both `m_trig` and `m_release`, including an ignored release while idle or a release superseded by a simultaneous trigger. The next call continues the envelope without caller cleanup. `m_triggered` reports whether the most recent call consumed a trigger, so scope consumers can record starts after processing. The shared `AHDControl` is not consumed; amp, modulation, and physical-model envelopes each receive their own input copy.

The running stage reads the current sample's modulated global phase on every call. Trigger and release are consumed on the sample they arrive.

Physical modeling's `ProcessWithAHD` runs its AHD and then the source once per engine sample, leaving `input.m_releaseFactor` at its default of 1. A new trigger on the next sample still retriggers the envelope and captures the previous raw output once. Running stages use phase-derived sample positions and use physical-model attack from 100 µs to 2.5 s and decay from 500 µs to 2.5 s. With a 10 ms attack and phase advancing one engine sample per call, eight calls starting at the trigger end at position 7 and raw output `7 / SampleTimer::SecondsToSamples(0.01)` (`7 / 1920` at 192 kHz).

Attack and decay controls remain sample-based increments. Hold is configured in loop periods (`m_holdLoops`) and converted with the captured envelope period during integration. Live hold changes therefore remain available without accidentally adopting a new topology's period. Tempo or phase modulation changes global phase motion and consequently envelope progression. The outer Idle/Running/Release state and `m_changed` lifecycle notifications are independent of the internal Attack/Hold/Decay stage.

Physical-model presets can have shorter or longer holds after the move to phase-driven hold. The earlier control setter converted hold using a temporary input's default period. It now maps the current hold control to a loop count and uses the period captured at the trigger, like the other envelope sources. Attack and decay mappings use engine-sample units.

`AHDControl` carries trigger, release, source phase ratio, and envelope period. It no longer relays an elapsed-sample counter from the gate. See [Multi-Phasor Gate](multi-phasor-gate.md) for how the voice period is captured.

The multi-phasor gate requests explicit release when an active note reaches or passes half its captured voice cycle and the voice is muted or new triggers are disabled (stopped or early-muted). Stopping the global clock also resets the gate and requests release. The gate keeps the release request set until the next accepted trigger; copying it into an envelope input again is harmless while that envelope is already releasing or idle. Normal attack–hold–decay completion does not require a release request.
